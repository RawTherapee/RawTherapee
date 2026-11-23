---
title: Phase-Conditioned Student-t Mixture Regression for X-Trans Demosaicing
subtitle: Method, reproducible corpus, native implementation, and experimental limitations
author:
  - Vaclav Haisman
institute: Independent researcher, Czech Republic
email: vhaisman@gmail.com
date: 5 September 2026
lang: en-US
documentclass: article
papersize: a4
geometry:
  - margin=24mm
fontsize: 10pt
mainfont: DejaVu Serif
sansfont: DejaVu Sans
monofont: DejaVu Sans Mono
titlepage: true
titlepage-color: 17324A
titlepage-text-color: FFFFFF
titlepage-rule-color: 8DB5D0
titlepage-rule-height: 2
header-left: Student-t mixture regression for X-Trans
toc: true
toc-depth: 2
numbersections: true
bibliography: doc/papers/xtrans-tgmr/citations.yml
csl: doc/papers/xtrans-tgmr/ieee.csl
link-citations: true
abstract: |
  We describe an experimental X-Trans demosaicer based on phase-conditioned Student-t mixture regression (TGMR). Each of eighteen observation contracts predicts the two missing center colors from the 49 measured samples in a 7 by 7 neighborhood. A common, color-balanced observable DC is removed before regression. Fixed-degree-of-freedom Student-t fitting, tempered mixture weights, and a nine-sample shortlist reduce brittle component selection and permit a compact CPU implementation. The released configuration uses 32 components per phase and evaluates eight full experts per pixel. We distinguish retrospective BSDS development evidence from a new, reproducible corpus of 5,000 rights-reviewed images and its separately trained production-v1 candidate. On 64,000 frozen coordinates from 500 diagnostic sources, that candidate obtains 34.064 dB versus 32.316 dB for Markesteijn three-pass, measured over all three center RGB channels. Average and aggregate tail errors improve, but 65 sources lose more than 0.5 dB and the worst paired loss is 6.170 dB. Subsequent synthetic hard-case replacement did not repair these failures sufficiently. The contribution is therefore a reproducible statistical demosaicing system and a documented quality–cost tradeoff, not a claim of universal superiority or completed release qualification. The corpus is publicly downloadable; model-license finalization, broader native-platform testing, and independent confirmation remain separate tasks.
keywords:
  - demosaicing
  - X-Trans
  - Student-t mixture regression
  - statistical image prior
  - reproducible corpus
  - RawTherapee
---

# Introduction and scope

Demosaicing estimates the two colors that a color filter array (CFA) does not
measure at each sensor location. The inverse problem is underdetermined:
different color images can produce the same scalar mosaic. Local geometry,
inter-channel correlation, and learned image statistics provide useful
priors, but do not remove this ambiguity.

Fujifilm's X-Trans array has a 6 by 6 repeating cell rather than Bayer's
2 by 2 cell. Its distinct local sampling patterns require explicit treatment
in a phase-dependent predictor. Dedicated frequency-domain work identifies
the associated luminance/chrominance replicas [@rafinazari2014]. RawTherapee's
Markesteijn implementation supplies a strong practical comparison; we cite
the source rather than attributing an unidentified paper to that algorithm
[@markesteijnsource].

This work develops a learned, non-neural local regressor. We use *TGMR* for
Student-t mixture regression, continuing terminology from an earlier
Gaussian-mixture-regression prototype. The final components are Student-t
distributions, not Gaussians. Each component supplies an affine predictor;
the input-dependent mixture weights make their combination nonlinear.

The engineering and experimental contributions are:

1. A task-reduced joint model of 49 measured values and two unmeasured
   center colors, separately fitted for eighteen X-Trans observation patterns.
2. Fixed-$\nu$ Student-t fitting and tempered, coarse-to-fine component
   weighting, with a frozen K32/S9/q8 inference contract.
3. A deterministic source and patch selection recipe, commercially reusable
   source-license filtering, and a downloadable authenticated patch corpus.
4. Algebraically specialized native inference, same-component SIMD batching,
   bounded worker storage, and integration with explicit runtime fallback.
5. An evidence-linked account of both average gains and substantial failures,
   including an unsuccessful hard-case retraining experiment.

None of Student-t mixtures, conditional affine regression, CFA-specific
predictors, or SIMD batching is claimed to be a new mathematical invention.
The contribution is their particular task formulation, combination,
implementation, and evaluation. We do not establish priority over every
possible combination in the literature or claim patent novelty.

This revision replaces an earlier research-only account. The historical
manuscript remains in Git. It distinguishes two model populations throughout:
the BSDS development models and the later *production-v1 candidate*. Here,
“production-v1” names an artifact, not a declaration that all quality and
release gates passed.

# Relation to previous work

## Linear statistics, residual interpolation, and learned priors

Joint spatial–chromatic LMMSE predicts missing colors using covariance learned
from complete image neighborhoods [@portilla2005]. A single joint Gaussian
conditioned on its observed coordinates is the affine version of this idea.
A mixture can represent several local statistical populations, but must infer
which populations are plausible from the incomplete CFA observation.

