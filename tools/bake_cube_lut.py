#!/usr/bin/env python3
"""Bake a technical 3D Cube LUT for RawTherapee Film Simulation.

RawTherapee associates a Film Simulation LUT with an RGB profile through the
profile name at the end of the filename. It converts linear working RGB to
that profile, applies the sRGB transfer function, evaluates the LUT, removes
the sRGB transfer function, and converts back to the working profile.

This tool composes a source LUT's input/output colour transforms into that
convention. Generated LUTs target Rec.2020 and must end in ``_Rec2020.cube``.
Like RawTherapee's ordinary Film Simulation path, the result is limited to
non-negative linear values in the range 0..1 and uses encoded-space strength
blending.
"""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, Iterable, Iterator, List, Sequence, Tuple


RGB = Tuple[float, float, float]
Matrix3 = Tuple[RGB, RGB, RGB]


SRGB_TO_XYZ: Matrix3 = (
    (0.4124564, 0.3575761, 0.1804375),
    (0.2126729, 0.7151522, 0.0721750),
    (0.0193339, 0.1191920, 0.9503041),
)

XYZ_TO_SRGB: Matrix3 = (
    (3.2404542, -1.5371385, -0.4985314),
    (-0.9692660, 1.8760108, 0.0415560),
    (0.0556434, -0.2040259, 1.0572252),
)

REC2020_TO_XYZ: Matrix3 = (
    (0.6369580483, 0.1446169036, 0.1688809752),
    (0.2627002120, 0.6779980715, 0.0593017165),
    (0.0000000000, 0.0280726930, 1.0609850577),
)

XYZ_TO_REC2020: Matrix3 = (
    (1.7166511880, -0.3556707838, -0.2533662814),
    (-0.6666843518, 1.6164812366, 0.0157685458),
    (0.0176398574, -0.0427706133, 0.9421031212),
)


GAMUT_ALIASES = {
    "srgb": "srgb",
    "bt709": "srgb",
    "rec2020": "rec2020",
    "fgamut": "rec2020",
    "f-gamut": "rec2020",
}

TRANSFER_ALIASES = {
    "srgb": "srgb",
    "flog2": "flog2",
    "f-log2": "flog2",
}


def clamp01(value: float) -> float:
    return max(0.0, min(1.0, value))


def matrix_vector(matrix: Matrix3, vector: RGB) -> RGB:
    return tuple(
        sum(matrix[row][column] * vector[column] for column in range(3))
        for row in range(3)
    )  # type: ignore[return-value]


def convert_gamut(rgb: RGB, source: str, destination: str) -> RGB:
    source = normalize_gamut(source)
    destination = normalize_gamut(destination)

    if source == destination:
        return rgb

    source_to_xyz = SRGB_TO_XYZ if source == "srgb" else REC2020_TO_XYZ
    xyz_to_destination = XYZ_TO_SRGB if destination == "srgb" else XYZ_TO_REC2020
    return matrix_vector(xyz_to_destination, matrix_vector(source_to_xyz, rgb))


def srgb_encode(value: float) -> float:
    value = clamp01(value)
    if value <= 0.003040:
        return 12.92310 * value
    return 1.055 * math.pow(value, 1.0 / 2.4) - 0.055


def srgb_decode(value: float) -> float:
    value = clamp01(value)
    if value <= 0.039286:
        return value / 12.92310
    return math.pow((value + 0.055) / 1.055, 2.4)


def flog2_encode(value: float) -> float:
    if value >= 0.000889:
        return 0.245281 * math.log10(5.555556 * value + 0.064829) + 0.384316
    return 8.799461 * value + 0.092864


def flog2_decode(value: float) -> float:
    if value >= 0.100686685370811:
        return math.pow(10.0, (value - 0.384316) / 0.245281) / 5.555556 - 0.064829 / 5.555556
    return (value - 0.092864) / 8.799461


ENCODERS: Dict[str, Callable[[float], float]] = {
    "srgb": srgb_encode,
    "flog2": flog2_encode,
}

DECODERS: Dict[str, Callable[[float], float]] = {
    "srgb": srgb_decode,
    "flog2": flog2_decode,
}


