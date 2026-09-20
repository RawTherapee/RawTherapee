/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 *
 Placeholder functions to build LibRaw w/o postprocessing tools
 
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

int LibRaw::dcraw_process(void)
{
  return LIBRAW_NOT_IMPLEMENTED;
}

void LibRaw::fuji_rotate() {}
void LibRaw::convert_to_rgb_loop(float /*out_cam*/ [3][4]) {}
libraw_processed_image_t *LibRaw::dcraw_make_mem_image(int *) {
  return NULL;
}
libraw_processed_image_t *LibRaw::dcraw_make_mem_thumb(int *){ return NULL;}
void LibRaw::lin_interpolate_loop(int * /*code*/, int /*size*/) {}
void LibRaw::scale_colors_loop(float /*scale_mul*/[4]) {}
