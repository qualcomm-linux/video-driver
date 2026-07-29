/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_MONACO_H_
#define _MSM_VIDC_MONACO_H_

struct msm_vidc_core;

#if defined(CONFIG_MSM_VIDC_QLI)
int msm_vidc_get_platform_data_monaco(struct msm_vidc_core *core);
int msm_vidc_init_platform_monaco(struct msm_vidc_core *core);
#else
int msm_vidc_get_platform_data_monaco(struct msm_vidc_core *core)
{
	return -EINVAL;
}

int msm_vidc_init_platform_monaco(struct msm_vidc_core *core)
{
	return -EINVAL;
}

#endif

#endif // _MSM_VIDC_MONACO_H_

