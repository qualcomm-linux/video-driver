/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_IRIS4_H_
#define _MSM_VIDC_IRIS4_H_

struct msm_vidc_core;
struct v4l2_ctrl;

#if defined(CONFIG_MSM_VIDC_CANOE) || defined(CONFIG_MSM_VIDC_SERAPH)
int msm_vidc_init_iris4(struct msm_vidc_core *core);
int msm_vidc_adjust_min_quality_iris4(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_bitrate_boost_iris4(void *instance, struct v4l2_ctrl *ctrl);
#else
static inline int msm_vidc_init_iris4(struct msm_vidc_core *core)
{
	return -EINVAL;
}

static inline int msm_vidc_adjust_bitrate_boost_iris4(void *instance, struct v4l2_ctrl *ctrl)
{
	return -EINVAL;
}

static inline int msm_vidc_adjust_min_quality_iris4(void *instance, struct v4l2_ctrl *ctrl)
{
	return -EINVAL;
}

#endif

#endif // _MSM_VIDC_IRIS4_H_
