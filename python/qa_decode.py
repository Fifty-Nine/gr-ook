#!/usr/bin/env python
# -*- coding: utf-8 -*-
# 
# Copyright 2017 Tim Prince
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
import json
import unittest
from itertools import chain

samples_dir = os.environ['OOK_TEST_SAMPLES_DIR']

def de_unicode(x):
  if isinstance(x, unicode):
    return x.encode('utf-8')
  if isinstance(x, list):
    return [de_unicode(y) for y in x]
  if isinstance(x, dict):
    return {de_unicode(k) : de_unicode(v) for k, v in x.iteritems()}
  return x

class qa_decode (gr_unittest.TestCase):

    def setUp (self):
        self.tb = gr.top_block ()

    def tearDown (self):
        self.tb = None

    def _run_test (self, src_block, tolerance):
      decode = ook.decode(tolerance)
      out = blocks.message_debug()
      self.tb.connect(src_block, decode)
      self.tb.msg_connect(decode, "packet", out, "store")
      self.tb.run()

      result = []
      for i in range(out.num_messages()):
        packet = pmt.to_python(out.get_message(i))
        packet['data'] = packet['data'].tolist()
        result.append(packet)

      return result

    def _data_test (self, data):
      nibbles = list(chain(*[(x >> 4, x & 0xf) for x in data]))
      packets = self._run_test(ook.packet_source(nibbles), tolerance=0.1)
      self.assertEqual(len(packets), 1)
      self.assertEqual(packets[0]['data'], data)
      self.assertEqual(packets[0]['valid_check'], True)
      self.assertEqual(packets[0]['bit_count'], 8 * len(data))

    def _file_test (self, test_spec):
      src = blocks.file_source(
          gr.sizeof_float * 1,
          str(os.path.join(samples_dir, test_spec['name'])),
          False
      )
      packets = self._run_test(src, test_spec['tolerance'])
      self.assertEqual(packets, test_spec['packets'])

    def test_samples (self):
      tests = json.load(
        open(os.path.join(samples_dir, 'decode-tests.json')),
        object_hook=de_unicode
      )

      for test_spec in tests:
        self._file_test(test_spec)

    def test_random (self):
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
      self._data_test([0x00] * 5)
      self._data_test([0xff] * 5)
      self._data_test([0x55] * 5)
      self._data_test([0xAA] * 5)
      self._data_test([0x12, 0x34, 0x56, 0x78, 0x9A])


if __name__ == '__main__':
    gr_unittest.run(qa_decode, "qa_decode.xml")
