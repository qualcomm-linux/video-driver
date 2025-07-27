/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __H_MSM_VIDC_BUFFER_IRIS36_H__
#define __H_MSM_VIDC_BUFFER_IRIS36_H__

struct msm_vidc_inst;
enum msm_vidc_buffer_type;

int msm_buffer_size_iris36(struct msm_vidc_inst *inst,
		enum msm_vidc_buffer_type buffer_type);
int msm_buffer_min_count_iris36(struct msm_vidc_inst *inst,
		enum msm_vidc_buffer_type buffer_type);
int msm_buffer_extra_count_iris36(struct msm_vidc_inst *inst,
		enum msm_vidc_buffer_type buffer_type);
int msm_vidc_encoder_decide_slice_max_mb_iris36(struct msm_vidc_inst *inst);

#endif // __H_MSM_VIDC_BUFFER_IRIS36_H__
