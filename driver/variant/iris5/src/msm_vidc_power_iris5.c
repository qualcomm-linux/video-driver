// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>

#include "msm_vidc_power_iris5.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_core.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_power.h"
#include "msm_vidc_variant.h"
#include "resources.h"

#define VPP_MIN_FREQ_MARGIN_PERCENT                   5 /* to be tuned */

bool is_vpp_cycles_close_to_freq_corner_iris5(struct msm_vidc_core *core,
	u64 vpp_min_freq)
{
	u64 margin_freq = 0, freq;
	u64 closest_freq_upper_corner = 0;
	u32 margin_percent = 0;
	int i = 0;

	if (!core || !core->resource) {
		d_vpr_e("%s: invalid params\n", __func__);
		return false;
	}

	vpp_min_freq = vpp_min_freq * 1000000; /* convert to hz */

	closest_freq_upper_corner =
		get_clock_freq(core, "video_cc_mvs0_clk_src", get_max_clock_index(core));

	/* return true if vpp_min_freq is more than max frequency */
	if (vpp_min_freq > closest_freq_upper_corner)
		return true;

	/* get the closest freq corner for vpp_min_freq */
	for (i = 0; i < get_clock_freq_count(core, "video_cc_mvs0_clk_src"); i++) {
		freq = get_clock_freq(core, "video_cc_mvs0_clk_src", i);
		if (vpp_min_freq <= freq)
			closest_freq_upper_corner = freq;
		else
			break;
	}

	margin_freq = closest_freq_upper_corner - vpp_min_freq;
	margin_percent = div_u64((margin_freq * 100), closest_freq_upper_corner);

	/* check if margin is less than cutoff */
	if (margin_percent < VPP_MIN_FREQ_MARGIN_PERCENT)
		return true;

	return false;
}

static int msm_vidc_calc_freq_iris5(struct msm_vidc_inst *inst,
			 struct vidc_clock_scaling_data *clock_scaling_data)
{
	u64 vpp_freq = 0, apv_freq = 0, bse_freq = 0, tensilica_freq = 0, nom_freq;
	struct msm_vidc_core *core;
	int ret = 0;
	struct api_calculation_input codec_input;
	struct api_calculation_freq_output codec_output;
	u32 fps, mbpf;

	core = inst->core;

	mbpf = msm_vidc_get_mbs_per_frame(inst);
	fps = inst->max_rate;

	memset(&codec_input, 0, sizeof(struct api_calculation_input));
	memset(&codec_output, 0, sizeof(struct api_calculation_freq_output));
	ret = msm_vidc_init_codec_input_freq_iris5p(inst,
				clock_scaling_data->data_size, &codec_input);
	if (ret)
		return ret;
	ret = msm_vidc_calculate_frequency_iris5(codec_input, &codec_output);
	if (ret)
		return ret;

	if (is_encode_session(inst)) {
		if (!inst->capabilities[ENC_RING_BUFFER_COUNT].value &&
			is_vpp_cycles_close_to_freq_corner_iris5(core,
				codec_output.vpp_min_freq)) {
			/*
			 * if ring buffer not enabled and required vpp cycles
			 * is too close to the frequency corner then increase
			 * the vpp cycles by VPP_MIN_FREQ_MARGIN_PERCENT
			 */
			codec_output.vpp_min_freq += div_u64(
				codec_output.vpp_min_freq *
				VPP_MIN_FREQ_MARGIN_PERCENT, 100);
			codec_output.hw_min_freq = max(
				codec_output.hw_min_freq,
				codec_output.vpp_min_freq);
		}
	}

	vpp_freq = (u64)codec_output.vpp_min_freq * 1000000; /* Convert to Hz */
	apv_freq = (u64)codec_output.apv_min_freq * 1000000; /* Convert to Hz */
	bse_freq = (u64)codec_output.vsp_min_freq * 1000000; /* Convert to Hz */
	tensilica_freq = (u64)codec_output.tensilica_min_freq * 1000000; /* Convert to Hz */