def normalize_gamut(value: str) -> str:
    try:
        return GAMUT_ALIASES[value.lower()]
    except KeyError as exc:
        raise ValueError(f"unsupported gamut: {value}") from exc


def normalize_transfer(value: str) -> str:
    try:
        return TRANSFER_ALIASES[value.lower()]
    except KeyError as exc:
        raise ValueError(f"unsupported transfer function: {value}") from exc


def map_channels(function: Callable[[float], float], rgb: RGB) -> RGB:
    return function(rgb[0]), function(rgb[1]), function(rgb[2])


@dataclass(frozen=True)
class CubeLUT:
    size: int
    values: Sequence[RGB]
    domain_min: RGB = (0.0, 0.0, 0.0)
    domain_max: RGB = (1.0, 1.0, 1.0)
    metadata: Dict[str, str] | None = None
    title: str | None = None

    def sample(self, rgb: RGB) -> RGB:
        # Adobe Cube LUT Specification 1.0, sections 7.1 and 8, specifies
        # tetrahedral interpolation for values between 3D table samples.
        fractions = []
        bases = []

        for channel in range(3):
            normalized = (
                (rgb[channel] - self.domain_min[channel])
                / (self.domain_max[channel] - self.domain_min[channel])
            )
            scaled = clamp01(normalized) * (self.size - 1)
            base = min(self.size - 2, int(math.floor(scaled)))
            bases.append(base)
            fractions.append(scaled - base)

        red, green, blue = bases
        red_fraction, green_fraction, blue_fraction = fractions
        level = self.size
        level_square = level * level
        base_index = red + green * level + blue * level_square

        if red_fraction >= green_fraction:
            if green_fraction >= blue_fraction:
                offsets = (1, 1 + level)
                ordered_fractions = (red_fraction, green_fraction, blue_fraction)
            elif red_fraction >= blue_fraction:
                offsets = (1, 1 + level_square)
                ordered_fractions = (red_fraction, blue_fraction, green_fraction)
            else:
                offsets = (level_square, level_square + 1)
                ordered_fractions = (blue_fraction, red_fraction, green_fraction)
        elif red_fraction >= blue_fraction:
            offsets = (level, level + 1)
            ordered_fractions = (green_fraction, red_fraction, blue_fraction)
        elif green_fraction >= blue_fraction:
            offsets = (level, level + level_square)
            ordered_fractions = (green_fraction, blue_fraction, red_fraction)
        else:
            offsets = (level_square, level_square + level)
            ordered_fractions = (blue_fraction, green_fraction, red_fraction)

        vertices = (
            self.values[base_index],
            self.values[base_index + offsets[0]],
            self.values[base_index + offsets[1]],
            self.values[base_index + 1 + level + level_square],
        )

        result = []
        for channel in range(3):
            c0, c1, c2, c3 = (vertex[channel] for vertex in vertices)
            f1, f2, f3 = ordered_fractions
            result.append(c0 + f1 * (c1 - c0) + f2 * (c2 - c1) + f3 * (c3 - c2))

        return tuple(result)  # type: ignore[return-value]


@dataclass(frozen=True)
class TransformSpec:
    input_gamut: str
    input_transfer: str
    output_gamut: str
    output_transfer: str

    def normalized(self) -> "TransformSpec":
        return TransformSpec(
            normalize_gamut(self.input_gamut),
            normalize_transfer(self.input_transfer),
            normalize_gamut(self.output_gamut),
            normalize_transfer(self.output_transfer),
        )


def parse_three_floats(arguments: Sequence[str], keyword: str, line_number: int) -> RGB:
    if len(arguments) != 3:
        raise ValueError(f"line {line_number}: {keyword} requires three values")
    try:
        return tuple(float(value) for value in arguments)  # type: ignore[return-value]
    except ValueError as exc:
        raise ValueError(f"line {line_number}: invalid {keyword} value") from exc


