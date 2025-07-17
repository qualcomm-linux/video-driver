/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __H_MSM_VIDC_POWER_IRIS36_H__
#define __H_MSM_VIDC_POWER_IRIS36_H__

#include "perf_static_model.h"

struct msm_vidc_inst;
struct vidc_bus_vote_data;

#define ENABLE_LEGACY_POWER_CALCULATIONS  0

int msm_vidc_ring_buf_count_iris36(struct msm_vidc_inst *inst, u32 data_size);
u64 msm_vidc_calc_freq_iris36(struct msm_vidc_inst *inst, u32 data_size);
int msm_vidc_calc_bw_iris36(struct msm_vidc_inst *inst,
					struct vidc_bus_vote_data *vote_data);
int msm_vidc_calculate_frequency_iris36(struct api_calculation_input codec_input,
					struct api_calculation_freq_output *codec_output);
int msm_vidc_calculate_bandwidth_iris36(struct api_calculation_input codec_input,
					struct api_calculation_bw_output *codec_output);
#endif
