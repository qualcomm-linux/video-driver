/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_X1E80100_H_
#define _MSM_VIDC_X1E80100_H_

#include "msm_vidc_core.h"

#if defined(CONFIG_MSM_VIDC_X1E80100)
int msm_vidc_get_platform_data_x1e80100(struct msm_vidc_core *core);
int msm_vidc_init_platform_x1e80100(struct msm_vidc_core *core);
#else
int msm_vidc_get_platform_data_x1e80100(struct msm_vidc_core *core)
{
	return -EINVAL;
}

int msm_vidc_init_platform_x1e80100(struct msm_vidc_core *core)
{
	return -EINVAL;
}
#endif

#endif // _MSM_VIDC_X1E80100_H_
