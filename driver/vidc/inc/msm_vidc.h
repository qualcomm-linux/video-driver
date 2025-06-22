/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_H_
#define _MSM_VIDC_H_

union msm_v4l2_cmd {
	struct v4l2_decoder_cmd dec;
	struct v4l2_encoder_cmd enc;
};

void *msm_vidc_open(struct msm_vidc_core *core, u32 session_type);
int msm_vidc_close(struct msm_vidc_inst *inst);
int msm_vidc_querycap(struct msm_vidc_inst *inst, void *cap);
int msm_vidc_enum_fmt(struct msm_vidc_inst *inst, void *f);
int msm_vidc_try_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f);
int msm_vidc_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f);
int msm_vidc_g_fmt(struct msm_vidc_inst *inst, void *f);
int msm_vidc_s_selection(struct msm_vidc_inst *inst, void *s);
int msm_vidc_g_selection(struct msm_vidc_inst *inst, void *s);
int msm_vidc_s_param(struct msm_vidc_inst *inst, void *sp);
int msm_vidc_g_param(struct msm_vidc_inst *inst, void *sp);
int msm_vidc_reqbufs(struct msm_vidc_inst *inst, void *b);
int msm_vidc_querybuf(struct msm_vidc_inst *inst, void *b);
int msm_vidc_create_bufs(struct msm_vidc_inst *inst, void *b);
int msm_vidc_prepare_buf(struct msm_vidc_inst *inst, void *b);
int msm_vidc_exportbuf(struct msm_vidc_inst *inst, void *b);
int msm_vidc_release_buffer(struct msm_vidc_inst *inst, int buffer_type,
			    unsigned int buffer_index);
int msm_vidc_qbuf(struct msm_vidc_inst *inst, void *b);
int msm_vidc_dqbuf(struct msm_vidc_inst *inst, void *b);
int msm_vidc_streamon(struct msm_vidc_inst *inst, void *data);
int msm_vidc_query_ctrl(struct msm_vidc_inst *inst, void *ctrl);
int msm_vidc_query_menu(struct msm_vidc_inst *inst, void *qmenu);
int msm_vidc_streamoff(struct msm_vidc_inst *inst, void *data);
int msm_vidc_try_cmd(struct msm_vidc_inst *inst, void *cmd);
int msm_vidc_start_cmd(struct msm_vidc_inst *inst);
int msm_vidc_stop_cmd(struct msm_vidc_inst *inst);
int msm_vidc_poll(struct msm_vidc_inst *inst, struct file *filp,
		  struct poll_table_struct *pt);
int msm_vidc_mmap(struct msm_vidc_inst *inst, struct file *filp, struct vm_area_struct *vma);
int msm_vidc_subscribe_event(struct msm_vidc_inst *inst,
			     void *sub);
int msm_vidc_unsubscribe_event(struct msm_vidc_inst *inst,
			       void *sub);
int msm_vidc_dqevent(struct msm_vidc_inst *inst, struct v4l2_event *event);
int msm_vidc_g_crop(struct msm_vidc_inst *inst, struct v4l2_crop *a);
int msm_vidc_enum_framesizes(struct msm_vidc_inst *inst, void *fsize);
int msm_vidc_enum_frameintervals(struct msm_vidc_inst *inst, void *fival);
int msm_vidc_session(struct msm_vidc_inst *inst,
	int (*function_op)(struct msm_vidc_inst *inst, void *arg), void *data,
	bool skip_error_check, const char *func);

#endif
