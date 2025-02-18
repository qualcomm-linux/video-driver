// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "msm_vidc_vb2.h"
#include "msm_vidc_core.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_power.h"
#include "msm_vidc_debug.h"
#include "msm_vdec.h"
#include "msm_venc.h"
#include "msm_vidc_control.h"
#include "msm_vidc_platform.h"

extern struct msm_vidc_core *g_core;

static void msm_vb2_vm_open(struct vm_area_struct *vma);
static void msm_vb2_vm_close(struct vm_area_struct *vma);

struct vb2_queue *msm_vidc_get_vb2q(struct msm_vidc_inst *inst,
	u32 type, const char *func)
{
	struct vb2_queue *q = NULL;

	if (type == INPUT_MPLANE) {
		q = inst->bufq[INPUT_PORT].vb2q;
	} else if (type == OUTPUT_MPLANE) {
		q = inst->bufq[OUTPUT_PORT].vb2q;
	} else if (type == INPUT_META_PLANE) {
		q = inst->bufq[INPUT_META_PORT].vb2q;
	} else if (type == OUTPUT_META_PLANE) {
		q = inst->bufq[OUTPUT_META_PORT].vb2q;
	} else {
		i_vpr_e(inst, "%s: invalid buffer type %d\n",
			__func__, type);
	}
	return q;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
void *msm_vb2_alloc(struct device *dev, unsigned long attrs,
	unsigned long size, enum dma_data_direction dma_dir,
	gfp_t gfp_flags)
{
	return (void *)0xdeadbeef;
}

void *msm_vb2_attach_dmabuf(struct device *dev, struct dma_buf *dbuf,
	unsigned long size, enum dma_data_direction dma_dir)
{
	return (void *)0xdeadbeef;
}

#else
void *msm_vb2_alloc(struct vb2_buffer *vb, struct device *dev,
		    unsigned long size)
{
	struct context_bank_info *cb = NULL;
	enum msm_vidc_buffer_region region;
	enum msm_vidc_buffer_type buf_type;
	struct msm_vidc_buffer *buf;
	struct msm_vidc_inst *inst;
	struct msm_vidc_core *core;

	if (!vb || !dev || !vb->vb2_queue)
		return ERR_PTR(-EINVAL);

	inst = vb->vb2_queue->drv_priv;
	inst = get_inst_ref(g_core, inst);
	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return ERR_PTR(-EINVAL);
	}

	core = inst->core;

	buf_type = v4l2_type_to_driver(vb->type, __func__);

	buf = msm_vidc_fetch_buffer(inst, vb);
	if (!buf) {
		i_vpr_e(inst, "%s: failed to fetch buffer\n", __func__);
		goto error;
	}

	region = call_mem_op(core, buffer_region, inst, buf_type);
	cb = msm_vidc_get_context_bank_for_region(inst->core, region);
	if (!cb) {
		i_vpr_e(inst, "%s: failed to get context bank device\n", __func__);
		goto error;
	}

	buf->inst = inst;
	buf->type = buf_type;
	buf->index = vb->index;
	buf->buffer_size = size;
	buf->dma_attrs = vb->vb2_queue->dma_attrs;

	buf->kvaddr = dma_alloc_attrs(cb->dev,
				      buf->buffer_size,
				      &buf->device_addr,
				      GFP_KERNEL | vb->vb2_queue->gfp_flags,
				      buf->dma_attrs);
	if (!buf->kvaddr) {
		i_vpr_e(inst, "dma alloc of size %lu failed\n", size);
		goto error;
	}

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = msm_vb2_put;
	buf->handler.arg = buf;

	refcount_set(&buf->refcount, 1);

	return buf;

error:
	msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	put_inst(inst);
	return ERR_PTR(-ENOMEM);
}

void *msm_vb2_attach_dmabuf(struct vb2_buffer *vb, struct device *dev,
	struct dma_buf *dbuf, unsigned long size)
{
	struct msm_vidc_inst *inst;
	struct msm_vidc_core *core;
	struct msm_vidc_buffer *buf = NULL;
	struct msm_vidc_buffer *ro_buf, *dummy;

