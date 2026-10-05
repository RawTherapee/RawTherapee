/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 *
 Placeholder functions to build LibRaw w/o postprocessing 
 and preprocessing calls
 
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

void LibRaw::copy_fuji_uncropped(unsigned short /*cblack*/[4],
				 unsigned short * /*dmaxp*/) {}
void LibRaw::copy_bayer(unsigned short /*cblack*/[4], unsigned short * /*dmaxp*/){}
void LibRaw::raw2image_start(){}

