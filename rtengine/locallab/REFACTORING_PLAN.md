# iplocallab.cc Refactoring Plan

## Overview
Split the 23,430-line `iplocallab.cc` into multiple files, one per tool/category.

## Proven Extraction Method

### Step-by-step process for each function:

1. **Find function boundaries:**
   ```bash
   grep -n "^void ImProcFunctions::FUNCNAME" rtengine/iplocallab.cc
   ```

2. **Read only the function (use line numbers from grep):**
   - Read from function start to next function start - 1

3. **Create/append to target file** with these includes:
   ```cpp
   #include <cmath>
   #include <cstdlib>
   #include <fftw3.h>

   #include "improcfun.h"
   #include "labimage.h"
   #include "rt_math.h"
   #include "sleef.h"
   #include "gauss.h"
   #include "guidedfilter.h"
   #include "cplx_wavelet_dec.h"  // for wavelet functions
   #include "jaggedarray.h"
   #include "color.h"

   #ifdef _OPENMP
   #include <omp.h>
   #endif

   namespace rtengine {
   // ... function code ...
   } // namespace rtengine
   ```

4. **Replace original in iplocallab.cc:**
   ```cpp
   // FUNCNAME moved to locallab/iplocallab_TOOL.cc
   ```

5. **Add to CMakeLists.txt** (if new file):
   ```cmake
   locallab/iplocallab_TOOL.cc
   ```

6. **Build and verify:**
   ```bash
   cmd.exe /c "C:\msys64\home\Billou\RawTherapee\build.bat"
   ```

---

## Function Inventory (with line ranges)

Format: `StartLine-EndLine | FunctionName | ~Lines | Status`

### Already Extracted:
- [x] `addGaNoise` (was 5339-5404, ~65 lines) -> `iplocallab_denoise.cc`
- [x] `mean_sig`, `log_encode`, `getAutoLogloc` -> `iplocallab_log.cc`
- [x] `tone_eqcam`, `tone_eqcam2`, `tonemapFreemanQ`, `tonemapFreeman`, `loccont`, `tone_eqdehaz` -> `iplocallab_tonemap.cc`
- [x] `ciecamloc_02float` (~1890 lines) -> `iplocallab_ciecam.cc`
- [x] `softproc`, `softprocess` (~145 lines) -> `iplocallab_softlight.cc`
- [x] `exlabLocal`, `exposure_pde` (~200 lines) -> `iplocallab_exposure.cc`
- Note: Helper functions `tone_eqsmooth`, `tone_eqblack` kept in iplocallab.cc (used by Lab_Local)
- Note: Helper functions `rolloff_freeman_function`, `scene_referred_contrast`, `get_freeman_parameters` moved to tonemap file
- Note: Helper functions `find_gray`, `sigmoidla`, `gamutjz` moved to ciecam file

### To Extract:

#### File: iplocallab_ciecam.cc (CIECAM Tool) ~1890 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| was 2504-4392 | ciecamloc_02float | 1889 | DONE |

#### File: iplocallab_softlight.cc (Soft Light Tool) ~145 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| was 2404-2501 | softproc | 99 | DONE |
| was 2504-2547 | softprocess | 44 | DONE |

#### File: iplocallab_exposure.cc (Exposure Tool) ~200 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| was 2406-2530 | exlabLocal | 125 | DONE |
| was 6977-7049 | exposure_pde | 73 | DONE |

#### File: iplocallab_denoise.cc (Denoise Tool) ~2750 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| 5339-5341 | addGaNoise | 65 | DONE |
| 5342-5499 | DeNoise_Local | 157 | pending |
| 5500-5659 | DeNoise_Local2 | 159 | pending |
| 11663-11933 | fftw_denoise | 270 | pending |
| 12012-13908 | DeNoise | 1896 | pending |
| 14532-15318 | NLMeans | 786 | pending |

#### File: iplocallab_retinex.cc (Retinex Tool) ~420 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| 5660-5779 | InverseReti_Local | 119 | pending |
| 6516-6595 | discrete_laplacian_threshold | 79 | pending |
| 6596-6647 | rex_poisson_dct | 51 | pending |
| 6648-6669 | mean_dt | 21 | pending |
| 6670-6723 | normalize_mean_dt | 53 | pending |
| 6724-6899 | retinex_pde | 175 | pending |

#### File: iplocallab_blur.cc (Blur Tool) ~560 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| 5780-6223 | InverseBlurNoise_Local | 443 | pending |
| 9287-9437 | BlurNoise_Local | 150 | pending |
| 9982-10142 | fftw_convol_blur | 160 | pending |
| 10143-10187 | fftw_convol_blur2 | 44 | pending |
| 10188-10393 | fftw_tile_blur | 205 | pending |

#### File: iplocallab_mask.cc (Mask Utilities) ~960 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| 6224-6369 | blendstruc | 145 | pending |
| 6370-6474 | deltaEforMask | 104 | pending |
| 6475-6515 | laplacian | 40 | pending |
| 6900-7652 | maskcalccol | 752 | pending |
| 14464-14531 | detail_mask | 67 | pending |

#### File: iplocallab_sharp.cc (Sharpening Tool) ~275 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| 7653-7798 | InverseSharp_Local | 145 | pending |
| 7799-7928 | Sharp_Local | 129 | pending |

#### File: iplocallab_exclude.cc (Exclude Tool) ~140 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| 7929-8070 | Exclude_Local | 141 | pending |