	if (!vb || !dev || !dbuf || !vb->vb2_queue) {
		d_vpr_e("%s: invalid params\n", __func__);
		return NULL;
	}
	inst = vb->vb2_queue->drv_priv;
	inst = get_inst_ref(g_core, inst);
	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return NULL;
	}
	core = inst->core;

	buf = msm_vidc_fetch_buffer(inst, vb);
	if (!buf) {
		i_vpr_e(inst, "%s: failed to fetch buffer\n", __func__);
		buf = NULL;
		goto exit;
	}
	buf->inst = inst;
	buf->dmabuf = dbuf;

	if (is_decode_session(inst) && is_output_buffer(buf->type)) {
		list_for_each_entry_safe(ro_buf, dummy, &inst->buffers.read_only.list, list) {
			if (ro_buf->dmabuf != buf->dmabuf)
				continue;
			print_vidc_buffer(VIDC_LOW, "low ", "attach: found ro buf", inst, ro_buf);
			buf->attach = ro_buf->attach;
			ro_buf->attach = NULL;
			goto exit;
		}
	}

	buf->attach = call_mem_op(core, dma_buf_attach, core, dbuf, dev);
	if (!buf->attach) {
		buf->attach = NULL;
		buf = NULL;
		goto exit;
	}
	print_vidc_buffer(VIDC_LOW, "low ", "attach", inst, buf);

exit:
	if (!buf)
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	put_inst(inst);
	return buf;
}
#endif

void msm_vb2_vm_open(struct vm_area_struct *vma)
{
	struct vb2_vmarea_handler *h = vma->vm_private_data;

	if (IS_ERR_OR_NULL(h))
		return;

	refcount_inc(h->refcount);
}

void msm_vb2_vm_close(struct vm_area_struct *vma)
{
	struct vb2_vmarea_handler *h = vma->vm_private_data;

	if (IS_ERR_OR_NULL(h))
		return;

	if (h->put && h->arg)
		h->put(h->arg);
}

static const struct vm_operations_struct msm_vb2_vm_dma_ops = {
	.open = msm_vb2_vm_open,
	.close = msm_vb2_vm_close,
};

void msm_vb2_put(void *buf_priv)
{
	struct msm_vidc_buffer *buf = buf_priv;
	enum msm_vidc_buffer_region region;
	struct context_bank_info *cb;
	struct msm_vidc_inst *inst;
	struct msm_vidc_core *core;

	if (IS_ERR_OR_NULL(buf_priv))
		return;

	inst = buf->inst;
	if (!inst || !inst->core)
		return;

	core = inst->core;

	if (refcount_read(&buf->refcount) == 0)
		return;

	region = call_mem_op(core, buffer_region, inst, buf->type);
	cb = msm_vidc_get_context_bank_for_region(inst->core, region);
	if (!cb) {
		i_vpr_e(inst, "%s: failed to get context bank device\n", __func__);
		return;
	}

	if (!refcount_dec_and_test(&buf->refcount))
		return;

	if (buf->kvaddr && buf->device_addr) {
		dma_free_attrs(cb->dev, buf->buffer_size, buf->kvaddr,
			       buf->device_addr, buf->dma_attrs);
		buf->kvaddr = NULL;
		buf->device_addr = 0x0;
	}
	put_inst(inst);
}

int msm_vb2_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct msm_vidc_buffer *buf = buf_priv;
	enum msm_vidc_buffer_region region;
	struct context_bank_info *cb;
	struct msm_vidc_inst *inst;
	struct msm_vidc_core *core;
	int ret;

	if (IS_ERR_OR_NULL(buf_priv))
		return -EINVAL;

	inst = buf->inst;
    if (!inst || !inst->core)
		return -EINVAL;

    core = inst->core;

	region = call_mem_op(core, buffer_region, inst, buf->type);
	cb = msm_vidc_get_context_bank_for_region(inst->core, region);
	if (!cb) {
		i_vpr_e(inst, "%s: failed to get context bank device\n", __func__);
		return 0;
	}

	ret = dma_mmap_attrs(cb->dev, vma, buf->kvaddr, buf->device_addr,
			     buf->buffer_size, buf->dma_attrs);
	if (ret) {
		i_vpr_e(inst, "Remapping memory failed, error: %d\n", ret);
		return ret;
	}

	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_private_data	= &buf->handler;
	vma->vm_ops		= &msm_vb2_vm_dma_ops;

	vma->vm_ops->open(vma);

	return 0;
}

struct msm_vb2_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