def read_cube(path: Path) -> CubeLUT:
    size: int | None = None
    domain_min: RGB = (0.0, 0.0, 0.0)
    domain_max: RGB = (1.0, 1.0, 1.0)
    values: List[RGB] = []
    metadata: Dict[str, str] = {}
    title: str | None = None

    with path.open("r", encoding="utf-8-sig") as source:
        for line_number, raw_line in enumerate(source, 1):
            line = raw_line.strip()
            if not line:
                continue

            if line.startswith("#"):
                comment = line[1:].strip()
                if ":" in comment:
                    key, value = comment.split(":", 1)
                    metadata[key.strip().lower()] = value.strip()
                continue

            line = line.split("#", 1)[0].strip()
            if not line:
                continue

            fields = line.split()
            keyword = fields[0].upper()
            arguments = fields[1:]

            if keyword == "TITLE":
                title = line[len(fields[0]):].strip().strip('"')
            elif keyword == "LUT_3D_SIZE":
                if len(arguments) != 1:
                    raise ValueError(f"line {line_number}: LUT_3D_SIZE requires one value")
                try:
                    size = int(arguments[0])
                except ValueError as exc:
                    raise ValueError(f"line {line_number}: invalid LUT_3D_SIZE") from exc
                if not 2 <= size <= 256:
                    raise ValueError(f"line {line_number}: LUT_3D_SIZE must be between 2 and 256")
            elif keyword == "DOMAIN_MIN":
                domain_min = parse_three_floats(arguments, keyword, line_number)
            elif keyword == "DOMAIN_MAX":
                domain_max = parse_three_floats(arguments, keyword, line_number)
            elif keyword.startswith("LUT_1D"):
                raise ValueError(f"line {line_number}: 1D/shaper LUTs are not supported")
            elif keyword in {"LUT_3D_INPUT_RANGE", "LUT_3D_INPUT_TABLE"}:
                raise ValueError(f"line {line_number}: {keyword} is not supported")
            else:
                if size is None:
                    raise ValueError(f"line {line_number}: LUT data precedes LUT_3D_SIZE")
                values.append(parse_three_floats(fields, "LUT entry", line_number))
                if len(values) > size * size * size:
                    raise ValueError(f"line {line_number}: too many LUT entries")

    if size is None:
        raise ValueError("missing LUT_3D_SIZE")
    if any(domain_min[channel] >= domain_max[channel] for channel in range(3)):
        raise ValueError("DOMAIN_MIN must be lower than DOMAIN_MAX for every channel")
    expected_entries = size * size * size
    if len(values) != expected_entries:
        raise ValueError(f"expected {expected_entries} LUT entries, found {len(values)}")

    return CubeLUT(size, values, domain_min, domain_max, metadata, title)


def infer_fujifilm_spec(lut: CubeLUT) -> TransformSpec | None:
    metadata = lut.metadata or {}
    gamma = metadata.get("gamma", "")
    gamut = metadata.get("gamut", "")

    if not gamma.startswith("F-Log2 to ") or gamut != "F-Gamut to ITU-R BT.709":
        return None

    output_transfer = "flog2" if gamma == "F-Log2 to F-Log2" else "srgb"
    return TransformSpec("rec2020", "flog2", "srgb", output_transfer)


def resolve_transform_spec(
    lut: CubeLUT,
    preset: str,
    input_gamut: str | None,
    input_transfer: str | None,
    output_gamut: str | None,
    output_transfer: str | None,
) -> TransformSpec:
    inferred: TransformSpec | None = None
    if preset in {"auto", "fujifilm-flog2-bt709"}:
        inferred = infer_fujifilm_spec(lut)
        if preset == "fujifilm-flog2-bt709" and inferred is None:
            metadata = lut.metadata or {}
            gamma = metadata.get("gamma", "F-Log2 to ETERNA")
            inferred = TransformSpec(
                "rec2020",
                "flog2",
                "srgb",
                "flog2" if gamma == "F-Log2 to F-Log2" else "srgb",
            )

    values = {
        "input_gamut": input_gamut or (inferred.input_gamut if inferred else None),
        "input_transfer": input_transfer or (inferred.input_transfer if inferred else None),
        "output_gamut": output_gamut or (inferred.output_gamut if inferred else None),
        "output_transfer": output_transfer or (inferred.output_transfer if inferred else None),
    }
    missing = [name.replace("_", "-") for name, value in values.items() if value is None]
    if missing:
        raise ValueError(
            "could not infer the source transform; specify "
            + ", ".join(f"--{name}" for name in missing)
        )

    return TransformSpec(**values).normalized()  # type: ignore[arg-type]


