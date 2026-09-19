# AI-assisted film negative conversion

**Use AI** asks a configured vision model to compare conversions of the current
photograph. RawTherapee renders every image. The model proposes Film Negative
settings; it does not generate or replace photographic content.

This is an optional, single-photo workflow. Manual controls remain available,
and no model is selected or downloaded automatically.

## Configure a provider

Open **Preferences → AI** and fill in these fields:

| Field | Purpose |
| --- | --- |
| Provider | Ollama, OpenAI, or an OpenAI-compatible service |
| API base URL | The server's base URL, without the chat or responses route |
| Vision model | An explicitly selected model that accepts images and returns JSON |
| API key environment variable | The name of the variable containing the credential, if required |
| Request timeout | Seconds per model request; default 120, allowed range 1–600 |

| Provider | Example base URL | API used |
| --- | --- | --- |
| Ollama (default) | `http://localhost:11434` | `/api/chat` |
| OpenAI | `https://api.openai.com/v1` | `/responses` |
| OpenAI-compatible | `http://localhost:1234/v1` | `/chat/completions` |

Use **Test connection** to send a generated test image. The test does not use the
open photograph, but a remote provider may charge for it. A successful test
confirms that the service accepted the image request and returned the expected
JSON; it does not measure photographic judgment.

For Ollama, start the server and enter the exact name of an already installed
vision model. Local Ollama normally does not need a credential. A model must
support image input; a text-only model is insufficient.

For OpenAI, set the credential field to an environment-variable name such as
`OPENAI_API_KEY`. Set that variable in the environment that launches RawTherapee.
**Do not paste the API key into Preferences.** The field is validated as an
environment-variable name before Preferences can be saved. Apps launched from Finder on macOS
do not automatically inherit variables from a terminal shell. Other compatible
services may also require an environment variable containing a bearer token.

Provider settings are saved in RawTherapee's application options. API key values
are read at request time and are not saved in options or processing profiles.

## Convert a negative

1. Open the photograph and crop to the film frame, excluding the holder and
   surrounding background. Set rotation and the desired inversion color space.
2. Enable **Film Negative** and wait for its preview to finish.
3. Click **Use AI**. The comparison dialog identifies the provider and model and
   shows progress. Further conversion requests are disabled while it is open.
4. Compare **Current conversion** with **AI conversion**. Choose **Apply** to
   accept the settings or **Cancel** to retain the current edit.

AI can change the reference exponent, red and blue exponent ratios, input RGB
reference, output level, and Film Negative color-balance controls. It does not
change the inversion color space, the separate White Balance tool, crop,
rotation, exposure, or other tools.

Apply creates one history entry. Accepted settings remain editable and are saved
in the existing Film Negative processing-profile fields. If the model selects
the current conversion, Apply stays disabled because there is no proposed edit.
Batch editing is not supported.

### Cropped film photographs

A small negative surrounded by a dark holder can produce an almost-black initial
conversion: the normal reference estimate includes the larger source image.
With an active crop, Use AI also evaluates a reference sampled from the interior
of the cropped frame. Sampling follows the editor's rotation and transformation
mapping. This supplies a more useful starting point without changing the edit.
It is still only a candidate until selected and applied.

### What happens during comparison

There are at most three model requests per click:

1. Inspect the current conversion and, when available, the crop-based reference;
   propose up to three alternatives.
2. Compare their locally rendered previews; select a conversion and optionally
   propose one refinement.
3. If a refinement was proposed, compare it with the previous winner.

The model is asked to preserve natural color, skin tones, intentional warm or
cool lighting, and tonal separation while limiting clipping. It may retain the
current conversion. Results depend on the model and photograph; review color
and brightness before applying.

The timeout applies to each HTTP request, not the entire operation. Rendering
also takes time and may use the full-resolution processing pipeline before
reducing the output. Cancel interrupts HTTP requests and prevents further work.
An engine render already in progress must finish before the dialog can close;
the dialog remains responsive while it waits.

## Privacy and network behavior

The configured provider receives pixel-only sRGB PNG previews, capped at 1,024
pixels on the longest edge, and numeric Film Negative settings. Requests do not
include RAW files, source filenames, EXIF, or processing profiles. Image content
itself remains visible to the provider.

Remote endpoints require HTTPS; plain HTTP is permitted only for loopback
addresses. The endpoint and chosen model determine where inference runs; an
Ollama server can itself use a cloud-backed model. No providers are contacted
until a connection test or conversion is requested. There are no automatic
retries, redirects, provider changes, or model downloads. OpenAI requests set
`store: false`; that setting does not replace the provider's data policies.

The feature does not log credentials, request bodies, provider response bodies,
or image payloads. Invalid responses and failed requests never apply partial
settings. Cancel, an image switch, editor closure, or a changed editing revision
prevents a pending result from being applied.

## Troubleshooting

| Symptom | Action |
| --- | --- |
| Asked to enable Film Negative | Turn on the tool, then wait for the initial preview. Cropping alone does not enable inversion. |
| Asked to wait for reference values or a legacy profile | Let the editor finish its normal reference calculation or legacy-profile upgrade before retrying. |
| Initial preview is almost black | Crop tightly to the film frame before Use AI so the crop-based reference can be evaluated. |
| Connection failed | Check the server, base URL, and TLS configuration. For local Ollama, ensure the server is running. |
| Authentication failed or credential missing | Check the variable name and whether the process launching RawTherapee has the variable set. |
| Invalid, incomplete, or rejected model output | Check image and structured-output support. The selected model/service may be incompatible. |
| Request timed out | Choose a faster installed vision model or increase the timeout. No retry is sent automatically. |
| Result is too dark or changes the scene's atmosphere | Cancel or adjust the Film Negative controls manually. A successful request does not guarantee the best photographic result. |

Developer architecture, dependencies, and validation instructions are in
[the implementation notes](../devnotes/ai-negative-conversion.md).
