---
title: Phase-Conditioned Student-t Mixture Regression for X-Trans Demosaicing
subtitle: A robust joint spatial-chromatic patch prior with exact CFA data consistency
author:
  - Vaclav Haisman
institute: Independent researcher, Czech Republic
email: vhaisman@gmail.com
date: 25 August 2026
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
toc: true
toc-depth: 2
numbersections: true
bibliography: doc/papers/xtrans-tgmr/citations.yml
csl: doc/papers/xtrans-tgmr/ieee.csl
link-citations: true
abstract: |
  The Fujifilm X-Trans color filter array samples red, green, and blue on an irregular 6 by 6 lattice. Its larger fundamental cell complicates both hand-designed interpolation and the direct transfer of Bayer demosaicers. This paper presents a phase-conditioned Student-t Gaussian-mixture-regression (TGMR) demosaicer that predicts the two missing center colors directly from the 49 physically observed samples in a 7 by 7 neighborhood. Eighteen phase-specific models are trained on synthetic X-Trans observations of linearized natural-image patches. Heavy-tailed component likelihoods replace the brittle Gaussian responsibilities of an earlier mixture regressor, while the conditional predictor remains affine within each component. A validation-selected K32/S9/q8 reduction first scores 32 components on a central 3 by 3 marginal, then evaluates eight full 49-dimensional experts. Exact algebraic specialization for degrees of freedom 3 and posterior temperature 4 enables an AVX2/FMA and OpenMP implementation without approximating the reviewed statistical model. On fixed center coordinates from 20 held-out BSDS sources, the reduced method reaches 35.056 dB, 3.002 dB above a global joint spatial-chromatic LMMSE baseline, while preserving the physically measured center sample exactly. It remains robust on frozen chromatic, star-field, and analytical controls, although a tiny-star control remains 0.271 dB below the LMMSE baseline. A 39.96-megapixel standalone mosaic is processed in 5.83 s on a Ryzen 9 5900X; integration into RawTherapee processes a 40.14-megapixel RAF in 11.28 s end to end, approximately three times the Markesteijn baseline. The results support Student-t mixture regression as a useful, CPU-viable experimental X-Trans demosaicer, while leaving model redistribution and cross-camera validation unresolved.
keywords:
  - demosaicing
  - X-Trans
  - Gaussian mixture regression
  - Student-t mixture
  - color filter array
  - RawTherapee
---

# Introduction

A single-sensor digital camera measures only one spectral component at each
pixel. Demosaicing reconstructs the two missing components from neighboring
measurements and from assumptions about natural-image structure. The problem
is ill-posed: multiple full-color images can produce the same mosaic, and
high-frequency spatial content can overlap the chromatic aliases introduced
by the color filter array (CFA).

Most published demosaicing work assumes a 2 by 2 Bayer lattice. Fujifilm's
X-Trans CFA instead repeats over a 6 by 6 cell and places green more densely
and less regularly. Rafinazari and Dubois analyzed the corresponding spectral
replicas and proposed dedicated X-Trans interpolation filters
[@rafinazari2014]. RawTherapee's principal classical X-Trans implementation is
attributed in its source to Frank Markesteijn; no separate formal Markesteijn
paper was identified, so this work cites the implementation rather than
inventing a publication [@markesteijnsource]. Both approaches can produce
convincing photographs, but difficult periodic textures, thin colored detail,
and metallic edges remain challenging.

Population statistics offer another route. Portilla, Otaduy, and Dorronsoro
showed that joint spatial-chromatic covariance learned from complete images
can produce compact linear CFA predictors [@portilla2005]. Sandeep and Jacob
extended the idea to a joint-color Gaussian mixture model (JCS-GMM), choosing
a local Gaussian from observed CFA values and applying its conditional mean
[@sandeep2019]. A mixture is attractive because a single covariance must
average incompatible structures such as smooth regions, oriented edges, and
chromatic texture. Yet an observed-sample Gaussian likelihood can become
confidently wrong when a patch is atypical or when CFA masking removes the
evidence separating two components.

This paper develops a robust mixture-regression method specifically for
X-Trans. Each of the 18 distinct phase/orientation observation contracts has
its own low-dimensional joint model over 49 observed CFA samples and two
missing center colors. The Gaussian mixture is continued into a multivariate
Student-t mixture using fixed-degree-of-freedom expectation conditional
maximization (ECM). Student-t tails soften component responsibilities for
unusual patches without changing the affine conditional location within an
expert. Model reduction then combines a genuinely retrained 32-component
mixture with observable coarse-to-fine component shortlisting.

