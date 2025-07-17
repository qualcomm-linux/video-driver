// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>

#include "msm_vidc_power_iris4.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_core.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_power.h"
#include "resources.h"

#define VPP_MIN_FREQ_MARGIN_PERCENT                   5 /* to be tuned */

static int msm_vidc_get_hier_layer_val(struct msm_vidc_inst *inst)
{
	int hierachical_layer = CODEC_GOP_IPP;

	if (is_encode_session(inst)) {
		if (inst->capabilities[ALL_INTRA].value) {
			/* no P and B frames case */
			hierachical_layer = CODEC_GOP_IONLY;
		} else if (inst->capabilities[B_FRAME].value == 0) {
			/* no B frames case */
			hierachical_layer = CODEC_GOP_IPP;
		} else { /* P and B frames enabled case */
			if (inst->capabilities[ENH_LAYER_COUNT].value == 0 ||
				inst->capabilities[ENH_LAYER_COUNT].value == 1)
				hierachical_layer = CODEC_GOP_IbP;
			else if (inst->capabilities[ENH_LAYER_COUNT].value == 2)
				hierachical_layer = CODEC_GOP_I1B2b1P;
			else
				hierachical_layer = CODEC_GOP_I3B4b1P;
		}
	}

	return hierachical_layer;
}

static int msm_vidc_init_codec(struct msm_vidc_inst *inst,
		struct api_calculation_input *codec_input)
{
	if (is_encode_session(inst)) {
		codec_input->decoder_or_encoder = CODEC_ENCODER;
	} else if (is_decode_session(inst)) {
		codec_input->decoder_or_encoder = CODEC_DECODER;
	} else {
		d_vpr_e("%s: invalid domain %d\n", __func__, inst->domain);
		return -EINVAL;
	}

	if (inst->codec == MSM_VIDC_H264) {
		codec_input->lcu_size = 16;
		if (inst->capabilities[ENTROPY_MODE].value ==
			V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC) {
			codec_input->codec = CODEC_H264;
			codec_input->entropy_coding_mode = CODEC_ENTROPY_CODING_CABAC;
		} else {
			codec_input->codec = CODEC_H264_CAVLC;
			codec_input->entropy_coding_mode = CODEC_ENTROPY_CODING_CAVLC;
		}
	} else if (inst->codec == MSM_VIDC_HEVC) {
		codec_input->codec = CODEC_HEVC;
		codec_input->lcu_size = 32;
	} else if (inst->codec == MSM_VIDC_VP9) {
		codec_input->codec = CODEC_VP9;
		codec_input->lcu_size = 32;
	} else if (inst->codec == MSM_VIDC_AV1) {
		codec_input->codec = CODEC_AV1;
		codec_input->lcu_size =
			inst->capabilities[SUPER_BLOCK].value ? 128 : 64;
	} else if (inst->codec == MSM_VIDC_APV) {
		codec_input->codec = CODEC_APV;
		codec_input->lcu_size = 16;
	} else {
		d_vpr_e("%s: invalid codec %d\n", __func__, inst->codec);
		return -EINVAL;
	}

	return 0;
}

static int msm_vidc_init_codec_input_freq(struct msm_vidc_inst *inst, u32 data_size,
		struct api_calculation_input *codec_input)
{
	enum msm_vidc_port_type port;
	u32 color_fmt, tile_rows_columns = 0;
	int rc = 0;
	u32 max_rate, frame_rate;
	struct msm_vidc_core *core;

	codec_input->chipset_gen = MSM_CANOE;

	rc = msm_vidc_init_codec(inst, codec_input);
	if (rc)
		return rc;

	codec_input->pipe_num = inst->capabilities[PIPE].value;
	codec_input->frame_rate = inst->max_rate;

	port = is_decode_session(inst) ? INPUT_PORT : OUTPUT_PORT;
	codec_input->frame_width = inst->fmts[port].fmt.pix_mp.width;
	codec_input->frame_height = inst->fmts[port].fmt.pix_mp.height;

