/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_VARIANT_H_
#define _MSM_VIDC_VARIANT_H_

struct msm_vidc_core;

int __write_register_masked(struct msm_vidc_core *core, u32 reg, u32 value,
		u32 mask);
int __write_register(struct msm_vidc_core *core, u32 reg, u32 value);
int __read_register(struct msm_vidc_core *core, u32 reg, u32 *value);
int __read_register_with_poll_timeout(struct msm_vidc_core *core, u32 reg,
		u32 mask, u32 exp_val, u32 sleep_us, u32 timeout_us);
int __set_registers(struct msm_vidc_core *core);
int msm_vidc_mem_protect_video_regions_v1(struct msm_vidc_core *core);
int msm_vidc_mem_protect_video_regions_v2(struct msm_vidc_core *core);
int msm_vidc_decide_scaling_iris5p(struct msm_vidc_inst *inst);
int msm_vidc_decide_quality_mode_iris5p(struct msm_vidc_inst *inst);
int msm_vidc_decide_work_route_iris5p(struct msm_vidc_inst *inst);
int msm_vidc_decide_work_mode_iris5p(struct msm_vidc_inst *inst);

#endif
