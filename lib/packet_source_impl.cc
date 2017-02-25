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

namespace {
const pmt::pmt_t packet_sym = pmt::mp("packets");
}

namespace gr
{
namespace ook
{
struct packet_source_impl::worker : public util::coroutine {
    int stop_after;
    int ms_between_xmit;
    const int ms;

    float* out;
    float* endptr;

    std::deque<std::vector<int>> packet_queue;

    worker(
        std::vector<int> init_data,
        int stop_after,
        int ms_between_xmit,
        int sample_rate) :
        stop_after(stop_after),
        ms_between_xmit(ms_between_xmit),
        ms(sample_rate / 1000),
        out(nullptr),
        endptr(nullptr)
    {
        if (!init_data.empty()) {
            packet_queue.push_back(init_data);
        }
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
        for (int i = 0; i < 8; ++i) {
            bit((c >> (7 - i)) & 1, i);
        }
    }

    void data(const std::vector<int>& packet)
    {
        for (int c : packet) {
            data(c);
        }
    }

    void enqueue(pmt::pmt_t packet)
    {
        packet_queue.push_back(s32vector_elements(packet));
    }

    void send_packet(const std::vector<int>& packet)
    {
        blank();
        sync();
        preamble();
        data(packet);
        midamble();
        data(packet);
        postamble();
        blank(ms_between_xmit);
        stop_after = std::max(-1, stop_after - 1);
    }

    void run()
    {
        while (stop_after) {
            while (packet_queue.empty()) {
                blank();
            }
            send_packet(packet_queue.front());
            packet_queue.pop_front();
        }
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
  const std::vector<int>& data,
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
  const std::vector<int>& data,
  int stop_after,
  int ms_between_xmit,
  int sample_rate) :
    gr::sync_block(
        "packet_source",
        gr::io_signature::make(0, 0, 0),
        gr::io_signature::make(1, 1, sizeof(float))
    ),
    worker_(
        new worker {
            data,
            stop_after,
            ms_between_xmit,
            sample_rate
        }
    )
{
    message_port_register_in(packet_sym);
    set_msg_handler(packet_sym, [this](pmt::pmt_t p) { worker_->enqueue(p); });
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