	if (inst->capabilities[STAGE].value == MSM_VIDC_STAGE_1) {
		codec_input->vsp_vpp_mode = CODEC_VSPVPP_MODE_1S;
	} else if (inst->capabilities[STAGE].value == MSM_VIDC_STAGE_2) {
		codec_input->vsp_vpp_mode = CODEC_VSPVPP_MODE_2S;
	} else {
		d_vpr_e("%s: invalid stage %lld\n", __func__,
				inst->capabilities[STAGE].value);
		return -EINVAL;
	}

	if (inst->capabilities[BIT_DEPTH].value == BIT_DEPTH_8)
		codec_input->bitdepth = CODEC_BITDEPTH_8;
	else
		codec_input->bitdepth = CODEC_BITDEPTH_10;

	codec_input->hierachical_layer =
		msm_vidc_get_hier_layer_val(inst);

	if (is_decode_session(inst)) {
		color_fmt = v4l2_colorformat_to_driver(inst,
			inst->fmts[OUTPUT_PORT].fmt.pix_mp.pixelformat, __func__);

		codec_input->linear_opb = is_linear_colorformat(color_fmt);

		codec_input->bitrate_mbps =
			(codec_input->frame_rate * data_size * 8) / 1000000;
	} else {
		color_fmt = v4l2_colorformat_to_driver(inst,
			inst->fmts[INPUT_PORT].fmt.pix_mp.pixelformat, __func__);

		codec_input->linear_ipb = is_linear_colorformat(color_fmt);

		if (codec_input->bitdepth == CODEC_BITDEPTH_10)
			codec_input->format_10bpp = __format_10bpp(color_fmt);

		frame_rate = msm_vidc_get_frame_rate(inst);
		max_rate = inst->max_rate;
		codec_input->bitrate_mbps =
			inst->capabilities[BIT_RATE].value / 1000000;

		/*
		 * In encoding cases, the bitrate should scale with the frame
		 * rate, especially for HFR cases.
		 * Otherwise, a lower bitrate may lead to a lower vsp frequency,
		 * resulting in insufficient performance.
		 */
		if (frame_rate && max_rate > frame_rate)
			codec_input->bitrate_mbps =
				codec_input->bitrate_mbps * max_rate / frame_rate;

	}

	/* av1d commercial tile */
	if (inst->codec == MSM_VIDC_AV1 && codec_input->lcu_size == 128) {
		tile_rows_columns = inst->power.fw_av1_tile_rows *
			inst->power.fw_av1_tile_columns;

		/* check resolution and tile info */
		codec_input->av1d_commer_tile_enable = 1;

		if (res_is_less_than_or_equal_to(codec_input->frame_width,
				codec_input->frame_height, 1920, 1088)) {
			if (tile_rows_columns <= 2)
				codec_input->av1d_commer_tile_enable = 0;
		} else if (res_is_less_than_or_equal_to(codec_input->frame_width,
				codec_input->frame_height, 4096, 2176)) {
			if (tile_rows_columns <= 4)
				codec_input->av1d_commer_tile_enable = 0;
		} else if (res_is_less_than_or_equal_to(codec_input->frame_width,
				codec_input->frame_height, 8192, 4320)) {
			if (tile_rows_columns <= 16)
				codec_input->av1d_commer_tile_enable = 0;
		}
	} else {
		codec_input->av1d_commer_tile_enable = 0;
	}

	/* set as sanity mode, this regression mode has no effect on power calculations */
	codec_input->regression_mode = REGRESSION_MODE_SANITY;

	codec_input->video_adv_feature = VIDEO_ADV_FEATURE_NONE;
	if (inst->capabilities[LOOKAHEAD_ENCODE_ENABLE].value)
		codec_input->video_adv_feature = FEATURE_LOOKAHEAD_ENCODE;

	if (inst->capabilities[ROTATION].value && codec_input->codec == CODEC_APV)
		codec_input->video_adv_feature = FEATURE_APV_ROTATION;

	core = inst->core;
	codec_input->vpu_ver = core->platform->data.vpu_ver;