The documented contributions are:

1. **Phase-conditioned task reduction.** Rather than learning a full
   147-dimensional RGB-patch density, the method trains 18 joint regressors
   over exactly the physically observed 7 by 7 CFA values and the two missing
   center targets. No initial demosaic, Bayer conversion, or target-RGB
   leakage is used.
2. **Robust Student-t responsibilities for X-Trans regression.** Fixed
   Student-t tails repair a held-out external failure of the Gaussian model
   while increasing population quality and lowering tail error.
3. **Observable coarse-to-fine inference.** A central S9 Student-t marginal
   selects eight candidates from a genuinely trained K=32 model; the final
   posterior is evaluated on the complete 49-sample observation.
4. **Exact CPU specialization.** For the frozen degrees of freedom and
   posterior temperature, shortlist ranking and posterior weights are
   rewritten without per-pixel logarithms or exponentials. AVX2/FMA,
   same-component bucketing, OpenMP, and bounded streaming yield practical
   CPU throughput without changing the reviewed model.
5. **End-to-end open-source integration.** The model is authenticated,
   phase-matched against the camera's actual CFA, evaluated on RawTherapee's
   camera-linear sensor scale, and backed by complete Markesteijn fallback
   [@rawtherapee2026].

These are implementation- and experiment-bounded contributions. The paper
does not claim that the constituent statistical ideas are new in isolation,
that the combination is the first possible one, or that the work establishes
patent novelty.

# Related Work

## Classical and X-Trans demosaicing

Classical demosaicers exploit smooth color differences, directional
interpolation, residual correction, frequency separation, or learned linear
statistics. Residual interpolation predicts a guide image and interpolates
the residual between observed and guided samples; Kiku et al. demonstrated
that this view can outperform direct color-difference interpolation on Bayer
data [@kiku2016]. Portilla et al. instead learned the joint covariance of
neighboring colors and derived one local LMMSE predictor for each CFA phase
[@portilla2005]. This learned-linear formulation is especially relevant here:
the K=1 phase-conditioned control is the affine Gaussian/LMMSE limit of the
proposed mixture model.

X-Trans requires explicit treatment of its 6 by 6 periodicity. The
frequency-domain analysis of Rafinazari and Dubois identifies overlapping
luminance and chrominance replicas that are not present in the same form for
Bayer [@rafinazari2014]. Markesteijn's RawTherapee implementation uses
directional interpolation and multiple passes over candidate reconstructions
[@markesteijnsource]. The proposed method does not modify either algorithm;
they serve as classical comparisons and, in the integration, Markesteijn
remains the complete fallback.

## Joint-color Gaussian patch priors

JCS-GMM learns full-covariance Gaussian components over concatenated RGB
patches and conditions a selected component on the observed Bayer samples
[@sandeep2019]. Its published configuration uses 150 components, 6 by 6
patches, hard observed-CFA component selection, overlapping aggregation, and
image-adaptive updates. Our earlier X-Trans reproduction deliberately isolated
the local probabilistic core. A soft posterior mean improved upon hard MAP,
but a full 147-dimensional RGB-patch mixture generalized poorly: likelihood
optimization modeled many variables that were irrelevant to the two missing
center components.

Zoran and Weiss use Gaussian patch mixtures in expected patch log likelihood
(EPLL), coupling a patch prior to a whole-image inverse problem
[@zoran2011]. The present work does not perform EPLL or iterative
whole-image refinement. It asks a narrower question: can a fixed learned
mixture predict a center RGB value directly and safely from an X-Trans
neighborhood? This separation prevents overlap iteration or image-specific
adaptation from hiding weaknesses in the local prior.

## Heavy-tailed mixtures and learned demosaicing

Peel and McLachlan formulate robust mixture modeling with multivariate
Student-t components and latent scale weights [@peel2000]. Ding gives the
conditional multivariate Student-t distribution used here to separate the
conditional location from its observation-dependent predictive scale
[@ding2016]. We apply these established results to phase-conditioned X-Trans
mixture regression and examine the effect of heavy-tailed responsibilities on
demosaicing safety.

