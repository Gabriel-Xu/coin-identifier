# coin-identifier

Detects and counts US coins in a photo, then totals their value. A from-scratch
implementation (`coin_identify_custom.cpp`) is compared against an OpenCV
version (`coin_identify_opencv.cpp`). Both share `coin_common.hpp`, which holds
the coin-type table, the scale-invariant classification and the result format,
so their outputs are directly comparable.

## Requirements

- `g++` (C++17)
- ImageMagick (`convert` or `magick`) to turn the JPEGs into PPMs for the custom detector
- OpenCV 4 with `pkg-config` (only for the OpenCV version)
- Python 3 (only for `make eval` / `make tune`)

## Usage

```sh
make                      # build ./custom and ./opencv
make run-custom           # custom detector on images/coins_3 (generates the .ppm if needed)
make run-opencv           # OpenCV detector on images/coins_3.jpg
make compare IMG=coins_2  # run both on coins_2, show results side by side
make eval                 # run both on every image, score against eval/ground_truth.txt
make tune                 # grid-search custom thresholds across all images
make debug                # sanitizer build of the custom detector (./custom-debug)
make clean
```

Outputs go to `out/<custom|opencv>/<image>/`:

- `results.txt` – counts per coin type, number of coins, reference penny radius, total value
- `circles.txt` – one line per coin: centre, radius, type, colour feature, support, mean RGB
- `imageCoins.*` – input with each coin outlined in its type colour
  (penny red, nickel yellow, dime blue, quarter magenta, half dollar green, dollar pink)
- with `-debug 1`: edge map, Hough accumulator, centre candidates, outlines (custom) or
  grayscale and Canny images (OpenCV)

Extra flags can be passed through `make`: `make run-custom IMG=coins_1 ARGS="-debug 1"`.
Run `./custom -h` or `./opencv -h` for the full list.

## How the custom detector works

1. Box-downscale so the shorter side is about 800 px (`-ds`), convert to luma, 5-tap Gaussian blur.
2. Sobel gradients; Canny with non-maximum suppression then stack-based hysteresis.
   The high threshold defaults to Otsu's threshold on the gradient magnitude, the low one
   to half of it (`-ht`, `-lt`, `-lowfrac`).
3. Each edge pixel votes along its gradient direction, both ways, only at distances within
   the coin radius band (`-rmin`, `-rmax`; default 2 % to 7 % of the shorter side).
4. Centres are local maxima of the 3x3-summed accumulator above a fraction of `2*pi*rmin`
   votes (`-vt`).
5. For each centre the radius is read off a distance histogram of edge pixels whose
   gradient points at the centre (`-cos`); engraving edges fail that test. Circles need at
   least `-ct` of their circumference supported. Overlapping candidates are suppressed in
   order of support.
6. Centre and radius are refined at full resolution with a gradient-weighted histogram,
   taking the innermost ring close to the strongest (`-inner`) so a coin's shadow is not
   mistaken for its rim.
7. Classification (shared with the OpenCV version): coins are split into copper/brass vs
   silver using red-minus-green (`-copper`) and tint-corrected red-minus-blue (`-gold`,
   for golden dollars) over the centre of each coin. A penny radius is fitted so that all
   radii best match the known diameter ratios (dime 0.94, nickel 1.11, quarter 1.27,
   golden dollar 1.39, half dollar 1.61, Eisenhower dollar 2.00 of a penny), re-estimated
   from the coins labelled penny, and each coin takes the nearest ratio for its colour
   class. `-ref` pins the penny radius instead.

All thresholds are relative (to image size, gradient statistics or the penny radius), so
the same defaults work across the three sample images at different scales and lighting.

## Flags

Common to both programs: `-f` input, `-o` output directory, `-ds`, `-rmin`, `-rmax`,
`-copper`, `-gold`, `-ref`, `-debug`. The OpenCV version works at a 1600 px shorter side
by default (the custom one at 800 px) because `HOUGH_GRADIENT_ALT` misses touching and
overlapping coins at the coarser scale.

| Flag | Custom | OpenCV |
|------|--------|--------|
| `-ht`, `-lt`, `-lowfrac` | Canny thresholds (0 = automatic) | `-ht`: Canny high threshold inside `HoughCircles` |
| `-vt` | centre vote threshold (fraction of `2*pi*rmin`) | — |
| `-ct` | min rim support fraction | accumulator threshold (`-method gradient`) |
| `-cos` | min cosine between edge gradient and radial direction | — |
| `-refine`, `-inner` | full-resolution refinement | — |
| `-method`, `-perf` | — | `alt` (default, `HOUGH_GRADIENT_ALT`, perfectness `-perf`, default 0.6) or `gradient` |
| `-blur` | — | Gaussian kernel size at working scale |

## Evaluation

`eval/ground_truth.txt` lists the true counts per image. `make eval` prints, for each
detector and image, detected-minus-true counts per type plus the total-coin and
dollar-value error, and sums the absolute errors over all images. `make tune` runs the
custom detector over a small parameter grid and ranks settings by that summed error, so a
setting only scores well if it works on every image.
