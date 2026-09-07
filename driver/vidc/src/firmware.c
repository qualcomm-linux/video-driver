// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/types.h>
#include <linux/list.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/iommu.h>
#include <linux/version.h>
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
#include <linux/firmware/qcom/qcom_scm.h>
#else
#include <linux/qcom_scm.h>
#endif
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/soc/qcom/smem.h>
#include <linux/devcoredump.h>
#include <linux/firmware.h>

#include "msm_vidc_core.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_md.h"
#include "firmware.h"
#include "resources.h"

#define MAX_FIRMWARE_NAME_SIZE	128

enum tzbsp_video_state {
	TZBSP_VIDEO_STATE_SUSPEND = 0,
	TZBSP_VIDEO_STATE_RESUME = 1,
	TZBSP_VIDEO_STATE_RESTORE_THRESHOLD = 2,
};

#if defined(HAVE_QCOM_TEE_PAS)
#include <linux/firmware/qcom/qcom_pas.h>
static int fw_pas_load(struct msm_vidc_core *core,
			const struct firmware *firmware,
			const char *fw_name,
			phys_addr_t phys, size_t res_size)
{
	struct device *dev = core->fw.dev ?: &core->pdev->dev;
	int pas_id = core->platform->data.pas_id;
	struct qcom_pas_context *ctx;
	int rc = 0;

	ctx = devm_qcom_pas_context_alloc(dev, pas_id, phys, res_size);
	if (!ctx) {
		d_vpr_e("%s: failed to initialize PAS context\n", __func__);
		return -ENOMEM;
	}

	ctx->use_tzmem = core->fw.dev;
	rc = qcom_mdt_pas_load(ctx, firmware, fw_name, NULL);
	if (rc) {
		d_vpr_e("%s: error %d pas-load fw \"%s\"\n", __func__, rc, fw_name);
		goto metadata_release;
	}

	if (core->fw.iommu_domain) {
		rc = iommu_map(core->fw.iommu_domain, 0, phys, res_size,
				IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV, GFP_KERNEL);
		if (rc) {
			d_vpr_e("%s: iommu_map failed rc=%d\n", __func__, rc);
			goto metadata_release;
		}
	}

	rc = qcom_pas_prepare_and_auth_reset(ctx);
	if (rc) {
		d_vpr_e("%s: auth+reset failed rc=%d fw \"%s\"\n", __func__, rc, fw_name);
		goto err_iommu_unmap;
	}

	qcom_pas_metadata_release(ctx);
	core->fw.ctx = ctx;
	return 0;

err_iommu_unmap:
	if (core->fw.iommu_domain)
		iommu_unmap(core->fw.iommu_domain, 0, res_size);
metadata_release:
	qcom_pas_metadata_release(ctx);
	return rc;
}

static inline void fw_pas_cleanup(struct msm_vidc_core *core)
{
	if (core->fw.iommu_domain && core->fw.ctx)
		iommu_unmap(core->fw.iommu_domain, 0, core->fw.ctx->mem_size);
	core->fw.ctx = NULL;
}

#elif defined(HAVE_QCOM_MDT_LOAD)
static int fw_pas_load(struct msm_vidc_core *core,
			const struct firmware *firmware,
			const char *fw_name,
			phys_addr_t phys, size_t res_size)
{
	int pas_id = core->platform->data.pas_id;
	int rc = 0;
	void *virt = NULL;

	virt = memremap(phys, res_size, MEMREMAP_WC);
	if (!virt) {
		d_vpr_e("%s: failed to remap fw memory phys %pa[p]\n",
			__func__, &phys);
		return -ENOMEM;
	}

	rc = qcom_mdt_load(&core->pdev->dev, firmware, fw_name,
			   pas_id, virt, phys, res_size, NULL);
	if (rc) {
		d_vpr_e("%s: error %d loading fw \"%s\"\n",
			__func__, rc, fw_name);
		memunmap(virt);
		return rc;
	}

	rc = qcom_scm_pas_auth_and_reset(pas_id);
	if (rc) {
		d_vpr_e("%s: error %d authenticating fw \"%s\"\n",
			__func__, rc, fw_name);
		memunmap(virt);
		return rc;
	}

	memunmap(virt);
	return 0;
}

static inline void fw_pas_cleanup(struct msm_vidc_core *core)
{

}