Deep networks learn far richer nonlinear priors. Gharbi et al. include an
X-Trans-specific network and emphasize mining difficult training patches
[@gharbi2016]. Kokkinos and Lefkimmiatis derive an optimization-inspired
iterative residual network that can accommodate non-Bayer patterns
[@kokkinos2019]. Such systems can achieve excellent output, but they require
licensed weights and a neural runtime. TGMR instead evaluates a compact fixed
statistical model with ordinary C++11, OpenMP, and optional AVX2/FMA. The
comparison is architectural rather than a claim that a mixture model generally
outperforms modern neural demosaicers.

# X-Trans Observation Model

Let a complete linear-light RGB image be

$$
x : \Omega \rightarrow \mathbb{R}^{3},
$$

and let $c(r) \in \{R,G,B\}$ be the color measured at pixel $r$ by a 6 by 6
X-Trans CFA. The scalar mosaic is

$$
y(r) = e_{c(r)}^{\mathsf T}x(r),
$$

where $e_c$ selects one color component. At a target pixel $r_0$, a 7 by 7
window contains one physical observation at every spatial location. Vectorize
those 49 values in row-major spatial order as $y_p \in \mathbb{R}^{49}$,
where $p$ denotes the local X-Trans phase pattern. The target vector
$t_p \in \mathbb{R}^{2}$ contains only the two unmeasured center components.
Its channel order is fixed by the measured center color:

| Center sample | Target order |
|:--|:--|
| R | G, B |
| G | R, B |
| B | R, G |

Across translated and transformed X-Trans representations there are 18 unique
7 by 7 observation patterns in the experiment. A model is trained separately
for each pattern. At runtime the complete local CFA pattern, not merely the
center color, selects the phase bank. Thus equally colored center samples with
different surrounding lattices do not share a regressor.

## Observable common DC

Absolute patch brightness consumes mixture capacity and makes likelihoods
sensitive to global intensity. For each phase, let $I_{p,c}$ be the indices of
the observed values whose physical CFA color is $c$. The common observable DC
is

$$
d(y_p)=\frac{1}{3}\sum_{c \in \{R,G,B\}}
       \frac{1}{|I_{p,c}|}\sum_{i\in I_{p,c}} y_{p,i}.
$$

The scalar $d$ is subtracted from all 49 observations and from both target
components during training. It is added back after prediction. Only measured
CFA values enter this operation. This formulation was selected on validation
and was stronger than either an absolute-RGB model or subtracting the mean of
all observations without color balancing.

# Phase-Conditioned Student-t Mixture Regression

For one phase, define the task-reduced vector

$$
z = \begin{bmatrix}y\\t\end{bmatrix}\in\mathbb{R}^{51}.
$$

The prior is a $K$-component multivariate Student-t mixture

$$
p(z)=\sum_{k=1}^{K}\pi_k\,
  \operatorname{St}_{\nu}(z;\mu_k,\Sigma_k),
$$

with fixed degrees of freedom $\nu=3$. Partition each component mean and scale
matrix into observed and target blocks. For the centered observation residual
$r_k=y-\mu_{y,k}$, define

$$
S_k=\Sigma_{yy,k}+\tau^2 I,
\qquad
\delta_k=r_k^{\mathsf T}S_k^{-1}r_k,
$$

where $\tau=3\times10^{-4}$ is a validation-selected model-mismatch floor, not
a calibrated camera-noise variance.

The observed log density, omitting the phase-common constant, is

$$
\ell_k=\log\pi_k-\frac{1}{2}\log|S_k|
 -\frac{\nu+49}{2}\log\left(1+\frac{\delta_k}{\nu}\right).
$$

Posterior responsibilities use a fixed temperature $T=4$:

$$
\gamma_k=
\frac{\exp(\ell_k/T)}{\sum_j\exp(\ell_j/T)}.
$$

The conditional Student-t location is the same affine form as Gaussian
mixture regression [@ding2016]:

$$
m_k(y)=\mu_{t,k}+
\Sigma_{ty,k}S_k^{-1}(y-\mu_{y,k}).
$$

The two missing center values are estimated by the posterior mean of component
locations,

$$
\hat t=\sum_k\gamma_km_k(y).
$$

The conditional predictive scale depends on $\delta_k$ and is retained for
calibration experiments, but it is not required to form the conditional
location. All factorizations use Cholesky solves, and responsibility
normalization uses log-sum-exp in the reference implementation.

