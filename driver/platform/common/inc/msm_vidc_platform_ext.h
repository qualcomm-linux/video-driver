/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_PLATFORM_EXT_H_
#define _MSM_VIDC_PLATFORM_EXT_H_

struct v4l2_ctrl;
enum msm_vidc_inst_capability_type;

/* Encoder Slice Delivery Mode
 * set format has a dependency on this control
 * and gets invoked when this control is updated.
 */
#define V4L2_CID_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE                         \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3C)

#define V4L2_CID_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE                         \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3D)

#define V4L2_CID_MPEG_VIDC_CRITICAL_PRIORITY                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3E)
#define V4L2_CID_MPEG_VIDC_RESERVE_DURATION                                  \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3F)

/*
 * Control to set fence fd to the driver for each I/P buf
 * set via V4L2_CID_MPEG_VIDC_INPUT_FENCE_FD
 */
#define V4L2_CID_MPEG_VIDC_INPUT_RX_FENCE_FD                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x50)

int msm_vidc_adjust_dec_frame_rate(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_dec_operating_rate(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_delivery_mode(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_set_signal_color_info(void *instance,
				   enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_adjust_csc(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_csc_custom_matrix(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_fence_info(void *instance, struct v4l2_ctrl *ctrl);

#endif
