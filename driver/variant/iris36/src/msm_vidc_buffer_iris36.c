// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>

#include "msm_vidc_buffer_iris36.h"
#include "msm_vidc_buffer.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_core.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_debug.h"
#include "msm_media_info.h"
#include "msm_vidc_platform.h"
#include "hfi_property.h"
#include "hfi_buffer_iris36.h"

static u32 msm_vidc_decoder_bin_size_iris36(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core = NULL;
	u32 size = 0;
	u32 width = 0, height = 0, num_vpp_pipes = 0;
	struct v4l2_format *f = NULL;
	bool is_interlaced = false;
	u32 vpp_delay = 0;

	core = inst->core;

	num_vpp_pipes = core->capabilities[NUM_VPP_PIPE].value;
	if (inst->decode_vpp_delay.enable)
		vpp_delay = inst->decode_vpp_delay.size;
	else
		vpp_delay = DEFAULT_BSE_VPP_DELAY;
	if (inst->capabilities[CODED_FRAMES].value ==
			CODED_FRAMES_PROGRESSIVE)
		is_interlaced = false;
	else
		is_interlaced = true;
	f = &inst->fmts[INPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	if (inst->codec == MSM_VIDC_H264)
		HFI_BUFFER_BIN_H264D(size, width, height,
			is_interlaced, vpp_delay, num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)
		HFI_BUFFER_BIN_H265D(size, width, height,
			0, vpp_delay, num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_VP9)
		HFI_BUFFER_BIN_VP9D(size, width, height,
			0, num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_AV1)
		HFI_BUFFER_BIN_AV1D(size, width, height, is_interlaced,
			0, num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_MPEG2)
		size = 0;
	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_decoder_comv_size_iris36(struct msm_vidc_inst *inst)
{
	u32 size = 0;
	u32 width = 0, height = 0, num_comv = 0, vpp_delay = 0;
	struct v4l2_format *f = NULL;

	f = &inst->fmts[INPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	if (inst->codec == MSM_VIDC_AV1) {
		/*
		 * AV1 requires larger COMV buffer size to meet performance
		 * for certain use cases. Increase the COMV buffer size by
		 * increasing COMV bufcount. Use lower count for 8k to
		 * achieve performance but save memory.
		 */
		if (res_is_greater_than(width, height, 4096, 2176))
			num_comv = inst->fw_min_count ?
				inst->fw_min_count + 3 : inst->buffers.output.min_count + 3;
		else
			num_comv = inst->fw_min_count ?
				inst->fw_min_count + 7 : inst->buffers.output.min_count + 7;
	} else {
		num_comv = inst->buffers.output.min_count;
	}
	msm_vidc_update_cap_value(inst, NUM_COMV, num_comv, __func__);

	if (inst->codec == MSM_VIDC_HEIC
		&& is_thumbnail_session(inst)) {
		vpp_delay = 0;
	} else {
		if (inst->decode_vpp_delay.enable)
			vpp_delay = inst->decode_vpp_delay.size;
		else
			vpp_delay = DEFAULT_BSE_VPP_DELAY;
	}

	num_comv = max(vpp_delay + 1, num_comv);
	if (inst->codec == MSM_VIDC_H264) {
		HFI_BUFFER_COMV_H264D(size, width, height, num_comv);
	} else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC) {
		HFI_BUFFER_COMV_H265D(size, width, height, num_comv);
	} else if (inst->codec == MSM_VIDC_AV1) {
		/*
		 * When DRAP is enabled, COMV buffer is part of PERSIST buffer and
		 * should not be allocated separately.
		 * When DRAP is disabled, COMV buffer must be allocated.
		 */
		if (inst->capabilities[DRAP].value)
			size = 0;
		else
			HFI_BUFFER_COMV_AV1D(size, width, height, num_comv);
	} else if (inst->codec == MSM_VIDC_MPEG2) {
		size = 0;
	}

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_decoder_non_comv_size_iris36(struct msm_vidc_inst *inst)
{
	u32 size = 0;
	u32 width = 0, height = 0, num_vpp_pipes = 0;
	struct msm_vidc_core *core = NULL;
	struct v4l2_format *f = NULL;

	core = inst->core;

	num_vpp_pipes = core->capabilities[NUM_VPP_PIPE].value;

	f = &inst->fmts[INPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	if (inst->codec == MSM_VIDC_H264)
		HFI_BUFFER_NON_COMV_H264D(size, width, height, num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)
		HFI_BUFFER_NON_COMV_H265D(size, width, height, num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_MPEG2)
		size = 0;

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_decoder_line_size_iris36(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core = NULL;
	u32 size = 0;
	u32 width = 0, height = 0, out_min_count = 0;
	u32 num_vpp_pipes = 0, vpp_delay = 0;
	struct v4l2_format *f = NULL;
	bool is_opb = false;
	u32 color_fmt = 0;

	core = inst->core;
	num_vpp_pipes = core->capabilities[NUM_VPP_PIPE].value;

	color_fmt = v4l2_colorformat_to_driver(inst,
			inst->fmts[OUTPUT_PORT].fmt.pix_mp.pixelformat, __func__);
	if (is_linear_colorformat(color_fmt))
		is_opb = true;
	else
		is_opb = false;
	/*
	 * assume worst case, since color format is unknown at this
	 * time.
	 */
	is_opb = true;

	if (inst->decode_vpp_delay.enable)
		vpp_delay = inst->decode_vpp_delay.size;
	else
		vpp_delay = DEFAULT_BSE_VPP_DELAY;

	f = &inst->fmts[INPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;
	out_min_count = inst->buffers.output.min_count;
	out_min_count = max(vpp_delay + 1, out_min_count);
	if (inst->codec == MSM_VIDC_H264)
		HFI_BUFFER_LINE_H264D(size, width, height, is_opb,
			num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)
		HFI_BUFFER_LINE_H265D(size, width, height, is_opb,
			num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_VP9)
		HFI_BUFFER_LINE_VP9D(size, width, height, out_min_count,
			is_opb, num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_AV1)
		HFI_BUFFER_LINE_AV1D(size, width, height, is_opb,
			num_vpp_pipes);
	else if (inst->codec == MSM_VIDC_MPEG2)
		HFI_BUFFER_LINE_MP2D(size, width, height, out_min_count,
			is_opb, num_vpp_pipes);
	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_decoder_partial_data_size_iris36(struct msm_vidc_inst *inst)
{
	u32 size = 0;
	u32 width = 0, height = 0;
	struct v4l2_format *f = NULL;

	f = &inst->fmts[INPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	if (inst->codec == MSM_VIDC_AV1)
		HFI_BUFFER_IBC_AV1D(size, width, height);

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_decoder_persist_size_iris36(struct msm_vidc_inst *inst)
{
	u32 size = 0;
	u32 rpu_enabled = 0;

	if (inst->capabilities[META_DOLBY_RPU].value)
		rpu_enabled = 1;

	if (inst->codec == MSM_VIDC_H264) {
		HFI_BUFFER_PERSIST_H264D(size, rpu_enabled);
	} else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC) {
		HFI_BUFFER_PERSIST_H265D(size, rpu_enabled);
	} else if (inst->codec == MSM_VIDC_VP9) {
		HFI_BUFFER_PERSIST_VP9D(size);
	} else if (inst->codec == MSM_VIDC_AV1) {
		/*
		 * When DRAP is enabled, COMV buffer is part of PERSIST buffer and
		 * should not be allocated separately. PERSIST buffer should include
		 * COMV buffer calculated with width, height, refcount.
		 * When DRAP is disabled, COMV buffer should not be included in PERSIST
		 * buffer.
		 */
		if (inst->capabilities[DRAP].value)
			HFI_BUFFER_PERSIST_AV1D(size,
				inst->capabilities[FRAME_WIDTH].max,
				inst->capabilities[FRAME_HEIGHT].max, 16);
		else
			HFI_BUFFER_PERSIST_AV1D(size, 0, 0, 0);
	} else if (inst->codec == MSM_VIDC_MPEG2) {
		HFI_BUFFER_PERSIST_MP2D(size);
	}

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_decoder_dpb_size_iris36(struct msm_vidc_inst *inst)
{

	u32 size = 0;
	u32 color_fmt = 0;
	u32 width = 0, height = 0;
	u32 interlace = 0;
	struct v4l2_format *f = NULL;

	/*
	 * For legacy codecs (non-AV1), DPB is calculated only
	 * for linear formats. For AV1, DPB is needed for film-grain
	 * enabled bitstreams (UBWC & linear).
	 */
	color_fmt = inst->capabilities[PIX_FMTS].value;
	if (!is_linear_colorformat(color_fmt)) {
		if (inst->codec != MSM_VIDC_AV1)
			return size;

		if (inst->codec == MSM_VIDC_AV1 &&
			!inst->capabilities[FILM_GRAIN].value)
			return size;
	}

	f = &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;


	if (inst->codec == MSM_VIDC_H264 &&
		res_is_less_than_or_equal_to(width, height, 1920, 1088))
		interlace = 1;

	if (color_fmt == MSM_VIDC_FMT_NV12 ||
		color_fmt == MSM_VIDC_FMT_NV12C) {
		color_fmt = MSM_VIDC_FMT_NV12C;
		HFI_NV12_UBWC_IL_CALC_BUF_SIZE_V2(size, width, height,
			video_y_stride_bytes(color_fmt, width),
			video_y_scanlines(color_fmt, height),
			video_uv_stride_bytes(color_fmt, width),
			video_uv_scanlines(color_fmt, height),
			video_y_meta_stride(color_fmt, width),
			video_y_meta_scanlines(color_fmt, height),
			video_uv_meta_stride(color_fmt, width),
			video_uv_meta_scanlines(color_fmt, height),
			interlace);
	} else if (color_fmt == MSM_VIDC_FMT_P010 ||
		color_fmt == MSM_VIDC_FMT_TP10C) {
		color_fmt = MSM_VIDC_FMT_TP10C;
		HFI_YUV420_TP10_UBWC_CALC_BUF_SIZE(size,
			video_y_stride_bytes(color_fmt, width),
			video_y_scanlines(color_fmt, height),
			video_uv_stride_bytes(color_fmt, width),
			video_uv_scanlines(color_fmt, height),
			video_y_meta_stride(color_fmt, width),
			video_y_meta_scanlines(color_fmt, height),
			video_uv_meta_stride(color_fmt, width),
			video_uv_meta_scanlines(color_fmt, height));
	}

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static bool vidc_session_is_multicore(struct msm_vidc_inst *inst)
{
	bool is_multicore = false;

	/*
	 * multi-core scheduling can be done for following scenarios:
	 * 1, All intra encoding
	 * 2, Lossless encoding
	 * 3, Hierarchical-P encoding
	 */
	if ((is_encode_session(inst)) &&
		((inst->capabilities[LAYER_TYPE].value ==
		V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P) ||
		(inst->capabilities[ALL_INTRA].value == 1) ||
		(inst->capabilities[LOSSLESS].value == 1))) {
		is_multicore = true;
	}
	i_vpr_l(inst, "is_multicore: %d session", is_multicore);

	return is_multicore;
}


/* encoder internal buffers */
static u32 msm_vidc_encoder_bin_size_iris36(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core = NULL;
	u32 size = 0;
	u32 width = 0, height = 0, num_vpp_pipes = 0, stage = 0;
	u32 profile = 0, ring_buf_count = 0;
	struct v4l2_format *f = NULL;
	bool is_dual_core = vidc_session_is_multicore(inst);

	core = inst->core;

	num_vpp_pipes = core->capabilities[NUM_VPP_PIPE].value;
	stage = inst->capabilities[STAGE].value;
	f = &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;
	profile = inst->capabilities[PROFILE].value;
	ring_buf_count = inst->capabilities[ENC_RING_BUFFER_COUNT].value;

	if (inst->codec == MSM_VIDC_H264)
		HFI_BUFFER_BIN_H264E(size, inst->hfi_rc_type, width,
			height, stage, num_vpp_pipes, profile, ring_buf_count,
			is_dual_core);
	else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)
		HFI_BUFFER_BIN_H265E(size, inst->hfi_rc_type, width,
			height, stage, num_vpp_pipes, profile, ring_buf_count,
			is_dual_core);

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_get_recon_buf_count(struct msm_vidc_inst *inst)
{
	u32 num_buf_recon = 0;
	s32 n_bframe = 0, ltr_count = 0, hp_layers = 0, hb_layers = 0;
	bool is_hybrid_hp = false;
	u32 hfi_codec = 0;
	bool is_dual_core = vidc_session_is_multicore(inst);

	n_bframe = inst->capabilities[B_FRAME].value;
	ltr_count = inst->capabilities[LTR_COUNT].value;

	if (inst->hfi_layer_type == HFI_HIER_B) {
		hb_layers = inst->capabilities[ENH_LAYER_COUNT].value + 1;
	} else {
		hp_layers = inst->capabilities[ENH_LAYER_COUNT].value + 1;
		if (inst->hfi_layer_type == HFI_HIER_P_HYBRID_LTR)
			is_hybrid_hp = true;
	}

	if (inst->codec == MSM_VIDC_H264)
		hfi_codec = HFI_CODEC_ENCODE_AVC;
	else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)
		hfi_codec = HFI_CODEC_ENCODE_HEVC;

	HFI_IRIS3_ENC_RECON_BUF_COUNT(num_buf_recon, n_bframe, ltr_count,
			hp_layers, hb_layers, is_hybrid_hp, hfi_codec,
			is_dual_core);

	return num_buf_recon;
}

static u32 msm_vidc_encoder_comv_size_iris36(struct msm_vidc_inst *inst)
{
	u32 size = 0;
	u32 width = 0, height = 0, num_recon = 0;
	struct v4l2_format *f = NULL;

	f = &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	num_recon = msm_vidc_get_recon_buf_count(inst);
	if (inst->codec == MSM_VIDC_H264)
		HFI_BUFFER_COMV_H264E(size, width, height, num_recon);
	else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)
		HFI_BUFFER_COMV_H265E(size, width, height, num_recon);

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_encoder_non_comv_size_iris36(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core = NULL;
	u32 size = 0;
	u32 width = 0, height = 0, num_vpp_pipes = 0;
	struct v4l2_format *f = NULL;
	bool is_dual_core = vidc_session_is_multicore(inst);
	u32 profile = inst->capabilities[PROFILE].value;

	core = inst->core;

	num_vpp_pipes = core->capabilities[NUM_VPP_PIPE].value;
	f = &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	if (inst->codec == MSM_VIDC_H264)
		HFI_BUFFER_NON_COMV_H264E(size, width, height, num_vpp_pipes,
			profile, is_dual_core);
	else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)
		HFI_BUFFER_NON_COMV_H265E(size, width, height, num_vpp_pipes,
			profile, is_dual_core);

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_encoder_line_size_iris36(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core = NULL;
	u32 size = 0;
	u32 width = 0, height = 0, pixfmt = 0, num_vpp_pipes = 0;
	bool is_tenbit = false;
	struct v4l2_format *f = NULL;
	bool is_dual_core = vidc_session_is_multicore(inst);

	core = inst->core;
	num_vpp_pipes = core->capabilities[NUM_VPP_PIPE].value;
	pixfmt = inst->capabilities[PIX_FMTS].value;

	f = &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;
	is_tenbit = (pixfmt == MSM_VIDC_FMT_P010 || pixfmt == MSM_VIDC_FMT_TP10C);

	if (inst->codec == MSM_VIDC_H264)
		HFI_BUFFER_LINE_H264E(size, width, height, is_tenbit,
			num_vpp_pipes, is_dual_core);
	else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)
		HFI_BUFFER_LINE_H265E(size, width, height, is_tenbit,
			num_vpp_pipes, is_dual_core);

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_encoder_dpb_size_iris36(struct msm_vidc_inst *inst)
{
	u32 size = 0;
	u32 width = 0, height = 0, pixfmt = 0;
	struct v4l2_format *f = NULL;
	bool is_tenbit = false;

	f = &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	pixfmt = inst->capabilities[PIX_FMTS].value;
	is_tenbit = (pixfmt == MSM_VIDC_FMT_P010 || pixfmt == MSM_VIDC_FMT_TP10C);

	if (inst->codec == MSM_VIDC_H264)
		HFI_BUFFER_DPB_H264E(size, width, height);
	else if (inst->codec == MSM_VIDC_HEVC || inst->codec == MSM_VIDC_HEIC)
		HFI_BUFFER_DPB_H265E(size, width, height, is_tenbit);

	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_encoder_arp_size_iris36(struct msm_vidc_inst *inst)
{
	u32 size = 0;
	bool is_dual_core = vidc_session_is_multicore(inst);

	HFI_BUFFER_ARP_ENC(size, is_dual_core);
	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

static u32 msm_vidc_encoder_vpss_size_iris36(struct msm_vidc_inst *inst)
{
	u32 size = 0;
	bool ds_enable = false, is_tenbit = false, blur = false;
	u32 rotation_val = HFI_ROTATION_NONE;
	u32 width = 0, height = 0, driver_colorfmt = 0;
	struct v4l2_format *f = NULL;

	ds_enable = is_scaling_enabled(inst);
	msm_vidc_v4l2_to_hfi_enum(inst, ROTATION, &rotation_val);

	f = &inst->fmts[OUTPUT_PORT];
	if (is_rotation_90_or_270(inst)) {
		/*
		 * output width and height are rotated,
		 * so unrotate them to use as arguments to
		 * HFI_BUFFER_VPSS_ENC.
		 */
		width = f->fmt.pix_mp.height;
		height = f->fmt.pix_mp.width;
	} else {
		width = f->fmt.pix_mp.width;
		height = f->fmt.pix_mp.height;
	}

	f = &inst->fmts[INPUT_PORT];
	driver_colorfmt = v4l2_colorformat_to_driver(inst,
			f->fmt.pix_mp.pixelformat, __func__);
	is_tenbit = is_10bit_colorformat(driver_colorfmt);
	if (inst->capabilities[BLUR_TYPES].value != MSM_VIDC_BLUR_NONE)
		blur = true;

	HFI_BUFFER_VPSS_ENC(size, width, height, ds_enable, blur, is_tenbit);
	i_vpr_l(inst, "%s: size %d\n", __func__, size);
	return size;
}

int msm_vidc_encoder_decide_slice_max_mb_iris36(struct msm_vidc_inst *inst)
{
	struct v4l2_format *f = &inst->fmts[INPUT_PORT];
	u32 slice_val, output_height, output_width, mbpf = 0;

	if (is_decode_session(inst) || inst->capabilities[SLICE_MODE].value !=
			V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB)
		return 0;

	/**
	 * In case of slice mode SLICE_MAX_MB, adjust SLICE_MAX_MB cap
	 * w.r.to minimum and maximum possible slice MB's based on
	 * the resolution and codec.
	 */
	output_width = f->fmt.pix_mp.width;
	output_height = f->fmt.pix_mp.height;
	slice_val = inst->capabilities[SLICE_MAX_MB].value;

	if (inst->codec == MSM_VIDC_HEVC)
		mbpf = NUM_MBS_PER_FRAME_HEVC(output_height, output_width);
	else
		mbpf = NUM_MBS_PER_FRAME(output_height, output_width);


	slice_val = max(slice_val, mbpf / MAX_SLICES_PER_FRAME);

	return slice_val;
}

static u32 msm_vidc_encoder_output_size_iris36(struct msm_vidc_inst *inst)
{
	u32 frame_size = 0;
	struct v4l2_format *f = NULL;
	bool is_ten_bit = false;
	int bitrate_mode = 0, frame_rc = 0;
	u32 hfi_rc_type = HFI_RC_VBR_CFR;
	enum msm_vidc_codec_type codec = MSM_VIDC_H264;

	f = &inst->fmts[OUTPUT_PORT];
	codec = v4l2_codec_to_driver(inst, f->fmt.pix_mp.pixelformat, __func__);
	if (codec == MSM_VIDC_HEVC || codec == MSM_VIDC_HEIC)
		is_ten_bit = true;

	bitrate_mode = inst->capabilities[BITRATE_MODE].value;
	frame_rc = inst->capabilities[FRAME_RC_ENABLE].value;
	if (!frame_rc && !is_image_session(inst))
		hfi_rc_type = HFI_RC_OFF;
	else if (bitrate_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		hfi_rc_type = HFI_RC_CQ;

	HFI_BUFFER_BITSTREAM_ENC(frame_size, f->fmt.pix_mp.width,
		f->fmt.pix_mp.height, hfi_rc_type, is_ten_bit);

	frame_size = msm_vidc_enc_delivery_mode_based_output_buf_size(inst, frame_size);

	return frame_size;
}

struct msm_vidc_buf_type_handle {
	enum msm_vidc_buffer_type type;
	u32 (*handle)(struct msm_vidc_inst *inst);
};

int msm_buffer_size_iris36(struct msm_vidc_inst *inst,
		enum msm_vidc_buffer_type buffer_type)
{
	int i = 0;
	u32 size = 0, buf_type_handle_size = 0;
	const struct msm_vidc_buf_type_handle *buf_type_handle_arr = NULL;
	static const struct msm_vidc_buf_type_handle dec_buf_type_handle[] = {
		{MSM_VIDC_BUF_INPUT,           msm_vidc_decoder_input_size              },
		{MSM_VIDC_BUF_OUTPUT,          msm_vidc_decoder_output_size             },
		{MSM_VIDC_BUF_INPUT_META,      msm_vidc_decoder_input_meta_size         },
		{MSM_VIDC_BUF_OUTPUT_META,     msm_vidc_decoder_output_meta_size        },
		{MSM_VIDC_BUF_BIN,             msm_vidc_decoder_bin_size_iris36         },
		{MSM_VIDC_BUF_COMV,            msm_vidc_decoder_comv_size_iris36        },
		{MSM_VIDC_BUF_NON_COMV,        msm_vidc_decoder_non_comv_size_iris36    },
		{MSM_VIDC_BUF_LINE,            msm_vidc_decoder_line_size_iris36        },
		{MSM_VIDC_BUF_PERSIST,         msm_vidc_decoder_persist_size_iris36     },
		{MSM_VIDC_BUF_DPB,             msm_vidc_decoder_dpb_size_iris36         },
		{MSM_VIDC_BUF_PARTIAL_DATA,    msm_vidc_decoder_partial_data_size_iris36 },
	};
	static const struct msm_vidc_buf_type_handle enc_buf_type_handle[] = {
		{MSM_VIDC_BUF_INPUT,           msm_vidc_encoder_input_size              },
		{MSM_VIDC_BUF_OUTPUT,          msm_vidc_encoder_output_size_iris36      },
		{MSM_VIDC_BUF_INPUT_META,      msm_vidc_encoder_input_meta_size         },
		{MSM_VIDC_BUF_OUTPUT_META,     msm_vidc_encoder_output_meta_size        },
		{MSM_VIDC_BUF_BIN,             msm_vidc_encoder_bin_size_iris36         },
		{MSM_VIDC_BUF_COMV,            msm_vidc_encoder_comv_size_iris36        },
		{MSM_VIDC_BUF_NON_COMV,        msm_vidc_encoder_non_comv_size_iris36    },
		{MSM_VIDC_BUF_LINE,            msm_vidc_encoder_line_size_iris36        },
		{MSM_VIDC_BUF_DPB,             msm_vidc_encoder_dpb_size_iris36         },
		{MSM_VIDC_BUF_ARP,             msm_vidc_encoder_arp_size_iris36         },
		{MSM_VIDC_BUF_VPSS,            msm_vidc_encoder_vpss_size_iris36        },
	};

	if (is_decode_session(inst)) {
		buf_type_handle_size = ARRAY_SIZE(dec_buf_type_handle);
		buf_type_handle_arr = dec_buf_type_handle;
	} else if (is_encode_session(inst)) {
		buf_type_handle_size = ARRAY_SIZE(enc_buf_type_handle);
		buf_type_handle_arr = enc_buf_type_handle;
	}

	/* handle invalid session */
	if (!buf_type_handle_arr || !buf_type_handle_size) {
		i_vpr_e(inst, "%s: invalid session %d\n", __func__, inst->domain);
		return size;
	}

	/* fetch buffer size */
	for (i = 0; i < buf_type_handle_size; i++) {
		if (buf_type_handle_arr[i].type == buffer_type) {
			size = buf_type_handle_arr[i].handle(inst);
			break;
		}
	}

	/* handle unknown buffer type */
	if (i == buf_type_handle_size) {
		i_vpr_e(inst, "%s: unknown buffer type %#x\n", __func__, buffer_type);
		goto exit;
	}

	i_vpr_l(inst, "buffer_size: type: %11s,  size: %9u\n", buf_name(buffer_type), size);

exit:
	return size;
}

static int msm_vidc_input_min_count_iris36(struct msm_vidc_inst *inst)
{
	u32 input_min_count = 0;
	u32 total_hb_layer = 0;

	if (is_decode_session(inst)) {
		input_min_count = MIN_DEC_INPUT_BUFFERS;
	} else if (is_encode_session(inst)) {
		total_hb_layer = is_hierb_type_requested(inst) ?
			inst->capabilities[ENH_LAYER_COUNT].value + 1 : 0;
		if (inst->codec == MSM_VIDC_H264 &&
			!inst->capabilities[LAYER_ENABLE].value) {
			total_hb_layer = 0;
		}
		HFI_IRIS3_ENC_MIN_INPUT_BUF_COUNT(input_min_count,
			total_hb_layer);
	} else {
		i_vpr_e(inst, "%s: invalid domain %d\n", __func__, inst->domain);
		return 0;
	}

	if (is_thumbnail_session(inst) || is_image_session(inst))
		input_min_count = 1;

	return input_min_count;
}

static int msm_buffer_dpb_count(struct msm_vidc_inst *inst)
{
	int count = 0;

	/* encoder dpb buffer count */
	if (is_encode_session(inst))
		return msm_vidc_get_recon_buf_count(inst);

	/* decoder dpb buffer count */
	if (is_split_mode_enabled(inst)) {
		count = inst->fw_min_count ?
			inst->fw_min_count : inst->buffers.output.min_count;
	}

	return count;
}

static int msm_buffer_delivery_mode_based_min_count_iris36(struct msm_vidc_inst *inst,
	uint32_t count)
{
	struct v4l2_format *f = NULL;
	struct msm_vidc_core *core = NULL;
	u32 width = 0, height = 0, total_num_slices = 1;
	u32 hfi_codec = 0;
	u32 max_mbs_per_slice = 0;
	u32 slice_mode = 0;
	u32 delivery_mode = 0;
	u32 num_vpp_pipes = 0;

	slice_mode = inst->capabilities[SLICE_MODE].value;
	delivery_mode = inst->capabilities[DELIVERY_MODE].value;

	if (slice_mode != V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB ||
		(!delivery_mode))
		return count;

	f = &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	max_mbs_per_slice = inst->capabilities[SLICE_MAX_MB].value;

	if (inst->codec == MSM_VIDC_H264)
		hfi_codec = HFI_CODEC_ENCODE_AVC;
	else if (inst->codec == MSM_VIDC_HEVC)
		hfi_codec = HFI_CODEC_ENCODE_HEVC;

	core = inst->core;
	num_vpp_pipes = core->capabilities[NUM_VPP_PIPE].value;

	HFI_IRIS3_ENC_MB_BASED_MULTI_SLICE_COUNT(total_num_slices, width, height,
			hfi_codec, max_mbs_per_slice, num_vpp_pipes);

	return (total_num_slices * count);
}

int msm_buffer_min_count_iris36(struct msm_vidc_inst *inst,
		enum msm_vidc_buffer_type buffer_type)
{
	int count = 0;

	switch (buffer_type) {
	case MSM_VIDC_BUF_INPUT:
	case MSM_VIDC_BUF_INPUT_META:
		count = msm_vidc_input_min_count_iris36(inst);
		break;
	case MSM_VIDC_BUF_OUTPUT:
	case MSM_VIDC_BUF_OUTPUT_META:
		count = msm_vidc_output_min_count(inst);
		count = msm_buffer_delivery_mode_based_min_count_iris36(inst, count);
		break;
	case MSM_VIDC_BUF_BIN:
	case MSM_VIDC_BUF_COMV:
	case MSM_VIDC_BUF_NON_COMV:
	case MSM_VIDC_BUF_LINE:
	case MSM_VIDC_BUF_PERSIST:
	case MSM_VIDC_BUF_ARP:
	case MSM_VIDC_BUF_VPSS:
	case MSM_VIDC_BUF_PARTIAL_DATA:
		count = msm_vidc_internal_buffer_count(inst, buffer_type);
		break;
	case MSM_VIDC_BUF_DPB:
		count = msm_buffer_dpb_count(inst);
		break;
	default:
		break;
	}

	i_vpr_l(inst, "  min_count: type: %11s, count: %9u\n", buf_name(buffer_type), count);
	return count;
}

int msm_buffer_extra_count_iris36(struct msm_vidc_inst *inst,
		enum msm_vidc_buffer_type buffer_type)
{
	int count = 0;

	switch (buffer_type) {
	case MSM_VIDC_BUF_INPUT:
	case MSM_VIDC_BUF_INPUT_META:
		count = msm_vidc_input_extra_count(inst);
		break;
	case MSM_VIDC_BUF_OUTPUT:
	case MSM_VIDC_BUF_OUTPUT_META:
		count = msm_vidc_output_extra_count(inst);
		break;
	default:
		break;
	}

	i_vpr_l(inst, "extra_count: type: %11s, count: %9u\n", buf_name(buffer_type), count);
	return count;
}
