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

#ifndef INCLUDED_OOK_DEBUG_H
#define INCLUDED_OOK_DEBUG_H

namespace gr
{
namespace ook
{
namespace util
{
namespace debug_flags
{
enum type { none = 0, decode = 1 };
}

bool debugEnabled(debug_flags::type);

void debug(debug_flags::type from, const char* fmt, ...);

} // namespace util
} // namespace ook
} // namespace gr

#endif /* INCLUDED_OOK_DEBUG_H */