	i_vpr_p(inst,
		"%s: filled len %d, required vpp_freq %llu, apv_freq %llu, bse_freq %llu, tensilica_freq %llu, vpp %u, vsp %u, tensilica %u, hw_freq %u, fps %u, mbpf %u\n",
		__func__, clock_scaling_data->data_size, vpp_freq, apv_freq,
		bse_freq, tensilica_freq, codec_output.vpp_min_freq,
		codec_output.vsp_min_freq, codec_output.tensilica_min_freq,
		codec_output.hw_min_freq, fps, mbpf);

	if (!is_realtime_session(inst) ||
	    inst->codec == MSM_VIDC_AV1 ||
	    is_lowlatency_session(inst) ||
	    (inst->iframe && is_hevc_10bit_decode_session(inst))) {
		/*
		 * TURBO is only allowed for:
		 * - NRT decoding/encoding session
		 * - AV1 decoding session
		 * - Low latency session
		 * - 10-bit I-Frame decoding session
		 * limit to NOM for all other cases
		 */
	} else {
		/* limit to NOM, index 0 is TURBO, index 1 is NOM clock rate */
		if (get_clock_freq_count(core, "video_cc_mvs0_clk_src") >= 2) {
			nom_freq = get_clock_freq(core, "video_cc_mvs0_clk_src", 1);
			if (vpp_freq > nom_freq)
				vpp_freq = nom_freq;
		}

		if (get_clock_freq_count(core, "video_cc_mvs0a_clk_src") >= 2) {
			nom_freq = get_clock_freq(core, "video_cc_mvs0a_clk_src", 1);
			if (apv_freq > nom_freq)
				apv_freq = nom_freq;
		}

		if (get_clock_freq_count(core, "video_cc_mvs0b_clk_src") >= 2) {
			nom_freq = get_clock_freq(core, "video_cc_mvs0b_clk_src", 1);
			if (bse_freq > nom_freq)
				bse_freq = nom_freq;
		}

		if (get_clock_freq_count(core, "video_cc_mvs0c_clk_src") >= 2) {
			nom_freq = get_clock_freq(core, "video_cc_mvs0c_clk_src", 1);
			if (tensilica_freq > nom_freq)
				tensilica_freq = nom_freq;
		}
	}

	clock_scaling_data->vpp_freq = vpp_freq;
	clock_scaling_data->apv_freq = apv_freq;
	clock_scaling_data->bse_freq = bse_freq;
	clock_scaling_data->tensilica_freq = tensilica_freq;

	return ret;
}

static int get_clock_corner_index_iris5(struct msm_vidc_core *core, u64 vpp_freq, u64 apv_freq,
			      u64 bse_freq, u64 tensilica_freq)
{
	int idx, vpp_idx = INT_MAX, apv_idx = INT_MAX;
	int bse_idx = INT_MAX, tns_idx = INT_MAX;
	struct clock_info *cl;
	u64 rate = 0;

	venus_hfi_for_each_clock(core, cl) {
		/*
		 * keep checking from lowest to highest rate until
		 * table rate >= requested rate
		 */
		if (vpp_freq && !strcmp(cl->name, "video_cc_mvs0_clk_src")) {
			for (vpp_idx = cl->freq_count - 1; vpp_idx >= 0; vpp_idx--) {
				rate = cl->freq[vpp_idx];
				if (rate >= vpp_freq)
					break;
			}
		}

		if (apv_freq && !strcmp(cl->name, "video_cc_mvs0a_clk_src")) {
			for (apv_idx = cl->freq_count - 1; apv_idx >= 0; apv_idx--) {
				rate = cl->freq[apv_idx];
				if (rate >= apv_freq)
					break;
			}
		}

		if (bse_freq && !strcmp(cl->name, "video_cc_mvs0b_clk_src")) {
			for (bse_idx = cl->freq_count - 1; bse_idx >= 0; bse_idx--) {
				rate = cl->freq[bse_idx];
				if (rate >= bse_freq)
					break;
			}
		}

		if (tensilica_freq && !strcmp(cl->name, "video_cc_mvs0c_clk_src")) {
			for (tns_idx = cl->freq_count - 1; tns_idx >= 0; tns_idx--) {
				rate = cl->freq[tns_idx];
				if (rate >= tensilica_freq)
					break;
			}
		}
	}

	idx = min3(vpp_idx, apv_idx, bse_idx);

	return min(idx, tns_idx);
}

