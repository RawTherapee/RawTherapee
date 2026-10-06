# AI negative conversion tests

The default tests use generated images, injected transports, and a loopback HTTP
server. They do not contact a provider, download a model, or require a real API
key. Python is a test-harness dependency only.

## Service and HTTP regression tests

From the repository root, with a C++17 compiler, CMake, libcurl development files
(version 7.62 or later), and Python 3:

```sh
cmake -S tests/ai-negative -B /tmp/rt-ai-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/rt-ai-tests --parallel 4
ctest --test-dir /tmp/rt-ai-tests --output-on-failure
```

The C++ tests cover provider payload/response adapters, strict settings
validation, malformed and unexpected fields, invalid numbers and candidate IDs,
the three-request ceiling, baseline/crop-candidate selection, and cancellation
between stages. The Python test runs the real libcurl transport against a local
HTTP mock, checking statuses, synthetic bearer credentials, credential/error
redaction, oversized responses, timeouts, cancellation during a request, and the
absence of redirects/retries. Python must be available for that test to be
registered; CMake reports when it is missing.

The `AI negative conversion tests` workflow runs these checks on Linux and macOS.
No workflow secret is needed.

## Desktop GUI and engine integration

Build RawTherapee's `rth` target using **Unix Makefiles** and
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, with a working desktop display. Then run:

```sh
python3 tests/ai-negative/run_gui.py /tmp/rawtherapee-ai-build /tmp/rt-ai-gui-tests
```

The script compiles a harness with the GUI's compile flags and links against the
existing GUI/engine objects. It renames the application's main entry point for
that harness. This is a developer integration tool; it currently does not support
Ninja, Visual Studio, or Xcode generators. It writes synthetic RGB/Bayer DNG
fixtures, profiles, preferences, screenshots, and its executable to the scratch
directory. Use a dedicated, disposable directory.

Coverage includes:

- Apply, Cancel, Escape, close, failure, stale-result handling, and an immutable
  provider configuration during a running dialog.
- One history notification and preservation of other processing tools.
- Profile round-tripping, history parameter restoration, and legacy-profile
  readiness/upgrade-flag restoration.
- Preferences persistence, validation before saving, default reset, repeated connection-test completion,
  and cancellation when Preferences is destroyed.
- Disabled batch-mode access and crop-reference median/invalid-sample behavior.
- Engine rendering of generated RGB and Bayer RAW inputs with crop and rotation.
- Pixel agreement between the RGB candidate and export using the same output
  settings, and the bounded, pixel-only PNG preview.

The stale-result tests inject an invalidated revision predicate. They do not
replace manual multi-editor/window lifecycle testing. Legacy tests check the
readiness guard and profile flag, not a collection of historical camera files.

## Optional local-model checks

These commands send images to a running **local Ollama server**. They are not
part of the default tests. Supply the name of an already installed vision model:

```sh
/tmp/rt-ai-tests/ai-negative-tests --vision MODEL_NAME
```

To run the complete comparison loop on the generated negative, supply the model explicitly:

```sh
python3 tests/ai-negative/run_gui.py /tmp/rawtherapee-ai-build /tmp/rt-ai-gui-tests --ollama MODEL_NAME
```

For a local RGB negative with an existing cropped/rotated profile:

```sh
python3 tests/ai-negative/run_gui.py /tmp/rawtherapee-ai-build /tmp/rt-ai-photo-test \
  --photo /path/to/negative.jpeg /path/to/negative.jpeg.pp3
```

This renders the baseline and crop-reference PNGs without changing the source or
profile. Append an installed Ollama model name to also run the comparison:

```sh
python3 tests/ai-negative/run_gui.py /tmp/rawtherapee-ai-build /tmp/rt-ai-photo-test \
  --photo /path/to/negative.jpeg /path/to/negative.jpeg.pp3 MODEL_NAME
```

The photo harness enables Film Negative in its private parameter copy and expects
an active crop. Output files remain in the scratch directory. Do not add private
photographs, sidecars, generated previews, credentials, or logs to the repository.
Live cloud-provider validation is a separate, explicitly configured test.