	return 0;
}

static int msm_vidc_init_codec_input_bus(struct msm_vidc_inst *inst, struct vidc_bus_vote_data *d,
		struct api_calculation_input *codec_input)
{
	u32 complexity_factor_int = 0, complexity_factor_frac = 0, tile_rows_columns = 0;
	bool opb_compression_enabled = false;
	int rc = 0;

	if (!d)
		return -EINVAL;

	codec_input->chipset_gen = MSM_CANOE;

	rc = msm_vidc_init_codec(inst, codec_input);
	if (rc)
		return rc;

	codec_input->lcu_size = d->lcu_size;
	codec_input->pipe_num = d->num_vpp_pipes;
	codec_input->frame_rate = d->fps;
	codec_input->frame_width = d->input_width;
	codec_input->frame_height = d->input_height;
	codec_input->opb_frame_width = d->input_width;
	codec_input->opb_frame_height = d->input_height;

	/*
	 * update opb_frame_width and opb_frame_height with downscale resolution
	 * when downscale is enabled.
	 */
	if (inst->capabilities[SCALE_ENABLE].value) {
		codec_input->opb_frame_width = inst->compose.width;
		codec_input->opb_frame_height = inst->compose.height;
	}

	if (d->work_mode == MSM_VIDC_STAGE_1) {
		codec_input->vsp_vpp_mode = CODEC_VSPVPP_MODE_1S;
	} else if (d->work_mode == MSM_VIDC_STAGE_2) {
		codec_input->vsp_vpp_mode = CODEC_VSPVPP_MODE_2S;
	} else {
		d_vpr_e("%s: invalid stage %d\n", __func__, d->work_mode);
		return -EINVAL;
	}

	codec_input->hierachical_layer =
		msm_vidc_get_hier_layer_val(inst);

	/*
	 * If the calculated motion_vector_complexity is > 2 then set the
	 * complexity_setting and refframe_complexity to be pwc(performance worst case)
	 * values. If the motion_vector_complexity is < 2 then set the complexity_setting
	 * and refframe_complexity to be average case values.
	 */

	complexity_factor_int = Q16_INT(d->complexity_factor);
	complexity_factor_frac = Q16_FRAC(d->complexity_factor);

	if (complexity_factor_int < COMPLEXITY_THRESHOLD ||
		(complexity_factor_int == COMPLEXITY_THRESHOLD &&
		complexity_factor_frac == 0)) {
		/* set as average case values */
		codec_input->complexity_setting = COMPLEXITY_SETTING_AVG;
		codec_input->refframe_complexity = REFFRAME_COMPLEXITY_AVG;
	} else {
		/* set as pwc */
		codec_input->complexity_setting = COMPLEXITY_SETTING_PWC;
		codec_input->refframe_complexity = REFFRAME_COMPLEXITY_PWC;
	}

	codec_input->status_llc_onoff = d->use_sys_cache;

	if (__bpp(d->color_formats[0]) == 8) {
		codec_input->bitdepth = CODEC_BITDEPTH_8;
		codec_input->format_10bpp = 0;
	} else {
		codec_input->bitdepth = CODEC_BITDEPTH_10;
		codec_input->format_10bpp =
			__format_10bpp(d->color_formats[d->num_formats - 1]);
	}

	if (d->num_formats == 1) {
		codec_input->split_opb = 0;
		codec_input->linear_opb = !__ubwc(d->color_formats[0]);
	} else if (d->num_formats == 2) {
		codec_input->split_opb = 1;
		codec_input->linear_opb = !__ubwc(d->color_formats[1]);
	} else {
		d_vpr_e("%s: invalid num_formats %d\n",
			__func__, d->num_formats);
		return -EINVAL;
	}

	codec_input->linear_ipb = 0;   /* set as ubwc ipb */

	/* TODO Confirm if we always LOSSLESS mode ie lossy_ipb = 0*/
	codec_input->lossy_ipb = 0;   /* set as lossless ipb */