static int msm_vidc_get_freq_corner_iris5(struct msm_vidc_inst *inst)
{
	u64 vpp_freq = 0, apv_freq = 0, bse_freq = 0, tensilica_freq = 0;
	bool increment = false, decrement = true;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *temp;
	int idx;

	core = inst->core;

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		/* skip for session where no input is there to process */
		if (!temp->max_input_data_size)
			continue;

		/* skip inactive session clock rate */
		if (!temp->active)
			continue;

		vpp_freq += temp->power.min_vpp_freq;
		apv_freq += temp->power.min_apv_freq;
		bse_freq += temp->power.min_bse_freq;
		tensilica_freq += temp->power.min_tensilica_freq;

		if (msm_vidc_vpp_clock_voting && msm_vidc_apv_clock_voting &&
			   msm_vidc_bse_clock_voting && msm_vidc_tensilica_clock_voting) {
			d_vpr_l("msm_vidc_vpp_clock_voting %d\n", msm_vidc_vpp_clock_voting);
			vpp_freq = msm_vidc_vpp_clock_voting;
			apv_freq = msm_vidc_apv_clock_voting;
			bse_freq = msm_vidc_bse_clock_voting;
			tensilica_freq = msm_vidc_tensilica_clock_voting;
			decrement = false;
			break;
		}

		/* increment even if one session requested for it */
		if (temp->power.dcvs_flags & MSM_VIDC_DCVS_INCR)
			increment = true;
		/* decrement only if all sessions requested for it */
		if (!(temp->power.dcvs_flags & MSM_VIDC_DCVS_DECR))
			decrement = false;
	}
	mutex_unlock(&core->lock);

	idx = get_clock_corner_index_iris5(core, vpp_freq, apv_freq, bse_freq, tensilica_freq);
	if (idx < 0)
		idx = 0;

	if (increment) {
		if (idx > get_max_clock_index(core))
			idx -= 1;
	} else if (decrement) {
		if (idx < get_min_clock_index(core))
			idx += 1;
	}

	i_vpr_p(inst, "%s: requested rate: vpp %llu apv %llu bse %llu tensilica %llu\n",
		__func__, vpp_freq, apv_freq, bse_freq, tensilica_freq);
	i_vpr_p(inst, "%s: increment %d decrement %d\n", __func__, increment, decrement);

	core->power.clk_freq_idx = idx;

	return idx;
}

int msm_vidc_scale_clocks_iris5(struct msm_vidc_inst *inst)
{
	struct vidc_clock_scaling_data *clock_data;
	struct msm_vidc_core *core;

	core = inst->core;
	clock_data = &inst->clock_data;

	if (inst->power.buffer_counter < DCVS_WINDOW ||
	    is_image_session(inst) ||
	    is_sub_state(inst, MSM_VIDC_DRC) ||
	    is_sub_state(inst, MSM_VIDC_DRAIN)) {
		inst->power.min_vpp_freq = get_clock_freq(core, "video_cc_mvs0_clk_src",
							  get_max_clock_index(core));
		inst->power.min_apv_freq = get_clock_freq(core, "video_cc_mvs0a_clk_src",
							  get_max_clock_index(core));
		inst->power.min_bse_freq = get_clock_freq(core, "video_cc_mvs0b_clk_src",
							  get_max_clock_index(core));
		inst->power.min_tensilica_freq = get_clock_freq(core, "video_cc_mvs0c_clk_src",
								get_max_clock_index(core));
		inst->power.dcvs_flags = 0;
	} else if (msm_vidc_clock_voting ||
		   (msm_vidc_vpp_clock_voting && msm_vidc_apv_clock_voting &&
		    msm_vidc_bse_clock_voting && msm_vidc_tensilica_clock_voting)) {
		inst->power.min_vpp_freq = msm_vidc_vpp_clock_voting;
		inst->power.min_apv_freq = msm_vidc_apv_clock_voting;
		inst->power.min_bse_freq = msm_vidc_bse_clock_voting;
		inst->power.min_tensilica_freq = msm_vidc_tensilica_clock_voting;
		inst->power.dcvs_flags = 0;
	} else {
		clock_data->data_size = inst->max_input_data_size;
		msm_vidc_calc_freq_iris5(inst, clock_data);
		inst->power.min_vpp_freq = clock_data->vpp_freq;
		inst->power.min_apv_freq = clock_data->apv_freq;
		inst->power.min_bse_freq = clock_data->bse_freq;
		inst->power.min_tensilica_freq = clock_data->tensilica_freq;
		msm_vidc_apply_dcvs(inst);
	}

	return msm_vidc_get_freq_corner_iris5(inst);
}

