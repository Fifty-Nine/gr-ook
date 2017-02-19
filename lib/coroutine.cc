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

#include "coroutine.h"
#include "debug.h"

#include <cassert>
#include <ucontext.h>

using namespace gr;
using namespace gr::ook;
using namespace gr::ook::util;

struct coroutine::coroutine_impl {
    ucontext_t run_ctxt;
    ucontext_t return_ctxt;
    ucontext_t main_ctxt;
    bool returned = true;

    static constexpr size_t rstack_size = 1 << 14;
    static constexpr size_t fstack_size = 1 << 12;
    char rstack[rstack_size];
    char fstack[fstack_size];

    void reset(coroutine* cr)
    {
        debug(debug_flags::coroutine, "coroutine reset %p\n", cr);
        assert(returned);
        returned = false;

        getcontext(&run_ctxt);
        getcontext(&return_ctxt);
        getcontext(&main_ctxt);

        run_ctxt.uc_stack.ss_sp = rstack;
        run_ctxt.uc_stack.ss_size = rstack_size;
        run_ctxt.uc_link = &return_ctxt;

        makecontext(&run_ctxt, (void (*)()) & run, 1, cr);
    }

    static void fallthrough(coroutine* cr)
    {
        debug(debug_flags::coroutine, "coroutine fallthrough %p\n", cr);
        cr->impl->returned = true;
        cr->on_exit();
    }

    void pre_run(coroutine* cr)
    {
        return_ctxt.uc_stack.ss_sp = fstack;
        return_ctxt.uc_stack.ss_size = fstack_size;
        return_ctxt.uc_link = &main_ctxt;

        makecontext(&return_ctxt, (void (*)()) & fallthrough, 1, cr);
    }

    static void run(coroutine* cr)
    {
        debug(debug_flags::coroutine, "coroutine run %p\n", cr);
        cr->impl->pre_run(cr);
        cr->run();
    }
};


coroutine::coroutine() : impl(new coroutine_impl{})
{
    reset();
}

coroutine::~coroutine()
{
}

void coroutine::resume()
{
    debug(debug_flags::coroutine, "coroutine resume %p\n", this);
    if (!impl->returned) {
        swapcontext(&impl->main_ctxt, &impl->run_ctxt);
    }
}

void coroutine::reset()
{
    impl->reset(this);
}

void coroutine::yield()
{
    debug(debug_flags::coroutine, "coroutine yield %p\n", this);
    swapcontext(&impl->run_ctxt, &impl->main_ctxt);
}
