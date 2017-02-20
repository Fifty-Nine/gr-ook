/* -*- c++ -*- */
/*
 * Copyright 2017 Tim Prince
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <cassert>
#include <deque>
#include <ucontext.h>

#include <cstdio>
#include <stdexcept>

#include "coroutine.h"
#include "debug.h"
#include "decode_impl.h"

using namespace gr;
using namespace gr::ook;
using namespace gr::ook::util;

namespace
{
bool within_range(double act, double exp, double tolerance)
{
    double max = exp * (1.0f + tolerance);
    double min = exp * (1.0f - tolerance);

    return (act > min) && (act < max);
}

struct timeout_error : public std::runtime_error {
    timeout_error() : std::runtime_error("timeout reading data")
    {
    }
};

struct too_many_bits_error : public std::runtime_error {
    too_many_bits_error() : std::runtime_error("exceeded max allowed data bits")
    {
    }
};
}

struct decode_impl::state : public util::coroutine {
    state(double tolerance_) : tolerance(tolerance_)
    {
    }

    double tolerance;
    bool need_reset = false;

    const float* data = nullptr;
    const float* endptr = nullptr;

    int sync_count = 0;
    int detected_width = 0;
    std::vector<char> packet_data;
    std::vector<char> packet_check;

    bool hasNext() const
    {
        return data != endptr;
    }

    float peekNext()
    {
        while (!hasNext()) {
            yield();
        }
        return *data;
    }

    float next()
    {
        while (!hasNext()) {
            yield();
        }

        float result = *data;
        data++;
        return result;
    }

    static bool is_high(float f)
    {
        return f > 0.5;
    }

    static bool is_low(float f)
    {
        return f < 0.5;
    }

    int count_until(bool (*fn)(float), int max = -1)
    {
        int count = 0;
        while (!fn(next()) && (max == -1 || count < max)) {
            count++;
        }
        if (max != -1 && count > max) {
            throw timeout_error{};
        }
        return count;
    }

    void wait_until(bool (*fn)(float), int max = -1)
    {
        (void)count_until(fn, max);
    }

    void print_packet()
    {
        for (int i = 0; i < sync_count; ++i) {
            printf("S");
        }
        printf("P ");

        for (size_t idx = 0;
             idx < std::max(packet_data.size(), packet_check.size());) {
            if (idx >= packet_data.size()) {
                printf("C");
            } else if (idx >= packet_check.size()) {
                printf("D");
            } else if (packet_data[idx] != packet_check[idx]) {
                printf("X");
            } else {
                printf("%c", packet_data[idx]);
            }

            if (++idx % 4 == 0) {
                printf(" ");
            }
        }
        printf("\n");
    }

    virtual void run() override
    {
        try {
            read_packet();
        } catch (const timeout_error& err) {
        } catch (const std::exception& ex) {
            debug(debug_flags::decode, "unhandled exception: %s\n", ex.what());
        }
    }

    virtual void on_exit() override
    {
        need_reset = true;
    }

    bool detect_sync_width()
    {
        detected_width = 0;
        int wait_time = -1;
        while (true) {
            int hi_count = count_until(&is_low, wait_time);
            int lo_count = count_until(&is_high, wait_time);

            if (detected_width > 1 && lo_count > (1.7 * detected_width)) {
                debug(
                  debug_flags::decode, "detected sync %d\n:", detected_width);
                return true;
            }

            int total = hi_count + lo_count;
            if (
              !within_range(hi_count, total / 2.0, tolerance) ||
              !within_range(lo_count, total / 2.0, tolerance)) {
                debug(
                  debug_flags::decode,
                  "bad sync: hi(%d) lo(%d) avg(%d)\n",
                  hi_count,
                  lo_count,
                  detected_width);
                return false;
            }

            detected_width =
              (detected_width * sync_count + hi_count) / (sync_count + 1);
            sync_count += 1;
            wait_time = detected_width * 4;
        }
    }

    void receive_data(std::vector<char>& out)
    {
        const int one_width = detected_width;
        const int zero_width = detected_width / 2;
        const int preamb_width = detected_width * 2;
        const int end_width = detected_width * 4;
        const int timeout = end_width * 2;

        while (true) {
            int hi = count_until(&is_low, timeout);

            bool logic_val;
            if (within_range(hi, one_width, tolerance)) {
                logic_val = true;
            } else if (within_range(hi, zero_width, tolerance)) {
                logic_val = false;
            } else {
                debug(
                  debug_flags::decode,
                  "Signal did not go low when expected.\n");
                debug(
                  debug_flags::decode,
                  "hi(%d) one(%d) zero(%d) bit(%d)\n",
                  hi,
                  (int)one_width,
                  (int)zero_width,
                  out.size());
                return;
            }

            int lo = count_until(&is_high, timeout);

            if (within_range(lo, preamb_width, tolerance)) {
                /* start of a mid-amble */
                out.pop_back();
                wait_until(&is_low, timeout);
                wait_until(&is_high, timeout);
                return;
            } else if (lo > end_width) {
                out.pop_back();
                return;
            } else if (within_range(lo, zero_width, tolerance)) {
            } else if (within_range(lo, one_width, tolerance)) {
            } else {
                debug(
                  debug_flags::decode,
                  "Signal did not go high when expected.\n");
                debug(
                  debug_flags::decode,
                  "hi(%d) lo(%d) one(%d) zero(%d) preamb(%d) bit(%d)\n",
                  hi,
                  lo,
                  (int)one_width,
                  (int)zero_width,
                  (int)preamb_width,
                  out.size());
                return;
            }

            out.push_back(logic_val ? '1' : '0');

            if (out.size() > 1024) {
                debug(debug_flags::decode, "Exceeded packet bit limit");
                throw too_many_bits_error{};
            }
        }
    }

    void read_packet()
    {
        wait_until(&is_high);

        if (!detect_sync_width()) {
            return;
        }

        int timeout = 4 * detected_width;

        int preamble_size = count_until(is_low, timeout);
        if (!within_range(preamble_size, 2.0 * detected_width, tolerance)) {
            debug(
              debug_flags::decode,
              "Bad preamble: %d != %d\n",
              preamble_size,
              2 * detected_width);
            return;
        } else {
            debug(
              debug_flags::decode,
              "preamble: actual(%d) expected(%d)\n",
              preamble_size,
              2 * detected_width);
        }

        wait_until(&is_high, timeout);

        debug(debug_flags::decode, "begin receive data\n");
        receive_data(packet_data);
        debug(debug_flags::decode, "begin receive check\n");
        receive_data(packet_check);

        print_packet();
    }

    void resume(const float* new_data, int size)
    {
        assert(!hasNext());

        data = new_data;
        endptr = data + size;

        while (hasNext()) {
            coroutine::resume();
            if (need_reset) {
                need_reset = false;
                reset();
            }
        }
    }

    ~state()
    {
        print_packet();
    }
};

decode::sptr decode::make(double tolerance)
{
    return gnuradio::get_initial_sptr(new decode_impl(tolerance));
}

/*
 * The private constructor
 */
decode_impl::decode_impl(double tolerance)
    : gr::block(
        "decode",
        gr::io_signature::make(1, 1, sizeof(float)),
        gr::io_signature::make(0, 0, 0)),
      state_(new state{tolerance})
{
}

/*
 * Our virtual destructor.
 */
decode_impl::~decode_impl()
{
    delete state_;
}

void decode_impl::forecast(
  int noutput_items,
  gr_vector_int& ninput_items_required)
{
}

int decode_impl::general_work(
  int noutput_items,
  gr_vector_int& ninput_items,
  gr_vector_const_void_star& input_items,
  gr_vector_void_star& output_items)
{
    state_->resume((const float*)input_items[0], ninput_items[0]);

    // Tell runtime system how many input items we consumed on
    // each input stream.
    consume_each(noutput_items);

    // Tell runtime system how many output items we produced.
    return noutput_items;
}
