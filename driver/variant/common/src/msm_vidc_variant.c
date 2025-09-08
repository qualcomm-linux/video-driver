// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/errno.h>
#include <linux/iopoll.h>
#include <linux/version.h>
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
#include <linux/firmware/qcom/qcom_scm.h>
#else
#include <linux/qcom_scm.h>
#endif
#include <linux/soc/qcom/smem.h>

#include "perf_static_model.h"
#include "msm_vidc_core.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_state.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_variant.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_events.h"
#include "venus_hfi.h"
#include "resources.h"
#include "msm_vidc_power.h"

enum video_memory_region {
	VIDEO_REGION_SECURE_FW_REGION_ID    = 0,
	VIDEO_REGION_VM0_SECURE_NP_ID       = 1,
	VIDEO_REGION_VM1_SECURE_NP_ID       = 2,
	VIDEO_REGION_VM2_SECURE_NP_ID       = 3,
	VIDEO_REGION_VM3_SECURE_NP_ID       = 4,
	VIDEO_REGION_VM0_NONSECURE_NP_ID    = 5,
	VIDEO_REGION_VM1_NONSECURE_NP_ID    = 6,
	VIDEO_REGION_VM2_NONSECURE_NP_ID    = 7,
	VIDEO_REGION_VM3_NONSECURE_NP_ID    = 8
};

int __write_register(struct msm_vidc_core *core, u32 reg, u32 value)
{
	u32 hwiosymaddr = reg;
	u8 *base_addr;
	int rc = 0;

	rc = __strict_check(core, __func__);
	if (rc)
		return rc;

	if (!is_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE)) {
		d_vpr_e("HFI Write register failed : Power is OFF\n");
		return -EINVAL;
	}

	base_addr = core->resource->register_base_addr;
	d_vpr_l("regwrite(%pK + %#x) = %#x\n", base_addr, hwiosymaddr, value);
	base_addr += hwiosymaddr;
	writel_relaxed(value, base_addr);

	/* Memory barrier to make sure value is written into the register */
	wmb();

	return rc;
}

/*
 * Argument mask is used to specify which bits to update. In case mask is 0x11,
 * only bits 0 & 4 will be updated with corresponding bits from value. To update
 * entire register with value, set mask = 0xFFFFFFFF.
 */
int __write_register_masked(struct msm_vidc_core *core, u32 reg, u32 value,
			    u32 mask)
{
	u32 prev_val, new_val;
	u8 *base_addr;
	int rc = 0;

	rc = __strict_check(core, __func__);
	if (rc)
		return rc;

	if (!is_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE)) {
		d_vpr_e("%s: register write failed, power is off\n",
			__func__);
		return -EINVAL;
	}

	base_addr = core->resource->register_base_addr;
	base_addr += reg;

	prev_val = readl_relaxed(base_addr);
	/*
	 * Memory barrier to ensure register read is correct
	 */
	rmb();

	new_val = (prev_val & ~mask) | (value & mask);
	d_vpr_l(
		"Base addr: %pK, writing to: %#x, previous-value: %#x, value: %#x, mask: %#x, new-value: %#x...\n",
		base_addr, reg, prev_val, value, mask, new_val);
	writel_relaxed(new_val, base_addr);
	/*
	 * Memory barrier to make sure value is written into the register.
	 */
	wmb();

	return rc;
}

int __read_register(struct msm_vidc_core *core, u32 reg, u32 *value)
{
	int rc = 0;
	u8 *base_addr;

	if (!is_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE)) {
		d_vpr_e("HFI Read register failed : Power is OFF\n");
		return -EINVAL;
	}

	base_addr = core->resource->register_base_addr;

	*value = readl_relaxed(base_addr + reg);
	/*
	 * Memory barrier to make sure value is read correctly from the
	 * register.
	 */
	rmb();
	d_vpr_l("regread(%pK + %#x) = %#x\n", base_addr, reg, *value);

	return rc;
}

