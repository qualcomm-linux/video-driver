/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __H_MSM_VIDC_POWER_IRIS5_H__
#define __H_MSM_VIDC_POWER_IRIS5_H__

#include "perf_static_model.h"

struct msm_vidc_inst;
struct vidc_bus_vote_data;
struct vidc_clock_scaling_data;

int msm_vidc_scale_clocks_iris5(struct msm_vidc_inst *inst);
int msm_vidc_calc_bw_iris5(struct msm_vidc_inst *inst,
			   struct vidc_bus_vote_data *vote_data);
int msm_vidc_calculate_bandwidth_iris5(struct api_calculation_input codec_input,
				struct api_calculation_bw_output *codec_output);
int msm_vidc_calculate_frequency_iris5(struct api_calculation_input codec_input,
				struct api_calculation_freq_output *codec_output);
int msm_vidc_init_codec_input_freq_iris5p(struct msm_vidc_inst *inst, u32 data_size,
		struct api_calculation_input *codec_input);
int msm_vidc_init_codec_input_bus_iris5p(struct msm_vidc_inst *inst,
	struct vidc_bus_vote_data *d, struct api_calculation_input *codec_input);
int msm_vidc_ring_buf_count_iris5(struct msm_vidc_inst *inst, u32 data_size);

#endif
