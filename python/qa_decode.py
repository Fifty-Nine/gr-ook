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

import os, sys, fnmatch
from fnmatch import fnmatch
import pmt

class qa_decode (gr_unittest.TestCase):

    def setUp (self):
        self.tb = gr.top_block ()

    def tearDown (self):
        self.tb = None

    def _run_test (self, src_block):
      decode = ook.decode(0.25)
      out = blocks.message_debug()
      self.tb.connect(src_block, decode)
      self.tb.msg_connect(decode, "packet", out, "store")
      self.tb.run()

      for i in range(out.num_messages()):
        print out.get_message(i)

    def _data_test (self, data):
      self._run_test(ook.packet_source(data))

    def _file_test (self, filename):
      print "Testing {0}".format(filename)
      src = blocks.file_source(
          gr.sizeof_float * 1,
          filename,
          False
      )
      self._run_test(src)

    def test_samples (self):
      print "Test samples"
      samples_dir = os.environ["OOK_TEST_SAMPLES_DIR"]
      files = [f for f in os.listdir(samples_dir) if fnmatch(f, '*.f32')]
      for f in files:
        self._file_test(os.path.join(samples_dir, f))

    def test_random (self):
      print "Test random"
      src = blocks.file_source(
          gr.sizeof_float * 1,
          "/dev/urandom",
          False
      )
      decode = ook.decode()
      out = blocks.message_debug()
      self.tb.connect(src, decode)
      self.tb.start()
      sleep(0.25)
      self.tb.stop()
      self.tb.wait()

    def test_patterns (self):
      print "Test 0s"
      self._data_test([0x0] * 11)
      print "Test 1s"
      self._data_test([0xf] * 11)
      print "Test 01s"
      self._data_test([0x5] * 11)
      print "Test 10s"
      self._data_test([0xA] * 11)
      print "Test counter"
      self._data_test([0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB])


if __name__ == '__main__':
    gr_unittest.run(qa_decode, "qa_decode.xml")
