#!/usr/bin/env python
# -*- coding: utf-8 -*-
# 
# Copyright 2017 <+YOU OR YOUR COMPANY+>.
# 
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
# 

from gnuradio import gr, gr_unittest
from gnuradio import blocks
from time import sleep
import ook_swig as ook
from packet_source import packet_source

class qa_decode (gr_unittest.TestCase):

    def setUp (self):
        self.tb = gr.top_block ()

    def tearDown (self):
        self.tb = None

    def test_basic (self):
      src = blocks.file_source(
          gr.sizeof_float * 1,
          "/tank/data/file8_0_0.00000586.extended.dat",
          False
      )
      decode = ook.decode()
      self.tb.connect(src, decode)
      self.tb.run()

    def test_random (self):
      src = blocks.file_source(
          gr.sizeof_float * 1,
          "/dev/urandom",
          False
      )
      decode = ook.decode()
      self.tb.connect(src, decode)
      self.tb.start()
      sleep(0.25)
      self.tb.stop()
      self.tb.wait()
#
#    def _data_test (self, data):
#      src = packet_source(data)
#      decode = ook.decode()
#      self.tb.connect(src, decode)
#      self.tb.run()
#
#    def test_patterns (self):
#      self._data_test([0x00] * 11)
#      self._data_test([0xff] * 11)
#      self._data_test([0x55] * 11)
#      self._data_test([0xAA] * 11)


if __name__ == '__main__':
    gr_unittest.run(qa_decode, "qa_decode.xml")
