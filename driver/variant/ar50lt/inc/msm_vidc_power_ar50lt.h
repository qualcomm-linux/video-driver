/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __H_MSM_VIDC_POWER_AR50LT_H__
#define __H_MSM_VIDC_POWER_AR50LT_H__

#include "msm_vidc_power.h"
#include "msm_vidc_inst.h"

int msm_vidc_scale_clocks_ar50lt(struct msm_vidc_inst *inst);
int msm_vidc_calc_bw_ar50lt(struct msm_vidc_inst *inst,
		struct vidc_bus_vote_data *vote_data);
#endif