#elif defined(HAVE_QCOM_MDT_PAS_LOAD_MEM)
static int fw_pas_load(struct msm_vidc_core *core,
			const struct firmware *firmware,
			const char *fw_name,
			phys_addr_t phys, size_t res_size)
{
	struct device *dev = core->fw.dev ?: &core->pdev->dev;
	int pas_id = core->platform->data.pas_id;
	struct qcom_scm_pas_context *ctx;
	void *mem_virt = NULL;
	int rc = 0;

	ctx = devm_qcom_scm_pas_context_alloc(dev, pas_id, phys, res_size);
	if (IS_ERR(ctx)) {
		d_vpr_e("%s: failed to initialize PAS context\n", __func__);
		return PTR_ERR(ctx);
	}

	ctx->use_tzmem = core->fw.dev;

	mem_virt = memremap(phys, res_size, MEMREMAP_WC);
	if (!mem_virt) {
		d_vpr_e("%s: failed to remap fw memory\n", __func__);
		return -ENOMEM;
	}

	rc = qcom_mdt_pas_load(ctx, firmware, fw_name, mem_virt, NULL);
	if (rc) {
		d_vpr_e("%s: error %d pas-load fw \"%s\"\n", __func__, rc, fw_name);
		goto unmap_virt;
	}

	if (core->fw.iommu_domain) {
		rc = iommu_map(core->fw.iommu_domain, 0, phys, res_size,
				IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV, GFP_KERNEL);
		if (rc) {
			d_vpr_e("%s: iommu_map failed rc=%d\n", __func__, rc);
			goto unmap_virt;
		}
	}

	rc = qcom_scm_pas_prepare_and_auth_reset(ctx);
	if (rc) {
		d_vpr_e("%s: auth+reset failed rc=%d fw \"%s\"\n", __func__, rc, fw_name);
		goto err_iommu_unmap;
	}

	qcom_scm_pas_metadata_release(ctx);
	memunmap(mem_virt);
	core->fw.ctx = ctx;
	return 0;

err_iommu_unmap:
	if (core->fw.iommu_domain)
		iommu_unmap(core->fw.iommu_domain, 0, res_size);
unmap_virt:
	qcom_scm_pas_metadata_release(ctx);
	memunmap(mem_virt);
	return rc;
}

static inline void fw_pas_cleanup(struct msm_vidc_core *core)
{
	if (core->fw.iommu_domain && core->fw.ctx)
		iommu_unmap(core->fw.iommu_domain, 0, core->fw.ctx->mem_size);
	core->fw.ctx = NULL;
}

#else
static int fw_pas_load(struct msm_vidc_core *core,
			const struct firmware *firmware,
			const char *fw_name,
			phys_addr_t phys, size_t res_size)
{
	struct device *dev = core->fw.dev ?: &core->pdev->dev;
	int pas_id = core->platform->data.pas_id;
	struct qcom_scm_pas_context *ctx;
	int rc = 0;

	ctx = devm_qcom_scm_pas_context_alloc(dev, pas_id, phys, res_size);
	if (IS_ERR(ctx)) {
		d_vpr_e("%s: failed to initialize PAS context\n", __func__);
		return PTR_ERR(ctx);
	}

	ctx->use_tzmem = core->fw.dev;
	rc = qcom_mdt_pas_load(ctx, firmware, fw_name, NULL);
	if (rc) {
		d_vpr_e("%s: error %d pas-load fw \"%s\"\n", __func__, rc, fw_name);
		goto metadata_release;
	}

	if (core->fw.iommu_domain) {
		rc = iommu_map(core->fw.iommu_domain, 0, phys, res_size,
				IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV, GFP_KERNEL);
		if (rc) {
			d_vpr_e("%s: iommu_map failed rc=%d\n", __func__, rc);
			goto metadata_release;
		}
	}

	rc = qcom_scm_pas_prepare_and_auth_reset(ctx);
	if (rc) {
		d_vpr_e("%s: auth+reset failed rc=%d fw \"%s\"\n", __func__, rc, fw_name);
		goto err_iommu_unmap;
	}

	qcom_scm_pas_metadata_release(ctx);
	core->fw.ctx = ctx;
	return 0;

err_iommu_unmap:
	if (core->fw.iommu_domain)
		iommu_unmap(core->fw.iommu_domain, 0, res_size);
metadata_release:
	qcom_scm_pas_metadata_release(ctx);
	return rc;
}

static inline void fw_pas_cleanup(struct msm_vidc_core *core)
{
	if (core->fw.iommu_domain && core->fw.ctx)
		iommu_unmap(core->fw.iommu_domain, 0, core->fw.ctx->mem_size);
	core->fw.ctx = NULL;
}

#endif

#if defined(HAVE_QCOM_TEE_PAS)
static inline int fw_pas_shutdown(int pas_id)
{
	return qcom_pas_shutdown(pas_id);
}

