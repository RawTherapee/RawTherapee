# AI film negative conversion: implementation notes

User-facing setup and behavior are documented in
[AI-assisted film negative conversion](../doc/AI-negative-conversion.md).

## Components and ownership

| Component | Responsibility |
| --- | --- |
| `rtgui/ainegative.h`, `ainegative.cc` | Provider configuration, JSON schema validation, cancellable libcurl transport, and the bounded comparison sequence; independent of GTK and the engine |
| `rtgui/ainegativegui.h`, `ainegativegui.cc` | Preferences controls, immutable rendering snapshots, crop-reference sampling, settings conversion, and the comparison dialog |
| `ToolPanelCoordinator::useFilmNegativeAI` | Capture the originating image and editing revision; reject stale results and forward accepted settings |
| `FilmNegative::applyAiSettings` | Update only Film Negative controls and emit one history event |
| `Options::aiNegative` | Persist provider configuration outside processing profiles |

The coordinator copies the current processing parameters, including pending
panel values, before starting comparison. Provider configuration is also copied
for the job, so changes to Preferences cannot alter an in-flight request. Each candidate render uses a separate
file-based `ProcessingJob` and image source. The worker never dereferences the
editor or a live GTK widget. It publishes progress and a final result to the
main thread; GTK updates run through a timeout callback.

The coordinator retains a shared lifetime/revision token. Edits, profile/history
changes, initialization, and image closure invalidate the revision. Before
Apply, both the originating source and the full parameter snapshot must still
match. The dialog's modal lifecycle and per-editor running guard prevent
concurrent conversion requests. Cancellation is checked before requests,
between renders, and after results arrive.

Crop-reference preparation reuses the editor's existing transformed linear spot
sampler. It takes a 5×5 grid of interior patches on the GTK thread before starting
the worker; network work and candidate renders run on the worker. Invalid samples
are discarded, at least five valid samples are required, and each channel uses
its median. The seed preserves the exponent controls, resets color balance, and
uses a neutral output reference near 18% linear intensity. The seed is an
additional candidate, not a change to the baseline.

## Provider contracts

- Ollama uses `/api/chat`, base64 PNGs in `messages[].images`, and a JSON schema
  in `format`. Requests disable streaming and thinking and cap output at 2,048
  tokens. Incomplete or truncated responses are rejected.
- OpenAI uses `/responses`, data-URL `input_image` content, strict JSON schema
  in `text.format`, and `store: false`.
- Compatible services use `/chat/completions` with data-URL `image_url` content.
  Because compatible servers differ in schema support, the schema is included
  in the prompt and responses are validated locally.

All providers go through the same internal validation. Numeric settings must be
finite and within the existing control ranges. Reference RGB must contain
exactly three values in 1–65535. Missing, duplicate, unexpected, or malformed
fields and unknown candidate IDs are rejected. Escaped NULs are rejected because
cJSON uses NUL-terminated field names and strings. Proposed slider values are
rounded to the controls' precision before rendering, and references use engine
float precision, so Apply reproduces the candidate parameters.

The comparison sequence sends two requests without refinement and three with
refinement. A cropped frame may add one locally rendered seed but does not add a
model request. The current conversion remains eligible at the ranking stage.
The response body is capped at 1 MiB. Errors expose translation keys, never raw
provider responses; unexpected rendering exceptions become a generic UI error.

API references:

- [Ollama chat](https://docs.ollama.com/api/chat)
- [OpenAI image input](https://developers.openai.com/api/docs/guides/images-vision)
- [OpenAI structured output](https://developers.openai.com/api/docs/guides/structured-outputs)

## Preview and profile boundaries

Rendering preserves crop, orientation, and surrounding processing settings.
Output is resized to a 1,024-pixel bounding box without upscaling, converted to
sRGB using the engine's no-output-profile fallback, and encoded from copied RGB
pixels. Source metadata is never copied into the PNG. Output framing is omitted
from comparison previews.

The comparison is an sRGB view, not an output-profile or printer soft proof.
Full-resolution export can differ through scaling, output sharpening, framing,
and the chosen export profile. Tests compare pixels against an export with
matching output settings; they do not assert that every export configuration is
visually identical to the comparison dialog.

Accepted settings reuse the existing `.pp3` fields. There is no new profile
version and no provider configuration or credential in a profile. Legacy
profiles must finish the engine's existing upgrade before AI is available.
Reading a restored profile resets the panel's prior upgrade flag.

## Build dependencies

The GUI requires libcurl 7.62 or later, discovered as `CURL::libcurl`, and the
platform threading library. The existing cJSON implementation is reused. The
libcurl notice is included in `licenses/curl_LICENSE`. No AI
SDK, Python runtime, or model is required to run the feature. The command-line
processor does not expose AI conversion or link the AI service.

Install the normal RawTherapee build dependencies plus the libcurl development
package. The Linux AppImage/CodeQL workflows install `libcurl4-openssl-dev`; the
Windows MSYS2 workflow installs `curl`. macOS can use the system SDK's libcurl
when it satisfies the minimum version. Packaging still needs normal platform
validation, including runtime dependencies and TLS trust configuration.

For a local macOS developer build, after installing the normal dependencies and
system `fmt` and `libraw`, an example is:

```sh
cmake -S . -B /tmp/rawtherapee-ai-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DOSX_DEV_BUILD=ON \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$(sw_vers -productVersion)" \
  -DOPTION_OMP=OFF \
  -DWITH_SYSTEM_FMT=ON \
  -DWITH_SYSTEM_LIBRAW=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build /tmp/rawtherapee-ai-build --target rth rth-cli --parallel 6
cmake --install /tmp/rawtherapee-ai-build
```

With `OSX_DEV_BUILD=ON`, the local executable and resources are installed beneath
`build/Release` in the source tree. Launch from the repository root with isolated
test settings and cache:

```sh
RT_SETTINGS="$PWD/build/ai-test-settings" \
RT_CACHE="$PWD/build/ai-test-cache" \
  "$PWD/build/Release/MacOS/rawtherapee"
```

This is a developer installation, not a signed/distributable `.app`. It does not
replace `/Applications/RawTherapee.app`. Restart the local executable after
rebuilding and reinstalling. Set a deployment target appropriate to the actual
installed dependencies; a build on a recent macOS release does not establish
compatibility with older releases.

## Validation

See [the test guide](../tests/ai-negative/README.md) for reproducible commands and
the boundary between offline tests and optional live requests. The added CI
workflow runs only service and localhost HTTP tests on Linux and macOS. Existing
platform workflows build the application; the GUI harness remains a desktop
integration check.

Before release, record platform/build versions, provider/model versions, test
results, and any remaining limitations in the pull request. In particular,
cloud mocks do not establish live-provider compatibility, and generated fixtures
do not establish photographic quality. Evaluate representative orange masks,
skin tones, mixed lighting, strong casts, and scenes without neutrals. Check
intentional warmth/coolness, highlight clipping, shadows, and Apply/Cancel and
history behavior. Use images with permission to share when posting examples.