Residual interpolation instead reconstructs a guide and interpolates its
sampled residual [@kiku2016]. The corrected MLRI implementation in our
development sequence is a comparison, not part of TGMR inference. TGMR uses
no preliminary demosaic or guide-image target estimate.

Learned convolutional demosaicers also use population information
[@gharbi2016; @kokkinos2019]. We make no claim that this compact local
statistical model outperforms contemporary neural methods. Its intended
advantages are an inspectable inference rule, small fixed model, and a
portable CPU implementation without a neural-network runtime.

## What is, and is not, reproduced from JCS-GMM

Sandeep and Jacob's JCS-GMM learns a joint RGB patch distribution, selects a
component from its observed CFA marginal, and estimates missing values from
inter-channel covariance [@sandeep2019]. The published Bayer method uses
6 by 6 RGB patches, 150 full-covariance components, hard component selection,
overlapping aggregation, and image-adaptive updates.

Our earlier full-RGB X-Trans GMM experiment isolated the local conditional
estimator. It is not a numerical reproduction of that complete published
algorithm. In particular, a local center-only, soft-mixture baseline without
image adaptation cannot support a claim to have beaten the published
JCS-GMM system. Moving from that baseline to phase-conditioned GMR changed
dimension, component count, and model organization together; it was a design
sequence, not a controlled dimension-only ablation.

EPLL applies an externally learned patch mixture through a whole-image
restoration objective [@zoran2011]. TGMR does not optimize EPLL, average
overlapping reconstructed RGB patches, search nonlocal neighbors, or refine
the image iteratively. It predicts one center RGB vector at a time.

## Heavy tails and the inference approximation

Peel and McLachlan give a latent-scale formulation for fitting mixtures of
Student-t distributions [@peel2000]. We use that established construction
with fixed degrees of freedom. The conditional location of a multivariate
Student-t is affine in the observed coordinates [@ding2016].

However, the released predictor is not the exact conditional-MMSE estimator
of the fitted mixture: it regularizes observed scale blocks, applies a
temperature of four, and renormalizes over a shortlist. We distinguish these
modeling approximations from later algebraic optimizations that preserve the
chosen reduced predictor.

# Observation contract

## CFA phase and target coordinates

Let $c_p(i)$ be the measured color at position $i$ in a 7 by 7 patch
with phase $p$. Given complete training RGB $x$, gather
$y_i=x_{i,c_p(i)}$ in row-major spatial order. Thus
$y\in\mathbb R^{49}$ contains only physically observable scalar samples.
The two-dimensional target $t$ contains the missing center colors in
ascending RGB order:

| Measured center | Target order |
|:--|:--|
| R | G, B |
| G | R, B |
| B | R, G |

The canonical training matrix is

$$
\begin{bmatrix}
G&R&G&G&B&G\\
B&G&B&R&G&R\\
G&R&G&G&B&G\\
G&B&G&G&R&G\\
R&G&R&B&G&B\\
G&B&G&G&R&G
\end{bmatrix}.
$$

Enumerate origin rows 0 through 5, then origin columns 0 through 5, retaining
the first occurrence of each distinct translated 6 by 6 matrix. This yields
eighteen explicit observation/target contracts. Runtime matching uses the
actual camera CFA pattern, including its phase, rather than only the center
color. Supported rotated/reflected representations must match one of those
contracts; they do not cause RGB channel swaps. An unsupported pattern fails
before the result can be accepted.

The eighteen phase versions of a training patch are *not eighteen independent
photographs or patches*. They are deterministic views of the same underlying
RGB data. We report independent source and patch counts separately from
phase-expanded training or evaluation counts.

## Common observable DC

For each observed color, let $I_{p,c}=\{i:c_p(i)=c\}$. Define

$$
d_p(y)=\frac13\sum_{c\in\{R,G,B\}}
              \frac{1}{|I_{p,c}|}\sum_{i\in I_{p,c}}y_i.
$$

Use $\widetilde y=y-d_p(y)\mathbf1$ and
$\widetilde t=t-d_p(y)\mathbf1$. This equal-weights the three observed
color means rather than over-weighting the more frequent green samples.
The same full 49-sample DC is used by the coarse and full stages. No
ground-truth color or initializer enters this calculation at inference.

The joint training vector is
$z=[\widetilde y^{\mathsf T},\widetilde t^{\mathsf T}]^{\mathsf T}
\in\mathbb R^{51}$. Removing DC imposes a linear constraint on these
features, making scale regularization important rather than optional.

# Fitting and inference

## Fixed-degree-of-freedom Student-t fitting

For each phase independently, fit

$$
p(z)=\sum_{k=1}^{K}\pi_k\,\operatorname{St}_{\nu}(z;\mu_k,\Sigma_k),
\qquad K=32,\quad \nu=3.
$$

Here $\Sigma_k$ denotes a *scale* matrix. It is not the marginal
covariance: when $\nu>2$, the latter is
$\nu\Sigma_k/(\nu-2)$. The production trainer starts with deterministic
k-means++ and ten full-covariance Gaussian EM iterations, then performs thirty
fixed-$\nu$ Student-t iterations in float64. The research prototype used
Python and scikit-learn initialization [@pedregosa2011]; the production path
is a standalone C++ trainer.