int __read_register_with_poll_timeout(struct msm_vidc_core *core, u32 reg,
				      u32 mask, u32 exp_val, u32 sleep_us,
				      u32 timeout_us)
{
	int rc = 0;
	u32 val = 0;
	u8 *addr;

	if (!is_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE)) {
		d_vpr_e("%s failed: Power is OFF\n", __func__);
		return -EINVAL;
	}

	addr = (u8 *)core->resource->register_base_addr + reg;

	rc = readl_relaxed_poll_timeout(addr, val, ((val & mask) == exp_val), sleep_us, timeout_us);
	/*
	 * Memory barrier to make sure value is read correctly from the
	 * register.
	 */
	rmb();
	d_vpr_l(
		"regread(%pK + %#x) = %#x. rc %d, mask %#x, exp_val %#x, cond %u, sleep %u, timeout %u\n",
		core->resource->register_base_addr, reg, val, rc, mask, exp_val,
		((val & mask) == exp_val), sleep_us, timeout_us);

	return rc;
}

int __set_registers(struct msm_vidc_core *core)
{
	const struct reg_preset_table *reg_prst;
	unsigned int prst_count;
	int cnt, rc = 0;

	reg_prst = core->platform->data.reg_prst_tbl;
	prst_count = core->platform->data.reg_prst_tbl_size;

	/* skip if there is no preset reg available */
	if (!reg_prst || !prst_count)
		return 0;

	for (cnt = 0; cnt < prst_count; cnt++) {
		rc = __write_register_masked(core, reg_prst[cnt].reg,
				reg_prst[cnt].value, reg_prst[cnt].mask);
		if (rc)
			return rc;
	}

	return rc;
}

int msm_vidc_mem_protect_video_regions_v1(struct msm_vidc_core *core)
{
	int rc = 0;
	struct context_bank_info *cb;
	u32 cp_start = 0, cp_size = 0, cp_nonpixel_start = 0, cp_nonpixel_size = 0;

	venus_hfi_for_each_context_bank(core, cb) {
		if (cb->region & MSM_VIDC_NON_SECURE) {
			cp_size = cb->addr_range.start;

			d_vpr_h("%s: cp_size: %#x\n",
				__func__, cp_size);
		}

		if (cb->region & MSM_VIDC_SECURE_NONPIXEL) {
			cp_nonpixel_start = cb->addr_range.start;
			cp_nonpixel_size = cb->addr_range.size;

			d_vpr_h("%s: cp_nonpixel_start: %#x size: %#x\n",
				__func__, cp_nonpixel_start,
				cp_nonpixel_size);
		}
	}

	rc = qcom_scm_mem_protect_video_var(cp_start, cp_size,
			cp_nonpixel_start, cp_nonpixel_size);
	if (rc) {
		d_vpr_e("Failed to protect memory(%d)\n", rc);
		return rc;
	}

	trace_venus_hfi_var_done(cp_start, cp_size, cp_nonpixel_start,
			cp_nonpixel_size);

	return rc;
}

int msm_vidc_mem_protect_video_regions_v2(struct msm_vidc_core *core)
{
	int rc = 0;
	struct context_bank_info *cb;
	int region = -1;
	u32 start = 0, size = 0;

	venus_hfi_for_each_context_bank(core, cb) {

		if (cb->region & MSM_VIDC_NON_SECURE)
			region = VIDEO_REGION_VM0_NONSECURE_NP_ID;
		else if (cb->region & MSM_VIDC_SECURE_NONPIXEL)
			region = VIDEO_REGION_VM0_SECURE_NP_ID;
		else
			continue;

		start = cb->addr_range.start;
		size = cb->addr_range.size;

		rc = qcom_scm_mem_protect_video_var(region, 0, start, size);
		if (rc) {
			d_vpr_e("%s Failed to protect memory(%d)\n", __func__, rc);
			return rc;
		}

		trace_venus_hfi_var_done(region, 0, start, size);
	}

	return rc;
}


