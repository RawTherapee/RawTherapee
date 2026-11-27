# Cube LUT baking tool

`bake_cube_lut.py` converts a technical 3D Cube LUT into a LUT that can be
used by RawTherapee's Film Simulation tool.

Technical LUTs commonly expect and produce values in a vendor-specific colour
gamut and transfer function, such as Fujifilm F-Gamut and F-Log2. RawTherapee
instead associates a Film Simulation LUT with an RGB profile through the
profile suffix in its filename. The baking tool composes the required transfer
function and gamut conversions into a new LUT following RawTherapee's
`_Rec2020.cube` convention.

The tool requires Python 3 but uses only the Python standard library. Python is
not required to run RawTherapee or to use LUTs that have already been baked.

## Usage

Run the tool with an input Cube file and an output filename ending in
`_Rec2020.cube`:

```text
python3 bake_cube_lut.py INPUT.cube OUTPUT_Rec2020.cube
```

For example, the Fujifilm Reala Ace LUT can be converted with:

```text
python3 bake_cube_lut.py \
    FLog2_to_REALA-ACE_65grid_V.1.00.cube \
    Fujifilm_REALA-ACE_Rec2020.cube
```

The default `auto` preset reads Fujifilm's `Gamma` and `Gamut` comments and
recognizes F-Log2/F-Gamut LUTs that output ITU-R BT.709. The output grid is
65 x 65 x 65 by default.

Use `--size` to select another output grid size:

```text
python3 bake_cube_lut.py INPUT.cube OUTPUT_Rec2020.cube --size 33
```

`--size` controls the side length of the generated cube independently of the
source cube's `LUT_3D_SIZE`. For every point in the generated grid, the tool
applies the input gamut and transfer conversions and then samples the source
cube at the resulting coordinate. If that coordinate falls between source
samples, the tool uses tetrahedral interpolation. It then applies the output
conversions and writes the result into the generated cube. Consequently, a
smaller output size downsamples the composed transform, while a larger output
size upsamples it; increasing the size cannot recover detail that is absent
from the source LUT.

This interpolation follows the *Adobe Cube LUT Specification 1.0*. Section
7.1, "The Three-Dimensional Table", states that 3D table values shall be set
so tetrahedral interpolation generates the correct output. Section 8,
"Application Requirements", says that readers should use tetrahedral
interpolation for three-dimensional tables when an input lies between stored
sample points.

For a source LUT whose transforms cannot be inferred from metadata, specify
them explicitly:

```text
python3 bake_cube_lut.py INPUT.cube OUTPUT_Rec2020.cube \
    --preset none \
    --input-gamut rec2020 \
    --input-transfer flog2 \
    --output-gamut bt709 \
    --output-transfer srgb
```

Supported gamut names are `srgb`, `bt709`, `rec2020`, `fgamut`, and
`f-gamut`. Supported transfer functions are `srgb`, `flog2`, and `f-log2`.
F-Gamut is treated as Rec.2020, while BT.709 uses the same primaries as sRGB.

Run the following command for the complete command-line reference:

```text
python3 bake_cube_lut.py --help
```

## Input and output range

The generated LUT targets RawTherapee's Rec.2020 Film Simulation profile and
therefore must retain the `_Rec2020.cube` filename suffix. It represents
non-negative linear input values from 0 to 1; source-LUT input coordinates are
clamped to the source `DOMAIN_MIN` and `DOMAIN_MAX` while baking. Input values
outside the generated range remain subject to RawTherapee Film Simulation's
input clipping.

Source LUT results and the subsequent output transforms are not clamped. The
generated Cube can therefore contain finite encoded output values below 0 or
above 1. RawTherapee preserves those extended Cube outputs through its inverse
sRGB transfer and working-profile conversion, although later processing or
final export can still clip them.