static int msm_vb2_dmabuf_ops_attach(struct dma_buf *dbuf,
				     struct dma_buf_attachment *dbuf_attach)
{
	struct msm_vb2_attachment *attach;
	unsigned int i;
	struct scatterlist *rd, *wr;
	struct sg_table *sgt;
	struct msm_vidc_buffer *buf = dbuf->priv;
	int ret;

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	sgt = &attach->sgt;
	/* Copy the buf->base_sgt scatter list to the attachment, as we can't
	 * map the same scatter list to multiple attachments at the same time.
	 */
	ret = sg_alloc_table(sgt, buf->sg_table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return -ENOMEM;
	}

	rd = buf->sg_table->sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; ++i) {
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	attach->dma_dir = DMA_NONE;
	dbuf_attach->priv = attach;

	return 0;
}

static void msm_vb2_dmabuf_ops_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *db_attach)
{
	struct msm_vb2_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	if (!attach)
		return;

	sgt = &attach->sgt;

	/* release the scatterlist cache */
	if (attach->dma_dir != DMA_NONE)
		/*
		 * Cache sync can be skipped here, as the vb2_dc memory is
		 * allocated from device coherent memory, which means the
		 * memory locations do not require any explicit cache
		 * maintenance prior or after being used by the device.
		 */
		dma_unmap_sgtable(db_attach->dev, sgt, attach->dma_dir,
				  DMA_ATTR_SKIP_CPU_SYNC);
	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;
}

static struct sg_table *msm_vb2_dmabuf_ops_map(
	struct dma_buf_attachment *db_attach, enum dma_data_direction dma_dir)
{
	struct msm_vb2_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	sgt = &attach->sgt;
	/* return previously mapped sg table */
	if (attach->dma_dir == dma_dir)
		return sgt;

	/* release any previous cache */
	if (attach->dma_dir != DMA_NONE) {
		dma_unmap_sgtable(db_attach->dev, sgt, attach->dma_dir,
				  DMA_ATTR_SKIP_CPU_SYNC);
		attach->dma_dir = DMA_NONE;
	}

	/*
	 * mapping to the client with new direction, no cache sync
	 * required see comment in msm_vb2_dmabuf_ops_detach()
	 */
	if (dma_map_sgtable(db_attach->dev, sgt, dma_dir,
			    DMA_ATTR_SKIP_CPU_SYNC)) {
		pr_err("failed to map scatterlist\n");
		return ERR_PTR(-EIO);
	}

	attach->dma_dir = dma_dir;

	return sgt;
}

static void msm_vb2_dmabuf_ops_unmap(struct dma_buf_attachment *db_attach,
	struct sg_table *sgt, enum dma_data_direction dma_dir)
{
	/* nothing to be done here */
}

static void msm_vb2_dmabuf_ops_release(struct dma_buf *dbuf)
{
	/* drop reference obtained in vb2_dc_get_dmabuf */
	msm_vb2_put(dbuf->priv);
}

static int
msm_vb2_dmabuf_ops_begin_cpu_access(struct dma_buf *dbuf,
				   enum dma_data_direction direction)
{
	return 0;
}

static int
msm_vb2_dmabuf_ops_end_cpu_access(struct dma_buf *dbuf,
				 enum dma_data_direction direction)
{
	return 0;
}

static int msm_vb2_dmabuf_ops_mmap(struct dma_buf *dbuf,
	struct vm_area_struct *vma)
{
	return msm_vb2_mmap(dbuf->priv, vma);
}

static const struct dma_buf_ops msm_vb2_dmabuf_ops = {
	.attach = msm_vb2_dmabuf_ops_attach,
	.detach = msm_vb2_dmabuf_ops_detach,
	.map_dma_buf = msm_vb2_dmabuf_ops_map,
	.unmap_dma_buf = msm_vb2_dmabuf_ops_unmap,
	.begin_cpu_access = msm_vb2_dmabuf_ops_begin_cpu_access,
	.end_cpu_access = msm_vb2_dmabuf_ops_end_cpu_access,
	.mmap = msm_vb2_dmabuf_ops_mmap,
	.release = msm_vb2_dmabuf_ops_release,
};

