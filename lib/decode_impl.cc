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
#include <cstdio>
#include <deque>
#include <stdexcept>
#include <ucontext.h>

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
    timeout_error() :
        std::runtime_error("timeout reading data")
    { }
};

struct too_many_bits_error : public std::runtime_error {
    too_many_bits_error() :
        std::runtime_error("exceeded max allowed data bits")
    { }
};

struct bad_transition_error : public std::runtime_error {
    bad_transition_error() :
        std::runtime_error("signal did not transition when expected")
    { }
};

struct bad_midamble_error : public std::runtime_error {
    bad_midamble_error() :
        std::runtime_error("bad midamble")
    { }
};
}

struct decode_impl::worker : public util::coroutine {
    worker(double tolerance_) : tolerance(tolerance_)
    {
    }

    double tolerance;
    bool need_reset = false;

    const float* data = nullptr;
    const float* endptr = nullptr;

    int sync_count = 0;
    std::vector<bool> packet_data;
    std::vector<bool> packet_check;

    pmt::pmt_t packet_sym = pmt::mp("packet");
    std::deque<pmt::pmt_t> packet_queue;

    struct timing_params {
        timing_params() :
            one(0),
            zero(0),
            preamble(0),
            end(0),
            timeout(-1)
        { }

        timing_params(int width) :
            one(width),
            zero(width / 2),
            preamble(width * 2),
            end(width * 4),
            timeout(width * 8)
        { }

        int one, zero, preamble, end, timeout;
    } timing;

    virtual void on_reset() override
    {
        need_reset = false;
        sync_count = 0;
        packet_data.clear();
        packet_check.clear();
        timing = { };
    }

    bool has_next() const
    {
        return data != endptr;
    }

    float peek_next()
    {
        while (!has_next()) {
            yield();
        }
        return *data;
    }

