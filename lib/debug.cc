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

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "debug.h"

using namespace gr;
using namespace gr::ook;
using namespace gr::ook::util;

namespace
{
debug_flags::type init_debug_flags()
{
    debug_flags::type result = debug_flags::none;

    if (getenv("OOK_DECODE_DEBUG") != 0) {
        result = (debug_flags::type)(result | debug_flags::decode);
    }

    return result;
}

debug_flags::type enabled = init_debug_flags();
}

bool gr::ook::util::debugEnabled(debug_flags::type flag)
{
    return enabled & flag;
}

void gr::ook::util::debug(debug_flags::type from, const char* fmt, ...)
{
    if (!debugEnabled(from)) return;

    fprintf(stderr, "debug: ");

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
