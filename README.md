# coin-identifier

Detects and counts US coins in an image, then totals their value. A from-scratch implementation (`coin_identify_custom.cpp`: Canny edges + circle voting) is compared against an OpenCV version (`coin_identify_opencv.cpp`: Hough circles).

## Requirements

- `g++` (C++17)
- OpenCV 4 with `pkg-config` (only for the OpenCV version)

## Usage

```sh
make                      # build ./custom and ./opencv
make run-custom           # custom detector only
make run-opencv           # OpenCV detector only
make compare              # run both, show results side by side
make compare IMG=coins_2  # use images/coins_2.{ppm,jpg}
make clean
```

The custom detector reads `images/<IMG>.ppm` (plain-text P3); the OpenCV detector reads `images/<IMG>.jpg`. Outputs (intermediate images and `results.txt`) go to `out/custom/` and `out/opencv/`.

## Flags

Both programs take `-f <input>` plus threshold overrides:

| Flag | Custom | OpenCV |
|------|--------|--------|
| `-lt`, `-ht` | Canny low/high threshold | Canny low/high threshold |
| `-pt` | penny cutoff | penny radius |
| `-TC` / `-TCircle` | center / circle vote thresholds | — |
| `-ct` | — | Hough accumulator threshold |
| `-bt` | — | median blur kernel size |
| `-dt`, `-dt2`, `-qt` | — | dime / quarter size cutoffs |
