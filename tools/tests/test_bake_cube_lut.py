#!/usr/bin/env python3

import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import bake_cube_lut as baker


def cube_text(values, domain_min=(0, 0, 0), domain_max=(1, 1, 1), comments=""):
    lines = [comments, "LUT_3D_SIZE 2"]
    lines.append("DOMAIN_MIN " + " ".join(str(value) for value in domain_min))
    lines.append("DOMAIN_MAX " + " ".join(str(value) for value in domain_max))
    lines.extend(" ".join(str(channel) for channel in value) for value in values)
    return "\n".join(lines) + "\n"


class CubeParsingTest(unittest.TestCase):
    def read(self, contents):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.cube"
            path.write_text(contents, encoding="utf-8")
            return baker.read_cube(path)

    def test_domain_is_applied_to_input_coordinates(self):
        values = [
            (0, 0, 0), (1, 0, 0),
            (0, 1, 0), (1, 1, 0),
            (0, 0, 1), (1, 0, 1),
            (0, 1, 1), (1, 1, 1),
        ]
        lut = self.read(cube_text(values, (-1, -2, -3), (1, 2, 3)))
        self.assertEqual(lut.sample((-1, -2, -3)), (0.0, 0.0, 0.0))
        self.assertEqual(lut.sample((1, 2, 3)), (1.0, 1.0, 1.0))
        self.assertEqual(lut.sample((0, 0, 0)), (0.5, 0.5, 0.5))

    def test_rejects_reversed_domain(self):
        values = [(0, 0, 0)] * 8
        with self.assertRaisesRegex(ValueError, "DOMAIN_MIN"):
            self.read(cube_text(values, (1, 0, 0), (0, 1, 1)))

    def test_rejects_wrong_entry_count(self):
        with self.assertRaisesRegex(ValueError, "expected 8"):
            self.read(cube_text([(0, 0, 0)] * 7))

    def test_rejects_non_finite_values(self):
        values = [(0, 0, 0)] * 7 + [(float("inf"), 0, 0)]
        with self.assertRaisesRegex(ValueError, "non-finite LUT entry"):
            self.read(cube_text(values))


class TetrahedralInterpolationTest(unittest.TestCase):
    VALUES = (
        (0.02, 0.02, 0.02),
        (0.11, 0.11, 0.11),
        (0.23, 0.23, 0.23),
        (0.37, 0.37, 0.37),
        (0.41, 0.41, 0.41),
        (0.59, 0.59, 0.59),
        (0.61, 0.61, 0.61),
        (0.83, 0.83, 0.83),
    )

    def setUp(self):
        self.lut = baker.CubeLUT(2, self.VALUES)

    def expected(self, red, green, blue):
        c000, c100, c010, c110, c001, c101, c011, c111 = (
            value[0] for value in self.VALUES
        )
        if red >= green >= blue:
            return c000 + red * (c100 - c000) + green * (c110 - c100) + blue * (c111 - c110)
        if red >= blue > green:
            return c000 + red * (c100 - c000) + blue * (c101 - c100) + green * (c111 - c101)
        if blue > red >= green:
            return c000 + blue * (c001 - c000) + red * (c101 - c001) + green * (c111 - c101)
        if green > red >= blue:
            return c000 + green * (c010 - c000) + red * (c110 - c010) + blue * (c111 - c110)
        if green >= blue > red:
            return c000 + green * (c010 - c000) + blue * (c011 - c010) + red * (c111 - c011)
        return c000 + blue * (c001 - c000) + green * (c011 - c001) + red * (c111 - c011)

    def test_all_six_fraction_orderings(self):
        points = (
            (0.75, 0.50, 0.25),
            (0.75, 0.25, 0.50),
            (0.50, 0.25, 0.75),
            (0.50, 0.75, 0.25),
            (0.25, 0.75, 0.50),
            (0.25, 0.50, 0.75),
        )
        for point in points:
            with self.subTest(point=point):
                expected = self.expected(*point)
                self.assertAlmostEqual(self.lut.sample(point)[0], expected, places=14)

    def test_equal_boundaries_and_endpoints(self):
        self.assertEqual(self.lut.sample((0, 0, 0)), self.VALUES[0])
        self.assertEqual(self.lut.sample((1, 1, 1)), self.VALUES[7])
        expected = self.expected(0.5, 0.5, 0.5)
        self.assertAlmostEqual(self.lut.sample((0.5, 0.5, 0.5))[0], expected, places=14)