For training sample $i$, compute the untempered joint responsibility
$r_{ik}=p(k\mid z_i)$, using log-sum-exp, and

$$
\delta_{ik}=(z_i-\mu_k)^{\mathsf T}\Sigma_k^{-1}(z_i-\mu_k),
\qquad u_{ik}=\frac{\nu+51}{\nu+\delta_{ik}}.
$$

Writing $N_k=\sum_i r_{ik}$ and $U_k=\sum_i r_{ik}u_{ik}$, the
updates are

$$
\pi_k^{+}=\frac{N_k}{N},\qquad
\mu_k^{+}=\frac{\sum_i r_{ik}u_{ik}z_i}{U_k},
$$

$$
\Sigma_k^{+}=
\frac{\sum_i r_{ik}u_{ik}(z_i-\mu_k^{+})(z_i-\mu_k^{+})^{\mathsf T}}{N_k}
+\epsilon I,\qquad \epsilon=10^{-6}.
$$

In particular, the scale denominator is $N_k$, not $U_k$. These are
the fixed-degree-of-freedom latent-scale updates [@peel2000]. Factorizations,
regularization, deterministic component ordering, and finite-value checks
are implementation requirements; regularization should not be mistaken for
a proof of strict likelihood monotonicity in finite arithmetic.
The implementation uses Cholesky factorization and triangular solves rather
than forming matrix inverses explicitly; the inverse notation specifies the
mathematics.

## Conditional component locations

Partition each joint component into observed and target blocks. The exported
predictor uses

$$
S_k=\Sigma_{yy,k}+\tau^2I,\qquad \tau=0.0003,
$$

$$
m_k(\widetilde y)=\mu_{t,k}
 +\Sigma_{ty,k}S_k^{-1}(\widetilde y-\mu_{y,k}).
$$

Without this added regularizer, the expression is the Student-t conditional
location [@ding2016]. Its conditional scale depends on the observation, but
that scale is not needed to compute the location. Here $\tau$ is a
model-mismatch/numerical regularizer, not a fitted sensor-noise standard
deviation. Adding $\tau^2 I$ is not claimed to implement the exact
convolution of a Student-t density with independent Gaussian sensor noise.

With $v_k=\widetilde y-\mu_{y,k}$, the observed log score is

$$
\ell_k=\log\pi_k-\frac12\log|S_k|
 -\frac{\nu+49}{2}\log\left(1+\frac{v_k^{\mathsf T}S_k^{-1}v_k}{\nu}\right),
$$

up to a phase-common constant. A dense tempered mixture would use

$$
\gamma_k=\frac{\exp(\ell_k/T)}{\sum_j\exp(\ell_j/T)},\qquad T=4.
$$

Because $T\ne1$, these are *tempered weights*, not ordinary Bayesian
posterior probabilities under the fitted mixture. Consequently, neither
their entropy nor their concentration is certified as calibrated confidence.

## K32/S9/q8: what is selected

Every query uses its fixed 7 by 7 neighborhood. There is no adaptive spatial
patch search. S9 denotes the central 3 by 3 marginal of the centered
observation. Score all 32 components on those nine scalar samples, retain
the eight highest-scoring IDs, and break ties by lower component ID. Call
the shortlist $Q$.

Evaluate full 49-dimensional scores and affine predictions only for $k\in Q$.
The final missing colors are

$$
\widehat t=d_p(y)\mathbf1+
\sum_{k\in Q}\frac{\exp(\ell_k/T)}{\sum_{j\in Q}\exp(\ell_j/T)}
                 m_k(\widetilde y).
$$

Copy the measured center component directly from the input. The inferred
components remain signed finite values; the engine adds no sample-dependent
clipping, denoising, or sharpening to this predictor.

```{=latex}
\begin{minipage}{\linewidth}
```

```text
For each output pixel:
    match the actual CFA to a stored phase contract
    gather 49 measured samples, reflecting only at the image boundary
    compute the color-balanced common DC and subtract it
    score 32 nine-sample marginals; select eight IDs stably
    evaluate their full scores and two-color affine predictors
    normalize the eight temperature-four weights; combine; add DC
    copy the measured center color exactly
```

```{=latex}
\end{minipage}
```

![The inference query has a fixed spatial support. Phase lookup and component shortlisting select predictors, not other image patches.](doc/papers/xtrans-tgmr/figures/pipeline.svg){width=100%}

K=32 models were fitted as K=32 mixtures, not made by deleting half the
components of a K=64 fit. Shortlisting is nevertheless an approximation to
dense K=32 inference, and the reduced model is not identical to dense K=64.

# Corpus construction and training-patch selection

## Development data versus the released corpus

The historical proof used 200 BSDS training images and 512 patches per image:
102,400 independent RGB patches, reused across all eighteen phase contracts.
Validation and test comparisons each used twenty sources with 24 by 24
center grids, or 11,520 centers. BSDS supports the development analysis
[@arbelaez2011], but it is not the source of the bundled production-v1
coefficients. Its research-oriented distribution terms motivated a separate
corpus.

