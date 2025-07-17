/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_TUNA_H_
#define _MSM_VIDC_TUNA_H_

#include "msm_vidc_core.h"

#if defined(CONFIG_MSM_VIDC_SUN)
int msm_vidc_get_platform_data_tuna(struct msm_vidc_core *core);
int msm_vidc_init_platform_tuna(struct msm_vidc_core *core);
int msm_vidc_deinit_platform_tuna(struct msm_vidc_core *core);
#else
int msm_vidc_get_platform_data_tuna(struct msm_vidc_core *core)
{
	return -EINVAL;
}

int msm_vidc_init_platform_tuna(struct msm_vidc_core *core)
{
	return -EINVAL;
}

int msm_vidc_deinit_platform_tuna(struct msm_vidc_core *core)
{
	return -EINVAL;
}
#endif

#endif // _MSM_VIDC_TUNA_H_