class TransferFunctionTest(unittest.TestCase):
    def test_extended_srgb_round_trip(self):
        for value in (-0.25, 0.0, 0.5, 1.0, 1.5, 2.0):
            with self.subTest(value=value):
                self.assertAlmostEqual(
                    baker.srgb_encode(baker.srgb_decode(value)),
                    value,
                    places=12,
                )

    def test_flog2_cut_points(self):
        self.assertAlmostEqual(baker.flog2_encode(0.000889), 0.100686685370811, places=14)
        self.assertAlmostEqual(baker.flog2_decode(0.100686685370811), 0.000889, places=14)

    def test_flog2_round_trip(self):
        for value in (-0.01, 0.0, 0.000889, 0.18, 1.0, 5.0, 10.0):
            with self.subTest(value=value):
                self.assertAlmostEqual(baker.flog2_decode(baker.flog2_encode(value)), value, places=12)


class TransformTest(unittest.TestCase):
    def test_fujifilm_metadata_detection(self):
        lut = baker.CubeLUT(
            2,
            [(0, 0, 0)] * 8,
            metadata={
                "gamma": "F-Log2 to ETERNA BLEACH BYPASS",
                "gamut": "F-Gamut to ITU-R BT.709",
            },
        )
        self.assertEqual(
            baker.infer_fujifilm_spec(lut),
            baker.TransformSpec("rec2020", "flog2", "srgb", "srgb"),
        )

    def test_flog2_preserving_metadata_detection(self):
        lut = baker.CubeLUT(
            2,
            [(0, 0, 0)] * 8,
            metadata={
                "gamma": "F-Log2 to F-Log2",
                "gamut": "F-Gamut to ITU-R BT.709",
            },
        )
        self.assertEqual(baker.infer_fujifilm_spec(lut).output_transfer, "flog2")

    def test_explicit_transform_without_preset(self):
        lut = baker.CubeLUT(2, [(0, 0, 0)] * 8)
        spec = baker.resolve_transform_spec(
            lut,
            "none",
            "f-gamut",
            "f-log2",
            "bt709",
            "srgb",
        )
        self.assertEqual(spec, baker.TransformSpec("rec2020", "flog2", "srgb", "srgb"))

    def test_identity_composition(self):
        identity = baker.CubeLUT(
            2,
            [
                (0, 0, 0), (1, 0, 0),
                (0, 1, 0), (1, 1, 0),
                (0, 0, 1), (1, 0, 1),
                (0, 1, 1), (1, 1, 1),
            ],
        )
        spec = baker.TransformSpec("rec2020", "srgb", "rec2020", "srgb")
        values = list(baker.iter_baked_values(identity, spec, 3))
        for blue in range(3):
            for green in range(3):
                for red in range(3):
                    index = red + green * 3 + blue * 9
                    expected = (red / 2, green / 2, blue / 2)
                    for actual_channel, expected_channel in zip(values[index], expected):
                        self.assertAlmostEqual(actual_channel, expected_channel, places=12)

    def test_extended_source_output_is_not_clamped(self):
        extended = (-0.25, 1.5, 2.0)
        lut = baker.CubeLUT(2, [extended] * 8)
        spec = baker.TransformSpec("rec2020", "srgb", "rec2020", "srgb")

        actual = baker.transform_sample(lut, (0.25, 0.5, 0.75), spec)

        for actual_channel, expected_channel in zip(actual, extended):
            self.assertAlmostEqual(actual_channel, expected_channel, places=12)

    def test_extended_values_are_written_to_cube(self):
        extended = (-0.25, 1.5, 2.0)
        source = baker.CubeLUT(2, [extended] * 8)
        spec = baker.TransformSpec("rec2020", "srgb", "rec2020", "srgb")

        with tempfile.TemporaryDirectory() as directory:
            directory_path = Path(directory)
            output_path = directory_path / "extended_Rec2020.cube"
            baker.write_cube(
                output_path,
                directory_path / "source.cube",
                source,
                spec,
                2,
                [extended] * 8,
            )
            written = baker.read_cube(output_path)
            contents = output_path.read_text(encoding="utf-8")

        self.assertEqual(written.values, [extended] * 8)
        self.assertIn("# Output values may be outside 0.0 to 1.0", contents)


if __name__ == "__main__":
    unittest.main()
