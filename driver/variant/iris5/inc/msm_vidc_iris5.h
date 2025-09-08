/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_IRIS5_H_
#define _MSM_VIDC_IRIS5_H_

struct msm_vidc_core;
struct v4l2_ctrl;

#if defined(CONFIG_MSM_VIDC_ART)
int msm_vidc_init_iris5(struct msm_vidc_core *core);
#else
static inline int msm_vidc_init_iris5(struct msm_vidc_core *core)
{
	return -EINVAL;
}

#endif

#endif // _MSM_VIDC_IRIS4_H_