Finally, the measured center component is copied exactly from the input
mosaic. Exact reinjection is important: even mathematically normalized
posterior weights can change a measured float by a rounding unit if all three
channels are averaged uniformly.

## Why Student-t responsibilities

A Gaussian likelihood penalizes a large Mahalanobis distance exponentially.
When a patch lies outside the population distribution, tiny covariance or
tail differences can produce a nearly one-hot yet incorrect component
assignment. A Student-t component replaces this exponential tail with a
polynomial one. In the frozen Gaussian experts, changing only the component
likelihood to Student-t increases held-out BSDS PSNR from 34.835 to 35.404 dB.
Continuing the parameters with true Student-t ECM adds a further 0.114 dB,
reaching 35.518 dB. Most of the improvement therefore comes from robust
responsibility assignment rather than from altered affine experts.

## Fixed-degree-of-freedom ECM

Training begins from a deterministic full-covariance Gaussian mixture. For a
sample $z_i$ and component $k$, the E step computes the posterior
responsibility and the standard latent Student-t scale weight

$$
u_{ik}=\frac{\nu+51}{\nu+delta_{ik}}.
$$

The conditional maximization updates mixture weights from responsibilities
and updates component locations and scales using the products
$\gamma_{ik}u_{ik}$, with covariance floor $10^{-6}$. Degrees of freedom are
held fixed. This follows the robust-mixture latent-scale formulation of Peel
and McLachlan [@peel2000], independently implemented without copying reference
source. Validation selected $\nu=3$ and 30 continuation iterations. Training
likelihood rises monotonically; validation is nearly flat from iterations 5
through 30 and falls by only 0.061 dB at iteration 50.

# Model Reduction and Component Shortlisting

The full model has 64 components in every phase and evaluates a complete 49-D
likelihood and predictor for every component. Reduction was performed only
after the statistical configuration and safety tests were frozen.

## Genuine K=32 training

Models with 8, 16, and 32 components were independently initialized from
their corresponding Gaussian checkpoints and continued with Student-t ECM.
They were not formed by slicing the K=64 model. K=32 is the smallest model
that retains at least 90% of the validation MSE gain: its dense test result is
35.450 dB, compared with 35.518 dB for K=64. K=16 remains strong but misses the
predeclared retention criterion, while K=8 regresses on a bright-star safety
subset.

## S9/q8 observable shortlist

Every target still uses the fixed 7 by 7 observed patch. There is no nonlocal
patch retrieval. The term *selection* refers to two deterministic operations:

1. choose one of 18 phase banks by matching the actual CFA pattern;
2. rank mixture components using an observable central marginal.

For each K=32 component, the algorithm evaluates a Student-t marginal on the
nine physical samples in the central 3 by 3 region (S9). Stable insertion
selection retains the top eight component identifiers, with ties resolved by
the lower identifier. Only those eight components receive the full 49-D
likelihood and conditional prediction. The observable RGB DC is still formed
from all 49 samples; changing its support would change the statistical model
rather than only the search cost.

On untouched test coordinates the shortlist contains the exact dense-K32 top
component 90.53% of the time and retains 83.31% mean exact posterior mass. The
combined K32/S9/q8 method retains 90.81% of the full K64 test MSE gain while
reducing the arithmetic estimate from 163,072 to 23,264 MAC-scale operations
per center, a factor of 7.01.

![TGMR inference pipeline. The observed patch is fixed; only the phase bank and mixture components are selected.](doc/papers/xtrans-tgmr/figures/pipeline.svg){width=100%}

![Validation quality-cost frontier used to freeze K32/S9/q8.](devnotes/images/xtrans-tgmr-reduce/pareto.png){width=86%}

# Exact Native Inference

## Algebraic specialization

The frozen parameters allow exact removal of per-pixel transcendental calls.
For S9, $\nu=3$ gives the component score

$$
C_k-6\log(1+\delta_k/3),
$$

where $C_k$ contains the mixture weight and determinant terms. Ordering can be
computed from the positive quantity

$$
\frac{\exp((C_k-C_{\max})/6)}{1+\delta_k/3}.
$$

The phase-common subtraction prevents overflow and does not affect ranking.
For the full 49-D likelihood with $T=4$, the posterior weight is proportional
to

$$
\exp((C_k-C_{\max})/4)
\left(1+\delta_k/3\right)^{-6.5}
=\frac{B_k}{s^6\sqrt{s}},
\quad s=1+\delta_k/3.
$$