int msm_vidc_decide_work_mode_iris5p(struct msm_vidc_inst *inst)
{
	u32 work_mode;
	struct v4l2_format *inp_f;
	u32 width, height;
	bool res_ok = false;

	work_mode = MSM_VIDC_STAGE_2;
	inp_f = &inst->fmts[INPUT_PORT];

	/* APV codec is only one stage for Canoe */
	if (inst->codec == MSM_VIDC_APV) {
		work_mode = MSM_VIDC_STAGE_1;
		goto exit;
	}

	if (is_image_decode_session(inst))
		work_mode = MSM_VIDC_STAGE_1;

	if (is_image_session(inst))
		goto exit;

	if (is_decode_session(inst)) {
		height = inp_f->fmt.pix_mp.height;
		width = inp_f->fmt.pix_mp.width;
		res_ok = res_is_less_than(width, height, 1280, 720);
		if (inst->capabilities[CODED_FRAMES].value ==
				CODED_FRAMES_INTERLACE ||
			inst->capabilities[LOWLATENCY_MODE].value ||
			res_ok) {
			work_mode = MSM_VIDC_STAGE_1;
		}
	} else if (is_encode_session(inst)) {
		height = inst->crop.height;
		width = inst->crop.width;
		res_ok = !res_is_greater_than(width, height, 4096, 2160);
		if (res_ok &&
			(inst->capabilities[LOWLATENCY_MODE].value)) {
			work_mode = MSM_VIDC_STAGE_1;
		}
		if (inst->capabilities[SLICE_MODE].value ==
			V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES) {
			work_mode = MSM_VIDC_STAGE_1;
		}
		if (inst->capabilities[LOSSLESS].value)
			work_mode = MSM_VIDC_STAGE_2;

		if (!inst->capabilities[GOP_SIZE].value)
			work_mode = MSM_VIDC_STAGE_2;
	} else {
		i_vpr_e(inst, "%s: invalid session type\n", __func__);
		return -EINVAL;
	}

exit:
	if (work_mode >= inst->capabilities[STAGE].max)
		work_mode = inst->capabilities[STAGE].max;

	i_vpr_h(inst, "Configuring work mode = %u low latency = %llu, gop size = %llu\n",
		work_mode, inst->capabilities[LOWLATENCY_MODE].value,
		inst->capabilities[GOP_SIZE].value);
	msm_vidc_update_cap_value(inst, STAGE, work_mode, __func__);

	return 0;
}

int msm_vidc_decide_work_route_iris5p(struct msm_vidc_inst *inst)
{
	u32 work_route;
	struct msm_vidc_core *core;

	core = inst->core;
	work_route = core->capabilities[NUM_VPP_PIPE].value;

	/* APV codec is only one pipe for Canoe */
	if (inst->codec == MSM_VIDC_APV) {
		work_route = MSM_VIDC_PIPE_1;
		goto exit;
	}

	if (is_image_session(inst))
		goto exit;

	if (is_decode_session(inst)) {
		if (inst->capabilities[CODED_FRAMES].value ==
				CODED_FRAMES_INTERLACE)
			work_route = MSM_VIDC_PIPE_1;
	} else if (is_encode_session(inst)) {
		u32 slice_mode;

		slice_mode = inst->capabilities[SLICE_MODE].value;

		/* TODO Pipe=1 for legacy CBR*/
		if (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES)
			work_route = MSM_VIDC_PIPE_1;

	} else {
		i_vpr_e(inst, "%s: invalid session type\n", __func__);
		return -EINVAL;
	}

exit:
	i_vpr_h(inst, "Configuring work route = %u", work_route);
	msm_vidc_update_cap_value(inst, PIPE, work_route, __func__);

	return 0;
}

int msm_vidc_decide_quality_mode_iris5p(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	u32 mbpf, mbps, max_hq_mbpf, max_hq_mbps;
	u32 mode = MSM_VIDC_POWER_SAVE_MODE;

	if (!is_encode_session(inst))
		return 0;

	/* image or lossless or all intra runs at quality mode */
	if (is_image_session(inst) || inst->capabilities[LOSSLESS].value ||
		inst->capabilities[ALL_INTRA].value) {
		mode = MSM_VIDC_MAX_QUALITY_MODE;
		goto decision_done;
	}

	/* for lesser complexity, make LP for all resolution */
	if (inst->capabilities[COMPLEXITY].value < DEFAULT_COMPLEXITY) {
		mode = MSM_VIDC_POWER_SAVE_MODE;
		goto decision_done;
	}

	mbpf = msm_vidc_get_mbs_per_frame(inst);
	mbps = mbpf * msm_vidc_get_fps(inst);
	core = inst->core;
	max_hq_mbpf = core->capabilities[MAX_MBPF_HQ].value;
	max_hq_mbps = core->capabilities[MAX_MBPS_HQ].value;

	if (!is_realtime_session(inst)) {
		if (((inst->capabilities[COMPLEXITY].flags & CAP_FLAG_CLIENT_SET) &&
			(inst->capabilities[COMPLEXITY].value >= DEFAULT_COMPLEXITY)) ||
			mbpf <= max_hq_mbpf) {
			mode = MSM_VIDC_MAX_QUALITY_MODE;
			goto decision_done;
		}
	}

	if (mbpf <= max_hq_mbpf && mbps <= max_hq_mbps)
		mode = MSM_VIDC_MAX_QUALITY_MODE;

decision_done:
	msm_vidc_update_cap_value(inst, QUALITY_MODE, mode, __func__);

	return 0;
}

