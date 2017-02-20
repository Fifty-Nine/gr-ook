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

#ifndef INCLUDED_OOK_COROUTINE_H
#define INCLUDED_OOK_COROUTINE_H

#include <memory>

namespace gr
{
namespace ook
{
namespace util
{
class coroutine
{
  private:
    struct coroutine_impl;
    std::unique_ptr<coroutine_impl> impl;

    virtual void run() = 0;
    virtual void on_exit()
    {
    }
    virtual void on_reset()
    {
    }

  public:
    coroutine();
    virtual ~coroutine();

    void resume();

  protected:
    void reset();
    void yield();
};

} // namespace util
} // namespace ook
} // namespace gr

#endif /* INCLUDED_OOK_COROUTINE_H */