The `tgmr-corpus-v1` release instead contains patches derived from 5,000
rights-reviewed images [@tgmrcorpus2026]. Its source allocation is:

| Catalog | Training | Validation | Diagnostic test | Total |
|:--|--:|--:|--:|--:|
| Open Images | 3,200 | 400 | 400 | 4,000 |
| Wikimedia Commons | 480 | 60 | 60 | 600 |
| Smithsonian Open Access | 320 | 40 | 40 | 400 |
| **Total** | **4,000** | **500** | **500** | **5,000** |

These are catalog sources, not guarantees of five thousand unprocessed
camera photographs. In particular, the supplementary collections include
artwork and objects. Catalog quotas diversify image characteristics; they
do not establish that the resulting distribution matches all camera RAWs.

Accepted catalog records have explicit CC BY or CC0 terms and complete
attribution under the project's documented review policy. Author grouping,
a five-image author cap, exact/pixel/perceptual duplicate checks, and human
people/ambiguous-duplicate review precede selection. Images must decode with
a usable color interpretation and satisfy the frozen resolution gate
(shortest side at least 512 pixels and at least 0.75 megapixels). Source
groups do not cross splits. The release records all individual source terms;
collection-level terms do not replace them. This describes the performed
metadata-based review, not a legal guarantee about every underlying work.

## Source balancing and nested training order

The classifier measures luminance, chroma, texture, saturation, clipping,
contrast, and related quality indicators. Selection balances measurable
signal strata while retaining content guardrails. Human review is not
replaced by an image classifier where people safety or duplicate ambiguity
requires a decision.

For learning curves, a deterministic nested order preserves the catalog
proportions at 250, 500, 1,000, 2,000, and 4,000 training sources. Within
catalogs, brightness/chroma/texture cells are interleaved. This avoids a
prefix consisting only of Open Images. The ordering changes neither source
membership nor review decisions. Validation and test sources are unchanged
across these fits.

## Exact coordinate-selection algorithm

There are 256 distinct 7 by 7 patches per training source and 128 per
validation/test source. Coordinates denote patch top-left corners. The
requested count is $n$. The candidate space contains $(W-6)(H-6)$ valid positions in the decoded linear
image; distinct coordinates may still have overlapping spatial support.

The algorithm in `patch_selection.cc` is deterministic for a fixed decoded
image and source seed:

1. Select 75% of the requested coordinates by seeded xorshift64* proposals
   modulo the number of valid positions, discarding repeated coordinates.
   These are approximately uniform spatial samples; modulo mapping has a
   negligible finite-generator bias and is not stratified sampling.
2. Form a separate unused-coordinate pool of up to
   $\max(4096,32n)$ candidates, bounded by the available positions.
3. Calculate each candidate's seven local statistics, described below.
4. Fill the remaining 25% by cycling through seventeen coverage scores,
   selecting the unused candidate with the highest score. Break equal
   scores by the lower flattened coordinate. If the pool is exhausted,
   use the first remaining valid coordinate.

For statistics only, RGB values are bounded to $[0,1]$. Per-pixel
luminance is $Y=0.2126R+0.7152G+0.0722B$; chroma is
$\max(R,G,B)-\min(R,G,B)$; saturation is chroma divided by the maximum
when it is nonzero. Average these quantities over the 49 samples. Contrast
is mean absolute luminance deviation from the patch mean. Texture is the
root mean square of the 84 horizontal and vertical adjacent luminance
differences. Clipping counts pixels whose maximum channel is within one
uint16 step of zero or one. Hue is a saturation-weighted circular mean.

The seventeen scores seek low/high luminance, low/high chroma, low/high
texture, high contrast, high clipping, high saturation, and eight evenly
spaced hue sectors. A hue-sector score is saturation times the cosine of
the angular distance to that sector's midpoint. Thus this is deliberate
signal-coverage enrichment, not selection by a demosaicer's error or by
ground-truth knowledge of which method would win.

```text
Select coordinates without replacement:
    192 spatial + 64 coverage for each training source
     96 spatial + 32 coverage for each evaluation source
Freeze coordinates and augmentation identities in the source manifest.
Extract RGB once; form eighteen CFA observation contracts during fitting.
```

The coverage portion biases the corpus toward specified signal extremes.
This is intentional and applies to evaluation too; reported pooled PSNR
describes this frozen sampling recipe, not a uniform average over all pixels
of all ordinary photographs. Required low/middle/high brightness, chroma,
and texture counts are checked independently after packing.
The released low/middle/high boundaries are 0.08/0.65 for mean luminance,
0.03/0.15 for mean chroma, and 0.01/0.05 for gradient RMS. Every such
marginal stratum contains at least 10,000 training and 1,000 records in each
evaluation split. These are marginal-coverage guarantees, not a claim that
all 27 joint cells are equally populated.

## Color interpretation, augmentation, and storage