static struct sg_table *vb2_dc_get_base_sgt(struct msm_vidc_buffer *buf)
{
	struct msm_vidc_inst *inst = buf->inst;
	struct msm_vidc_core *core = inst->core;
	struct context_bank_info *cb = NULL;
	enum msm_vidc_buffer_region region;
	struct sg_table *sgt;
	int ret;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		i_vpr_e(inst, "%s: failed to alloc sg_table\n", __func__);
		return NULL;
	}

	region = call_mem_op(core, buffer_region, inst, buf->type);
	cb = msm_vidc_get_context_bank_for_region(inst->core, region);
	if (!cb) {
		i_vpr_e(inst, "%s: failed to get context bank device\n", __func__);
		return NULL;
	}

	ret = dma_get_sgtable_attrs(cb->dev, sgt, buf->kvaddr, buf->device_addr,
				    buf->buffer_size, buf->dma_attrs);
	if (ret < 0) {
		i_vpr_e(inst, "%s: failed to get scatterlist from DMA API\n", __func__);
		kfree(sgt);
		return NULL;
	}

	return sgt;
}

struct dma_buf *msm_vb2_get_dmabuf(struct vb2_buffer *vb,
				  void *buf_priv,
				  unsigned long flags)
{
	struct dma_buf_export_info exp_info = { .exp_name = KBUILD_MODNAME, \
						.owner = THIS_MODULE };
	struct msm_vidc_buffer *buf = buf_priv;
	struct msm_vidc_inst *inst = buf->inst;
	struct dma_buf *dbuf;

	exp_info.ops = &msm_vb2_dmabuf_ops;
	exp_info.size = buf->buffer_size;
	exp_info.flags = flags;
	exp_info.priv = buf;

	if (!buf->sg_table)
		buf->sg_table = vb2_dc_get_base_sgt(buf);

	if (WARN_ON(!buf->sg_table))
		return NULL;

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf)) {
		i_vpr_e(inst, "%s: failed to export dma buf\n", __func__);
		return NULL;
	}

	/* dmabuf keeps reference to vb2 buffer */
	refcount_inc(&buf->refcount);

	return dbuf;
}

void msm_vb2_detach_dmabuf(void *buf_priv)
{
	struct msm_vidc_buffer *vbuf = buf_priv;
	struct msm_vidc_buffer *ro_buf, *dummy;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *inst;

	if (!vbuf || !vbuf->inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}
	inst = vbuf->inst;
	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return;
	}
	core = inst->core;

	if (is_decode_session(inst) && is_output_buffer(vbuf->type)) {
		list_for_each_entry_safe(ro_buf, dummy, &inst->buffers.read_only.list, list) {
			if (ro_buf->dmabuf != vbuf->dmabuf)
				continue;
			print_vidc_buffer(VIDC_LOW, "low ", "detach: found ro buf", inst, ro_buf);
			ro_buf->attach = vbuf->attach;
			vbuf->attach = NULL;
			goto exit;
		}
	}

	print_vidc_buffer(VIDC_LOW, "low ", "detach", inst, vbuf);
	if (vbuf->attach && vbuf->dmabuf) {
		call_mem_op(core, dma_buf_detach, core, vbuf->dmabuf, vbuf->attach);
		vbuf->attach = NULL;
	}

exit:
	vbuf->dmabuf = NULL;
	vbuf->inst = NULL;
	return;
}