static inline int fw_pas_set_remote_state(bool resume, int pas_id)
{
	return qcom_pas_set_remote_state(resume, pas_id);
}
#else
static inline int fw_pas_shutdown(int pas_id)
{
	return qcom_scm_pas_shutdown(pas_id);
}

static inline int fw_pas_set_remote_state(bool resume, int pas_id)
{
	return qcom_scm_set_remote_state(resume, pas_id);
}
#endif

static int __load_fw_to_memory(struct platform_device *pdev,
			const char *firmware_name)
{
	int rc = 0;
	const struct firmware *firmware = NULL;
	struct msm_vidc_core *core;
	struct device_node *node = NULL;
	struct resource res = { 0 };
	phys_addr_t phys = 0;
	size_t res_size = 0;
	ssize_t fw_size = 0;
	void *virt = NULL;
	int pas_id = 0;

	if (!firmware_name || !(*firmware_name) || !pdev) {
		d_vpr_e("%s: Invalid inputs\n", __func__);
		return -EINVAL;
	}

	if (strlen(firmware_name) >= MAX_FIRMWARE_NAME_SIZE - 4) {
		d_vpr_e("%s: Invalid fw name\n", __func__);
		return -EINVAL;
	}

	core = dev_get_drvdata(&pdev->dev);
	if (!core) {
		d_vpr_e("%s: core not found in device %s",
			__func__, dev_name(&pdev->dev));
		return -EINVAL;
	}

	pas_id = core->platform->data.pas_id;

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!node) {
		d_vpr_e("%s: failed to read \"memory-region\"\n",
			__func__);
		return -EINVAL;
	}

	rc = of_address_to_resource(node, 0, &res);
	if (rc) {
		d_vpr_e("%s: failed to read \"memory-region\", error %d\n",
			__func__, rc);
		goto exit;
	}
	phys = res.start;
	res_size = (size_t)resource_size(&res);

	rc = request_firmware(&firmware, firmware_name, &pdev->dev);
	if (rc) {
		d_vpr_e("%s: failed to request fw \"%s\", error %d\n",
			__func__, firmware_name, rc);
		goto exit;
	}

	fw_size = qcom_mdt_get_size(firmware);
	if (fw_size < 0 || res_size < (size_t)fw_size) {
		rc = -EINVAL;
		d_vpr_e("%s: out of bound fw image fw size: %ld, res_size: %lu",
			__func__, fw_size, res_size);
		goto exit;
	}

	pm_stay_awake(pdev->dev.parent);
	rc = fw_pas_load(core, firmware, firmware_name, phys, res_size);
	pm_relax(pdev->dev.parent);
	if (rc)
		goto exit;

	/* Enabling FW memory-region dump during Kernel panic */
	virt = memremap(phys, res_size, MEMREMAP_WC);
	if (virt) {
		call_md_op(core, md_dump_fw_region, core, "vidc_core", virt, phys, res_size);
		memunmap(virt);
	}

	release_firmware(firmware);
	d_vpr_h("%s: firmware \"%s\" loaded successfully\n",
		__func__, firmware_name);

	return pas_id;

exit:
	if (firmware)
		release_firmware(firmware);

	return rc;
}

int fw_load(struct msm_vidc_core *core)
{
	const char *fwpath = NULL;
	int rc;

	if (!core->resource->fw_cookie) {
		fwpath = core->platform->data.fwname;

		core->resource->fw_cookie = __load_fw_to_memory(core->pdev, fwpath);
		if (core->resource->fw_cookie <= 0) {
			rc = core->resource->fw_cookie;
			d_vpr_e("%s: firmware download failed %d\n",
				__func__, core->resource->fw_cookie);
			core->resource->fw_cookie = 0;
			return rc ? rc : -ENOMEM;
		}
	}

	rc = call_venus_op(core, scm_mem_protect, core);
	if (rc) {
#if defined(HAVE_QCOM_TEE_PAS)
		/*
		 * When PAS is backed by OP-TEE, secure memory protection is
		 * owned by the TEE and this SCM call is not serviced, so its
		 * failure is expected. Only treat it as fatal when OP-TEE is
		 * not on the bus.
		 */
		if (qcom_pas_is_available()) {
			d_vpr_h("%s: scm_mem_protect failed %d; OP-TEE PAS in use, ignoring\n",
				__func__, rc);
			return 0;
		}
#endif
		d_vpr_e("%s scm_mem_protect failed\n", __func__);
		goto fail_scm_mem_protect;
	}

	return rc;

fail_scm_mem_protect:
	if (core->resource->fw_cookie)
		fw_pas_shutdown(core->resource->fw_cookie);
	fw_pas_cleanup(core);
	core->resource->fw_cookie = 0;
	return rc;
}

