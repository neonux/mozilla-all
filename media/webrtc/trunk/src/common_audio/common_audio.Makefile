# This file is generated by gyp; do not edit.

export builddir_name ?= trunk/src/common_audio/out
.PHONY: all
all:
	$(MAKE) -C ../.. signal_processing vad resampler