int msm_vidc_update_scaling_iris5p(struct msm_vidc_inst *inst,
		u32 aspect_ratio_w, u32 aspect_ratio_h)
{
	u32 wxh_contraint = 32;
	u32 input_width, input_height;
	u32 factor, factor_w, factor_h;

	input_width = inst->fmts[INPUT_PORT].fmt.pix_mp.width;
	input_height = inst->fmts[INPUT_PORT].fmt.pix_mp.height;

	/* adjust compose width and height based on video hardware requirements */
	factor_w = inst->compose.width / (aspect_ratio_w * wxh_contraint);

	if ((factor_w * (aspect_ratio_w * wxh_contraint)) < inst->compose.width)
		factor_w++;
	factor_h = inst->compose.height / (aspect_ratio_h * wxh_contraint);
	if ((factor_h * (aspect_ratio_h * wxh_contraint)) < inst->compose.height)
		factor_h++;
	factor = (factor_w < factor_h) ? factor_w : factor_h;

	inst->compose.top = 0;
	inst->compose.left = 0;
	inst->compose.width = factor * aspect_ratio_w * wxh_contraint;
	inst->compose.height = factor * aspect_ratio_h * wxh_contraint;

	/* disable downscaling if updated compose >= input width/height */
	if (inst->compose.width >= input_width ||
	    inst->compose.height >= input_height) {
		i_vpr_h(inst, "%s: compose wxh %ux%u >= input wxh %ux%u\n",
			__func__, inst->compose.width, inst->compose.height,
			input_width, input_height);
		return -EINVAL;
	}

	/* disable downscaling if updated compose is beyond 1/8 of input */
	if (inst->compose.width < input_width / 8 ||
	    inst->compose.height < input_height / 8) {
		i_vpr_h(inst, "%s: compose wxh %ux%u < 1/8 of input wxh %ux%u\n",
			__func__, inst->compose.width, inst->compose.height,
			input_width, input_height);
		return -EINVAL;
	}

	return 0;
}

