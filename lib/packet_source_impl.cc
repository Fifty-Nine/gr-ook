/* -*- c++ -*- */
/*
 * Copyright 2017 Tim Prince.
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
#include <vector>
#include "packet_source_impl.h"

#include "coroutine.h"

namespace gr
{
namespace ook
{
struct packet_source_impl::worker : public util::coroutine {
    std::vector<char> in;
    int stop_after;
    int ms_between_xmit;
    const int ms;

    float* out;
    float* endptr;

    worker(
        const std::vector<char>& init_data,
        int stop_after,
        int ms_between_xmit,
        int sample_rate) :
        in(init_data.begin(), init_data.end()),
        stop_after(stop_after),
        ms_between_xmit(ms_between_xmit),
        ms(sample_rate / 1000),
        out(nullptr),
        endptr(nullptr)
    {
    }

    void produce(float value)
    {
        while (out == endptr) {
            yield();
        }

        *out = value;
        out++;
    }

    void produce_many(int n, float value)
    {
        for (int i = 0; i < n; ++i) {
            produce(value);
        }
    }

    void pulse(int hi, int lo)
    {
        produce_many(hi, 1.0f);
        produce_many(lo, 0.0f);
    }

    void pulse(int w)
    {
        pulse(w, w);
    }

    void blank(int time = 10)
    {
        produce_many(time * ms, 0.0f);
    }

    void sync()
    {
        for (int i = 0; i < 40; ++i) {
            pulse(1.0f * ms);
        }
        produce_many(1.0 * ms, 1.0f);
    }

    void preamble()
    {
        produce_many(2 * ms, 0.0f);
        produce_many(2 * ms, 1.0f);
    }

    void midamble()
    {
        preamble();
    }

    void postamble()
    {
    }

    static float value(int idx)
    {
        return (idx & 1) ? 1.0f : 0.0f;
    }

    void zero(int idx)
    {
        produce_many(ms / 2, value(idx));
    }

    void one(int idx)
    {
        produce_many(1 * ms, value(idx));
    }

    void bit(bool v, int idx)
    {
        if (v)
            one(idx);
        else
            zero(idx);
    }

    void data(int c)
    {
        for (int i = 0; i < 4; ++i) {
            bit((c >> (3 - i)) & 1, i);
        }
    }

    void data()
    {
        for (int c : in) {
            data(c);
        }
    }

    void run()
    {
        blank();
        sync();
        preamble();
        data();
        midamble();
        data();
        postamble();
        stop_after = std::max(-1, stop_after - 1);

        /* Always blank for 10 ms after the final packet. */
        blank((stop_after == 0) ? 10 : ms_between_xmit);
    }

    int resume(float* data, int size)
    {
        if (stop_after == 0) {
            return 0;
        }

        out = data;
        endptr = data + size;
        coroutine::resume();

        return (int)(out - data);
    }
};

packet_source::sptr packet_source::make(
  const std::vector<char>& data,
  int stop_after,
  int ms_between_xmit,
  int sample_rate)
{
    return gnuradio::get_initial_sptr(
      new packet_source_impl {
        data,
        stop_after,
        ms_between_xmit,
        sample_rate
      }
    );
}

/*
 * The private constructor
 */
packet_source_impl::packet_source_impl(
  const std::vector<char>& data,
  int stop_after,
  int ms_between_xmit,
  int sample_rate)
    : gr::sync_block(
        "packet_source",
        gr::io_signature::make(0, 0, 0),
        gr::io_signature::make(1, 1, sizeof(float))
      ),
      worker_(new worker {
            data,
            stop_after,
            ms_between_xmit,
            sample_rate
      })
{
}

/*
 * Our virtual destructor.
 */
packet_source_impl::~packet_source_impl()
{
}

int packet_source_impl::work(
  int noutput_items,
  gr_vector_const_void_star& input_items,
  gr_vector_void_star& output_items)
{
    int result =
      worker_->resume(reinterpret_cast<float*>(output_items[0]), noutput_items);

    if (!result) return -1;
    return result;
}

} /* namespace ook */
} /* namespace gr */
