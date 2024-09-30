/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2024, Red Hat Inc.
 *
 * Black level handling
 */

#include "blc.h"

#include <numeric>

#include <libcamera/base/log.h>

namespace libcamera {

namespace ipa::soft::algorithms {

LOG_DEFINE_CATEGORY(IPASoftBL)

BlackLevel::BlackLevel()
{
}

int BlackLevel::configure(IPAContext &context,
			  [[maybe_unused]] const IPAConfigInfo &configInfo)
{
	context.activeState.blc.level = 255;
	return 0;
}

void BlackLevel::process(IPAContext &context,
			 [[maybe_unused]] const uint32_t frame,
			 IPAFrameContext &frameContext,
			 const SwIspStats *stats,
			 [[maybe_unused]] ControlList &metadata)
{
	if (frameContext.sensor.exposure == exposure_ &&
	    frameContext.sensor.gain == gain_) {
		return;
	}

	const SwIspStats::Histogram &histogram = stats->yHistogram;

	/*
	 * The constant is selected to be "good enough", not overly
	 * conservative or aggressive. There is no magic about the given value.
	 */
	constexpr float ignoredPercentage = 0.02;
	const unsigned int total =
		std::accumulate(begin(histogram), end(histogram), 0);
	const unsigned int pixelThreshold = ignoredPercentage * total;
	const unsigned int histogramRatio = 256 / SwIspStats::kYHistogramSize;
	const unsigned int currentBlackIdx =
		context.activeState.blc.level / histogramRatio;

	for (unsigned int i = 0, seen = 0;
	     i < currentBlackIdx && i < SwIspStats::kYHistogramSize;
	     i++) {
		seen += histogram[i];
		if (seen >= pixelThreshold) {
			context.activeState.blc.level = i * histogramRatio;
			exposure_ = frameContext.sensor.exposure;
			gain_ = frameContext.sensor.gain;
			LOG(IPASoftBL, Debug)
				<< "Auto-set black level: "
				<< i << "/" << SwIspStats::kYHistogramSize
				<< " (" << 100 * (seen - histogram[i]) / total << "% below, "
				<< 100 * seen / total << "% at or below)";
			break;
		}
	};
}

REGISTER_IPA_ALGORITHM(BlackLevel, "BlackLevel")

} /* namespace ipa::soft::algorithms */

} /* namespace libcamera */