int msm_vidc_decide_scaling_iris5p(struct msm_vidc_inst *inst)
{
	u32 aspect_ratio_w = 0, aspect_ratio_h = 0;
	u32 input_width = 0, input_height = 0;

	/* check if scaling requested */
	if (!inst->capabilities[SCALE_ENABLE].value)
		return 0;

	/* decide downscaing after reconfig event */
	if (!inst->fw_min_count)
		return 0;

	/* disable downscaling if scaling is not supported */
	if (inst->capabilities[SCALE_FACTOR].max <= 1)
		goto exit;

	/* downscaling supported for AVC, HEVC, AV1 (not VP9, APV) */
	if (inst->codec != MSM_VIDC_H264 &&
	    inst->codec != MSM_VIDC_HEVC &&
	    inst->codec != MSM_VIDC_AV1)
		goto exit;

	input_width = inst->fmts[INPUT_PORT].fmt.pix_mp.width;
	input_height = inst->fmts[INPUT_PORT].fmt.pix_mp.height;

	/* downscaling not supported for odd resolution */
	if (input_width & 0x1 || input_height & 0x1) {
		i_vpr_h(inst, "%s: odd wxh %ux%u\n",
			__func__, input_width, input_height);
		goto exit;
	}

	/* downscaling not supported if crop is present */
	if (inst->crop.top || inst->crop.left ||
	    inst->crop.width != input_width ||
	    inst->crop.height != input_height) {
		i_vpr_h(inst, "%s: crop %ux%u != wxh %ux%u\n",
			__func__, inst->crop.width, inst->crop.height,
			input_width, input_height);
		goto exit;
	}

	/* disable downscaling if compose not less than crop */
	if (inst->compose.width >= inst->crop.width ||
	    inst->compose.height >= inst->crop.height) {
		i_vpr_h(inst, "%s: compose %ux%u >= crop %ux%u\n",
			__func__, inst->compose.width, inst->compose.height,
			inst->crop.width, inst->crop.height);
		goto exit;
	}

	/*
	 * downscaling not supported in below cases
	 * low latency mode
	 * film grain enabled
	 * one pipe case
	 * Linear OPB colorformat
	 */
	if (is_lowlatency_session(inst) ||
	    inst->capabilities[FILM_GRAIN].value ||
	    inst->capabilities[PIPE].value == MSM_VIDC_PIPE_1 ||
	    is_linear_colorformat(inst->capabilities[PIX_FMTS].value)) {
		i_vpr_h(inst, "%s: latency %u, linear %u, pipe %lld, film_grain %lld\n",
			__func__, is_lowlatency_session(inst),
			is_linear_colorformat(inst->capabilities[PIX_FMTS].value),
			inst->capabilities[PIPE].value,
			inst->capabilities[FILM_GRAIN].value);
		goto exit;
	}

	/*
	 * downscaling supported for input resolutions
	 * 7680x4320, 4320x7680, 8192x4320 or 4320x8192 only
	 */
	if (input_width == 7680 && input_height == 4320) {
		if (inst->compose.width > inst->compose.height) {
			aspect_ratio_w = 16;
			aspect_ratio_h = 9;
		}
	} else if (input_width == 4320 && input_height == 7680) {
		if (inst->compose.width < inst->compose.height) {
			aspect_ratio_w = 9;
			aspect_ratio_h = 16;
		}
	} else if (input_width == 8192 && input_height == 4320) {
		if (inst->compose.width > inst->compose.height) {
			aspect_ratio_w = 19;
			aspect_ratio_h = 10;
		}
	} else if (input_width == 4320 && input_height == 8192) {
		if (inst->compose.width < inst->compose.height) {
			aspect_ratio_w = 10;
			aspect_ratio_h = 19;
		}
	}
	if (!aspect_ratio_w || !aspect_ratio_h) {
		i_vpr_h(inst, "%s: aspect ratio %ux%u\n",
			__func__, aspect_ratio_w, aspect_ratio_h);
		goto exit;
	}

	if (msm_vidc_update_scaling_iris5p(inst, aspect_ratio_w, aspect_ratio_h))
		goto exit;

	i_vpr_h(inst,
		"%s: scaling enabled, input wxh: %dx%d, compose wxh: %dx%d\n",
		__func__, input_width, input_height,
		inst->compose.width, inst->compose.height);

	return 0;

exit:
	inst->compose.top = 0;
	inst->compose.left = 0;
	inst->compose.width = inst->crop.width;
	inst->compose.height = inst->crop.height;
	msm_vidc_update_cap_value(inst, SCALE_ENABLE, 0, __func__);
	i_vpr_h(inst,
		"%s: scaling disabled, input wxh: %dx%d, compose wxh: %dx%d\n",
		__func__, input_width, input_height,
		inst->compose.width, inst->compose.height);

	return 0;
}

int msm_vidc_get_hier_layer_val_iris5p(struct msm_vidc_inst *inst)
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

int msm_vidc_init_codec_iris5p(struct msm_vidc_inst *inst,
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

int msm_vidc_init_codec_input_freq_iris5p(struct msm_vidc_inst *inst, u32 data_size,
		struct api_calculation_input *codec_input)
{
	enum msm_vidc_port_type port;
	u32 color_fmt, tile_rows_columns = 0;
	int rc = 0;
	u32 max_rate, frame_rate;
	struct msm_vidc_core *core;

	codec_input->chipset_gen = MSM_CANOE;

	rc = msm_vidc_init_codec_iris5p(inst, codec_input);
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
		msm_vidc_get_hier_layer_val_iris5p(inst);

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

int msm_vidc_init_codec_input_bus_iris5p(struct msm_vidc_inst *inst,
	struct vidc_bus_vote_data *d, struct api_calculation_input *codec_input)
{
	u32 complexity_factor_int = 0, complexity_factor_frac = 0, tile_rows_columns = 0;
	bool opb_compression_enabled = false;
	int rc = 0;

	if (!d)
		return -EINVAL;

	codec_input->chipset_gen = MSM_CANOE;

	rc = msm_vidc_init_codec_iris5p(inst, codec_input);
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
		msm_vidc_get_hier_layer_val_iris5p(inst);

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
