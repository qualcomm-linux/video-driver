// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <media/videobuf2-core.h>
#include <media/v4l2_vidc_extensions.h>

#include "msm_vidc_platform_ext.h"
#include "venus_hfi.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_debug.h"
#include "hfi_command.h"

static void v4l2_fence_info_to_driver(
	struct v4l2_vidc_fence_info *v4l2_finfo,
	struct msm_vidc_fence_info *vidc_finfo)
{
	vidc_finfo->v4l2_type  = v4l2_finfo->v4l2_type;
	vidc_finfo->index      = v4l2_finfo->index;
	vidc_finfo->num_rx_fds = v4l2_finfo->num_rx_fds;
	memcpy(&vidc_finfo->fd, &v4l2_finfo->fd, sizeof(int) * vidc_finfo->num_rx_fds);
}

static void v4l2_fence_info_from_driver(
	struct v4l2_vidc_fence_info *v4l2_finfo,
	struct msm_vidc_fence_info *vidc_finfo)
{
	v4l2_finfo->num_tx_fds = vidc_finfo->num_tx_fds;
	memcpy(&v4l2_finfo->fd, &vidc_finfo->fd, sizeof(int) * v4l2_finfo->num_tx_fds);
	memcpy(&v4l2_finfo->handle, &vidc_finfo->handle, sizeof(u64) * v4l2_finfo->num_tx_fds);
}

