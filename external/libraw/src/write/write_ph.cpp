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


#include "../../internal/dcraw_defs.h"
int LibRaw::flip_index(int row, int col)
{
  if (flip & 4)
    SWAP(row, col);
  if (flip & 2)
    row = iheight - 1 - row;
  if (flip & 1)
    col = iwidth - 1 - col;
  return row * iwidth + col;
}

void LibRaw::write_ppm_tiff(){}
void LibRaw::jpeg_thumb_writer(FILE *tfp, char *t_humb, int t_humb_length){}
#if 0
void LibRaw::ppm_thumb(){}
void LibRaw::jpeg_thumb(){}
void LibRaw::rollei_thumb(){}
void LibRaw::ppm16_thumb(){}
void LibRaw::layer_thumb(){}
#endif