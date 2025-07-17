/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __H_MSM_VIDC_POWER_IRIS3_H__
#define __H_MSM_VIDC_POWER_IRIS3_H__

#include "perf_static_model.h"

struct msm_vidc_inst;
struct vidc_bus_vote_data;
struct vidc_clock_scaling_data;

#define ENABLE_LEGACY_POWER_CALCULATIONS  0

int msm_vidc_scale_clocks_iris3(struct msm_vidc_inst *inst);
int msm_vidc_calc_bw_iris3(struct msm_vidc_inst *inst,
					struct vidc_bus_vote_data *vote_data);
int msm_vidc_calculate_frequency_iris3(struct api_calculation_input codec_input,
					struct api_calculation_freq_output *codec_output);
int msm_vidc_calculate_bandwidth_iris3(struct api_calculation_input codec_input,
					struct api_calculation_bw_output *codec_output);
#endif