int msm_vidc_adjust_dec_frame_rate(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 adjusted_value = 0;

	if (is_encode_session(inst)) {
		d_vpr_e("%s: adjust framerate invalid for enc\n", __func__);
		return -EINVAL;
	}

	adjusted_value = ctrl ? ctrl->val : inst->capabilities[FRAME_RATE].value;
	msm_vidc_update_cap_value(inst, FRAME_RATE, adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_dec_operating_rate(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 adjusted_value = 0;

	if (is_encode_session(inst)) {
		d_vpr_e("%s: adjust operating rate invalid for enc\n", __func__);
		return -EINVAL;
	}

	adjusted_value = ctrl ? ctrl->val : inst->capabilities[OPERATING_RATE].value;
	msm_vidc_update_cap_value(inst, OPERATING_RATE, adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_delivery_mode(void *instance, struct v4l2_ctrl *ctrl)
{
	s32 adjusted_value;
	s64 slice_mode = -1;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;

	if (is_decode_session(inst))
		return 0;

	adjusted_value = ctrl ? ctrl->val : inst->capabilities[DELIVERY_MODE].value;

	if (msm_vidc_get_parent_value(inst, DELIVERY_MODE, SLICE_MODE,
				      &slice_mode, __func__))
		return -EINVAL;

	/* Slice encode delivery mode is only supported for Max MB slice mode */
	if (slice_mode != V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB)
		adjusted_value = 0;

	msm_vidc_update_cap_value(inst, DELIVERY_MODE, adjusted_value, __func__);

	return 0;
}

int msm_vidc_set_signal_color_info(void *instance,
				   enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 color_info, matrix_coeff, transfer_char, primaries, range;
	u32 full_range = 0;
	u32 colour_description_present_flag = 0;
	u32 video_signal_type_present_flag = 0, hfi_value = 0;
	struct v4l2_format *input_fmt;
	u32 pix_fmt;
	/* Unspecified video format */
	u32 video_format = 5;

	if (!(inst->capabilities[cap_id].flags & CAP_FLAG_CLIENT_SET)) {
		i_vpr_h(inst, "%s: colorspace not configured via control\n", __func__);
		return 0;
	}

	color_info = inst->capabilities[cap_id].value;
	matrix_coeff = color_info & 0xFF;
	transfer_char = (color_info & 0xFF00) >> 8;
	primaries = (color_info & 0xFF0000) >> 16;
	range = (color_info & 0xFF000000) >> 24;

	input_fmt = &inst->fmts[INPUT_PORT];
	pix_fmt = v4l2_colorformat_to_driver(inst,
					     input_fmt->fmt.pix_mp.pixelformat, __func__);
	if (primaries != V4L2_COLORSPACE_DEFAULT ||
	    matrix_coeff != V4L2_YCBCR_ENC_DEFAULT ||
	    transfer_char != V4L2_XFER_FUNC_DEFAULT) {
		colour_description_present_flag = 1;
		video_signal_type_present_flag = 1;
		primaries = v4l2_color_primaries_to_driver(inst,
							   primaries, __func__);
		matrix_coeff = v4l2_matrix_coeff_to_driver(inst,
							   matrix_coeff, __func__);
		transfer_char = v4l2_transfer_char_to_driver(inst,
							     transfer_char, __func__);
	} else if (is_rgba_colorformat(pix_fmt)) {
		colour_description_present_flag = 1;
		video_signal_type_present_flag = 1;
		primaries = MSM_VIDC_PRIMARIES_BT709;
		matrix_coeff = MSM_VIDC_MATRIX_COEFF_BT709;
		transfer_char = MSM_VIDC_TRANSFER_BT709;
		full_range = 0;
	}

	if (range != V4L2_QUANTIZATION_DEFAULT) {
		video_signal_type_present_flag = 1;
		full_range = range == V4L2_QUANTIZATION_FULL_RANGE ? 1 : 0;
	}

	hfi_value = (matrix_coeff & 0xFF) |
		((transfer_char << 8) & 0xFF00) |
		((primaries << 16) & 0xFF0000) |
		((colour_description_present_flag << 24) & 0x1000000) |
		((full_range << 25) & 0x2000000) |
		((video_format << 26) & 0x1C000000) |
		((video_signal_type_present_flag << 29) & 0x20000000);

	rc = msm_vidc_packetize_control(inst, cap_id, HFI_PAYLOAD_32_PACKED,
					&hfi_value, sizeof(u32), __func__);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_adjust_csc(void *instance, struct v4l2_ctrl *ctrl)
{
	s32 adjusted_value;
	s64 pix_fmt = -1;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;

	if (is_decode_session(inst))
		return 0;

	adjusted_value = ctrl ? ctrl->val : inst->capabilities[CSC].value;

	if (msm_vidc_get_parent_value(inst, CSC, PIX_FMTS,
				      &pix_fmt, __func__))
		return -EINVAL;

	/* disable csc for 10-bit encoding */
	if (is_10bit_colorformat(pix_fmt))
		adjusted_value = 0;

	msm_vidc_update_cap_value(inst, CSC, adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_csc_custom_matrix(void *instance, struct v4l2_ctrl *ctrl)
{
	s32 adjusted_value;
	s64 cscVal = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;

	if (is_decode_session(inst))
		return 0;

	adjusted_value = ctrl ? ctrl->val : inst->capabilities[CSC_CUSTOM_MATRIX].value;

	if (msm_vidc_get_parent_value(inst, CSC_CUSTOM_MATRIX, CSC,
				      &cscVal, __func__))
		return -EINVAL;

	/* disable CSC_CUSTOM_MATRIX if CSC is not enabled */
	if (!cscVal)
		adjusted_value = 0;

	msm_vidc_update_cap_value(inst, CSC_CUSTOM_MATRIX, adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_fence_info(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	struct v4l2_vidc_fence_info *v4l2_finfo;
	struct msm_vidc_fence_info vidc_finfo;
	struct msm_vidc_buffer *buf;
	int rc = 0;

	if (!ctrl)
		return 0;

	v4l2_finfo = (struct v4l2_vidc_fence_info *)ctrl->p_new.p;
	if (v4l2_finfo->num_rx_fds >= ARRAY_SIZE(v4l2_finfo->fd)) {
		i_vpr_e(inst, "%s: invalid rx fd count %u, max supported %lu\n",
			__func__, v4l2_finfo->num_rx_fds, ARRAY_SIZE(v4l2_finfo->fd));
		return -EINVAL;
	}

	v4l2_fence_info_to_driver(v4l2_finfo, &vidc_finfo);
	buf = msm_vidc_fetch_buffer(inst, vidc_finfo.v4l2_type, vidc_finfo.index);
	if (!buf) {
		i_vpr_e(inst, "%s: failed to fetch buffer\n", __func__);
		return -EINVAL;
	}

	/* print rx fd details */
	if (vidc_finfo.num_rx_fds)
		i_vpr_h(inst, "%s: %s: idx %2d, num_rx %d, rx_fds %*ph\n",
			__func__, buf_name(buf->type), buf->index, vidc_finfo.num_rx_fds,
			(int)(vidc_finfo.num_rx_fds * sizeof(vidc_finfo.fd[0])),
			&vidc_finfo.fd[0]);

	/* populate fence info */
	rc = msm_vidc_populate_fence_info(inst, buf, &vidc_finfo);
	if (rc)
		return rc;

	/* print tx fd details */
	if (vidc_finfo.num_tx_fds)
		i_vpr_h(inst, "%s: %s: idx %2d, num_tx %d, tx_fds %*ph, handle %*ph\n",
			__func__, buf_name(buf->type), buf->index, vidc_finfo.num_tx_fds,
			(int)(vidc_finfo.num_tx_fds * sizeof(vidc_finfo.fd[0])),
			&vidc_finfo.fd[0],
			(int)(vidc_finfo.num_tx_fds * sizeof(vidc_finfo.handle[0])),
			&vidc_finfo.handle[0]);
	v4l2_fence_info_from_driver(v4l2_finfo, &vidc_finfo);

	return 0;
}
