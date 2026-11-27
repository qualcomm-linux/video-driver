// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/sync_file.h>
#include <linux/file.h>
#include <linux/vmalloc.h>

#include "msm_vidc_fence.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_debug.h"

static const char *msm_vidc_dma_fence_get_driver_name(struct dma_fence *df)
{
	return "msm_vidc_fence";
}

static const char *msm_vidc_dma_fence_get_timeline_name(struct dma_fence *df)
{
	struct msm_vidc_fence *fence = container_of(df, struct msm_vidc_fence, dma_fence);

	return fence->name;
}

static void msm_vidc_dma_fence_release(struct dma_fence *df)
{
	struct msm_vidc_fence *fence = container_of(df, struct msm_vidc_fence, dma_fence);

	d_vpr_l("%s: name %s\n", __func__, fence->name);
	vfree(fence);
}

static const struct dma_fence_ops msm_vidc_dma_fence_ops = {
	.get_driver_name = msm_vidc_dma_fence_get_driver_name,
	.get_timeline_name = msm_vidc_dma_fence_get_timeline_name,
	.release = msm_vidc_dma_fence_release,
};

void populate_fence_name(struct msm_vidc_inst *inst,
	struct msm_vidc_fence *f, bool override_tl)
{
	snprintf(f->name, sizeof(f->name),
		"%s%sfence: %s: %s: fd %3d id %10llu mid %5llu f.no %5llu",
		override_tl ? (char *)inst->debug_str : "",
		override_tl ? ": " : "",
		f->f_context->name,
		f->session ? "hw" : "sw",
		f->fd,
		f->fence_id,
		f->fence_id & 0x7fffffff,
		f->seqno);
}

int msm_vidc_fence_init(struct msm_vidc_inst *inst)
{
	struct msm_vidc_fence_context *fcontext;
	int rc = 0;

	fcontext = &inst->input_rx_f_context;
	fcontext->ctx_num = dma_fence_context_alloc(1);
	fcontext->seq_num = 0;
	fcontext->fences_per_buffer_counter = 0;
	fcontext->prev_seqno = 0;
	snprintf(fcontext->name, sizeof(fcontext->name), "input: rx ");
	INIT_LIST_HEAD(&fcontext->fence_list);

	fcontext = &inst->input_tx_f_context;
	fcontext->ctx_num = dma_fence_context_alloc(1);
	fcontext->seq_num = 0;
	fcontext->fences_per_buffer_counter = 0;
	fcontext->prev_seqno = 0;
	snprintf(fcontext->name, sizeof(fcontext->name), "input: tx ");
	INIT_LIST_HEAD(&fcontext->fence_list);

	fcontext = &inst->output_rx_f_context;
	fcontext->ctx_num = dma_fence_context_alloc(1);
	fcontext->seq_num = 0;
	fcontext->fences_per_buffer_counter = 0;
	fcontext->prev_seqno = 0;
	snprintf(fcontext->name, sizeof(fcontext->name), "output: rx ");
	INIT_LIST_HEAD(&fcontext->fence_list);

	fcontext = &inst->output_tx_f_context;
	fcontext->ctx_num = dma_fence_context_alloc(1);
	fcontext->seq_num = 0;
	fcontext->fences_per_buffer_counter = 0;
	fcontext->prev_seqno = 0;
	snprintf(fcontext->name, sizeof(fcontext->name), "output: tx ");
	INIT_LIST_HEAD(&fcontext->fence_list);

	i_vpr_h(inst, "%s: successful\n", __func__);

	return rc;
}

void msm_vidc_fence_deinit(struct msm_vidc_inst *inst)
{
	struct msm_vidc_fence_context *fcontext;

	fcontext = &inst->input_rx_f_context;
	fcontext->ctx_num = 0;
	fcontext->seq_num = 0;
	fcontext->fences_per_buffer_counter = 0;
	fcontext->prev_seqno = 0;
	snprintf(fcontext->name, sizeof(fcontext->name), "%s", "");

	fcontext = &inst->input_tx_f_context;
	fcontext->ctx_num = 0;
	fcontext->seq_num = 0;
	fcontext->fences_per_buffer_counter = 0;
	fcontext->prev_seqno = 0;
	snprintf(fcontext->name, sizeof(fcontext->name), "%s", "");

	fcontext = &inst->output_rx_f_context;
	fcontext->ctx_num = 0;
	fcontext->seq_num = 0;
	fcontext->fences_per_buffer_counter = 0;
	fcontext->prev_seqno = 0;
	snprintf(fcontext->name, sizeof(fcontext->name), "%s", "");

	fcontext = &inst->output_tx_f_context;
	fcontext->ctx_num = 0;
	fcontext->seq_num = 0;
	fcontext->fences_per_buffer_counter = 0;
	fcontext->prev_seqno = 0;
	snprintf(fcontext->name, sizeof(fcontext->name), "%s", "");

	i_vpr_h(inst, "%s: successful\n", __func__);
}