	/* TODO Confirm if no multiref */
	codec_input->encoder_multiref = 0;  /* set as no multiref */
	codec_input->bitrate_mbps = (d->bitrate / 1000000);

	opb_compression_enabled = d->num_formats >= 2 && __ubwc(d->color_formats[1]);

	/* video driver CR is in Q16 format, StaticModel CR in x100 format */
	if (d->domain == MSM_VIDC_DECODER) {
		codec_input->cr_dpb = ((Q16_INT(d->compression_ratio)*100) +
			Q16_FRAC(d->compression_ratio));
		codec_input->cr_opb = codec_input->cr_dpb;
		if (codec_input->split_opb == 1) {
			/* need to check the value if linear opb, currently set min cr */
			codec_input->cr_opb = 100;
		}
	} else {
		codec_input->cr_ipb = ((Q16_INT(d->input_cr)*100) + Q16_FRAC(d->input_cr));
		codec_input->cr_rpb = ((Q16_INT(d->compression_ratio)*100) +
			Q16_FRAC(d->compression_ratio));
	}

	/* disable by default, only enable for aurora depth map session */
	codec_input->lumaonly_decode = 0;

	/* set as custom regression mode, as are using cr,cf values from FW */
	codec_input->regression_mode = REGRESSION_MODE_CUSTOM;

	/* av1d commercial tile */
	if (inst->codec == MSM_VIDC_AV1 && codec_input->lcu_size == 128) {
		tile_rows_columns = inst->power.fw_av1_tile_rows *
			inst->power.fw_av1_tile_columns;

		/* check resolution and tile info */
		codec_input->av1d_commer_tile_enable = 1;

		if (res_is_less_than_or_equal_to(codec_input->frame_width,
					codec_input->frame_height, 1920, 1088)) {
			if (tile_rows_columns <= 2)
				codec_input->av1d_commer_tile_enable = 0;
		} else if (res_is_less_than_or_equal_to(codec_input->frame_width,
					codec_input->frame_height, 4096, 2176)) {
			if (tile_rows_columns <= 4)
				codec_input->av1d_commer_tile_enable = 0;
		} else if (res_is_less_than_or_equal_to(codec_input->frame_width,
					codec_input->frame_height, 8192, 4320)) {
			if (tile_rows_columns <= 16)
				codec_input->av1d_commer_tile_enable = 0;
		}
	} else {
		codec_input->av1d_commer_tile_enable = 0;
	}

	codec_input->video_adv_feature = VIDEO_ADV_FEATURE_NONE;
	if (inst->capabilities[LOOKAHEAD_ENCODE_ENABLE].value)
		codec_input->video_adv_feature = FEATURE_LOOKAHEAD_ENCODE;

	/* Dump all the variables for easier debugging */
	if (msm_vidc_debug & VIDC_BUS) {
		struct dump dump[] = {
		{"complexity_factor_int", "%d", complexity_factor_int},
		{"complexity_factor_frac", "%d", complexity_factor_frac},
		{"refframe_complexity", "%d", codec_input->refframe_complexity},
		{"complexity_setting", "%d", codec_input->complexity_setting},
		{"cr_dpb", "%d", codec_input->cr_dpb},
		{"cr_opb", "%d", codec_input->cr_opb},
		{"cr_ipb", "%d", codec_input->cr_ipb},
		{"cr_rpb", "%d", codec_input->cr_rpb},
		{"lcu size", "%d", codec_input->lcu_size},
		{"pipe number", "%d", codec_input->pipe_num},
		{"frame_rate", "%d", codec_input->frame_rate},
		{"frame_width", "%d", codec_input->frame_width},
		{"frame_height", "%d", codec_input->frame_height},
		{"opb_frame_width", "%d", codec_input->opb_frame_width},
		{"opb_frame_height", "%d", codec_input->opb_frame_height},
		{"work_mode", "%d", d->work_mode},
		{"encoder_or_decode", "%d", inst->domain},
		{"chipset_gen", "%d", codec_input->chipset_gen},
		{"codec_input", "%d", codec_input->codec},
		{"entropy_coding_mode", "%d", codec_input->entropy_coding_mode},
		{"hierachical_layer", "%d", codec_input->hierachical_layer},
		{"status_llc_onoff", "%d", codec_input->status_llc_onoff},
		{"bit_depth", "%d", codec_input->bitdepth},
		{"format_10bpp", "%d", codec_input->format_10bpp},
		{"split_opb", "%d", codec_input->split_opb},
		{"linear_opb", "%d", codec_input->linear_opb},
		{"linear_ipb", "%d", codec_input->linear_ipb},
		{"lossy_ipb", "%d", codec_input->lossy_ipb},
		{"encoder_multiref", "%d", codec_input->encoder_multiref},
		{"bitrate_mbps", "%d", codec_input->bitrate_mbps},
		{"lumaonly_decode", "%d", codec_input->lumaonly_decode},
		{"av1d_commer_tile_enable", "%d", codec_input->av1d_commer_tile_enable},
		{"regression_mode", "%d", codec_input->regression_mode},
		{"video_adv_feature", "%d", codec_input->video_adv_feature},
		};
		__dump(dump, ARRAY_SIZE(dump));
	}

