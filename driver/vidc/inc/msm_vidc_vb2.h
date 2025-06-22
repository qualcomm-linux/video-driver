/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_VB2_H_
#define _MSM_VIDC_VB2_H_

struct vb2_queue;
struct msm_vidc_inst;
struct msm_vidc_core;

struct vb2_queue *msm_vidc_get_vb2q(struct msm_vidc_inst *inst,
				    u32 type, const char *func);
int msm_vidc_start_streaming(struct msm_vidc_inst *inst, struct vb2_queue *q);
int msm_vidc_stop_streaming(struct msm_vidc_inst *inst, struct vb2_queue *q);
int msm_vidc_core_init_vb2_ops(struct msm_vidc_core *core);

#endif // _MSM_VIDC_VB2_H_