Image decoding and color management precede patch extraction. The frozen
recipe starts from linearized RGB derived from rendered source images, not
paired sensor RAW and spectral ground truth. Exactly every fourth patch is
identity-transformed. For the remaining 75%, a source-hash/sequence rule
chooses an exposure from $\{-2,-1.5,-1,-0.5,0.5,1,1.5,2\}$ stops, one
of six fixed white-balance gain triplets, and a camera matrix. Gains span
0.5–2 before their Q12 quantization; four training matrix IDs and two held-out
evaluation matrix IDs are disjoint. Apply the matrix to linear RGB, multiply
by exposure and per-channel gains, clamp to $[0,1]$, and round to uint16.
The selected production recipe has no added
sensor noise. These transforms increase variation, but do not reconstruct
the original optics, sensor spectral sensitivities, or unknown processing
already embedded in JPEGs and other source images.

Importantly, identical validation/test bytes across augmentation candidates
do **not** mean unaugmented evaluation: both splits have their own fixed
transformations, including held-out camera matrices. Only training payloads
changed during augmentation selection. The released manifest freezes the
actual transformation of every record.

The corpus contains 1,024,000 training records and 64,000 records in each
evaluation split, for 1,152,000 total. TGPC is a data-only patch container:
each 384-byte record includes a channel-major, little-endian uint16 7 by 7
RGB patch and provenance/augmentation fields. The 256-byte header binds the
payload. The uncompressed file is 442,368,256 bytes. Deterministic gzip uses
the existing zlib dependency; neither the trainer nor runtime requires a
Python machine-learning library.

# Native implementation

## Algebraic specialization, not an additional likelihood approximation

For a $d$-dimensional observed marginal, set
$C_k=\log\pi_k-\tfrac12\log|S_k|$. With $\nu=3$, S9 ranking is
equivalent to ordering

$$
\frac{\exp((C_k-C_{\max})/6)}{1+\delta_k/3}.
$$

For the full $d=49$ stage and $T=4$, weights are proportional to

$$
\frac{\exp((C_k-C_{\max})/4)}{s_k^6\sqrt{s_k}},
\qquad s_k=1+\delta_k/3.
$$

Model-only factors are precomputed. These identities remove per-pixel
logarithms and exponentials without changing the mathematical K32/S9/q8
rule. Floating-point execution is still compared with tolerances, not
assumed bit-identical across scalar, FMA, and different architectures.

## SIMD organization and bounded work

The coarse AVX2 path evaluates eight components together for one target.
After shortlisting, requests are bucketed by component; full-stage vectors
then process eight pixels sharing one expert, broadcasting its Cholesky
coefficients and two conditional gain rows. Scalar tails handle incomplete
groups. A scalar implementation and an ARM64/NEON implementation share the
same contract. This is a CPU method; runtime GPU inference is not included.

OpenMP distributes phase/chunk work. Tiles schedule computation but do not
create reconstruction boundaries: every neighborhood reads the complete
input mosaic. Only the global boundary is reflected without edge repetition.
The 512-pixel work chunk avoids allocating a full-frame 49-feature matrix.
Recorded worker scratch is 419,840 bytes, about 7.2 MiB for eighteen workers,
in addition to model, prepared coefficients, input, and RGB output storage.

## RawTherapee and the model boundary

The clean implementation exposes “Student-t GMR (experimental)” with PP3
identifier `tgmr`. Markesteijn three-pass remains the default. An installed
data-only TGMR v2 model is authenticated and cached; an explicit
`RT_XTRANS_TGMR_MODEL` override supports compatible custom models.

Runtime receives already preprocessed/scaled scalar RAW values. It does not
apply a second white balance, a gamma wrapper, or an extra sharpening stage.
Model training on rendered-image-derived RGB and runtime on camera data are
different domains; matrix augmentation is only an approximation to that gap.

Missing/corrupt/incompatible models, unsupported CFAs, allocation failure, or
non-finite inference trigger a diagnostic and complete Markesteijn overwrite.
This protects execution integrity. **It cannot detect a finite but visibly
wrong interpolation, false color, or ringing.** Ordinary downstream profile
processing can amplify differences and is not itself evidence of which
demosaicer is colorimetrically correct.

# Evaluation protocol and metric definitions

## Three-channel center errors

All headline PSNR values below use the normalized range $[0,1]$ and the
complete center RGB vector, including the exactly restored measured channel:

$$
\operatorname{MSE}_{\mathrm{RGB}}=
\frac{1}{3N}\sum_{i=1}^{N}\sum_{c\in\{R,G,B\}}
        (\widehat x_{i,c}-x_{i,c})^2,
\qquad
\operatorname{PSNR}=-10\log_{10}\operatorname{MSE}_{\mathrm{RGB}}.
$$

An earlier manuscript incorrectly described the denominator as two missing
channels. The implementation and recorded numbers always used three. No
result is rescaled in this revision. When the measured channel is exact,
missing-channel-only PSNR is lower by $10\log_{10}(3/2)$, approximately
1.761 dB. Comparisons require the same convention for every method.