The factors involving only model coefficients are precomputed. This is an
algebraic specialization, not a likelihood approximation. Across 1,152 phase
probes, the specialized scalar and AVX2 paths produce zero shortlist
mismatches relative to the logarithmic reference.

## SIMD organization

The S9 model is stored coefficient-major in four array-of-structures-of-arrays
groups. Each AVX2 vector solves eight independent component systems for one
pixel. Stable scalar insertion selects component identifiers.

The more important optimization is applied to the full stage. Shortlisted
requests are bucketed locally by component. An AVX2 vector then processes
eight pixels that share one component, broadcasting that component's 49-D
Cholesky coefficients and two conditional gain rows. This avoids gathers
across unrelated experts. Incomplete vector groups use a scalar tail. The
implementation does not use `-ffast-math`, approximate reciprocal square root,
or an approximate statistical model.

## OpenMP and bounded streaming

The engine flattens `(phase, chunk)` work and schedules it statically in one
OpenMP loop. A 128 by 128 tile is a scheduling unit, not a reconstruction
boundary: every 7 by 7 patch is read from the complete input mosaic. Only the
true image boundary is reflected, without repeating the edge sample. Tile
coordinate lists are grouped by phase, while 49-value observations are
materialized only for the current 512-pixel chunk.

The worker scratch is 419,840 bytes. At 18 workers, worker scratch is about
7.2 MiB; the model is approximately 6 MiB and the prepared AoSoA cache about
210 KiB. No pixels-by-49 full-frame feature matrix is allocated.

![Incremental native optimization. The largest gain comes from same-component full-likelihood SIMD.](devnotes/images/xtrans-tgmr-native-opt/variant-throughput.png){width=88%}

# Training and Evaluation Protocol

## Population data

Natural-image training uses a mirror of the BSDS500 segmentation corpus at
revision `a04b7c6c3a9f0ace74bf205c72a43d32e1c72722` [@arbelaez2011]. Stored
sRGB JPEG images are decoded to linear light before patch extraction and
synthetic mosaicking. The dataset's research and educational terms apply
independently of this paper.

| Split | Sources | Sampling per source | Patches |
|:--|--:|:--|--:|
| Training | 200 official training images | 512 deterministic random 7 by 7 patches | 102,400 per phase |
| Validation | 20 frozen validation images | dense 24 by 24 center grid | 11,520 |
| Test | 20 untouched test images | dense 24 by 24 center grid | 11,520 |

Every training RGB patch is presented under each of the 18 phase contracts,
so each phase model receives 102,400 examples. Training patch positions are
selected without replacement using deterministic SHA-256-derived seeds. The
Gaussian initialization uses deterministic full-covariance EM implemented
with scikit-learn [@pedregosa2011]; four training threads and one evaluation
thread are part of the reproduction contract because BLAS reduction order can
change final float bits.

Model and hyperparameter selection use only training and validation. The
untouched test, external images, star fields, and analytical scenes are opened
only after the configuration is frozen.

## External and analytical controls

The external chromatic suite contains three fixed 12 by 12 grids from each of
Chelsea, Coffee, IHC, Motorcycle, Retina, and Rocket, totaling 2,592 target
pixels. Hubble and an independent NASA Hydra image contribute uniform and
bright-target star samples. Six analytical scenes - two gradients, a
red/gray edge, periodic chromatic texture, a tiny star, and a saturated point
- are evaluated under all 18 X-Trans phases.

The primary quality metric pools squared error over the two predicted center
channels at fixed coordinates:

$$
\operatorname{PSNR}=10\log_{10}\frac{1}{\operatorname{MSE}},
$$

for normalized linear RGB. Reported headline PSNR is therefore a center-pixel
conditional-reconstruction metric, not full-image rendered PSNR. We also
report p99 absolute channel error, per-source deltas, phase spread, exact
native-sample restoration, and frozen safety cases.

## Comparisons

Markesteijn and corrected-final MLRI are classical X-Trans implementations.
GMAX is a same-population global joint spatial-chromatic affine LMMSE bank.
FULL-GMM is a 16-component, 147-dimensional full-RGB patch mixture. Gaussian
phase GMR is the direct 51-dimensional predecessor of TGMR. All center-pixel
methods are evaluated on identical coordinates.

# Results

## Main held-out result

