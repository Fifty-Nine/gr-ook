/* -*- c++ -*- */
/*
 * Copyright 2017 <+YOU OR YOUR COMPANY+>.
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

#ifndef INCLUDED_OOK_PACKET_SOURCE_IMPL_H
#define INCLUDED_OOK_PACKET_SOURCE_IMPL_H

#include <ook/packet_source.h>
#include <memory>

namespace gr
{
namespace ook
{
class packet_source_impl : public packet_source
{
  private:
    struct impl;
    std::unique_ptr<impl> impl_;

  public:
    packet_source_impl(const std::vector<char>& nibbles, int sample_rate = 32000);
    ~packet_source_impl();

    // Where all the action really happens
    int work(
      int noutput_items,
      gr_vector_const_void_star& input_items,
      gr_vector_void_star& output_items);
};

} // namespace ook
} // namespace gr

#endif /* INCLUDED_OOK_PACKET_SOURCE_IMPL_H */