Historical `p99_abs` means the 99th percentile of absolute *scalar-channel*
errors, including the native-channel zeros. The production balanced-phase
comparison instead reports the 99th percentile of *center RGB RMS*:
$e_i=\sqrt{\sum_c(\widehat x_{i,c}-x_{i,c})^2/3}$.
These tail statistics are not interchangeable. The all-phase native
validation report averages each record's squared error over all eighteen
phases before forming its patch RMS; that is a third, explicitly different
aggregation from the balanced-phase comparison.

Per-source PSNR is computed from that source's pooled errors; the paired
source delta compares the same source under both methods. Median source
delta is not the difference of two marginal median PSNRs. The production
evaluator uses its recorded order-statistic convention for medians and p99.

## Holdout boundaries and adaptive reuse

Development hyperparameters were selected on validation, and BSDS test,
chromatic controls, Hubble/Hydra bright targets, and synthetic probes were
excluded from parameter fitting. Nevertheless, failures on these controls
motivated subsequent changes, notably Student-t stabilization. They are
retrospective research diagnostics, not untouched confirmation of the entire
adaptive design sequence. This distinction does not imply those images were
inserted into the training patches.

For production-v1, augmentation and source-count studies used the frozen
validation split. Two canonical fits established the model identity before
the original test comparison. That comparison exposed hard cases and
influenced retraining experiments; the old 500-source test is now correctly
called *diagnostic*. No new independent confirmatory population is claimed.

## Native production comparison

The production comparison reconstructs exactly 64,000 frozen centers from
500 sources. Each coordinate receives a deterministic balanced phase
(`record index modulo 18`); it is not evaluated eighteen times in this
particular head-to-head table. The separate trainer validation uses all
eighteen phases.

Both demosaicers see the same transformed input and target. Markesteijn uses
96-pixel source crops with padding, and a deterministic crop/full-image
center check reported zero difference. That check supports this evaluated
configuration, not a proof for every conceivable image and crop size.
Source imagery is authenticated before decoding. The three-channel metric,
phase assignment, crop geometry, and model identities are recorded alongside
the results.

# Results

## Retrospective development evidence

The following table is generated from the frozen BSDS development records.
It concerns research weights, not the later corpus-v1 model. GMAX is the
earlier global joint spatial–chromatic linear estimator, not Markesteijn.

<!-- evidence:research-table:start -->

| Research configuration | RGB PSNR (dB) | p99 scalar absolute error |
|:--|--:|--:|
| GMAX linear reference | 32.054 | — |
| Gaussian K64 experts and weights | 34.835 | 0.084625 |
| Gaussian experts, t weights ($\nu=5$) | 35.404 | 0.078690 |
| Fitted dense K64 Student-t ($\nu=3$) | 35.518 | 0.077291 |
| Reduced K32/S9/q8 ($\nu=3$) | 35.056 | 0.081494 |

<!-- evidence:research-table:end -->

The unchanged Gaussian-expert / Student-t-responsibility control isolates a
useful effect of heavier-tailed weighting within that development setting.
The fitted variant is stronger again, but also changes $\nu$ from five to
three; that increment is not a fitting-only ablation. By contrast, differences
between earlier full-RGB GMM and phase-conditioned GMR designs cannot be
assigned solely to dimensionality because other settings changed too.

K32/S9/q8 trades some quality for arithmetic cost. The frozen reduction
records about 90.8% retention of the dense K64 MSE gain over GMAX on these
research test coordinates. Estimated MAC-scale operations fall from 163,072
to 23,264 per center. Such an arithmetic count is not a wall-clock prediction;
memory access, shortlisting, square roots, and vector utilization matter.

![Historical validation quality–cost study. This selected the inference contract; it does not evaluate the production-v1 corpus.](devnotes/images/xtrans-tgmr-reduce/pareto.png){width=86%}

The reduced research model improves some sparse-star results, but does not
eliminate all sparse failures. The tiny-star control remains below GMAX.
Uniform dark-background samples are insufficient evidence of star safety;
the bright-target subsets must be reported separately.

<!-- evidence:research-safety:start -->

| Research subset | K32/S9/q8 PSNR (dB) | Delta to GMAX (dB) |
|:--|--:|--:|
| Hubble bright targets | 15.921 | +0.486 |
| Hydra bright targets | 36.447 | +3.663 |
| Tiny-star control | 8.267 | -0.271 |

<!-- evidence:research-safety:end -->

## Production-v1: improvement with important exceptions

The original canonical balanced-phase population comparison is:

<!-- evidence:production-table:start -->

| Diagnostic measure | TGMR production-v1 | Markesteijn 3-pass |
|:--|--:|--:|
| Pooled RGB PSNR (dB) | 34.063575 | 32.316274 |
| p99 center RGB RMS | 0.095072 | 0.115206 |
| Worst center RGB RMS | 0.408201 | 0.483703 |
| Median source PSNR (dB) | 36.312600 | 34.701333 |

<!-- evidence:production-table:end -->

<!-- evidence:production-gates:start -->

Pooled gain is **+1.747301 dB**; paired median source gain is **+1.625443 dB**. TGMR wins on **408** sources and loses on **92**. **65** sources lose more than 0.5 dB; the worst paired delta is **-6.169889 dB**.