| Method | Pooled PSNR (dB) | p99 absolute error |
|:--|--:|--:|
| Markesteijn | 30.867 | 0.137975 |
| corrected-final MLRI | 30.985 | 0.134802 |
| GMAX LMMSE | 32.054 | 0.122019 |
| FULL-GMM | 33.531 | 0.099805 |
| Gaussian phase GMR | 34.835 | 0.084625 |
| Gaussian experts, Student-t responsibilities | 35.404 | 0.078690 |
| trained K64 Student-t GMR | **35.518** | **0.077291** |
| reduced K32/S9/q8 TGMR | **35.056** | **0.081494** |

The reduced method is 3.002 dB above GMAX and 4.189 dB above Markesteijn on
the fixed-coordinate test. It retains 90.81% of the K64 MSE improvement over
GMAX. Relative to Gaussian GMR, the full K64 Student-t model reduces p99 error
by 8.67%. The K64 model wins 18 of 20 test sources over Gaussian GMR; the two
losses are 0.296 and 0.043 dB.

The progression also isolates why the method succeeds. Task reduction from
FULL-GMM to Gaussian phase GMR adds 1.304 dB. Reinterpreting only the
responsibilities with Student-t tails adds 0.569 dB. Robust Student-t fitting
adds 0.114 dB. The final reduction gives back 0.462 dB for a 7.01-fold
arithmetic reduction.

![Validation quality as Gaussian components are added to the phase-conditioned task regressor.](devnotes/images/xtrans-gmr/component-count.png){width=84%}

## Heavy-tail robustness

With fixed Gaussian experts, validation PSNR has a broad maximum for
$\nu=3$ through 10 rather than a fragile single optimum. At $\nu=5$, mean
maximum responsibility falls from 0.653 for the Gaussian likelihood to 0.494,
and effective component count rises from 3.55 to 6.59. After Student-t ECM,
validation selects $\nu=3$.

The critical external failure of Gaussian GMR was `motorcycle-left-0`, at
-0.853 dB relative to GMAX. Student-t responsibilities alone move it to
+2.435 dB, and trained TGMR reaches +2.811 dB. This crop was not used for
selection. For the final reduced model, all 18 external crops remain within
the frozen -0.5 dB safety bound; the worst is `coffee-1` at -0.314 dB.

![Validation PSNR and responsibility behavior as the Student-t tail changes.](devnotes/images/xtrans-tgmr/robustness-curve.png){width=88%}

![External chromatic crop deltas of the full trained Student-t model relative to GMAX.](devnotes/images/xtrans-tgmr/external-safety.png){width=88%}

## Sparse and phase-sensitive structures

| Control | GMAX (dB) | K64 TGMR (dB) | K32/S9/q8 (dB) | Reduced delta vs GMAX |
|:--|--:|--:|--:|--:|
| Hubble bright | 15.435 | 15.707 | **15.921** | +0.486 |
| Hydra bright | 32.784 | 40.165 | **36.447** | +3.663 |
| gray gradient | 69.907 | 76.609 | **75.398** | +5.491 |
| chromatic gradient | 59.401 | 70.101 | **69.812** | +10.411 |
| red/gray edge | 19.537 | 27.890 | **26.577** | +7.040 |
| periodic chromatic | 16.175 | 18.574 | **18.454** | +2.279 |
| tiny star | **8.539** | 8.224 | 8.267 | **-0.271** |
| saturated point | 3.807 | 3.984 | **4.018** | +0.211 |

The star-field controls prevent a natural-image average from hiding sparse
failures. Reduced TGMR improves both bright subsets over GMAX, but neither
mixture model surpasses Markesteijn on Hubble bright in the full study. The
tiny-star analytical scene remains a visible limitation: TGMR is stable and
inside the predeclared -2 dB bound, but 0.271 dB below GMAX. A heavy-tailed
population prior does not recover information absent from the CFA, and it can
still prefer a statistically common smooth explanation for a one-pixel
structure.

Phase behavior is mixed. Reduction decreases the K64 phase-PSNR range on five
of six analytical scenes, but the absolute range remains large for the two
gradients and periodic texture. Phase-conditioned training handles each local
sampling pattern correctly; it does not make the CFA information content
phase invariant.

## Numerical parity

Float32 model and inference differ from the float64 selected result by
$2.59\times10^{-8}$ RMS and $3.18\times10^{-7}$ maximum over the frozen
evaluation. The final AVX2/FMA executor differs from the frozen float32
reference by $4.29\times10^{-8}$ RMS and $4.77\times10^{-7}$ maximum over
15,564 evaluation samples. No non-finite value occurs, shortlist identifiers
match on all audit probes, output hashes are identical at 1, 12, 18, and 24
threads, and measured center samples are exact.

