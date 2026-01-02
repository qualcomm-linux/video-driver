/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __H_MSM_VIDC_BUFFER_IRIS2_H__
#define __H_MSM_VIDC_BUFFER_IRIS2_H__

#include "msm_vidc_inst.h"
struct msm_vidc_inst;
enum msm_vidc_buffer_type;

int msm_buffer_size_iris2(struct msm_vidc_inst *inst,
		enum msm_vidc_buffer_type buffer_type);
int msm_buffer_min_count_iris2(struct msm_vidc_inst *inst,
		enum msm_vidc_buffer_type buffer_type);
int msm_buffer_extra_count_iris2(struct msm_vidc_inst *inst,
		enum msm_vidc_buffer_type buffer_type);
#endif // __H_MSM_VIDC_BUFFER_IRIS2_H__