int msm_vidc_calc_bw_iris5(struct msm_vidc_inst *inst,
		struct vidc_bus_vote_data *vidc_data)
{
	u32 ret = 0;
	struct api_calculation_input codec_input;
	struct api_calculation_bw_output codec_output;

	if (!vidc_data)
		return 0;

	memset(&codec_input, 0, sizeof(struct api_calculation_input));
	memset(&codec_output, 0, sizeof(struct api_calculation_bw_output));

	ret = msm_vidc_init_codec_input_bus_iris5p(inst, vidc_data, &codec_input);
	if (ret)
		return ret;
	ret = msm_vidc_calculate_bandwidth_iris5(codec_input, &codec_output);
	if (ret)
		return ret;

	vidc_data->calc_bw_ddr = kbps(codec_output.ddr_bw_rd + codec_output.ddr_bw_wr);
	vidc_data->calc_bw_llcc = kbps(codec_output.noc_bw_rd + codec_output.noc_bw_wr);

	/*
	 * if lookahead encoding enabled, then increase the bandwidth
	 * based on downscaled reslution extra processing, downscaling
	 * is equal to half the original resolution
	 */
	if (inst->capabilities[LOOKAHEAD_ENCODE_ENABLE].value) {
		codec_input.frame_width /= 2;
		codec_input.frame_height /= 2;
		ret = msm_vidc_calculate_bandwidth_iris5(codec_input, &codec_output);
		if (ret)
			return ret;
		vidc_data->calc_bw_ddr +=
			kbps(codec_output.ddr_bw_rd + codec_output.ddr_bw_wr);
		vidc_data->calc_bw_llcc +=
			kbps(codec_output.noc_bw_rd + codec_output.noc_bw_wr);
		i_vpr_l(inst, "%s: lookahead extra bw %u, %u kbps\n", __func__,
			kbps(codec_output.ddr_bw_rd + codec_output.ddr_bw_wr),
			kbps(codec_output.noc_bw_rd + codec_output.noc_bw_wr));
	}

	i_vpr_l(inst, "%s: calc_bw_ddr %llu calc_bw_llcc %llu kbps\n",
		__func__, vidc_data->calc_bw_ddr, vidc_data->calc_bw_llcc);

	return ret;
}

int msm_vidc_ring_buf_count_iris5(struct msm_vidc_inst *inst, u32 data_size)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct api_calculation_input codec_input;
	struct api_calculation_freq_output codec_output;

	core = inst->core;

	if (!core->resource) {
		i_vpr_e(inst, "%s: invalid frequency table\n", __func__);
		return -EINVAL;
	}

	memset(&codec_input, 0, sizeof(struct api_calculation_input));
	memset(&codec_output, 0, sizeof(struct api_calculation_freq_output));
	rc = msm_vidc_init_codec_input_freq_iris5p(inst, data_size, &codec_input);
	if (rc)
		return rc;
	rc = msm_vidc_calculate_frequency_iris5(codec_input, &codec_output);
	if (rc)
		return rc;

	/* check if vpp_min_freq is exceeding closest freq corner margin */
	if (is_vpp_cycles_close_to_freq_corner_iris5(core,
		codec_output.vpp_min_freq)) {
		/* enable ring buffer */
		i_vpr_h(inst,
			"%s: vpp_min_freq %d, ring_buffer_count %d\n",
			__func__, codec_output.vpp_min_freq, MAX_ENC_RING_BUF_COUNT);
		inst->capabilities[ENC_RING_BUFFER_COUNT].value =
			MAX_ENC_RING_BUF_COUNT;
	} else {
		inst->capabilities[ENC_RING_BUFFER_COUNT].value = 0;
	}
	return 0;
}
