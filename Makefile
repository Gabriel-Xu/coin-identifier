CXX      ?= g++
CXXFLAGS += -O3 -std=c++17 -Wall -Wextra -Wshadow
# Expanded lazily so `make custom` works on a machine without OpenCV.
OPENCV    = $(shell pkg-config --cflags --libs opencv4)
CONVERT  ?= $(shell command -v magick 2>/dev/null || command -v convert 2>/dev/null)
IMG      ?= coins_3
IMAGES   := $(basename $(notdir $(wildcard images/*.jpg)))
PPMS     := $(IMAGES:%=images/%.ppm)

.PHONY: all run run-custom run-opencv compare eval tune ppm debug clean

all: custom opencv

custom: coin_identify_custom.cpp coin_common.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<

opencv: coin_identify_opencv.cpp coin_common.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(OPENCV)

# Sanitizer build of the custom detector (./custom-debug).
debug: coin_identify_custom.cpp coin_common.hpp
	$(CXX) -O1 -g -std=c++17 -Wall -Wextra -Wshadow -fsanitize=address,undefined -o custom-debug $<

# The custom detector reads PPM; generate binary (P6) PPMs from the JPEGs.
images/%.ppm: images/%.jpg
	$(CONVERT) $< $@

ppm: $(PPMS)

run: run-custom run-opencv

run-custom: custom images/$(IMG).ppm
	mkdir -p out/custom/$(IMG)
	./custom -f images/$(IMG).ppm -o out/custom/$(IMG) $(ARGS)

run-opencv: opencv
	mkdir -p out/opencv/$(IMG)
	./opencv -f images/$(IMG).jpg -o out/opencv/$(IMG) $(ARGS)

compare: run-custom run-opencv
	@echo "--- custom | opencv ($(IMG))"
	@diff -y out/custom/$(IMG)/results.txt out/opencv/$(IMG)/results.txt || true

# Run both detectors on every image and score against eval/ground_truth.txt.
eval: custom opencv $(PPMS)
	@for img in $(IMAGES); do \
	  mkdir -p out/custom/$$img out/opencv/$$img; \
	  ./custom -f images/$$img.ppm -o out/custom/$$img $(ARGS) > /dev/null || exit 1; \
	  ./opencv -f images/$$img.jpg -o out/opencv/$$img $(ARGS) > /dev/null || exit 1; \
	done
	python3 eval/eval.py

# Grid-search custom detector thresholds across all images (slow-ish).
tune: custom $(PPMS)
	python3 eval/tune.py $(TUNE_ARGS)

clean:
	rm -rf custom opencv custom-debug out