| Original gate | Required | Outcome |
|:--|--:|--:|
| Pooled gain | at least +2 dB | FAIL |
| Median paired source gain | positive | PASS |
| Worst paired source loss | at most 0.5 dB | FAIL |
| Aggregate p99 RMS | no regression | PASS |

<!-- evidence:production-gates:end -->

Thus the candidate improves the population average and aggregate p99, but
does not pass the originally required 2 dB pooled gain or the 0.5 dB maximum
source-loss bound. Many source regressions are hidden by the aggregate gain.
The gate decision is not changed merely because Markesteijn also has visible
errors in a difficult region.

The clean-branch repeat reproduced every TGMR statistic in the original
comparison exactly. Its Markesteijn pooled result was 32.317383 dB instead
of 32.316274 dB. We retain the original table and its artifact identity above,
rather than silently mixing baseline values from two builds. This small
baseline difference does not change the failed-gate conclusion.

## Signal strata

The production model improves pooled PSNR in each of the recorded marginal
brightness, chroma, and texture strata. This is useful but weaker than safety
on every source or every type of structure. The strata overlap, and their
individual pixel counts must not be summed into additional independent data.

<!-- evidence:strata-table:start -->

| Marginal stratum | Centers | TGMR (dB) | Mark. (dB) | Delta (dB) |
|:--|--:|--:|--:|--:|
| Brightness / low | 23,611 | 48.560 | 45.888 | +2.672 |
| Brightness / middle | 30,029 | 32.790 | 30.783 | +2.007 |
| Brightness / high | 10,360 | 30.619 | 29.413 | +1.206 |
| Chroma / low | 20,292 | 47.876 | 45.848 | +2.028 |
| Chroma / middle | 20,273 | 35.425 | 33.561 | +1.864 |
| Chroma / high | 23,435 | 30.919 | 29.214 | +1.706 |
| Texture / low | 34,993 | 51.735 | 50.036 | +1.698 |
| Texture / middle | 17,550 | 40.320 | 37.808 | +2.512 |
| Texture / high | 11,457 | 26.928 | 25.239 | +1.689 |

<!-- evidence:strata-table:end -->

## Hard-case retraining did not close the gap

Digitally sharp borders, very thin structures, and some isolated chromatic
highlights can provoke false color or ringing. Such patterns are not
representative of every photograph, but cannot be dismissed as impossible
in actual camera data: labels, fine edges, saturated lights, and processing
can supply related difficult structures.

A subsequent controlled screen replaced up to 5% of training patches with
synthetic hard cases and compared direct extraction with a sensor-physical
rendering variant. At 1,000 training sources, the best reported synthetic
candidate reduced held-out synthetic mean MSE by only 15.83%, below the
required 50%. Physical-rendering candidates lost 0.177–0.312 dB on ordinary
validation. The screen therefore stopped without qualifying a full-scale
replacement. The current model bytes were retained unchanged; neither the
generator nor physical renderer is part of the clean inference product.

This is a negative result for the tested recipes, not proof that no training
distribution could improve the failure. No new blind-test success or
unexecuted full-factorial training is implied.

## Numerical parity and timing

The research native-optimization study provides a controlled implementation
measurement on a Ryzen 9 5900X with GCC 13.3, AVX2/FMA, and eighteen workers:

<!-- evidence:native-table:start -->

| Research native measurement | Value |
|:--|--:|
| Reference/native error RMS | 4.293898e-08 |
| Maximum absolute error | 4.768372e-07 |
| Standalone mosaic pixels | 39,959,032 |
| Median mosaic time (s) | 5.833451 |
| Throughput (megapixels/s) | 6.850 |
| Measured center exact / thread-repeat deterministic | yes / yes |

<!-- evidence:native-table:end -->

These are standalone mosaic measurements for the research model. They are
not the execution time of the production trainer, a complete RAW export,
or an ARM64 benchmark. The specialization agrees numerically with its
reference; deterministic thread-count repetition does not imply that all
scalar/FMA paths are bit-identical.

For the production model, clean-build descriptive full-RAW exports recorded
10.19–10.62 seconds and approximately 1.90 GiB peak RSS on the available
X-T50 examples. Those runs occurred during other verification work and are
not a controlled warm-up plus three-run performance gate. They establish
successful execution, not a new portable speed claim. Full-frame portraits
remain private; only reviewed earring crops accompany the repository.

# Reproducibility and artifact identity

## Corpus, trainer, and model

The public `RT-TGMR-corpus` release provides the compressed patch corpus,
expanded coordinate metadata, attribution, reconstruction instructions, and
checksums [@tgmrcorpus2026]. Original full photographs and private RAW exports
are not release attachments. Reconstructing original images from third-party
URLs is best effort; the authenticated TGPC payload is the durable training
input. The release is a maintainer publication, not an official RawTherapee
project release.

The canonical CPU trainer runs one thread with fixed ordering, float64
fitting, contraction disabled, and Ubuntu 24.04/GCC 13.3.0 as the pinned
release environment. The fit uses seed `0x0000005847544d52`, batches of
4,096, and an 8 GiB memory limit. Its
configuration is serialized from actual checkpoint settings rather than an
arbitrary supplied label. Source/build-input identity and corpus payload are
bound to the model. The recorded original clean canonical fits produced
byte-identical checkpoints and model artifacts; a later clean trainer build
also re-exported the retained checkpoints to the same model. This manuscript
rewrite does not constitute another training run.