def transform_sample(lut: CubeLUT, encoded_rec2020: RGB, spec: TransformSpec) -> RGB:
    spec = spec.normalized()

    linear_rec2020 = map_channels(srgb_decode, encoded_rec2020)
    linear_input = convert_gamut(linear_rec2020, "rec2020", spec.input_gamut)
    encoded_input = map_channels(ENCODERS[spec.input_transfer], linear_input)

    encoded_output = tuple(clamp01(value) for value in lut.sample(encoded_input))  # type: ignore[assignment]
    linear_output = map_channels(DECODERS[spec.output_transfer], encoded_output)
    linear_rec2020_output = convert_gamut(linear_output, spec.output_gamut, "rec2020")
    return map_channels(srgb_encode, linear_rec2020_output)


def iter_baked_values(lut: CubeLUT, spec: TransformSpec, size: int) -> Iterator[RGB]:
    denominator = float(size - 1)
    for blue in range(size):
        for green in range(size):
            for red in range(size):
                yield transform_sample(
                    lut,
                    (red / denominator, green / denominator, blue / denominator),
                    spec,
                )


def write_cube(
    path: Path,
    source_path: Path,
    source: CubeLUT,
    spec: TransformSpec,
    size: int,
    values: Iterable[RGB],
) -> None:
    metadata = source.metadata or {}
    source_gamma = metadata.get("gamma", spec.input_transfer)
    source_gamut = metadata.get("gamut", f"{spec.input_gamut} to {spec.output_gamut}")

    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(f'TITLE "{source.title or source_path.stem} - RawTherapee Rec2020"\n')
        output.write("# Generated by tools/bake_cube_lut.py\n")
        output.write(f"# Source: {source_path.name}\n")
        output.write(f"# Source gamma: {source_gamma}\n")
        output.write(f"# Source gamut: {source_gamut}\n")
        output.write("# RawTherapee profile: Rec2020\n")
        output.write("# Linear input range represented: 0.0 to 1.0\n")
        output.write(f"LUT_3D_SIZE {size}\n")
        output.write("DOMAIN_MIN 0.0 0.0 0.0\n")
        output.write("DOMAIN_MAX 1.0 1.0 1.0\n\n")
        for red, green, blue in values:
            output.write(f"{red:.10f} {green:.10f} {blue:.10f}\n")


def create_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="source 3D .cube LUT")
    parser.add_argument("output", type=Path, help="output path ending in _Rec2020.cube")
    parser.add_argument(
        "--preset",
        choices=("auto", "none", "fujifilm-flog2-bt709"),
        default="auto",
        help="source transform preset (default: infer from metadata)",
    )
    parser.add_argument("--size", type=int, default=65, help="generated cube size (default: 65)")
    parser.add_argument("--input-gamut", choices=tuple(GAMUT_ALIASES))
    parser.add_argument("--input-transfer", choices=tuple(TRANSFER_ALIASES))
    parser.add_argument("--output-gamut", choices=tuple(GAMUT_ALIASES))
    parser.add_argument("--output-transfer", choices=tuple(TRANSFER_ALIASES))
    return parser


def main(arguments: Sequence[str] | None = None) -> int:
    parser = create_argument_parser()
    options = parser.parse_args(arguments)

    if not 2 <= options.size <= 256:
        parser.error("--size must be between 2 and 256")
    if options.output.suffix.lower() != ".cube" or not options.output.stem.endswith("_Rec2020"):
        parser.error("output filename must end in _Rec2020.cube")

    try:
        source = read_cube(options.input)
        spec = resolve_transform_spec(
            source,
            options.preset,
            options.input_gamut,
            options.input_transfer,
            options.output_gamut,
            options.output_transfer,
        )
        write_cube(
            options.output,
            options.input,
            source,
            spec,
            options.size,
            iter_baked_values(source, spec, options.size),
        )
    except (OSError, ValueError) as exc:
        parser.error(str(exc))

    print(f"Wrote {options.size}x{options.size}x{options.size} Rec2020 LUT to {options.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