int msm_vb2_map_dmabuf(void *buf_priv)
{
	int rc = 0;
	struct msm_vidc_buffer *buf = buf_priv;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *inst;
	struct msm_vidc_buffer *ro_buf, *dummy;

	if (!buf || !buf->inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	inst = buf->inst;
	inst = get_inst_ref(g_core, inst);
	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;

	if (is_decode_session(inst) && is_output_buffer(buf->type)) {
		list_for_each_entry_safe(ro_buf, dummy, &inst->buffers.read_only.list, list) {
			if (ro_buf->dmabuf != buf->dmabuf)
				continue;
			print_vidc_buffer(VIDC_LOW, "low ", "map: found ro buf", inst, ro_buf);
			buf->sg_table = ro_buf->sg_table;
			buf->device_addr = ro_buf->device_addr;
			ro_buf->sg_table = NULL;
			goto exit;
		}
	}

	buf->sg_table = call_mem_op(core, dma_buf_map_attachment, core, buf->attach);
	if (!buf->sg_table || !buf->sg_table->sgl) {
		buf->sg_table = NULL;
		rc = -ENOMEM;
		goto exit;
	}
	buf->device_addr = sg_dma_address(buf->sg_table->sgl);
	print_vidc_buffer(VIDC_HIGH, "high", "map", inst, buf);

exit:
	if (rc)
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	put_inst(inst);
	return rc;
}

void msm_vb2_unmap_dmabuf(void *buf_priv)
{
	struct msm_vidc_buffer *vbuf = buf_priv;
	struct msm_vidc_buffer *ro_buf, *dummy;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *inst;

	if (!vbuf || !vbuf->inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}
	inst = vbuf->inst;
	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return;
	}
	core = inst->core;

	if (is_decode_session(inst) && is_output_buffer(vbuf->type)) {
		list_for_each_entry_safe(ro_buf, dummy, &inst->buffers.read_only.list, list) {
			if (ro_buf->dmabuf != vbuf->dmabuf)
				continue;
			print_vidc_buffer(VIDC_LOW, "low ", "unmap: found ro buf", inst, ro_buf);
			ro_buf->sg_table = vbuf->sg_table;
			vbuf->sg_table = NULL;
			vbuf->device_addr = 0x0;
			goto exit;
		}
	}

	print_vidc_buffer(VIDC_HIGH, "high", "unmap", inst, vbuf);
	if (vbuf->attach && vbuf->sg_table) {
		call_mem_op(core, dma_buf_unmap_attachment, core, vbuf->attach, vbuf->sg_table);
		vbuf->sg_table = NULL;
		vbuf->device_addr = 0x0;
	}

exit:
	return;
}

int msm_vb2_queue_setup(struct vb2_queue *q,
		unsigned int *num_buffers, unsigned int *num_planes,
		unsigned int sizes[], struct device *alloc_devs[])
{
	int rc = 0;
	struct msm_vidc_inst *inst;
	struct msm_vidc_core *core;
	int port;
	struct v4l2_format *f;
	enum msm_vidc_buffer_type buffer_type = 0;
	enum msm_vidc_buffer_region region = MSM_VIDC_REGION_NONE;
	struct context_bank_info *cb = NULL;
	struct msm_vidc_buffers *buffers;

	if (!q || !num_buffers || !num_planes
		|| !sizes || !q->drv_priv) {
		d_vpr_e("%s: invalid params, q = %pK, %pK, %pK\n",
			__func__, q, num_buffers, num_planes);
		return -EINVAL;
	}
	inst = q->drv_priv;
	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	core = inst->core;

	port = v4l2_type_to_driver_port(inst, q->type, __func__);
	if (port < 0)
		return -EINVAL;

	if (!is_state(inst, MSM_VIDC_STREAMING)) {
		/* prepare dependency list once per session */
		if (!inst->caps_list_prepared) {
			rc = msm_vidc_prepare_dependency_list(inst);
			if (rc)
				return rc;
			inst->caps_list_prepared = true;
		}

		/* adjust v4l2 properties for master port */
		if ((is_encode_session(inst) && port == OUTPUT_PORT) ||
			(is_decode_session(inst) && port == INPUT_PORT)) {
			rc = msm_vidc_adjust_v4l2_properties(inst);
			if (rc) {
				i_vpr_e(inst, "%s: failed to adjust properties\n", __func__);
				return rc;
			}
		}
	}

	if (*num_planes && (port == INPUT_PORT || port == OUTPUT_PORT)) {
		f = &inst->fmts[port];
		if (*num_planes != f->fmt.pix_mp.num_planes) {
			i_vpr_e(inst, "%s: requested num_planes %d not supported %d\n",
			__func__, *num_planes, f->fmt.pix_mp.num_planes);
			return -EINVAL;
		}
		if (sizes[0] < inst->fmts[port].fmt.pix_mp.plane_fmt[0].sizeimage) {
			i_vpr_e(inst, "%s: requested size %d not acceptable\n",
			__func__, sizes[0]);
			return -EINVAL;
		}
	}

	buffer_type = v4l2_type_to_driver(q->type, __func__);
	if (!buffer_type)
		return -EINVAL;

	if (!is_state(inst, MSM_VIDC_STREAMING)) {
		rc = msm_vidc_free_buffers(inst, buffer_type);
		if (rc) {
			i_vpr_e(inst, "%s: failed to free buffers, type %s\n",
				__func__, v4l2_type_name(q->type));
			return rc;
		}
	}

	buffers = msm_vidc_get_buffers(inst, buffer_type, __func__);
	if (!buffers)
		return -EINVAL;

	if (!is_state(inst, MSM_VIDC_STREAMING)) {
		buffers->min_count = call_session_op(core, min_count, inst, buffer_type);
		buffers->extra_count = call_session_op(core, extra_count, inst, buffer_type);
		if (*num_buffers < buffers->min_count + buffers->extra_count)
			*num_buffers = buffers->min_count + buffers->extra_count;
		buffers->actual_count = *num_buffers;
	} else {
		buffers->actual_count += *num_buffers;
	}
	*num_planes = 1;

	buffers->size = call_session_op(core, buffer_size, inst, buffer_type);
	if (port == INPUT_PORT || port == OUTPUT_PORT) {
		inst->fmts[port].fmt.pix_mp.plane_fmt[0].sizeimage = buffers->size;
		sizes[0] = inst->fmts[port].fmt.pix_mp.plane_fmt[0].sizeimage;
	} else if (port == OUTPUT_META_PORT) {
		inst->fmts[port].fmt.meta.buffersize = buffers->size;
		sizes[0] = inst->fmts[port].fmt.meta.buffersize;
	} else if (port == INPUT_META_PORT) {
		inst->fmts[port].fmt.meta.buffersize = buffers->size;
		if (inst->capabilities[SUPER_FRAME].value)
			sizes[0] = inst->capabilities[SUPER_FRAME].value *
				inst->fmts[port].fmt.meta.buffersize;
		else
			sizes[0] = inst->fmts[port].fmt.meta.buffersize;
	}

	rc = msm_vidc_allocate_buffers(inst, buffer_type, *num_buffers);
	if (rc) {
		i_vpr_e(inst, "%s: failed to allocate buffers, type %s\n",
			__func__, v4l2_type_name(q->type));
		return rc;
	}

	region = call_mem_op(core, buffer_region, inst, buffer_type);
	cb = msm_vidc_get_context_bank_for_region(core, region);
	if (!cb) {
		d_vpr_e("%s: Failed to get context bank device\n",
			__func__);
		return -EIO;
	}
	q->dev = cb->dev;

	i_vpr_h(inst,
		"queue_setup: type %s num_buffers %d sizes[0] %d cb %s\n",
		v4l2_type_name(q->type), *num_buffers, sizes[0], cb->name);
	return rc;
}