<!-- evidence:identity-block:start -->

Model size: **6,073,768 bytes**. SHA-256 values below are
displayed as two consecutive halves; join them without whitespace.

Production-v1 TGMR v2 model

```text
5707fbd67d1998ed3bac646ecce96729
7a2022776821d62944a24dbbb8615285
```

TGPC payload (not the compressed file)

```text
573a0bf7f073282f715fbafc68ace1e8
366c8ca19e99e7f2f797a75c11444c74
```

Trainer configuration

```text
13bf483df2104617785cd0c9be5eeb93
86a841a00b2e30087881811e7f19bcbf
```

Trainer source/build inputs

```text
df0fc05b7b62f35c87a8ad42caf07e21
9721bddc81307cbf2756c9ba14d71ee6
```

Balanced-phase comparison JSON

```text
9e2b799fa829d8cac33576a113594891
caf0396a1f30037f6164474aff8a6dba
```

<!-- evidence:identity-block:end -->

The production artifact differs from the historical BSDS research model,
whose SHA-256 is
`6279b6a593ef4b595b1aff246182682b60c7eea701373ee2eb9f80cfcb50485c`.
Research and production numbers must never be associated with the other's
coefficient file.

## Evidence-linked document build

This paper is Markdown with a CSL YAML bibliography, rendered with pinned
Pandoc 3.10.2 and citeproc to self-contained HTML and a typeset PDF. Title page
and table of contents are retained. A verifier regenerates named result
tables from authenticated tracked JSON, checks metric arithmetic and model
identity, and rejects changed table text or inconsistent headline values.
The companion README lists the evidence files and commands.

Git preserves earlier revisions. The document manifest authenticates this
revision's inputs and rendered files; it is not a promise that every PDF
rerender has identical binary bytes. PDF font subsetting can alter bytes
without changing text or page appearance. HTML reproducibility and PDF
text/layout stability are tested separately.

# Limitations and release status

**Not universal superiority.** Production-v1 misses two important original
quality gates. Its current status is an experimental option with accepted,
documented limitations, not a fully qualified replacement for Markesteijn.
Finite-value/runtime checks do not provide a quality selector.

**Adaptive research.** Earlier test and external controls influenced the
sequence of experiments. The current production test also became diagnostic
after its failures motivated retraining. Independent confirmation remains
necessary for strong generalization claims. No uncertainty intervals over
independent source draws are supplied here, and the many phase views do not
increase the number of independent images.

**Approximate sensor domain.** The training RGB derives from heterogeneous
rendered images, including their earlier demosaicing, compression, and tone
processing. Linearization and camera-matrix augmentation do not invert those
operations. Noise, clipped highlights, extreme exposures, and camera-specific
spectral effects can remain mismatched.

**Confounding in comparisons.** Development stages changed several design
choices together. The paper does not isolate every contribution, reproduce
all published competing systems, or establish a state-of-the-art ranking.
Production and research datasets differ; their absolute PSNR values are not
a before/after measure of the same evaluation.

**Incomplete platform/camera matrix.** GCC and Clang x86-64 checks and
scalar/AVX2 parity support local viability. ARM64/NEON compilation is not
native execution validation. Windows/macOS packaging, wider native hardware,
and complete X-Trans generation I–V camera coverage remain qualification
work. The currently documented X-T50 examples cannot stand in for that matrix.

**Rights and publication boundaries.** Corpus redistribution retains source
attribution and individual terms. This does not itself finalize the trained
model's intended CC BY 4.0 licensing or every relevant personality right.
Model-license finalization is separate from software licensing and from the
already published corpus. A permanent archival DOI/second mirror is also
distinct from the existing GitHub release.

# Conclusion

Phase-conditioned Student-t regression provides a compact, interpretable
statistical demosaicer with practical native CPU execution. Its specific
advantages in this study come from modeling the measured task directly,
using robust tempered component weighting, and organizing reduced inference
around predictable local memory access. The downloadable corpus and
authenticated model make the current result reproducible beyond the
original research setup.

The evidence supports an *experimental* implementation, not an unconditional
quality claim. The licensed diagnostic population improves on average, but
some source losses are large and the tested hard-case retraining did not
repair them. Retaining that distinction is part of the result: a useful
average prior and efficient implementation can coexist with important,
unresolved image-quality failures.

# Reproducibility and assistance disclosure {.unnumbered}

Code, data identities, reports, and implementation references are associated
with the pinned clean feature revision [@rawtherapee2026]. ChatGPT/Codex
assisted with literature retrieval, code, experiments, and manuscript
preparation. That assistance does not establish novelty, reference fidelity,
or validity by itself. The artifact-linked checks support the specified
numbers and contracts; broader conclusions remain bounded by the limitations
above. This is a research/engineering manuscript, not a peer-reviewed paper.

# References {.unnumbered}