int fw_unload(struct msm_vidc_core *core)
{
	int ret;

	if (!core->resource->fw_cookie)
		return -EINVAL;

	d_vpr_h("%s: unloading video firmware\n", __func__);
	ret = fw_pas_shutdown(core->resource->fw_cookie);
	fw_pas_cleanup(core);
	if (ret)
		d_vpr_e("Firmware unload failed rc=%d\n", ret);

	core->resource->fw_cookie = 0;

	return ret;
}

int fw_suspend(struct msm_vidc_core *core)
{
	return fw_pas_set_remote_state(TZBSP_VIDEO_STATE_SUSPEND,
					core->platform->data.pas_id);
}

int fw_resume(struct msm_vidc_core *core)
{
	return fw_pas_set_remote_state(TZBSP_VIDEO_STATE_RESUME,
					core->platform->data.pas_id);
}

int fw_init(struct msm_vidc_core *core)
{
	static const char * const fw_node_name[] = { "video-firmware", "firmware" };
	struct platform_device_info info;
	struct platform_device *pdev;
	struct iommu_domain *dom;
	struct device_node *np;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(fw_node_name); i++) {
		np = of_get_child_by_name(core->pdev->dev.of_node, fw_node_name[i]);
		if (np)
			break;
	}

	if (!np)
		return 0;

	memset(&info, 0, sizeof(info));
	info.fwnode = &np->fwnode;
	info.parent = &core->pdev->dev;
	info.name = np->name;
	info.dma_mask = DMA_BIT_MASK(32);

	pdev = platform_device_register_full(&info);
	if (IS_ERR(pdev)) {
		of_node_put(np);
		return PTR_ERR(pdev);
	}
	pdev->dev.of_node = np;

	ret = of_dma_configure(&pdev->dev, np, true);
	if (ret) {
		d_vpr_e("of_dma_configure failed rc=%d\n", ret);
		goto err_unregister;
	}

	core->fw.dev = &pdev->dev;
	dom = iommu_get_domain_for_dev(core->fw.dev);
	if (!dom) {
		d_vpr_e("Failed to get iommu domain\n");
		ret = -EINVAL;
		goto err_unset_fw_dev;
	}

	core->fw.iommu_domain = dom;
	of_node_put(np);
	return 0;

err_unset_fw_dev:
	core->fw.dev = NULL;
err_unregister:
	platform_device_unregister(pdev);
	of_node_put(np);
	return ret;
}

void fw_deinit(struct msm_vidc_core *core)
{
	if (!core->fw.dev)
		return;

	core->fw.iommu_domain = NULL;
	platform_device_unregister(to_platform_device(core->fw.dev));
	core->fw.dev = NULL;
}

void fw_coredump(struct msm_vidc_core *core)
{
	int rc = 0;
	struct platform_device *pdev;
	struct device_node *node = NULL;
	struct resource res = {0};
	phys_addr_t mem_phys = 0;
	size_t res_size = 0;
	void *mem_va = NULL;
	char *data = NULL, *dump = NULL;
	u64 total_size;

	pdev = core->pdev;

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!node) {
		d_vpr_e("%s: DT error getting \"memory-region\" property\n",
			__func__);
		return;
	}

	rc = of_address_to_resource(node, 0, &res);
	if (rc) {
		d_vpr_e("%s: error %d while getting \"memory-region\" resource\n",
			__func__, rc);
		return;
	}

	mem_phys = res.start;
	res_size = (size_t)resource_size(&res);

	mem_va = memremap(mem_phys, res_size, MEMREMAP_WC);
	if (!mem_va) {
		d_vpr_e("%s: unable to remap firmware memory\n", __func__);
		return;
	}
	total_size = res_size + TOTAL_QSIZE + ALIGNED_SFR_SIZE;

	data = vmalloc(total_size);
	if (!data) {
		memunmap(mem_va);
		return;
	}
	dump = data;

	/* copy firmware dump */
	memcpy(data, mem_va, res_size);
	memunmap(mem_va);

	/* copy queues(cmd, msg, dbg) dump(along with headers) */
	data += res_size;
	memcpy(data, (char *)core->iface_q_table.align_virtual_addr, TOTAL_QSIZE);

	/* copy sfr dump */
	data += TOTAL_QSIZE;
	memcpy(data, (char *)core->sfr.align_virtual_addr, ALIGNED_SFR_SIZE);

	dev_coredumpv(&pdev->dev, dump, total_size, GFP_KERNEL);
}