    float next()
    {
        while (!has_next()) {
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

    int count_until(bool (*fn)(float), int max)
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
    int count_until(bool (*fn)(float))
    {
        return count_until(fn, timing.timeout);
    }

    void wait_until(bool (*fn)(float), int max)
    {
        (void)count_until(fn, max);
    }

    void wait_until(bool (*fn)(float))
    {
        wait_until(fn, timing.timeout);
    }

    std::string phy_pretty_packet()
    {
        std::ostringstream os;
        os << std::setw(2) << sync_count << "SP ";
        for (size_t idx = 0;
             idx < std::max(packet_data.size(), packet_check.size());) {
            if (idx >= packet_data.size()) {
                os << "C";
            } else if (idx >= packet_check.size()) {
                os << "D";
            } else if (packet_data[idx] != packet_check[idx]) {
                os << "X";
            } else {
                os << (packet_data[idx] ? '1' : '0');
            }

            if (++idx % 4 == 0) {
                os << " ";
            }
        }
        return os.str();
    }

    pmt::pmt_t pretty_packet(const std::vector<uint8_t> data, bool check_valid)
    {
        std::ostringstream os;
        os << std::setfill('0');
        os << std::setw(2) << sync_count << "S ";
        os << std::setw(3) << packet_data.size() << "B ";
        os << (check_valid ? "\u2713" : "\u2717");
        for (auto c : data) {
            os << " " << std::hex << std::setw(2) << (int)c;
        }
        return pmt::mp(os.str());
    }

    void push_bit(bool bit, uint8_t& c, size_t idx, std::vector<uint8_t>& out)
    {
        c <<= 1;
        c |= bit;
        if (((idx + 1) % 8) == 0) {
            out.push_back(c);
            c = 0;
        }
    }

    void produce_packet()
    {
        bool check_valid = true;

        const size_t num_bytes =
          (packet_data.size() / 8) + ((packet_data.size() % 8) != 0);
        uint8_t byte = 0;
        size_t idx = 0;
        std::vector<uint8_t> data;
        data.reserve(num_bytes);
        for (; idx < packet_data.size(); idx++) {
            if (idx >= packet_data.size()) {
                check_valid = false;
                break;
            }

            if (
              idx >= packet_check.size() ||
              packet_data[idx] != packet_check[idx]) {
                check_valid = false;
            }

            push_bit(packet_data[idx], byte, idx, data);
        }

        while (data.size() < num_bytes) {
            push_bit(0, byte, idx++, data);
        }

        auto phy_packet = phy_pretty_packet();
        debug(debug_flags::decode, "phy: %s\n", phy_packet.c_str());

        auto packet = pmt::make_dict();
        packet = dict_add(
            packet, pmt::mp("data"), pmt::init_u8vector(data.size(), data)
        );
        packet = dict_add(
            packet, pmt::mp("pretty"), pretty_packet(data, check_valid)
        );
        packet = dict_add(
            packet, pmt::mp("phy_pretty"), pmt::mp(phy_packet)
        );
        packet = dict_add(
            packet, pmt::mp("bit_count"), pmt::mp(packet_data.size())
        );
        packet = dict_add(
            packet, pmt::mp("sync_count"), pmt::mp(sync_count)
        );
        packet = dict_add(
            packet, pmt::mp("valid_check"), pmt::from_bool(check_valid)
        );

        packet_queue.push_back(packet);
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
        int detected_width = 0;
        int wait_time = -1;
        while (true) {
            int hi_count = count_until(&is_low, wait_time);
            int lo_count = count_until(&is_high, wait_time);

            if (detected_width > 1 && lo_count > (1.7 * detected_width)) {
                debug(
                  debug_flags::decode, "detected sync %d\n:", detected_width);
                timing = timing_params { detected_width };
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

    int receive_bit(bool (*fn)(float), std::vector<bool>& out)
    {
        if (out.size() > 1024) {
            debug(debug_flags::decode, "Exceeded packet bit limit");
            throw too_many_bits_error{};
        }

        int count = count_until(fn);
        if (within_range(count, timing.one, tolerance)) {
            out.push_back(true);
            return 0;
        } else if (within_range(count, timing.zero, tolerance)) {
            out.push_back(false);
            return 0;
        }

        return count;
    }

    void receive_data(std::vector<bool>& out)
    {
        while (true) {
            int lo = receive_bit(&is_high, out);

            if (within_range(lo, timing.preamble, tolerance)) {
                /* start of a mid-amble */
                if (!within_range(count_until(&is_low), timing.preamble, tolerance)) {
                    throw bad_midamble_error { };
                }
            } else if (lo > timing.end) {
                return;
            } else if (lo != 0) {
                debug(
                  debug_flags::decode,
                  "Signal did not go high when expected.\n");
                debug(
                  debug_flags::decode,
                  "lo(%d) one(%d) zero(%d) bit(%d)\n",
                  lo,
                  (int)timing.one,
                  (int)timing.zero,
                  out.size());
                return;
            }

            int hi = receive_bit(&is_low, out);
            if (hi != 0) {
                debug(
                  debug_flags::decode,
                  "Signal did not go low when expected.\n");
                debug(
                  debug_flags::decode,
                  "hi(%d) lo(%d) one(%d) zero(%d) preamb(%d) bit(%d)\n",
                  hi,
                  lo,
                  (int)timing.one,
                  (int)timing.zero,
                  (int)timing.preamble,
                  out.size());
            }

            if (lo != 0) {
                return;
            }
        }
    }

    void read_packet()
    {
        wait_until(&is_high);

        if (!detect_sync_width()) {
            return;
        }

        int preamble_size = count_until(is_low);
        if (!within_range(preamble_size, timing.preamble, tolerance)) {
            debug(
              debug_flags::decode,
              "Bad preamble: %d != %d\n",
              preamble_size,
              timing.preamble);
            return;
        } else {
            debug(
              debug_flags::decode,
              "preamble: actual(%d) expected(%d)\n",
              preamble_size,
              timing.preamble);
        }

        debug(debug_flags::decode, "begin receive data\n");
        receive_data(packet_data);
        debug(debug_flags::decode, "begin receive check\n");
        receive_data(packet_check);

        if (packet_data.size() > 0 && packet_check.size() > 0) {
            produce_packet();
        }
    }

    void resume(const float* new_data, int size)
    {
        assert(!has_next());

        data = new_data;
        endptr = data + size;

        while (has_next()) {
            coroutine::resume();
            if (need_reset) {
                reset();
            }
        }
    }

    bool has_packet()
    {
        return packet_queue.size();
    }

    pmt::pmt_t next_packet()
    {
        auto result = packet_queue.front();
        packet_queue.pop_front();
        return result;
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
      worker_(new worker{tolerance})
{
    message_port_register_out(worker_->packet_sym);
}

/*
 * Our virtual destructor.
 */
decode_impl::~decode_impl()
{
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
    worker_->resume((const float*)input_items[0], ninput_items[0]);

    while (worker_->has_packet()) {
        auto packet = worker_->next_packet();
        message_port_pub(worker_->packet_sym, packet);
    }

    // Tell runtime system how many input items we consumed on
    // each input stream.
    consume_each(noutput_items);

    // Tell runtime system how many output items we produced.
    return noutput_items;
}