int msm_vb2_start_streaming(struct vb2_queue *q, unsigned int count)
{
	int rc = 0;
	struct msm_vidc_inst *inst;

	if (!q || !q->drv_priv) {
		d_vpr_e("%s: invalid input, q = %pK\n", __func__, q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = inst->event_handle(inst, MSM_VIDC_STREAMON, q);
	if (rc) {
		i_vpr_e(inst, "Streamon: %s failed\n", v4l2_type_name(q->type));
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		goto exit;
	}

exit:
	return rc;
}

int msm_vidc_start_streaming(struct msm_vidc_inst *inst, struct vb2_queue *q)
{
	enum msm_vidc_buffer_type buf_type;
	int rc = 0;

	if (q->type == INPUT_META_PLANE || q->type == OUTPUT_META_PLANE) {
		i_vpr_h(inst, "%s: nothing to start on %s\n",
			__func__, v4l2_type_name(q->type));
		return 0;
	}
	if (q->type != INPUT_MPLANE && q->type != OUTPUT_MPLANE) {
		i_vpr_e(inst, "%s: invalid type %d\n", __func__, q->type);
		return -EINVAL;
	}
	if (!is_decode_session(inst) && !is_encode_session(inst)) {
		i_vpr_e(inst, "%s: invalid session %d\n", __func__, inst->domain);
		return -EINVAL;
	}
	i_vpr_h(inst, "Streamon: %s\n", v4l2_type_name(q->type));

	if (!inst->once_per_session_set) {
		inst->once_per_session_set = true;
		rc = msm_vidc_session_set_codec(inst);
		if (rc)
			return rc;

		rc = msm_vidc_session_set_secure_mode(inst);
		if (rc)
			return rc;

		if (is_encode_session(inst)) {
			rc = msm_vidc_alloc_and_queue_session_internal_buffers(inst,
				MSM_VIDC_BUF_ARP);
			if (rc)
				return rc;
		} else if (is_decode_session(inst)) {
			rc = msm_vidc_session_set_default_header(inst);
			if (rc)
				return rc;

			rc = msm_vidc_alloc_and_queue_session_internal_buffers(inst,
				MSM_VIDC_BUF_PERSIST);
			if (rc)
				return rc;
		}
	}

	if (is_decode_session(inst))
		inst->decode_batch.enable = msm_vidc_allow_decode_batch(inst);

	msm_vidc_allow_dcvs(inst);
	msm_vidc_power_data_reset(inst);

	if (q->type == INPUT_MPLANE) {
		if (is_decode_session(inst))
			rc = msm_vdec_streamon_input(inst);
		else if (is_encode_session(inst))
			rc = msm_venc_streamon_input(inst);
	} else if (q->type == OUTPUT_MPLANE) {
		if (is_decode_session(inst))
			rc = msm_vdec_streamon_output(inst);
		else if (is_encode_session(inst))
			rc = msm_venc_streamon_output(inst);
	}
	if (rc)
		return rc;

	/* print final buffer counts & size details */
	msm_vidc_print_buffer_info(inst);

	/* print internal buffer memory usage stats */
	msm_vidc_print_memory_stats(inst);

	buf_type = v4l2_type_to_driver(q->type, __func__);
	if (!buf_type)
		return -EINVAL;

	/* queue pending buffers */
	rc = msm_vidc_queue_deferred_buffers(inst, buf_type);
	if (rc)
		return rc;

	/* initialize statistics timer(one time) */
	if (!inst->stats.time_ms)
		inst->stats.time_ms = ktime_get_ns() / 1000 / 1000;

	/* schedule to print buffer statistics */
	rc = schedule_stats_work(inst);
	if (rc)
		return rc;

	if ((q->type == INPUT_MPLANE && inst->bufq[OUTPUT_PORT].vb2q->streaming) ||
		(q->type == OUTPUT_MPLANE && inst->bufq[INPUT_PORT].vb2q->streaming)) {
		rc = msm_vidc_get_properties(inst);
		if (rc)
			return rc;
	}

	i_vpr_h(inst, "Streamon: %s successful\n", v4l2_type_name(q->type));
	return rc;
}

int msm_vidc_stop_streaming(struct msm_vidc_inst *inst, struct vb2_queue *q)
{
	int rc = 0;

	if (q->type == INPUT_META_PLANE || q->type == OUTPUT_META_PLANE) {
		i_vpr_h(inst, "%s: nothing to stop on %s\n",
			__func__, v4l2_type_name(q->type));
		return 0;
	}
	if (q->type != INPUT_MPLANE && q->type != OUTPUT_MPLANE) {
		i_vpr_e(inst, "%s: invalid type %d\n", __func__, q->type);
		return -EINVAL;
	}
	if (!is_decode_session(inst) && !is_encode_session(inst)) {
		i_vpr_e(inst, "%s: invalid session %d\n", __func__, inst->domain);
		return -EINVAL;
	}
	i_vpr_h(inst, "Streamoff: %s\n", v4l2_type_name(q->type));

	if (q->type == INPUT_MPLANE) {
		if (is_decode_session(inst))
			rc = msm_vdec_streamoff_input(inst);
		else if (is_encode_session(inst))
			rc = msm_venc_streamoff_input(inst);
	} else if (q->type == OUTPUT_MPLANE) {
		if (is_decode_session(inst))
			rc = msm_vdec_streamoff_output(inst);
		else if (is_encode_session(inst))
			rc = msm_venc_streamoff_output(inst);
	}
	if (rc)
		return rc;

	/* Input port streamoff */
	if (q->type == INPUT_MPLANE) {
		/* flush timestamps list */
		msm_vidc_flush_ts(inst);

		/* flush buffer_stats list */
		msm_vidc_flush_buffer_stats(inst);
	}

	/* print internal buffer memory usage stats */
	msm_vidc_print_memory_stats(inst);

	i_vpr_h(inst, "Streamoff: %s successful\n", v4l2_type_name(q->type));
	return rc;
}

void msm_vb2_stop_streaming(struct vb2_queue *q)
{
	struct msm_vidc_inst *inst;
	int rc = 0;

	if (!q || !q->drv_priv) {
		d_vpr_e("%s: invalid input, q = %pK\n", __func__, q);
		return;
	}
	inst = q->drv_priv;
	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}

	rc = inst->event_handle(inst, MSM_VIDC_STREAMOFF, q);
	if (rc) {
		i_vpr_e(inst, "Streamoff: %s failed\n", v4l2_type_name(q->type));
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
	}

	return;
}

void msm_vb2_buf_queue(struct vb2_buffer *vb2)
{
	int rc = 0;
	struct msm_vidc_inst *inst;
	struct msm_vidc_core *core;
	u64 timestamp_us = 0;
	u64 ktime_ns = ktime_get_ns();

	if (!vb2) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}

	inst = vb2_get_drv_priv(vb2->vb2_queue);
	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}
	core = inst->core;

	/*
	 * As part of every qbuf initalise request to true.
	 * If there are any dynamic controls associated with qbuf,
	 * they will set as part s_ctrl() from v4l2_ctrl_request_setup().
	 * Once v4l2_ctrl_request_setup() is done, reset request variable.
	 * If the buffer does not have any requests with it, then
	 * v4l2_ctrl_request_setup() will return 0.
	 */
	if (core->capabilities[SUPPORTS_REQUESTS].value) {
		/*
		 * If Request API is enabled:
		 * Call request_setup and request_complete without acquiring lock
		 * to avoid deadlock issues because request_setup or request_complete
		 * would call .s_ctrl and .g_volatile_ctrl respectively which acquire
		 * lock too.
		 */
		inst->request = true;
		rc = v4l2_ctrl_request_setup(vb2->req_obj.req,
				&inst->ctrl_handler);
		inst->request = false;
		v4l2_ctrl_request_complete(vb2->req_obj.req, &inst->ctrl_handler);
		if (rc) {
			i_vpr_e(inst, "%s: request setup failed, error %d\n",
				__func__, rc);
			goto exit;
		}
	}

	if (!vb2->planes[0].bytesused) {
		if (vb2->type == INPUT_MPLANE) {
			/* Expecting non-zero filledlen on INPUT port */
			i_vpr_e(inst,
				"%s: zero bytesused input buffer not supported\n", __func__);
			rc = -EINVAL;
			goto exit;
		}
		if ((vb2->type == OUTPUT_META_PLANE && is_any_meta_tx_out_enabled(inst)) ||
			(vb2->type == INPUT_META_PLANE && is_any_meta_tx_inp_enabled(inst))) {
			/*
			 * vb2 is not allowing client to pass data in output meta plane.
			 * adjust the bytesused as client will send buffer tag metadata
			 * in output meta plane if DPB_TAG_LIST, or OUTBUF_FENCE metadata
			 * is enabled.
			 */
			vb2->planes[0].bytesused = vb2->planes[0].length;
		}
	}

	if (is_encode_session(inst) && vb2->type == INPUT_MPLANE) {
		timestamp_us = div_u64(vb2->timestamp, 1000);
		msm_vidc_set_auto_framerate(inst, timestamp_us);
	}
	inst->last_qbuf_time_ns = ktime_ns;

	if (vb2->type == INPUT_MPLANE) {
		rc = msm_vidc_update_input_rate(inst, div_u64(ktime_ns, 1000));
		if (rc)
			goto exit;
	}

	if (is_decode_session(inst))
		rc = msm_vdec_qbuf(inst, vb2);
	else if (is_encode_session(inst))
		rc = msm_venc_qbuf(inst, vb2);
	else
		rc = -EINVAL;
	if (rc) {
		print_vb2_buffer("failed vb2-qbuf", inst, vb2);
		goto exit;
	}

exit:
	if (rc) {
		msm_vidc_change_state(inst, MSM_VIDC_ERROR, __func__);
		vb2_buffer_done(vb2, VB2_BUF_STATE_ERROR);
	}
}

int msm_vb2_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf;

	if (!vb) {
		d_vpr_e("%s: invalid vb\n", __func__);
		return -EINVAL;
	}

	vbuf = to_vb2_v4l2_buffer(vb);
	vbuf->field = V4L2_FIELD_NONE;
	return 0;
}

void msm_vb2_request_complete(struct vb2_buffer *vb)
{
	struct msm_vidc_inst *inst;

	if (!vb) {
		d_vpr_e("%s: invalid vb\n", __func__);
		return;
	}
	inst = vb2_get_drv_priv(vb->vb2_queue);
	if (!inst) {
		d_vpr_e("%s: invalid inst\n", __func__);
		return;
	}

	i_vpr_l(inst, "%s: vb type %d, index %d\n",
		__func__, vb->type, vb->index);
	v4l2_ctrl_request_complete(vb->req_obj.req, &inst->ctrl_handler);
}