## Runtime and memory

All native timings use an AMD Ryzen 9 5900X and GCC 13.3. Medians follow one
warm-up. The final standalone streaming executor uses 18 OpenMP workers,
AVX2/FMA, 128 by 128 tiles, and 512-pixel chunks.

| Geometry | Pixels | Median (s) | Throughput (MP/s) | Peak RSS (MiB) |
|:--|--:|--:|--:|--:|
| 2048 by 2048 | 4.19 M | 0.603 | 6.957 | 85 |
| 4000 by 3000 | 12.00 M | 1.749 | 6.862 | 204 |
| 6000 by 4000 | 24.00 M | 3.487 | 6.883 | 387 |
| 7738 by 5164 | 39.96 M | **5.833** | **6.850** | 631 |

Throughput is nearly flat from 4 to 40 megapixels. The large-run memory is
explained by scalar input and RGB output planes plus bounded scratch, not by a
full-frame observation matrix. Scaling peaks at 18 hardware threads; using all
24 SMT threads reduces throughput.

![Standalone streaming throughput remains approximately constant through 40 megapixels.](devnotes/images/xtrans-tgmr-native-opt/large-mosaic-throughput.png){width=86%}

# RawTherapee Integration

The experimental method is registered as `tgmr`, displayed as **TGMR
(experimental)** [@rawtherapee2026]. It loads one external binary artifact of
6,073,164 bytes with SHA-256
`6279b6a593ef4b595b1aff246182682b60c7eea701373ee2eb9f80cfcb50485c`.
The loader authenticates the complete file, then validates dimensions,
indices, phase patterns, center/target channels, S9 support, finite values,
positive Cholesky diagonals, and canonical end of file. Failed loads are not
cached.

RawTherapee supplies a black-subtracted, preprocessing-balanced camera-sensor
mosaic in its nominal 0 to 65535 float scale. TGMR divides by 65535 before
inference and multiplies its predictions afterward. It applies no additional
white balance, color matrix, clipping, false-color suppression, sharpening,
or denoising. The measured channel is copied from the original engine input,
not round-tripped through the normalization.

The engine matches the camera's actual 6 by 6 CFA against all authenticated
phase patterns. Any load, CFA, allocation, non-finite, or inference failure
discards partial output and reruns Markesteijn over the complete RGB planes.
The fallback is loud and carries a structured error code.

On a deterministic 39.96-megapixel mosaic, the integrated kernel reaches
5.248 MP/s, 79.2% of a contemporaneous standalone run. Four 40-megapixel
Fujifilm X-T50 RAFs spend 7.34 to 7.47 s in the kernel and 10.78 to 11.28 s in
the complete minimal CLI pipeline. For DSCF0771, Markesteijn takes 3.78 s and
TGMR 11.28 s, about a factor of three. Peak RSS is effectively identical near
2 GiB for both complete exports.

Visual inspection of these four RAFs found plausible color and no obvious
tile seam or global cast. On one metallic earring, TGMR reduces visible
magenta/cyan segmentation relative to Markesteijn; fine foliage and
cobblestones are somewhat smoother. These are qualitative observations from
one camera model without ground truth. No identifiable real-image figure is
included in this paper, and the observation is not treated as a general
quality claim.

# Discussion

## What the mixture learns

The large gap between K=1 and the mixture shows that one global covariance
does not capture all useful joint spatial-chromatic behavior. A phase-local
component bank supplies edge, texture, and chromatic alternatives, while
posterior averaging avoids brittle hard switching. The component oracle still
reaches 38.684 dB with 7 by 7 selection and 45.239 dB per pixel for K64, so
the learned experts contain more capacity than the observable posterior can
recover. That gap is not an invitation to train an external selector:
component identifiers have phase-local meaning, and earlier hard-selection
experiments were unsafe.

Student-t tails address a narrower failure. They reduce the influence of a
large Mahalanobis residual on component choice, making atypical observations
less likely to produce a one-hot Gaussian assignment. The result is unusual
in that robustness and average quality improve together. The remaining
`coffee-1` and tiny-star regressions show that heavy tails do not eliminate
model mismatch or CFA ambiguity.

## Why task reduction matters

