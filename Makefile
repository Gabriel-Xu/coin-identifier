CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra
OPENCV   := $(shell pkg-config --cflags --libs opencv4)
IMG      ?= coins_3

.PHONY: all run run-custom run-opencv compare clean

all: custom opencv

custom: coin_identify_custom.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

opencv: coin_identify_opencv.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(OPENCV)

run: run-custom run-opencv

run-custom: custom
	mkdir -p out/custom
	cd out/custom && $(CURDIR)/custom -f $(CURDIR)/images/$(IMG).ppm

run-opencv: opencv
	mkdir -p out/opencv
	cd out/opencv && $(CURDIR)/opencv -f $(CURDIR)/images/$(IMG).jpg

compare:
	@diff -y out/custom/results.txt out/opencv/results.txt || true

clean:
	rm -rf custom opencv out
