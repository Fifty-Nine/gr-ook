/* -*- c++ -*- */

#define OOK_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "ook_swig_doc.i"

%{
#include "ook/decode.h"
%}


%include "ook/decode.h"
GR_SWIG_BLOCK_MAGIC2(ook, decode);