FULL-GMM models all 147 RGB variables even though inference needs only two
center colors. Density components that explain neighboring unobserved RGB do
not necessarily minimize target error. The 51-dimensional task model removes
those nuisance variables and trains one population for each actual
observation operator. This increases validation stability, makes K=1 exactly
comparable to same-support LMMSE, and improves external generalization before
Student-t robustness is introduced.

## Cost-quality tradeoff

The K32/S9/q8 candidate is a deliberately frozen knee rather than the maximum
quality point. Genuine K32 training is superior to retaining the most common
32 K64 components. Low-rank covariance approximation is not useful: even rank
24 retains only 64.14% of the validation gain while costing nearly as much as
dense inference. Observable S9 shortlisting is less accurate than an S25
marginal but substantially cheaper and remains above the 90% validation rule.

The successful native implementation changes the computational organization,
not the model. Same-component bucketing creates the SIMD axis missing from a
naive per-pixel triangular solver. Bounded streaming then removes hundreds of
megabytes of observation materialization for only a small throughput cost.

# Limitations and Reproducibility Boundary

The principal limitations are:

- **Evaluation unit.** Headline PSNR is measured at fixed center coordinates,
  not over every pixel of a reconstructed full image. The engine integration
  establishes numerical and qualitative behavior but does not provide
  ground-truth full-RAF PSNR.
- **Training domain.** BSDS JPEGs are linearized and remosaicked synthetically.
  Real RawTherapee input is camera-native sensor RGB after black subtraction
  and preprocessing balance. Transfer across sensor generations,
  illuminants, noise levels, and color matrices is not established.
- **Sparse null-space detail.** The tiny-star control remains below GMAX, and
  Hubble bright remains below Markesteijn in the full comparison. Statistical
  priors cannot guarantee recovery of unobserved exceptional colors.
- **Phase spread.** Correct phase modeling does not remove phase-dependent
  information loss. Several analytical PSNR ranges remain large.
- **Posterior calibration.** Predictive-risk correlation improves under
  Student-t fitting, but confidence is not reliable enough to define a safe
  automatic fallback gate.
- **Model rights.** The fitted model derives from BSDS training material and
  remains external while redistribution terms are unresolved. The CC BY 4.0
  license of this paper does not license the model or dataset.
- **Hardware scope.** Optimized timings describe one Zen 3 CPU. A scalar
  fallback exists, but equivalent throughput is not claimed on other
  architectures.

Reproduction is anchored to tracked canonical JSON, deterministic patch
coordinates, fixed thread counts, and logical model hashes. The final native
model artifact is intentionally not committed. The paper's verification
script checks the headline numbers and hashes before rendering.

# Conclusion

Phase-conditioned Student-t mixture regression provides a practical classical
statistical alternative for X-Trans demosaicing. The method conditions directly
on the 49 physical samples of a 7 by 7 neighborhood, predicts only the two
missing center channels, and restores the measured center sample exactly.
Heavy-tailed responsibilities correct a demonstrated Gaussian-generalization
failure while improving held-out quality. A genuinely trained K32 model with
S9/q8 observable shortlisting retains 90.81% of the full Student-t gain and
reaches 35.056 dB on untouched BSDS coordinates, 3.002 dB above global
LMMSE.

The implementation result is equally important: algebraic specialization,
component-bucketed AVX2/FMA, phase/chunk OpenMP, and bounded streaming turn a
0.98 MP/s research kernel into a 6.85 MP/s standalone executor without changing
the frozen statistical contract. RawTherapee integration retains 79.2% of
that throughput and provides complete authenticated fallback.

The method should remain experimental. A broader camera-linear corpus,
cross-camera ground truth, and resolved model licensing are necessary before
routine distribution to users. Within those limits, the study demonstrates that a
robust, task-reduced mixture of joint spatial-chromatic regressors can capture
substantial nonlinear population information without a neural runtime or a
brittle external content classifier.

# Artifact and Ethics Statement

The manuscript source, original diagram, and rendered paper are released
under CC BY 4.0. RawTherapee remains under its project license. BSDS500, cited
papers, source images, and the external learned model retain their own terms.
No identifiable comparison photograph is reproduced.

AI-assisted tools were used for code generation, experiment orchestration,
data checking, bibliography preparation, and drafting. The named author is
responsible for verifying the mathematical statements, numerical claims,
citations, interpretation, and final text.

# References