#### File: iplocallab_transit.cc (Transit/Shape Detection) ~870 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| 8071-8324 | transit_shapedetect_retinex | 253 | pending |
| 8325-8597 | transit_shapedetect | 272 | pending |
| 8598-8939 | transit_shapedetect_getBalance | 341 | pending |
| 9438-9907 | transit_shapedetect2 | 469 | pending |

#### File: iplocallab_common.cc (Common Utilities) ~560 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| 8940-9286 | calc_ref | 346 | pending |
| 11934-12011 | recovm | 77 | pending |
| 14044-14463 | avoidcolshi | 419 | pending |

#### File: iplocallab_wavelet.cc (Wavelet/CBDL Tool) ~1100 lines
| Lines | Function | ~Size | Status |
|-------|----------|-------|--------|
| 10394-10491 | wavcbd | 97 | pending |
| 10492-10600 | Compresslevels | 108 | pending |
| 10601-10708 | wavlc | 107 | pending |
| 10709-10935 | wavcont | 226 | pending |
| 10936-11662 | wavcontrast4 | 726 | pending |
| 13909-14043 | clarimerge | 134 | pending |

#### File: iplocallab_main.cc (Keep in iplocallab.cc)
| Line | Function | Status |
|------|----------|--------|
| 15319 | Lab_Local | KEEP - main entry point |

---

## File Structure After Refactoring

```
rtengine/
├── iplocallab.cc           # Lab_Local + anonymous namespace constants
├── iplocallab.h            # Shared types (local_params, constants)
└── locallab/
    ├── iplocallab_common.cc    # calc_ref, recovm, avoidcolshi
    ├── iplocallab_log.cc       # Log encoding functions
    ├── iplocallab_tonemap.cc   # Tone mapping functions
    ├── iplocallab_ciecam.cc    # CIECAM functions
    ├── iplocallab_softlight.cc # Soft light functions
    ├── iplocallab_exposure.cc  # Exposure functions
    ├── iplocallab_denoise.cc   # Denoise functions (started)
    ├── iplocallab_retinex.cc   # Retinex functions
    ├── iplocallab_blur.cc      # Blur functions
    ├── iplocallab_mask.cc      # Mask utility functions
    ├── iplocallab_sharp.cc     # Sharpening functions
    ├── iplocallab_exclude.cc   # Exclude functions
    ├── iplocallab_transit.cc   # Transit/shape detection
    └── iplocallab_wavelet.cc   # Wavelet/CBDL functions
```

---

## CMakeLists.txt Updates Needed

Add these lines after `locallab/iplocallab_common.cc`:
```cmake
    locallab/iplocallab_denoise.cc
    locallab/iplocallab_log.cc
    locallab/iplocallab_tonemap.cc
    locallab/iplocallab_ciecam.cc
    locallab/iplocallab_softlight.cc
    locallab/iplocallab_exposure.cc
    locallab/iplocallab_retinex.cc
    locallab/iplocallab_blur.cc
    locallab/iplocallab_mask.cc
    locallab/iplocallab_sharp.cc
    locallab/iplocallab_exclude.cc
    locallab/iplocallab_transit.cc
    locallab/iplocallab_wavelet.cc
```

---

## Progress Tracking

### Session 1 (current):
- [x] Analyzed file structure
- [x] Created extraction method
- [x] Test extracted: addGaNoise -> iplocallab_denoise.cc
- [x] Verified build succeeds
- [x] Created this plan

### Next Session Tasks:
1. Continue with iplocallab_denoise.cc (add remaining denoise functions)
2. Then proceed file by file

---

## Important Notes

1. **Anonymous namespace constants** in iplocallab.cc (lines 65-100) are used by many functions. Keep them in iplocallab.cc or move to iplocallab.h.

2. **local_params struct** is already in iplocallab.h - good!

3. **Lab_Local function** (line 15319, ~8000 lines) stays in iplocallab.cc - it's the main entry point that calls all other functions.

4. **Include dependencies vary by function** - check each function for:
   - `<fftw3.h>` - for FFT functions
   - `"cplx_wavelet_dec.h"` - for wavelet functions
   - `"guidedfilter.h"` - for guided filter
   - `"imagesource.h"` - for ImageSource*

5. **Build command:**
   ```
   cmd.exe /c "C:\msys64\home\Billou\RawTherapee\build.bat"
   ```

---

## Resume Instructions

To resume this refactoring:

1. Read this file to see current progress
2. Check which functions are marked "pending" vs "DONE"
3. For each pending function:
   - Grep to find exact line numbers
   - Read only that function's code
   - Add to appropriate target file
   - Replace in iplocallab.cc with comment
   - Build to verify
4. Update this file's status tables as you go

---

## Quick Commands for Claude

### To check current function line numbers (they shift as we extract):
```
grep -n "^void ImProcFunctions::FUNCNAME" rtengine/iplocallab.cc
```

### To read a specific function (adjust line numbers):
```
Read file rtengine/iplocallab.cc from line START to line END
```

### Standard file header for new tool files:
```cpp
/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2016-2024 Jacques Desmis <jdesmis@gmail.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>
#include "improcfun.h"
#include "labimage.h"
#include "rt_math.h"
#include "sleef.h"
#include "gauss.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace rtengine {

// Functions go here

} // namespace rtengine
```

### Build command:
```
cmd.exe /c "C:\msys64\home\Billou\RawTherapee\build.bat"
```

### Replacement text for removed functions:
```cpp
// FUNCNAME moved to locallab/iplocallab_TOOL.cc
```