int msm_vidc_put_sw_fence(struct msm_vidc_inst *inst,
	struct msm_vidc_fence *fence)
{
	/* remove fence entry from list */
	list_del_init(&fence->list);

	if (fence->imp_fence) {
		dma_fence_put(fence->imp_fence);
		vfree(fence);
	} else {
		/* override fence name with timeline for raw fence */
		populate_fence_name(inst, fence, true);
		dma_fence_put(&fence->dma_fence);
	}

	return 0;
}

struct msm_vidc_fence *msm_vidc_get_sw_fence(
	struct msm_vidc_inst *inst, struct msm_vidc_fence_context *f_context,
	enum msm_vidc_fence_type f_type, enum msm_vidc_fence_direction f_dir, int fence_fd)
{
	struct list_head *fence_list = &f_context->fence_list;
	struct msm_vidc_fence *fence = NULL;
	struct dma_fence *d_fence = NULL;
	u64 fence_seqno = 0, fence_id = 0;

	fence = vzalloc(sizeof(struct msm_vidc_fence));
	if (!fence) {
		i_vpr_e(inst, "%s: allocation failed\n", __func__);
		return NULL;
	}
	fence_seqno = ++f_context->seq_num;
	/* reset seqno to avoid going beyond INT_MAX */
	if (fence_seqno >= INT_MAX)
		f_context->seq_num = 0;

	/* import */
	if (fence_fd != INVALID_FD) {
		d_fence = sync_file_get_fence(fence_fd);
		if (!d_fence) {
			i_vpr_e(inst, "%s: getting dma fence failed. fd %d\n",
				__func__, fence_fd);
			goto error;
		}
		fence_id = d_fence->seqno;
	} else {
		spin_lock_init(&fence->lock);
#if defined(MSM_VIDC_HAS_DMA_FENCE_INIT_FLAGS)
		dma_fence_init(&fence->dma_fence, &msm_vidc_dma_fence_ops,
			&fence->lock, f_context->ctx_num, fence_seqno, 0);
#else
		dma_fence_init(&fence->dma_fence, &msm_vidc_dma_fence_ops,
			&fence->lock, f_context->ctx_num, fence_seqno);
#endif
		fence_id = fence->dma_fence.seqno;
		fence_fd = INVALID_FD;
	}
	fence->fd = fence_fd;
	fence->fence_id = fence_id;
	fence->seqno = fence_seqno;
	fence->imp_fence = d_fence;
	fence->f_context = f_context;
	fence->type = f_type;
	fence->direction = f_dir;
	fence->sync_file = NULL;
	fence->session = NULL;

	/* prepare sw fence name */
	populate_fence_name(inst, fence, false);

	/* insert into fence_list */
	INIT_LIST_HEAD(&fence->list);
	list_add_tail(&fence->list, fence_list);

	return fence;

error:
	vfree(fence);
	return NULL;
}

int msm_vidc_get_sw_fence_fd(struct msm_vidc_inst *inst,
	struct msm_vidc_fence *fence)
{
	int rc = 0;

	fence->fd = get_unused_fd_flags(0);
	if (fence->fd < 0) {
		i_vpr_e(inst, "%s: getting fd (%d) failed\n", __func__,
			fence->fd);
		rc = -EINVAL;
		goto err_fd;
	}
	fence->sync_file = sync_file_create(&fence->dma_fence);
	if (!fence->sync_file) {
		i_vpr_e(inst, "%s: sync_file_create failed\n", __func__);
		rc = -EINVAL;
		goto err_sync_file;
	}
	/* prepare fence name with fd */
	populate_fence_name(inst, fence, false);

	return 0;

err_sync_file:
	put_unused_fd(fence->fd);
	fence->fd = INVALID_FD;
err_fd:
	return rc;
}

int msm_vidc_put_sw_fence_fd(struct msm_vidc_inst *inst,
	struct msm_vidc_fence *fence)
{
	int rc = 0;

	/* destroy sync_file */
	if (fence->sync_file)
		fput(fence->sync_file->file);

