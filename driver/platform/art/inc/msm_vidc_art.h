/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_ART_H_
#define _MSM_VIDC_ART_H_

struct msm_vidc_core;
extern struct msm_vidc_format_capability format_data_common;

#if defined(CONFIG_MSM_VIDC_ART)
int msm_vidc_get_platform_data_art(struct msm_vidc_core *core);
int msm_vidc_init_platform_art(struct msm_vidc_core *core);
#else
int msm_vidc_get_platform_data_art(struct msm_vidc_core *core)
{
	return -EINVAL;
}
int msm_vidc_init_platform_art(struct msm_vidc_core *core)
{
	return -EINVAL;
}
#endif

#endif // _MSM_VIDC_ART_H_
