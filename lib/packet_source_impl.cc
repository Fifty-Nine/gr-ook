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
    const int ms;

    float* out;
    float* endptr;

    worker(const std::vector<char>& init_data, int sample_rate)
        : in(init_data.begin(), init_data.end()),
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

    void blank()
    {
        produce_many(5 * ms, 0.0f);
    }

    void sync()
    {
        for (int i = 0; i < 40; ++i) {
            pulse(1.0f * ms);
        }
    }

    void preamble()
    {
        produce_many(1 * ms, 0.0f);
        produce_many(2 * ms, 1.0f);
        produce_many(ms / 2, 0.0f);
    }

    void midamble()
    {
        produce_many(1 * ms, 1.0f);
        produce_many(1 * ms, 0.0f);
        produce_many(1 * ms, 1.0f);
        produce_many(1 * ms, 0.0f);
        preamble();
    }

    void postamble()
    {
        pulse(1 * ms);
        pulse(1 * ms);
    }

    static float value(int idx)
    {
        return (idx & 1) ? 0.0f : 1.0f;
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
        blank();
    }

    int resume(float* data, int size)
    {
        out = data;
        endptr = data + size;
        coroutine::resume();

        return (int)(out - data);
    }
};

packet_source::sptr packet_source::make(
  const std::vector<char>& data,
  int sample_rate)
{
    return gnuradio::get_initial_sptr(
      new packet_source_impl{data, sample_rate});
}

/*
 * The private constructor
 */
packet_source_impl::packet_source_impl(
  const std::vector<char>& data,
  int sample_rate)
    : gr::sync_block(
        "packet_source",
        gr::io_signature::make(0, 0, 0),
        gr::io_signature::make(1, 1, sizeof(float))),
      worker_(new worker{data, sample_rate})
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