	return 0;
}

static bool is_vpp_cycles_close_to_freq_corner(struct msm_vidc_core *core,
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

static int
msm_vidc_calc_freq_iris4(struct msm_vidc_inst *inst,
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
	ret = msm_vidc_init_codec_input_freq(inst, clock_scaling_data->data_size, &codec_input);
	if (ret)
		return ret;
	ret = msm_vidc_calculate_frequency_iris4(codec_input, &codec_output);
	if (ret)
		return ret;

	if (is_encode_session(inst)) {
		if (!inst->capabilities[ENC_RING_BUFFER_COUNT].value &&
			is_vpp_cycles_close_to_freq_corner(core,
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

static int get_clock_corner_index(struct msm_vidc_core *core, u64 vpp_freq, u64 apv_freq,
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

static int msm_vidc_get_freq_corner(struct msm_vidc_inst *inst)
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

	idx = get_clock_corner_index(core, vpp_freq, apv_freq, bse_freq, tensilica_freq);
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

int msm_vidc_scale_clocks_iris4(struct msm_vidc_inst *inst)
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
		msm_vidc_calc_freq_iris4(inst, clock_data);
		inst->power.min_vpp_freq = clock_data->vpp_freq;
		inst->power.min_apv_freq = clock_data->apv_freq;
		inst->power.min_bse_freq = clock_data->bse_freq;
		inst->power.min_tensilica_freq = clock_data->tensilica_freq;
		msm_vidc_apply_dcvs(inst);
	}

	return msm_vidc_get_freq_corner(inst);
}

int msm_vidc_calc_bw_iris4(struct msm_vidc_inst *inst,
		struct vidc_bus_vote_data *vidc_data)
{
	u32 ret = 0;
	struct api_calculation_input codec_input;
	struct api_calculation_bw_output codec_output;

	if (!vidc_data)
		return 0;

	memset(&codec_input, 0, sizeof(struct api_calculation_input));
	memset(&codec_output, 0, sizeof(struct api_calculation_bw_output));

	ret = msm_vidc_init_codec_input_bus(inst, vidc_data, &codec_input);
	if (ret)
		return ret;
	ret = msm_vidc_calculate_bandwidth_iris4(codec_input, &codec_output);
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
		ret = msm_vidc_calculate_bandwidth_iris4(codec_input, &codec_output);
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

int msm_vidc_ring_buf_count_iris4(struct msm_vidc_inst *inst, u32 data_size)
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
	rc = msm_vidc_init_codec_input_freq(inst, data_size, &codec_input);
	if (rc)
		return rc;
	rc = msm_vidc_calculate_frequency_iris4(codec_input, &codec_output);
	if (rc)
		return rc;

	/* check if vpp_min_freq is exceeding closest freq corner margin */
	if (is_vpp_cycles_close_to_freq_corner(core,
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