	/* release unused fd */
	put_unused_fd(fence->fd);
	fence->fd = INVALID_FD;

	return rc;
}

static struct msm_vidc_fence *msm_vidc_fence_create(
	struct msm_vidc_inst *inst, struct msm_vidc_fence_context *f_context,
	enum msm_vidc_fence_type f_type, enum msm_vidc_fence_direction f_dir, int fence_fd)
{
	struct msm_vidc_fence *fence = NULL;

	/* create dma fence */
	fence = msm_vidc_get_sw_fence(inst, f_context, f_type, f_dir, fence_fd);
	if (!fence) {
		i_vpr_e(inst, "%s: failed to create sw fence\n", __func__);
		return NULL;
	}

	return fence;
}

static struct msm_vidc_fence *msm_vidc_fence_import(
	struct msm_vidc_inst *inst, struct msm_vidc_fence_context *f_context,
	enum msm_vidc_fence_type f_type, enum msm_vidc_fence_direction f_dir, int fence_fd)
{
	struct msm_vidc_fence *fence = NULL;

	/* import dma fence */
	fence = msm_vidc_get_sw_fence(inst, f_context, f_type, f_dir, fence_fd);
	if (!fence) {
		i_vpr_e(inst, "%s: failed to import sw fence\n", __func__);
		return NULL;
	}

	return fence;
}

static int msm_vidc_fence_create_fd(struct msm_vidc_inst *inst,
	struct msm_vidc_fence *fence)
{
	int rc = 0;

	rc = msm_vidc_get_sw_fence_fd(inst, fence);
	if (rc) {
		i_vpr_e(inst, "%s: failed. %s\n", __func__, fence->name);
		return rc;
	}

	return rc;
}

static int msm_vidc_fence_destroy_fd(struct msm_vidc_inst *inst,
	struct msm_vidc_fence *fence)
{
	int rc = 0;

	rc = msm_vidc_put_sw_fence_fd(inst, fence);
	if (rc) {
		i_vpr_e(inst, "%s: failed. %s\n", __func__, fence->name);
		return rc;
	}

	return rc;
}

int msm_vidc_fence_signal(struct msm_vidc_inst *inst,
	struct msm_vidc_fence *fence, bool is_error)
{
	int rc = 0;

	/* sanity - only fence owner is allowed to signal */
	if (fence->imp_fence) {
		i_vpr_e(inst, "%s: unexpected. name %s\n", __func__, fence->name);
		return -EINVAL;
	}

	/* signal sw fence */
	if (is_error)
		dma_fence_set_error(&fence->dma_fence, -EINVAL);
	dma_fence_signal(&fence->dma_fence);

	return rc;
}

static int msm_vidc_fence_destroy(struct msm_vidc_inst *inst,
	struct msm_vidc_fence *fence, bool is_error)
{
	int rc = 0;

	/* sanity - calling fence_destroy for imp fence not expected */
	if (fence->imp_fence) {
		i_vpr_e(inst, "%s: unexpected. name %s\n", __func__, fence->name);
		return -EINVAL;
	}

	/* signal sw fence before destroy */
	rc = msm_vidc_fence_signal(inst, fence, is_error);
	if (rc) {
		i_vpr_e(inst, "%s: fence signal failed. name %s\n", __func__, fence->name);
		return rc;
	}

	/* destroy sw fence */
	rc = msm_vidc_put_sw_fence(inst, fence);
	if (rc)
		return rc;

	return rc;
}

static int msm_vidc_fence_release(struct msm_vidc_inst *inst,
	struct msm_vidc_fence *fence, bool is_error)
{
	int rc = 0;

	/* sanity - calling fence_release for raw fence not expected */
	if (!fence->imp_fence) {
		i_vpr_e(inst, "%s: unexpected. name %s\n", __func__, fence->name);
		return -EINVAL;
	}

	/* destroy sw fence */
	rc = msm_vidc_put_sw_fence(inst, fence);
	if (rc)
		return rc;

	return rc;
}

static const struct msm_vidc_fence_ops msm_dma_fence_ops = {
	.fence_create             = msm_vidc_fence_create,
	.fence_import             = msm_vidc_fence_import,
	.fence_create_fd          = msm_vidc_fence_create_fd,
	.fence_destroy_fd         = msm_vidc_fence_destroy_fd,
	.fence_destroy            = msm_vidc_fence_destroy,
	.fence_release            = msm_vidc_fence_release,
};

const struct msm_vidc_fence_ops *get_dma_fence_ops(void)
{
	return &msm_dma_fence_ops;
}
