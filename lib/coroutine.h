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
/*
 * Implements a simple coroutine API. Clients should inherit from this class
 * and implement the body of the coroutine in the 'run()' method. The coroutine
 * can be started (or resumed) by calling 'resume()'. The coroutine function can
 * call 'yield()' to yield back to the calling context.
 */
class coroutine
{
  private:
    struct coroutine_impl;
    std::unique_ptr<coroutine_impl> impl;

    /* The body of the coroutine. */
    virtual void run() = 0;

    /* Callback invoked when the coroutine exits. */
    virtual void on_exit() {}
    /* Callback invoked when the coroutine is reset. */
    virtual void on_reset() {}
  public:
    coroutine();
    virtual ~coroutine();

    /* Resume execution of the coroutine. */
    void resume();

    /*
     * Reset the coroutine. Execution will resume from the
     * beginning of 'run()' and all local context will be
     * discarded.
     */
    void reset();

  protected:
    /*
     * Pause execution of this coroutine and resume the
     * calling context.
     */
    void yield();
};

} // namespace util
} // namespace ook
} // namespace gr

#endif /* INCLUDED_OOK_COROUTINE_H */
