// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/delay.h>
#include <linux/reset.h>
#include <media/videobuf2-core.h>

#include "msm_vidc_iris5.h"
#include "msm_vidc_buffer_iris5.h"
#include "msm_vidc_power_iris5.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_core.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_buffer.h"
#include "msm_vidc_state.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_variant.h"
#include "venus_hfi.h"
#include "resources.h"

#define VIDEO_ARCH_LX 1

/*
 * --------------------------------------------------------------------------
 * MODULE: IRIS_APV_TOP
 * --------------------------------------------------------------------------
 */
#define WRAPPER_APV_BASE_OFFS_IRIS5                   0x00040000
#define WRAPPER_IRIS_APV_TOP_IRQ_STATUS_IRIS5        (WRAPPER_APV_BASE_OFFS_IRIS5 + 0x00)
#define WRAPPER_IRIS_APV_TOP_IRQ_MASK_IRIS5          (WRAPPER_APV_BASE_OFFS_IRIS5 + 0x04)
#define WRAPPER_IRIS_APV_TOP_IRQ_CLEAR_IRIS5         (WRAPPER_APV_BASE_OFFS_IRIS5 + 0x08)
#define WRAPPER_IRIS_APV_TOP_CLK_HALT_IRIS5          (WRAPPER_APV_BASE_OFFS_IRIS5 + 0x10)
#define WRAPPER_IRIS_APV_TOP_IDLE_STATUS_IRIS5       (WRAPPER_APV_BASE_OFFS_IRIS5 + 0x70)
#define WRAPPER_IRIS_APV_TOP_VPP_START_IRIS5         (WRAPPER_APV_BASE_OFFS_IRIS5 + 0xE0)
#define WRAPPER_IRIS_APV_TOP_VPP_CONFIG_IRIS5        (WRAPPER_APV_BASE_OFFS_IRIS5 + 0xEC)
#define WRAPPER_IRIS_APV_TOP_ENC_CONFIG_IRIS5        (WRAPPER_APV_BASE_OFFS_IRIS5 + 0xF0)

/*
 * --------------------------------------------------------------------------
 * MODULE: VCODEC_CPU_CS
 * --------------------------------------------------------------------------
 */
#define VCODEC_CPU_CS_IRIS5                            0x000A0000
#define CPU_CS_A2HSOFTINTCLR_IRIS5                     (VCODEC_CPU_CS_IRIS5 + 0x1C)
#define VCODEC_VPU_CPU_CS_VCICMDARG0_IRIS5             (VCODEC_CPU_CS_IRIS5 + 0x24)
#define VCODEC_VPU_CPU_CS_VCICMDARG1_IRIS5             (VCODEC_CPU_CS_IRIS5 + 0x28)
#define VCODEC_VPU_CPU_CS_SCIACMD_IRIS5                (VCODEC_CPU_CS_IRIS5 + 0x48)
#define VCODEC_VPU_CPU_CS_SCIACMDARG0_IRIS5            (VCODEC_CPU_CS_IRIS5 + 0x4C)
#define VCODEC_VPU_CPU_CS_SCIACMDARG1_IRIS5            (VCODEC_CPU_CS_IRIS5 + 0x50)
#define VCODEC_VPU_CPU_CS_SCIACMDARG2_IRIS5            (VCODEC_CPU_CS_IRIS5 + 0x54)
#define VCODEC_VPU_CPU_CS_SCIBCMD_IRIS5                (VCODEC_CPU_CS_IRIS5 + 0x5C)
#define VCODEC_VPU_CPU_CS_SCIBCMDARG0_IRIS5            (VCODEC_CPU_CS_IRIS5 + 0x60)
#define VCODEC_VPU_CPU_CS_SCIBARG1_IRIS5               (VCODEC_CPU_CS_IRIS5 + 0x64)
#define VCODEC_VPU_CPU_CS_SCIBARG2_IRIS5               (VCODEC_CPU_CS_IRIS5 + 0x68)
#define CPU_CS_H2XSOFTINTEN_IRIS5                      (VCODEC_CPU_CS_IRIS5 + 0x148)
#define CPU_IC_SOFTINT_IRIS5                           (VCODEC_CPU_CS_IRIS5 + 0x150)
#define CPU_CS_AHB_BRIDGE_SYNC_RESET_IRIS5             (VCODEC_CPU_CS_IRIS5 + 0x160)
#define CPU_CS_X2RPMh_IRIS5                            (VCODEC_CPU_CS_IRIS5 + 0x168)
#define VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_IRIS5  (VCODEC_CPU_CS_IRIS5 + 0x174)
#define VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_STATUS_IRIS5 (VCODEC_CPU_CS_IRIS5 + 0x178)
#define CPU_IC_SOFTINT_H2A_SHFT_IRIS5                  0x0

#define HFI_CTRL_INIT_IRIS5                            VCODEC_VPU_CPU_CS_SCIACMD_IRIS5
#define HFI_CTRL_STATUS_IRIS5                          VCODEC_VPU_CPU_CS_SCIACMDARG0_IRIS5
enum {
	HFI_CTRL_NOT_INIT                   = 0x0,
	HFI_CTRL_READY                      = 0x1,
	HFI_CTRL_ERROR_FATAL                = 0x2,
	HFI_CTRL_ERROR_UC_REGION_NOT_SET    = 0x4,
	HFI_CTRL_ERROR_HW_FENCE_QUEUE       = 0x8,
	HFI_CTRL_PC_READY                   = 0x100,
	HFI_CTRL_VCODEC_IDLE                = 0x40000000
} hfi_ctrl_status_type;

#define HFI_QTBL_INFO_IRIS5                          VCODEC_VPU_CPU_CS_SCIACMDARG1_IRIS5
enum {
	HFI_QTBL_DISABLED    = 0x00,
	HFI_QTBL_ENABLED     = 0x01,
} hfi_qtbl_status_type;

#define HFI_QTBL_ADDR_IRIS5                          VCODEC_VPU_CPU_CS_SCIACMDARG2_IRIS5
#define HFI_MMAP_ADDR_IRIS5                          VCODEC_VPU_CPU_CS_SCIBCMDARG0_IRIS5
#define HFI_UC_REGION_ADDR_IRIS5                     VCODEC_VPU_CPU_CS_SCIBARG1_IRIS5
#define HFI_UC_REGION_SIZE_IRIS5                     VCODEC_VPU_CPU_CS_SCIBARG2_IRIS5
#define HFI_DEVICE_REGION_ADDR_IRIS5                 VCODEC_VPU_CPU_CS_VCICMDARG0_IRIS5
#define HFI_DEVICE_REGION_SIZE_IRIS5                 VCODEC_VPU_CPU_CS_VCICMDARG1_IRIS5
#define HFI_SFR_ADDR_IRIS5                           VCODEC_VPU_CPU_CS_SCIBCMD_IRIS5

/*
 * --------------------------------------------------------------------------
 * MODULE: VCODEC_IRIS_WRAPPER_TOP
 * --------------------------------------------------------------------------
 */
#define WRAPPER_BASE_OFFS_IRIS5                      0x000B0000
#define WRAPPER_APV_HW_VERSION_IRIS5                 (WRAPPER_BASE_OFFS_IRIS5 + 0x04)
#define WRAPPER_EFUSE_MONITOR_IRIS5                  (WRAPPER_BASE_OFFS_IRIS5 + 0x08)
#define WRAPPER_INTR_STATUS_IRIS5                    (WRAPPER_BASE_OFFS_IRIS5 + 0x0C)
#define WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS5         0x8
#define WRAPPER_INTR_STATUS_A2H_BMSK_IRIS5           0x4

#define WRAPPER_INTR_MASK_IRIS5                      (WRAPPER_BASE_OFFS_IRIS5 + 0x10)
#define WRAPPER_INTR_MASK_A2HWD_BMSK_IRIS5           0x8
#define WRAPPER_INTR_MASK_A2HCPU_BMSK_IRIS5          0x4

#define WRAPPER_GPIO_IN_IRIS5                         (WRAPPER_BASE_OFFS_IRIS5 + 0x28)
#define WRAPPER_GPIO_OUT_IRIS5                        (WRAPPER_BASE_OFFS_IRIS5 + 0x2C)
#define WRAPPER_DEBUG_BRIDGE_LPI_CONTROL_IRIS5        (WRAPPER_BASE_OFFS_IRIS5 + 0x54)
#define WRAPPER_DEBUG_BRIDGE_LPI_STATUS_IRIS5         (WRAPPER_BASE_OFFS_IRIS5 + 0x58)
#define WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS5        (WRAPPER_BASE_OFFS_IRIS5 + 0x5C)
#define WRAPPER_IRIS_CPU_NOC_LPI_STATUS_IRIS5         (WRAPPER_BASE_OFFS_IRIS5 + 0x60)
#define WRAPPER_IRIS_VCODEC_VPU_WRAPPER_SPARE_0_IRIS5 (WRAPPER_BASE_OFFS_IRIS5 + 0x78)
#define WRAPPER_CORE_POWER_STATUS_IRIS5               (WRAPPER_BASE_OFFS_IRIS5 + 0x80)
#define WRAPPER_CORE_POWER_CONTROL_IRIS5              (WRAPPER_BASE_OFFS_IRIS5 + 0x84)
#define WRAPPER_CORE_CLOCK_CONFIG_IRIS5               (WRAPPER_BASE_OFFS_IRIS5 + 0x88)
#define WRAPPER_MVP_NOC_CX_LPI_CONTROL_IRIS5          (WRAPPER_BASE_OFFS_IRIS5 + 0x118)
#define WRAPPER_MVP_NOC_CX_LPI_STATUS_IRIS5           (WRAPPER_BASE_OFFS_IRIS5 + 0x11C)
#define WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5             (WRAPPER_BASE_OFFS_IRIS5 + 0x110)
#define WRAPPER_MVP_NOC_LPI_STATUS_IRIS5              (WRAPPER_BASE_OFFS_IRIS5 + 0x114)

#define WRAPPER_CI_VERSION_IRIS5                      (WRAPPER_BASE_OFFS_IRIS5 + 0x200)

/*
 * --------------------------------------------------------------------------
 * MODULE: TZ_WRAPPER
 * --------------------------------------------------------------------------
 */
#define WRAPPER_TSW_BASE_OFFS_IRIS5                   0x000C0000
#define WRAPPER_TSW_CPU_STATUS_IRIS5                  (WRAPPER_TSW_BASE_OFFS_IRIS5 + 0x10)
#define WRAPPER_TSW_CTL_AXI_CLOCK_CONFIG_IRIS5        (WRAPPER_TSW_BASE_OFFS_IRIS5 + 0x14)
#define WRAPPER_TSW_QNS4PDXFIFO_RESET_IRIS5           (WRAPPER_TSW_BASE_OFFS_IRIS5 + 0x18)

/*
 * --------------------------------------------------------------------------
 * MODULE: VCODEC_IRIS_CPU_NOC
 * --------------------------------------------------------------------------
 */
#define CPU_NOC_BASE_OFFS_IRIS5                        0x000D0000
#define CPU_NOC_ERRORLOGGER_MAINCTL_LOW                (CPU_NOC_BASE_OFFS_IRIS5 + 0x8)
#define CPU_NOC_SBM_FAULTINEN0_LOW                     (CPU_NOC_BASE_OFFS_IRIS5 + 0x240)

/*
 * --------------------------------------------------------------------------
 * MODULE: AON_WRAPPER
 * --------------------------------------------------------------------------
 */
#define AON_BASE_OFFS_IRIS5                             0x000E0000
#define AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5           (AON_BASE_OFFS_IRIS5)
#define AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS5            (AON_BASE_OFFS_IRIS5 + 0x4)
#define AON_WRAPPER_MVP_NOC_ARCG_CONTROL_IRIS5          (AON_BASE_OFFS_IRIS5 + 0x10)
#define AON_WRAPPER_MVP_NOC_CORE_SW_RESET_IRIS5         (AON_BASE_OFFS_IRIS5 + 0x18)
#define AON_WRAPPER_MVP_NOC_CORE_CLK_CONTROL_IRIS5      (AON_BASE_OFFS_IRIS5 + 0x20)
#define AON_WRAPPER_SPARE_IRIS5                         (AON_BASE_OFFS_IRIS5 + 0x28)
#define AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS5 (AON_BASE_OFFS_IRIS5 + 0x2C)
#define AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_STATUS_IRIS5  (AON_BASE_OFFS_IRIS5 + 0x30)

/*
 * --------------------------------------------------------------------------
 * MODULE: IRIS_AON_MVP_NOC_RESET
 * --------------------------------------------------------------------------
 */
#define AON_MVP_NOC_RESET_BASE_OFFS_IRIS5              0x0001F000
#define AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS5            (AON_MVP_NOC_RESET_BASE_OFFS_IRIS5 + 0x0)
#define AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5            (AON_MVP_NOC_RESET_BASE_OFFS_IRIS5 + 0x04)
#define AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS5        (AON_MVP_NOC_RESET_BASE_OFFS_IRIS5 + 0x08)
#define AON_WRAPPER_MVP_NOC_RESET_SPARE_IRIS5          (AON_MVP_NOC_RESET_BASE_OFFS_IRIS5 + 0x0C)

/*
 * --------------------------------------------------------------------------
 * MODULE: VCODEC_SS registers
 * --------------------------------------------------------------------------
 */
#define VCODEC_BASE_OFFS_IRIS5                       0x00000000
#define VCODEC_SS_IDLE_STATUSn_IRIS5                 (VCODEC_BASE_OFFS_IRIS5 + 0x70)

/*
 * --------------------------------------------------------------------------
 * MODULE: VCODEC_NOC
 * --------------------------------------------------------------------------
 */
#define NOC_BASE_OFFS                                      0x00010000
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_MAINCTL_LOW   (NOC_BASE_OFFS + 0xA008)
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRCLR_LOW    (NOC_BASE_OFFS + 0xA018)
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG0_LOW   (NOC_BASE_OFFS + 0xA020)
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG0_HIGH  (NOC_BASE_OFFS + 0xA024)
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG1_LOW   (NOC_BASE_OFFS + 0xA028)
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG1_HIGH  (NOC_BASE_OFFS + 0xA02C)
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG2_LOW   (NOC_BASE_OFFS + 0xA030)
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG2_HIGH  (NOC_BASE_OFFS + 0xA034)
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG3_LOW   (NOC_BASE_OFFS + 0xA038)
#define NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG3_HIGH  (NOC_BASE_OFFS + 0xA03C)
#define NOC_SIDEBANDMANAGER_MAIN_SIDEBANDMANAGER_FAULTINEN0_LOW (NOC_BASE_OFFS + 0x7040)

static int __interrupt_init_iris5(struct msm_vidc_core *core)
{
	u32 mask_val = 0;
	int rc = 0;

	/* All interrupts should be disabled initially 0x1F6 : Reset value */
	rc = __read_register(core, WRAPPER_INTR_MASK_IRIS5, &mask_val);
	if (rc)
		return rc;

	/* Write 0 to unmask CPU and WD interrupts */
	mask_val &= ~(WRAPPER_INTR_MASK_A2HWD_BMSK_IRIS5|
			WRAPPER_INTR_MASK_A2HCPU_BMSK_IRIS5);
	rc = __write_register(core, WRAPPER_INTR_MASK_IRIS5, mask_val);
	if (rc)
		return rc;

	return 0;
}

static int __raise_interrupt_iris5(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = __write_register(core, CPU_IC_SOFTINT_IRIS5, 1 << CPU_IC_SOFTINT_H2A_SHFT_IRIS5);
	if (rc)
		return rc;

	return 0;
}

static int __clear_interrupt_iris5(struct msm_vidc_core *core)
{
	u32 intr_status = 0, mask = 0;
	int rc = 0;

	rc = __read_register(core, WRAPPER_INTR_STATUS_IRIS5, &intr_status);
	if (rc)
		return rc;

	mask = (WRAPPER_INTR_STATUS_A2H_BMSK_IRIS5 |
		WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS5 |
		HFI_CTRL_VCODEC_IDLE);

	if (intr_status & mask) {
		core->intr_status |= intr_status;
		core->reg_count++;
		d_vpr_l("INTERRUPT: times: %d interrupt_status: %d\n",
			core->reg_count, intr_status);
	} else {
		core->spur_count++;
	}

	rc = __write_register(core, CPU_CS_A2HSOFTINTCLR_IRIS5, 1);
	if (rc)
		return rc;

	return 0;
}

static int __get_device_region_info_iris5(struct msm_vidc_core *core,
	u32 *min_dev_addr, u32 *dev_reg_size)
{
	struct device_region_set *dev_set;
	u32 min_addr, max_addr, count = 0;
	int rc = 0;

	dev_set = &core->resource->device_region_set;

	if (!dev_set->count) {
		d_vpr_h("%s: device region not available\n", __func__);
		return 0;
	}

	min_addr = 0xFFFFFFFF;
	max_addr = 0x0;
	for (count = 0; count < dev_set->count; count++) {
		if (dev_set->device_region_tbl[count].dev_addr > max_addr)
			max_addr = dev_set->device_region_tbl[count].dev_addr +
				dev_set->device_region_tbl[count].size;
		if (dev_set->device_region_tbl[count].dev_addr < min_addr)
			min_addr = dev_set->device_region_tbl[count].dev_addr;
	}
	if (min_addr == 0xFFFFFFFF || max_addr == 0x0) {
		d_vpr_e("%s: invalid device region\n", __func__);
		return -EINVAL;
	}

	*min_dev_addr = min_addr;
	*dev_reg_size = max_addr - min_addr;

	return rc;
}

static int __program_bootup_registers_iris5(struct msm_vidc_core *core)
{
	u32 min_dev_reg_addr = 0, dev_reg_size = 0;
	struct device *dev = NULL;
	u32 value;
	int rc = 0;

	dev = &core->pdev->dev;

	value = (u32)core->iface_q_table.align_device_addr;
	rc = __write_register(core, HFI_UC_REGION_ADDR_IRIS5, value);
	if (rc)
		return rc;

	value = SHARED_QSIZE;
	rc = __write_register(core, HFI_UC_REGION_SIZE_IRIS5, value);
	if (rc)
		return rc;

	value = (u32)core->iface_q_table.align_device_addr;
	rc = __write_register(core, HFI_QTBL_ADDR_IRIS5, value);
	if (rc)
		return rc;

	rc = __write_register(core, HFI_QTBL_INFO_IRIS5, HFI_QTBL_ENABLED);
	if (rc)
		return rc;

	if (core->mmap_buf.align_device_addr) {
		value = (u32)core->mmap_buf.align_device_addr;
		rc = __write_register(core, HFI_MMAP_ADDR_IRIS5, value);
		if (rc)
			return rc;
	} else {
		d_vpr_e("%s: skip mmap buffer programming\n", __func__);
		/* ignore the error for now for backward compatibility */
		/* return -EINVAL; */
	}

	rc = __get_device_region_info_iris5(core, &min_dev_reg_addr, &dev_reg_size);
	if (rc)
		return rc;

	if (min_dev_reg_addr && dev_reg_size) {
		rc = __write_register(core, HFI_DEVICE_REGION_ADDR_IRIS5, min_dev_reg_addr);
		if (rc)
			return rc;

		rc = __write_register(core, HFI_DEVICE_REGION_SIZE_IRIS5, dev_reg_size);
		if (rc)
			return rc;
	} else {
		d_vpr_h("%s: skip device region programming\n", __func__);
		/* ignore the error for now for backward compatibility */
		/* return -EINVAL; */
	}

	if (core->sfr.align_device_addr) {
		value = (u32)core->sfr.align_device_addr + VIDEO_ARCH_LX;
		rc = __write_register(core, HFI_SFR_ADDR_IRIS5, value);
		if (rc)
			return rc;
	}

	/* Based on below register programming, firmware WA for art-v2 would be enabled */
	if (of_device_is_compatible(dev->of_node, "qcom,art-vidc-v2")) {
		rc = __write_register(core, WRAPPER_IRIS_VCODEC_VPU_WRAPPER_SPARE_0_IRIS5, 0x1);
		if (rc)
			return rc;
	} else {
		rc = __write_register(core, WRAPPER_IRIS_VCODEC_VPU_WRAPPER_SPARE_0_IRIS5, 0x0);
		if (rc)
			return rc;
	}

	return 0;
}

static int __boot_firmware_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 ctrl_init_val = 0, ctrl_status = 0, count = 0, max_tries = 1000;

	rc = __program_bootup_registers_iris5(core);
	if (rc)
		return rc;

	ctrl_init_val = BIT(0);

	rc = __write_register(core, HFI_CTRL_INIT_IRIS5, ctrl_init_val);
	if (rc)
		return rc;

	while (count < max_tries) {
		rc = __read_register(core, HFI_CTRL_STATUS_IRIS5, &ctrl_status);
		if (rc)
			return rc;

		if ((ctrl_status & HFI_CTRL_ERROR_FATAL) ||
		    (ctrl_status & HFI_CTRL_ERROR_UC_REGION_NOT_SET) ||
		    (ctrl_status & HFI_CTRL_ERROR_HW_FENCE_QUEUE)) {
			d_vpr_e("%s: boot firmware failed, ctrl status %#x\n",
				__func__, ctrl_status);
			return -EINVAL;
		} else if (ctrl_status & HFI_CTRL_READY) {
			d_vpr_h("%s: boot firmware is successful, ctrl status %#x\n",
				__func__, ctrl_status);
			break;
		}

		usleep_range(50, 100);
		count++;
	}

	if (count >= max_tries) {
		d_vpr_e(FMT_STRING_BOOT_FIRMWARE_ERROR, ctrl_status, ctrl_init_val);
		return -ETIME;
	}

	/* Enable interrupt before sending commands to venus */
	rc = __write_register(core, CPU_CS_H2XSOFTINTEN_IRIS5, 0x1);
	if (rc)
		return rc;

	rc = __write_register(core, CPU_CS_X2RPMh_IRIS5, 0x0);
	if (rc)
		return rc;

	return rc;
}

static bool is_hw_power_collapsed_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 value = 0, pwr_status = 0;

	rc = __read_register(core, WRAPPER_CORE_POWER_STATUS_IRIS5, &value);
	if (rc)
		return false;

	/* if BIT(1) is 1 then video hw power is on else off */
	pwr_status = value & BIT(1);
	return pwr_status ? false : true;
}

static bool is_hw_enabled_iris5(struct msm_vidc_core *core, const char *name)
{
	int i = 0;

	for (i = 0 ; i < core->platform->data.pd_tbl_size; i++) {
		if (!strcmp(core->platform->data.pd_tbl[i].name, name))
			if (!core->platform->data.pd_tbl[i].hw_enable) {
				d_vpr_h("%s: hw %s not enabled\n", __func__,
						core->platform->data.pd_tbl[i].name);
				return false;
			}
	}
	return true;
}

static bool is_vpu_1p_iris5(struct msm_vidc_core *core)
{
	return !!(core->platform->data.vpu_ver == VPU_VERSION_IRIS5_1P);
}

static bool is_fallback_mode_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	int value = 0;

	rc = __read_register(core, WRAPPER_GPIO_IN_IRIS5, &value);
	if (rc) {
		d_vpr_e("%s register read WRAPPER_GPIO_IN_IRIS5 failed\n", __func__);
		return false;
	}
	return !!(value & 0x1);
}

static int __power_off_apv_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 value = 0;
	u32 count = 0;

	if (!is_hw_enabled_iris5(core, "apv"))
		return 0;

	rc = __read_register(core, WRAPPER_EFUSE_MONITOR_IRIS5, &value);
	if (rc)
		goto fail_read_efuse;

	if (is_vpu_1p_iris5(core) || (value & BIT(27)))
		return 0;

	/*
	 * check to make sure core clock branch enabled else
	 * we cannot read apv top idle register
	 * BIT(1) is set implies APV system clock is disabled
	 */
	rc = __read_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS5, &value);
	if (rc)
		return rc;

	if ((value & BIT(1))) {
		d_vpr_e("%s: core clock config not enabled, enabling it to read apv registers\n",
			__func__);
		rc = __write_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS5, 0);
		if (rc)
			return rc;
	}

	/*
	 * add APV TOP IDLE STATUS check before collapsing APV per HPG update
	 * poll for APV TOP IDLE STATUS -> HPG 3.4.4.2
	 * rc = __read_register_with_poll_timeout(core, WRAPPER_IRIS_APV_TOP_IDLE_STATUS_IRIS5,
	 *		0x11F, 0x11F, 2000, 20000);
	 * if (rc)
	 *	d_vpr_e("%s: APV_TOP_IDLE_STATUS (%d) is not idle (%#x)\n",
	 *		__func__, value);
	 */

	/* set MNoC to low power, set PD_NOC_QREQ (bit 0) */
	rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
					0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS5, &value);
	if (rc)
		return rc;

	while ((!(value & BIT(0))) && (value & BIT(2) || value & BIT(1))) {
		rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
					     0x0, BIT(0));
		if (rc)
			return rc;

		usleep_range(10, 20);

		rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
					     0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS5, &value);
		if (rc)
			return rc;

		++count;
		if (count >= 1000) {
			d_vpr_e("%s: AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5 failed\n", __func__);
			break;
		}
	}

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS5,
					       0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5 failed1\n", __func__);

	rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
					0x0, BIT(0));
	if (rc)
		return rc;

	rc = __write_register(core,  AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS5, 0x080200);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5,
					       0xffffffff, 0x080200, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5 failed\n", __func__);

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS5, 0x080200);
	if (rc)
		return rc;

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS5, 0);
	if (rc)
		return rc;

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS5, 0);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5,
					       0xffffffff, 0x0, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5 failed\n", __func__);

	/*
	 * Reset both sides of 2 ahb2ahb_bridges (TZ and non-TZ)
	 * do we need to check status register here?
	 */
	rc = __write_register(core, VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_IRIS5, 0x3);
	if (rc)
		return rc;
	rc = __write_register(core, VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_IRIS5, 0x2);
	if (rc)
		return rc;
	rc = __write_register(core, VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_IRIS5, 0x0);
	if (rc)
		return rc;

	/* VCODEC_VIDEO_CC_MVS0A_GDSCR --> apv */
	rc = call_res_op(core, gdsc_off, core, "apv");
	if (rc) {
		d_vpr_e("%s: disable apv regulator failed\n", __func__);
		rc = 0;
	}

	/* VCODEC_VIDEO_CC_MVS0A_CBCR --> video_cc_mvs0a_clk */
	rc = call_res_op(core, clk_disable, core, "video_cc_mvs0a_clk");
	if (rc) {
		d_vpr_e("%s: disable video_cc_mvs0a_clk failed\n", __func__);
		rc = 0;
	}

fail_read_efuse:
	return rc;
}

static int __power_off_mm_int_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 value = 0, count = 0;

	/* MVP_NoC MM Q-Channel */
	rc = __write_register_masked(core, NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRCLR_LOW,
			0x1, BIT(0));
	if (rc)
		return rc;

	rc = __write_register_masked(core, WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
				0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register(core, WRAPPER_MVP_NOC_LPI_STATUS_IRIS5, &value);
	if (rc)
		return rc;

	while ((!(value & BIT(0))) && (value & BIT(2) || value & BIT(1))) {
		rc = __write_register_masked(core, WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
						 0x0, BIT(0));
		if (rc)
			return rc;

		usleep_range(10, 20);

		rc = __write_register_masked(core, NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRCLR_LOW,
				0x1, BIT(0));
		if (rc)
			return rc;

		rc = __write_register_masked(core, WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
						 0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, WRAPPER_MVP_NOC_LPI_STATUS_IRIS5, &value);
		if (rc)
			return rc;

		++count;
		if (count >= 1000) {
			d_vpr_e("%s: WRAPPER_MVP_NOC_LPI_STATUS_IRIS5 failed\n", __func__);
			break;
		}
	}

	rc = __read_register_with_poll_timeout(core, WRAPPER_MVP_NOC_LPI_STATUS_IRIS5,
						   0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_e("%s: WRAPPER_MVP_NOC_LPI_STATUS_IRIS5 failed1\n", __func__);

	rc = __write_register_masked(core, WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
					0x0, BIT(0));
	if (rc)
		return rc;

	/* mm-int gdsc off */
	rc = call_res_op(core, gdsc_off, core, "mm-int");
	if (rc) {
		d_vpr_e("%s: disable regulator vcodec failed\n", __func__);
		rc = 0;
	}

	rc = call_res_op(core, clk_disable, core, "video_cc_mvs0c_freerun_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare video_cc_mvs0c_freerun_clk\n", __func__);
		rc = 0;
	}

	return rc;
}

static int __power_off_cx_int_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 value = 0, count = 0;

	/* MVP_NoC CX Q-Channel */
	rc = __write_register_masked(core, NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRCLR_LOW,
				0x1, BIT(0));
	if (rc)
		return rc;

	rc = __write_register_masked(core, WRAPPER_MVP_NOC_CX_LPI_CONTROL_IRIS5,
				0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register(core, WRAPPER_MVP_NOC_CX_LPI_STATUS_IRIS5, &value);
	if (rc)
		return rc;

	while ((!(value & BIT(0))) && (value & BIT(2) || value & BIT(1))) {
		rc = __write_register_masked(core, WRAPPER_MVP_NOC_CX_LPI_CONTROL_IRIS5,
						 0x0, BIT(0));
		if (rc)
			return rc;

		usleep_range(10, 20);

		rc = __write_register_masked(core, NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRCLR_LOW,
				0x1, BIT(0));
		if (rc)
			return rc;

		rc = __write_register_masked(core, WRAPPER_MVP_NOC_CX_LPI_CONTROL_IRIS5,
						 0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, WRAPPER_MVP_NOC_CX_LPI_STATUS_IRIS5, &value);
		if (rc)
			return rc;

		++count;
		if (count >= 1000) {
			d_vpr_e("%s: WRAPPER_MVP_NOC_CX_LPI_STATUS_IRIS5 failed\n", __func__);
			break;
		}
	}

	rc = __read_register_with_poll_timeout(core, WRAPPER_MVP_NOC_CX_LPI_STATUS_IRIS5,
						   0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_e("%s: WRAPPER_MVP_NOC_LPI_STATUS_IRIS5 failed1\n", __func__);

	rc = __write_register_masked(core, WRAPPER_MVP_NOC_CX_LPI_CONTROL_IRIS5,
					0x0, BIT(0));
	if (rc)
		return rc;

	/* Disable cx-int gdsc */
	rc = call_res_op(core, gdsc_off, core, "cx-int");
	if (rc) {
		d_vpr_e("%s: disable regulator vcodec failed\n", __func__);
		rc = 0;
	}

	rc = call_res_op(core, clk_disable, core, "video_cc_cx_axi0_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare video_cc_cx_axi0_clk\n", __func__);
		rc = 0;
	}

	return rc;
}

static int __power_off_hardware_iris5(struct msm_vidc_core *core)
{
	int rc = 0, i = 0;
	u32 value = 0;
	bool pwr_collapsed = false;
	u32 count = 0;
	u32 mvp_noc_partial_reset_val = 0;
	u32 ci_version = 0;

	/*
	 * Incase hw power control is enabled, for any error case
	 * CPU WD, video hw unresponsive cases, NOC error case etc,
	 * execute NOC reset sequence before disabling power. If there
	 * is no CPU WD and hw power control is enabled, fw is expected
	 * to power collapse video hw always.
	 */
	if (is_core_sub_state(core, CORE_SUBSTATE_FW_PWR_CTRL)) {
		pwr_collapsed = is_hw_power_collapsed_iris5(core);
		if (pwr_collapsed) {
			d_vpr_h("%s: video hw power collapsed %s\n",
				__func__, core->sub_state_name);
			goto disable_power;
		} else {
			d_vpr_e("%s: video hw is power ON, try power collpase hw %s\n",
				__func__, core->sub_state_name);
		}
	}

	/*
	 * check to make sure core clock branch enabled else
	 * we cannot read vcodec top idle register
	 * BIT(0) --> CORE_CLK_HALT
	 */
	rc = __read_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS5, &value);
	if (rc)
		return rc;

	if ((value & BIT(0))) {
		d_vpr_e("%s: core clock config not enabled, enabling it to read vcodec registers\n",
			__func__);
		rc = __write_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS5, 0);
		if (rc)
			return rc;
	}

	rc = __read_register(core, WRAPPER_CI_VERSION_IRIS5, &ci_version);
	if (rc)
		return rc;

	d_vpr_h("%s: WRAPPER_CI_VERSION value:0x%x\n", __func__, ci_version);

	/*
	 * add MNoC idle check before collapsing MVS0 per HPG update
	 * poll for VCODEC_SS_IDLE_STATUS -> HPG 3.4.4
	 */
	rc = __read_register_with_poll_timeout(core, VCODEC_SS_IDLE_STATUSn_IRIS5,
			0x7103, 0x7103, 2000, 20000);
	if (rc)
		d_vpr_e("%s: VCODEC_SS_IDLE_STATUS (%d) is not idle (%#x)\n",
			__func__, i, value);

	if (ci_version >= 0x10010000) {
		rc = __read_register(core, WRAPPER_EFUSE_MONITOR_IRIS5, &value);
		if (rc)
			return rc;

		if (!is_hw_enabled_iris5(core, "vpp0") || (value & BIT(29)))
			mvp_noc_partial_reset_val = 0x36010E;
		else if (!is_hw_enabled_iris5(core, "vpp1") || value & BIT(28))
			mvp_noc_partial_reset_val = 0x35000F;
		else
			mvp_noc_partial_reset_val = 0x37010F;

		rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS5,
				mvp_noc_partial_reset_val);
		if (rc)
			return rc;

		rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5,
				0xffffffff, mvp_noc_partial_reset_val, 200, 2000);
		if (rc)
			d_vpr_e("%s: AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5 failed1\n", __func__);

		rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS5,
				mvp_noc_partial_reset_val);
		if (rc)
			return rc;

		rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS5, 0x0);
		if (rc)
			return rc;
	} else { /* (ci_version < 0x10010000) */
		/* set MNoC to low power, set PD_NOC_QREQ (bit 0) */
		d_vpr_e("%s: WRAPPER_CI_VERSION is 0x%x we should not enter here for hawi\n",
					__func__, ci_version);

		rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
						0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS5, &value);
		if (rc)
			return rc;

		while ((!(value & BIT(0))) && (value & BIT(2) || value & BIT(1))) {
			rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
							 0x0, BIT(0));
			if (rc)
				return rc;

			usleep_range(10, 20);

			rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
							 0x1, BIT(0));
			if (rc)
				return rc;

			rc = __read_register(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS5, &value);
			if (rc)
				return rc;

			++count;
			if (count >= 1000) {
				d_vpr_e("%s: AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5 failed\n",
						__func__);
				break;
			}
		}

		rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS5,
							   0x1, 0x1, 200, 2000);
		if (rc)
			d_vpr_e("%s: AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5 failed1\n", __func__);

		rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
						0x0, BIT(0));
		if (rc)
			return rc;

		rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS5, 0x070103);
		if (rc)
			return rc;

		rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5,
				0xffffffff, 0x070103, 200, 2000);
		if (rc)
			d_vpr_e("%s: AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5 failed1\n", __func__);

		rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS5, 0x070103);
		if (rc)
			return rc;

		rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS5, 0x0);
		if (rc)
			return rc;
	}

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS5, 0x0);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5,
					       0xffffffff, 0x0, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS5\n", __func__);

	/*
	 * Reset both sides of 2 ahb2ahb_bridges (TSW and non-TSW)
	 */
	rc = __write_register(core, CPU_CS_AHB_BRIDGE_SYNC_RESET_IRIS5, 0x3);
	if (rc)
		return rc;
	rc = __write_register(core, CPU_CS_AHB_BRIDGE_SYNC_RESET_IRIS5, 0x2);
	if (rc)
		return rc;
	rc = __write_register(core, CPU_CS_AHB_BRIDGE_SYNC_RESET_IRIS5, 0x0);
	if (rc)
		return rc;

disable_power:
	/* power down process */

	rc = __read_register(core, WRAPPER_EFUSE_MONITOR_IRIS5, &value);
	if (rc)
		return rc;

	/* VCODEC_VIDEO_CC_MVS0_VPP1_GDSCR --> "vpp1" - To be named as per dtsi*/
	if (is_hw_enabled_iris5(core, "vpp1") && (!is_vpu_1p_iris5(core) || !(value & BIT(28)))) {
		rc = call_res_op(core, gdsc_off, core, "vpp1");
		if (rc) {
			d_vpr_e("%s: disable vpp1 regulator failed\n", __func__);
			rc = 0;
		}

		/* VIDEO_CC_MVS0_VPP1_CBCR --> video_cc_mvs0_vpp1_clk */
		rc = call_res_op(core, clk_disable, core, "video_cc_mvs0_vpp1_clk");
		if (rc) {
			d_vpr_e("%s: disable video_cc_mvs0_vpp1_clk failed\n", __func__);
			rc = 0;
		}
	}

	/* VCODEC_VIDEO_CC_MVS0_VPP0_GDSCR --> "vpp0" - To be named as per dtsi*/
	if (is_hw_enabled_iris5(core, "vpp0") && !(value & BIT(29))) {
		rc = call_res_op(core, gdsc_off, core, "vpp0");
		if (rc) {
			d_vpr_e("%s: disable vpp0 regulator failed\n", __func__);
			rc = 0;
		}

		/* VIDEO_CC_MVS0_VPP0_CBCR --> video_cc_mvs0_vpp0_clk */
		rc = call_res_op(core, clk_disable, core, "video_cc_mvs0_vpp0_clk");
		if (rc) {
			d_vpr_e("%s: disable video_cc_mvs0_vpp0_clk failed\n", __func__);
			rc = 0;
		}
	}

	rc = call_res_op(core, gdsc_off, core, "vcodec");
	if (rc) {
		d_vpr_e("%s: disable regulator vcodec failed\n", __func__);
		rc = 0;
	}

	rc = call_res_op(core, clk_disable, core, "video_cc_mvs0_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare video_cc_mvs0_clk failed\n", __func__);
		rc = 0;
	}

	rc = call_res_op(core, clk_disable, core, "video_cc_mvs0b_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare video_cc_mvs0b_clk failed\n", __func__);
		rc = 0;
	}

	if (ci_version >= 0x10010000) {
		rc = call_res_op(core, clk_disable, core, "video_cc_mvs0_vpp0_vpp1_gating_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepre video_cc_mvs0_vpp0_vpp1_gating_clk failed\n",
					__func__);
			rc = 0;
		}
		/*  Power down MVP_NoC */
		rc = __power_off_mm_int_iris5(core);
		if (rc) {
			d_vpr_e("%s: power off mm-int failed\n", __func__);
			rc = 0;
		}
		if (!is_fallback_mode_iris5(core)) {
			rc = __power_off_cx_int_iris5(core);
			if (rc) {
				d_vpr_e("%s: power off cx-int failed\n", __func__);
				rc = 0;
			}
		}
	}
	return rc;
}

static int __power_off_controller_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	int value = 0;
	u32 count = 0;
	u32 ci_version = 0;

	rc = __read_register(core, WRAPPER_CI_VERSION_IRIS5, &ci_version);
	if (rc)
		return rc;

	/*
	 * mask fal10_veto QLPAC error since fal10_veto can go 1
	 * when pwwait == 0 and clamped to 0 -> HPG 3.7.4
	 */
	rc = __write_register(core, CPU_CS_X2RPMh_IRIS5, 0x3);
	if (rc)
		return rc;

	/* Set Iris CPU NoC to Low power */
	rc = __write_register_masked(core, WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS5,
			0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register(core, WRAPPER_IRIS_CPU_NOC_LPI_STATUS_IRIS5, &value);
	if (rc)
		return rc;

	while ((!(value & BIT(0))) && (value & BIT(1))) {
		rc = __write_register_masked(core, WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS5,
					     0x0, BIT(0));
		if (rc)
			return rc;

		usleep_range(10, 20);

		rc = __write_register_masked(core, WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS5,
					     0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, WRAPPER_IRIS_CPU_NOC_LPI_STATUS_IRIS5, &value);
		if (rc)
			return rc;

		++count;
		if (count >= 1000) {
			d_vpr_e("%s: WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS5 failed\n", __func__);
			break;
		}
	}

	rc = __read_register_with_poll_timeout(core, WRAPPER_IRIS_CPU_NOC_LPI_STATUS_IRIS5,
			0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_e("%s: WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS5 failed\n", __func__);

	rc = __write_register_masked(core, WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS5,
				     0x0, BIT(0));
	if (rc)
		return rc;

	rc = __write_register_masked(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS5,
				     0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_STATUS_IRIS5, &value);
	if (rc)
		return rc;

	while ((!(value & BIT(0))) && (value & BIT(1) || value & BIT(2))) {
		rc = __write_register_masked(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS5,
					     0x0, BIT(0));
		if (rc)
			return rc;

		usleep_range(10, 20);

		rc = __write_register_masked(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS5,
					     0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_STATUS_IRIS5, &value);
		if (rc)
			return rc;

		++count;
		if (count >= 1000) {
			d_vpr_e("%s: AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS5 failed\n",
							__func__);
			break;
		}
	}

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_STATUS_IRIS5,
					       0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS5 failed\n", __func__);

	rc = __write_register_masked(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS5,
				     0x0, BIT(0));
	if (rc)
		return rc;

	/* Debug bridge LPI release */
	rc = __write_register(core, WRAPPER_DEBUG_BRIDGE_LPI_CONTROL_IRIS5, 0x0);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, WRAPPER_DEBUG_BRIDGE_LPI_STATUS_IRIS5,
					       0xffffffff, 0x0, 200, 2000);
	if (rc)
		d_vpr_e("%s: debug bridge release failed\n", __func__);

	/* power down process */
	rc = call_res_op(core, gdsc_off, core, "iris-ctl");
	if (rc) {
		d_vpr_e("%s: disable regulator iris-ctl failed\n", __func__);
		rc = 0;
	}

	rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_ARCG_CONTROL_IRIS5,
				     0x1, BIT(0));
	if (rc)
		return rc;

	if (ci_version >= 0x10010000) {
		rc = call_res_op(core, clk_disable, core, "gcc_video_axi0c_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare gcc_video_axi0c_clk failed\n", __func__);
			rc = 0;
		}

		rc = call_res_op(core, clk_disable, core, "gcc_video_axi0_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare gcc_video_axi0_clk failed\n", __func__);
			rc = 0;
		}

		rc = call_res_op(core, clk_disable, core, "video_cc_mvs0c_ctl_freerun_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare video_cc_mvs0c_ctl_freerun_clk failed\n",
					__func__);
			rc = 0;
		}

		/* rc = call_res_op(core, clk_disable, core, "video_cc_mvs0c_debug_clk");
		 * if (rc) {
		 *	d_vpr_e("%s: disable unprepare video_cc_mvs0c_debug_clk failed\n",
		 *					__func__);
		 *	rc = 0;
		 *   }
		 */

		rc = call_res_op(core, clk_disable, core, "video_cc_mvs0c_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare video_cc_mvs0c_clk failed\n", __func__);
			rc = 0;
		}

		rc = call_res_op(core, reset_control_assert, core, "video_axi0c_reset");
		if (rc)
			d_vpr_e("%s: assert video_axi0c_reset failed\n", __func__);

		rc = call_res_op(core, reset_control_assert, core, "video_mvs0c_ctl_freerun_reset");
		if (rc)
			d_vpr_e("%s: assert video_mvs0c_ctl_freerun_reset failed\n", __func__);

		usleep_range(400, 500);

		rc = call_res_op(core, reset_control_deassert, core,
				"video_mvs0c_ctl_freerun_reset");
		if (rc)
			d_vpr_e("%s: deassert video_mvs0c_ctl_freerun_reset failed\n", __func__);

		rc = call_res_op(core, reset_control_deassert, core, "video_axi0c_reset");
		if (rc)
			d_vpr_e("%s: deassert video_axi0c_reset failed\n", __func__);
	} else { /* (ci_version < 0x10010000) */
		d_vpr_e("%s: WRAPPER_CI_VERSION is 0x%x we should not enter here for hawi\n",
					__func__, ci_version);

		rc = call_res_op(core, clk_disable, core, "gcc_video_axi1_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare gcc_video_axi1_clk failed\n", __func__);
			rc = 0;
		}

		rc = call_res_op(core, clk_disable, core, "gcc_video_axi0_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare gcc_video_axi0_clk failed\n", __func__);
			rc = 0;
		}

		rc = call_res_op(core, clk_disable, core, "video_cc_mvs0c_freerun_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare video_cc_mvs0c_freerun_clk failed\n",
					__func__);
			rc = 0;
		}

		rc = call_res_op(core, clk_disable, core, "video_cc_mvs0_freerun_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare video_cc_mvs0_freerun_clk failed\n",
					__func__);
			rc = 0;
		}

		rc = call_res_op(core, clk_disable, core, "video_cc_mvs0c_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare video_cc_mvs0c_clk failed\n", __func__);
			rc = 0;
		}

		rc = call_res_op(core, reset_control_assert, core, "video_axi1_reset");
		if (rc)
			d_vpr_e("%s: assert video_axi1_reset failed\n", __func__);

		rc = call_res_op(core, reset_control_assert, core, "video_axi0_reset");
		if (rc)
			d_vpr_e("%s: assert video_axi0_reset failed\n", __func__);

		rc = call_res_op(core, reset_control_assert, core, "video_mvs0c_freerun_reset");
		if (rc)
			d_vpr_e("%s: assert video_mvs0c_reset failed\n", __func__);

		rc = call_res_op(core, reset_control_assert, core, "video_mvs0_freerun_reset");
		if (rc)
			d_vpr_e("%s: assert video_mvs0_reset failed\n", __func__);

		usleep_range(400, 500);

		rc = call_res_op(core, reset_control_deassert, core, "video_mvs0_freerun_reset");
		if (rc)
			d_vpr_e("%s: deassert video_mvs0_reset failed\n", __func__);

		rc = call_res_op(core, reset_control_deassert, core, "video_mvs0c_freerun_reset");
		if (rc)
			d_vpr_e("%s: deassert video_mvs0c_reset failed\n", __func__);

		rc = call_res_op(core, reset_control_deassert, core, "video_axi0_reset");
		if (rc)
			d_vpr_e("%s: deassert video_axi0_reset failed\n", __func__);

		rc = call_res_op(core, reset_control_deassert, core, "video_axi1_reset");
		if (rc)
			d_vpr_e("%s: deassert video_axi1_reset failed\n", __func__);
	}

	return rc;
}

static int __power_off_iris5(struct msm_vidc_core *core)
{
	int rc = 0;

	if (!is_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE))
		return 0;

	/*
	 * Reset video_cc_mvs0_clk_src value to resolve MMRM high video
	 * clock projection issue.
	 */
	rc = call_res_op(core, set_clks, core, get_min_clock_index(core));
	if (rc)
		d_vpr_e("%s: resetting core clocks failed\n", __func__);

	rc = call_res_op(core, gdsc_sw_ctrl, core);
	if (rc)
		d_vpr_e("%s: gdsc_sw_ctrl failed\n", __func__);

	if (__power_off_apv_iris5(core))
		d_vpr_e("%s: failed to power off apv\n", __func__);

	if (__power_off_hardware_iris5(core))
		d_vpr_e("%s: failed to power off hardware\n", __func__);

	if (__power_off_controller_iris5(core))
		d_vpr_e("%s: failed to power off controller\n", __func__);

	rc = call_res_op(core, set_bw, core, 0, 0);
	if (rc)
		d_vpr_e("%s: failed to unvote buses\n", __func__);

	if (!call_venus_op(core, watchdog, core, core->intr_status))
		disable_irq_nosync(core->resource->irq);

	msm_vidc_change_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE, 0, __func__);

	return rc;
}

static int __power_on_controller_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 mask_val = 0;
	u32 ci_version = 0;

	rc = call_res_op(core, gdsc_on, core, "iris-ctl");
	if (rc)
		goto fail_regulator;

	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0c_clk");
	if (rc)
		goto fail_clk_controller;

	/* video controller powered on successfully */
	rc = msm_vidc_change_core_sub_state(core, 0, CORE_SUBSTATE_POWER_ENABLE, __func__);
	if (rc)
		goto fail_power_on_substate;

	rc = __read_register(core, WRAPPER_CI_VERSION_IRIS5, &ci_version);
	if (rc)
		return rc;

	if (ci_version >= 0x10010000) {
		rc = call_res_op(core, clk_enable, core, "gcc_video_axi0c_clk");
		if (rc)
			goto fail_video_axi0c;

		rc = call_res_op(core, clk_enable, core, "video_cc_mvs0c_ctl_freerun_clk");
		if (rc)
			goto fail_clk_ctl_freerun;

		rc = call_res_op(core, clk_enable, core, "video_cc_mvs0c_debug_clk");
		if (rc)
			goto fail_clk_ctl_debug;

		if (is_fallback_mode_iris5(core)) {
			rc = __write_register_masked(core, WRAPPER_GPIO_OUT_IRIS5,
				1 << 3, BIT(3));
			if (rc) {
				d_vpr_e("%s: write register WRAPPER_GPIO_OUT failed\n", __func__);
				return rc;
			}
			rc = __write_register_masked(core, WRAPPER_GPIO_OUT_IRIS5,
				1 << 2, BIT(2));
			if (rc) {
				d_vpr_e("%s: write register WRAPPER_GPIO_OUT failed\n", __func__);
				return rc;
			}
			rc = __read_register(core, WRAPPER_INTR_MASK_IRIS5, &mask_val);
			if (rc) {
				d_vpr_e("%s: write register WRAPPER_GPIO_OUT failed\n", __func__);
				return rc;
			}
			rc = __read_register_with_poll_timeout(core, WRAPPER_GPIO_IN_IRIS5,
							   BIT(5), 0x0, 200, 2000);
			if (rc) {
				d_vpr_e("%s: read register WRAPPER_GPIO_IN failed\n", __func__);
				return rc;
			}
			rc = __write_register_masked(core, WRAPPER_GPIO_OUT_IRIS5,
				0 << 2, BIT(2));
			if (rc) {
				d_vpr_e("%s: write register WRAPPER_GPIO_OUT failed\n", __func__);
				return rc;
			}
			rc = __write_register_masked(core, WRAPPER_GPIO_OUT_IRIS5,
				0 << 3, BIT(3));
			if (rc) {
				d_vpr_e("%s: write register WRAPPER_GPIO_OUT failed\n", __func__);
				return rc;
			}
		}

		rc = __write_register_masked(core, CPU_NOC_SBM_FAULTINEN0_LOW,
				0x1, BIT(0));
		if (rc)
			return rc;

		rc = __write_register_masked(core, CPU_NOC_ERRORLOGGER_MAINCTL_LOW,
					0x1, BIT(0));
		if (rc)
			return rc;

		rc = __write_register_masked(core, WRAPPER_INTR_MASK_IRIS5,
					0 << 6, BIT(6));
		if (rc)
			return rc;

		rc = __write_register_masked(core,
				NOC_SIDEBANDMANAGER_MAIN_SIDEBANDMANAGER_FAULTINEN0_LOW,
					0x1, BIT(0));
		if (rc)
			return rc;

		rc = __write_register_masked(core, NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_MAINCTL_LOW,
					0x1, BIT(0));
		if (rc)
			return rc;

		rc = __write_register_masked(core, WRAPPER_INTR_MASK_IRIS5,
					0 << 5, BIT(5));
		if (rc)
			return rc;

	} else {
		rc = call_res_op(core, clk_enable, core, "gcc_video_axi1_clk");
		if (rc)
			goto fail_video_axi1;

		rc = call_res_op(core, clk_enable, core, "video_cc_mvs0c_freerun_clk");
		if (rc)
			goto fail_clk_freerun;
	}

	return 0;

fail_clk_ctl_debug:
call_res_op(core, clk_disable, core, "video_cc_mvs0c_ctl_freerun_clk");
fail_clk_ctl_freerun:
	call_res_op(core, clk_disable, core, "gcc_video_axi0c_clk");
fail_clk_freerun:
	if (ci_version >= 0x10010000)
		// do nothing
	else
		call_res_op(core, clk_disable, core, "gcc_video_axi1_clk");
fail_power_on_substate:
fail_video_axi1:
fail_video_axi0c:
	call_res_op(core, clk_disable, core, "video_cc_mvs0c_clk");
fail_clk_controller:
	call_res_op(core, gdsc_off, core, "iris-ctl");
fail_regulator:
	return rc;
}

static int __power_on_mm_int_iris5(struct msm_vidc_core *core)
{
	int rc = 0;

	/* Enable mm-int */
	rc = call_res_op(core, gdsc_on, core, "mm-int");
	if (rc)
		goto fail_regulator_mm_int;

	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0c_freerun_clk");
	if (rc)
		goto fail_mvs0c_freerun_clk;

	rc = __write_register_masked(core, WRAPPER_MVP_NOC_LPI_CONTROL_IRIS5,
		0x0, BIT(0));
	if (rc) {
		d_vpr_e("%s: WRAPPER_MVP_NOC_LPI_CONTROL failed\n", __func__);
		goto fail_mvp_noc_lpi_register_write;
	}

	rc = __read_register_with_poll_timeout(core, WRAPPER_MVP_NOC_LPI_STATUS_IRIS5,
		BIT(0), 0x0, 200, 2000);
	if (rc) {
		d_vpr_e("%s: WRAPPER_MVP_NOC_LPI_STATUS failed\n", __func__);
		goto fail_mvp_noc_lpi_register_write;
	}

	return 0;

fail_mvp_noc_lpi_register_write:
	call_res_op(core, clk_disable, core, "video_cc_mvs0c_freerun_clk");
fail_mvs0c_freerun_clk:
	call_res_op(core, gdsc_off, core, "mm-int");
fail_regulator_mm_int:
	return rc;
}

static int __power_on_cx_int_iris5(struct msm_vidc_core *core)
{
	int rc = 0;

	/* Enable CX-INT */
	rc = call_res_op(core, gdsc_on, core, "cx-int");
	if (rc)
		goto fail_regulator_cx_int;

	rc = call_res_op(core, clk_enable, core, "video_cc_cx_axi0_clk");
	if (rc)
		goto fail_clk_cx_axi0;

	rc = __write_register_masked(core, WRAPPER_MVP_NOC_CX_LPI_CONTROL_IRIS5,
			0x0, BIT(0));
	if (rc) {
		d_vpr_e("%s: WRAPPER_MVP_NOC_CX_LPI_CONTROL failed\n", __func__);
		goto fail_mvp_noc_cx_lpi_register;
	}

	rc = __read_register_with_poll_timeout(core, WRAPPER_MVP_NOC_CX_LPI_STATUS_IRIS5,
			       BIT(0), 0x0, 200, 2000);
	if (rc) {
		d_vpr_e("%s: WRAPPER_MVP_NOC_CX_LPI_STATUS failed\n", __func__);
		goto fail_mvp_noc_cx_lpi_register;
	}

	return 0;

fail_mvp_noc_cx_lpi_register:
	call_res_op(core, clk_disable, core, "video_cc_cx_axi0_clk");
fail_clk_cx_axi0:
	call_res_op(core, gdsc_off, core, "cx-int");
fail_regulator_cx_int:
	return rc;
}
static int __power_on_hardware_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	int value = 0;
	u32 ci_version = 0;

	rc = __read_register(core, WRAPPER_CI_VERSION_IRIS5, &ci_version);
	if (rc)
		return rc;

	if (ci_version >= 0x10010000) {
		if (!is_fallback_mode_iris5(core)) {
			rc = __power_on_cx_int_iris5(core);
			if (rc) {
				d_vpr_e("%s: power of cx-int failed\n", __func__);
				goto fail_cx_int;
			}
		}
		rc = __power_on_mm_int_iris5(core);
		if (rc) {
			d_vpr_e("%s: power of mm-int failed\n", __func__);
			goto fail_mm_int;
		}
	}

	rc = call_res_op(core, gdsc_on, core, "vcodec");
	if (rc)
		goto fail_regulator;

	rc = __read_register(core, WRAPPER_EFUSE_MONITOR_IRIS5, &value);
	if (rc)
		goto fail_read_efuse;

	/* VIDEO_CC_MVS0_VPP0_GDSCR --> vpp0 */
	if (is_hw_enabled_iris5(core, "vpp0") && !(value & BIT(29))) {
		rc = call_res_op(core, gdsc_on, core, "vpp0");
		if (rc)
			goto fail_regulator_vpp0;
	}

	/* VIDEO_CC_MVS0_VPP1_GDSCR --> vpp1 */
	if (is_hw_enabled_iris5(core, "vpp1") && (!is_vpu_1p_iris5(core) || !(value & BIT(28)))) {
		rc = call_res_op(core, gdsc_on, core, "vpp1");
		if (rc)
			goto fail_regulator_vpp1;
	}

	rc = call_res_op(core, gdsc_sw_ctrl, core);
	if (rc)
		goto fail_sw_ctrl;

	rc = call_res_op(core, clk_enable, core, "gcc_video_axi0_clk");
	if (rc)
		goto fail_clk_axi;

	if (ci_version >= 0x10010000) {
		// do nothing
	} else {
		rc = call_res_op(core, clk_enable, core, "video_cc_mvs0_freerun_clk");
		if (rc)
			goto fail_clk_freerun;
	}

	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0_clk");
	if (rc)
		goto fail_clk_controller;

	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0b_clk");
	if (rc)
		goto fail_clk_bse_controller;

	/* VIDEO_CC_MVS0_VPP0_GDSCR --> vpp0 */
	if (is_hw_enabled_iris5(core, "vpp0") && !(value & BIT(29))) {
		/* VIDEO_CC_MVS0_VPP0_CBCR --> video_cc_mvs0_vpp0_clk */
		rc = call_res_op(core, clk_enable, core, "video_cc_mvs0_vpp0_clk");
		if (rc)
			goto fail_clk_vpp0;
	}

	/* VIDEO_CC_MVS0_VPP1_GDSCR --> vpp1 */
	if (is_hw_enabled_iris5(core, "vpp1") && (!is_vpu_1p_iris5(core) || !(value & BIT(28)))) {
		/* VIDEO_CC_MVS0_VPP1_CBCR --> video_cc_mvs0_vpp1_clk */
		rc = call_res_op(core, clk_enable, core, "video_cc_mvs0_vpp1_clk");
		if (rc)
			goto fail_clk_vpp1;
	}

	if (ci_version >= 0x10010000) {
		rc = call_res_op(core, clk_enable, core, "video_cc_mvs0_vpp0_vpp1_gating_clk");
		if (rc)
			goto fail_gating_clk;
	}

	return 0;

fail_gating_clk:
	call_res_op(core, clk_disable, core, "video_cc_mvs0_vpp1_clk");
fail_clk_vpp1:
	if (is_hw_enabled_iris5(core, "vpp0") && !(value & BIT(29)))
		call_res_op(core, clk_disable, core, "video_cc_mvs0_vpp0_clk");
fail_clk_vpp0:
	call_res_op(core, clk_disable, core, "video_cc_mvs0b_clk");
fail_clk_bse_controller:
	call_res_op(core, clk_disable, core, "video_cc_mvs0_clk");
fail_clk_controller:
	if (ci_version >= 0x10010000)
		// do nothing
	else
		call_res_op(core, clk_disable, core, "video_cc_mvs0_freerun_clk");
fail_clk_freerun:
	call_res_op(core, clk_disable, core, "gcc_video_axi0_clk");
fail_clk_axi:
fail_sw_ctrl:
	if (is_hw_enabled_iris5(core, "vpp1") && (!is_vpu_1p_iris5(core) || !(value & BIT(28))))
		call_res_op(core, gdsc_off, core, "vpp1");
fail_regulator_vpp1:
	if (is_hw_enabled_iris5(core, "vpp0") && !(value & BIT(29)))
		call_res_op(core, gdsc_off, core, "vpp0");
fail_regulator_vpp0:
fail_read_efuse:
	call_res_op(core, gdsc_off, core, "vcodec");
fail_regulator:
	if (ci_version >= 0x10010000)
		__power_off_mm_int_iris5(core);
fail_mm_int:
	if (ci_version >= 0x10010000)
		__power_off_cx_int_iris5(core);
fail_cx_int:
	return rc;
}

static int __power_on_apv_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	int value = 0;

	if (!is_hw_enabled_iris5(core, "apv"))
		return 0;

	rc = __read_register(core, WRAPPER_EFUSE_MONITOR_IRIS5, &value);
	if (rc)
		goto fail_read_efuse;

	if (is_vpu_1p_iris5(core) || (value & BIT(27)))
		return 0;

	/* VIDEO_CC_MVS0A_GDSCR --> apv*/
	rc = call_res_op(core, gdsc_on, core, "apv");
	if (rc)
		goto fail_regulator;

	rc = call_res_op(core, gdsc_sw_ctrl, core);
	if (rc)
		goto fail_sw_ctrl;

	/* VIDEO_CC_MVS0A_CBCR --> video_cc_mvs0a_clk */
	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0a_clk");
	if (rc)
		goto fail_clk_controller;

	return 0;

fail_clk_controller:
fail_sw_ctrl:
	call_res_op(core, gdsc_off, core, "apv");
fail_regulator:
fail_read_efuse:
	return rc;
}

static int __power_on_iris5(struct msm_vidc_core *core)
{
	u32 idx = 0;
	int rc = 0;

	if (is_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE))
		return 0;

	if (!core_in_valid_state(core)) {
		d_vpr_e("%s: invalid core state %s\n",
			__func__, core_state_name(core->state));
		return -EINVAL;
	}

	/* Vote for all hardware resources */
	rc = call_res_op(core, set_bw, core, INT_MAX, INT_MAX);
	if (rc) {
		d_vpr_e("%s: failed to vote buses, rc %d\n", __func__, rc);
		goto fail_vote_buses;
	}

	rc = __power_on_controller_iris5(core);
	if (rc) {
		d_vpr_e("%s: failed to power on IRIS5 controller\n", __func__);
		goto fail_power_on_controller;
	}

	rc = __power_on_hardware_iris5(core);
	if (rc) {
		d_vpr_e("%s: failed to power on IRIS5 hardware\n", __func__);
		goto fail_power_on_hardware;
	}

	rc = __power_on_apv_iris5(core);
	if (rc) {
		d_vpr_e("%s: failed to power on IRIS5 apv\n", __func__);
		goto fail_power_on_apv;
	}

	idx = core->power.clk_freq_idx ? core->power.clk_freq_idx : 0;
	rc = call_res_op(core, set_clks, core, idx);
	if (rc) {
		d_vpr_e("%s: failed to scale clocks\n", __func__);
		rc = 0;
	}

	__set_registers(core);

	__interrupt_init_iris5(core);
	core->intr_status = 0;
	enable_irq(core->resource->irq);

	return rc;

fail_power_on_apv:
	__power_off_hardware_iris5(core);
fail_power_on_hardware:
	__power_off_controller_iris5(core);
fail_power_on_controller:
	call_res_op(core, set_bw, core, 0, 0);
fail_vote_buses:
	msm_vidc_change_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE, 0, __func__);

	return rc;
}

static int __prepare_pc_iris5(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 wfi_status = 0, idle_status = 0, pc_ready = 0;
	u32 ctrl_status = 0;

	rc = __read_register(core, HFI_CTRL_STATUS_IRIS5, &ctrl_status);
	if (rc)
		return rc;

	pc_ready = ctrl_status & HFI_CTRL_PC_READY;
	idle_status = ctrl_status & BIT(30);

	if (pc_ready) {
		d_vpr_h("Already in pc_ready state\n");
		return 0;
	}
	rc = __read_register(core, WRAPPER_TSW_CPU_STATUS_IRIS5, &wfi_status);
	if (rc)
		return rc;

	wfi_status &= BIT(0);
	if (!wfi_status || !idle_status) {
		d_vpr_e("Skipping PC, wfi status not set\n");
		goto skip_power_off;
	}

	rc = __prepare_pc(core);
	if (rc) {
		d_vpr_e("Failed __prepare_pc %d\n", rc);
		goto skip_power_off;
	}

	rc = __read_register_with_poll_timeout(core, HFI_CTRL_STATUS_IRIS5,
			HFI_CTRL_PC_READY, HFI_CTRL_PC_READY, 250, 2500);
	if (rc) {
		d_vpr_e("%s: Skip PC. Ctrl status not set\n", __func__);
		goto skip_power_off;
	}

	rc = __read_register_with_poll_timeout(core, WRAPPER_TSW_CPU_STATUS_IRIS5,
			BIT(0), 0x1, 250, 2500);
	if (rc) {
		d_vpr_e("%s: Skip PC. Wfi status not set\n", __func__);
		goto skip_power_off;
	}
	return rc;

skip_power_off:
	rc = __read_register(core, HFI_CTRL_STATUS_IRIS5, &ctrl_status);
	if (rc)
		return rc;
	rc = __read_register(core, WRAPPER_TSW_CPU_STATUS_IRIS5, &wfi_status);
	if (rc)
		return rc;
	wfi_status &= BIT(0);
	d_vpr_e("Skip PC, wfi=%#x, idle=%#x, pcr=%#x, ctrl=%#x)\n",
		wfi_status, idle_status, pc_ready, ctrl_status);
	return -EAGAIN;
}

static int __watchdog_iris5(struct msm_vidc_core *core, u32 intr_status)
{
	int rc = 0;

	if (intr_status & WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS5) {
		d_vpr_e("%s: received watchdog interrupt\n", __func__);
		rc = 1;
	}

	return rc;
}

static int __noc_error_info_iris5(struct msm_vidc_core *core)
{
	u32 value;
	int rc = 0;

	if (is_hw_power_collapsed_iris5(core)) {
		d_vpr_e("%s: video hardware already power collapsed\n", __func__);
		return rc;
	}

	rc = __read_register(core,
			NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG0_LOW, &value);
	if (!rc)
		d_vpr_e("%s: NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG0_LOW:  %#x\n",
			__func__, value);
	rc = __read_register(core,
			NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG0_HIGH, &value);
	if (!rc)
		d_vpr_e("%s: NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG0_HIGH:  %#x\n",
			__func__, value);
	rc = __read_register(core,
			NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG1_LOW, &value);
	if (!rc)
		d_vpr_e("%s: NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG1_LOW:  %#x\n",
			__func__, value);
	rc = __read_register(core,
			NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG1_HIGH, &value);
	if (!rc)
		d_vpr_e("%s: NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG1_HIGH:  %#x\n",
			__func__, value);
	rc = __read_register(core,
			NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG2_LOW, &value);
	if (!rc)
		d_vpr_e("%s: NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG2_LOW:  %#x\n",
			__func__, value);
	rc = __read_register(core,
			NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG2_HIGH, &value);
	if (!rc)
		d_vpr_e("%s: NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG2_HIGH:  %#x\n",
			__func__, value);
	rc = __read_register(core,
			NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG3_LOW, &value);
	if (!rc)
		d_vpr_e("%s: NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG3_LOW:  %#x\n",
			__func__, value);
	rc = __read_register(core,
			NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG3_HIGH, &value);
	if (!rc)
		d_vpr_e("%s: NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRLOG3_HIGH:  %#x\n",
			__func__, value);

	return rc;
}

static int __hw_ctrl_gdsc_iris5(struct msm_vidc_core *core)
{
	return call_res_op(core, gdsc_hw_ctrl, core);
}

static int __sw_ctrl_gdsc_iris5(struct msm_vidc_core *core)
{
	return call_res_op(core, gdsc_sw_ctrl, core);
}

static struct msm_vidc_venus_ops iris5_ops = {
	.raise_interrupt = __raise_interrupt_iris5,
	.clear_interrupt = __clear_interrupt_iris5,
	.boot_firmware = __boot_firmware_iris5,
	.power_on = __power_on_iris5,
	.power_off = __power_off_iris5,
	.prepare_pc = __prepare_pc_iris5,
	.watchdog = __watchdog_iris5,
	.noc_error_info = __noc_error_info_iris5,
	.hw_ctrl_gdsc = __hw_ctrl_gdsc_iris5,
	.sw_ctrl_gdsc = __sw_ctrl_gdsc_iris5,
	.scm_mem_protect = msm_vidc_mem_protect_video_regions_v2,
};

struct msm_vidc_session_ops msm_session_ops = {
	.buffer_size = msm_buffer_size_iris5,
	.min_count = msm_buffer_min_count_iris5,
	.extra_count = msm_buffer_extra_count_iris5,
	.ring_buf_count = msm_vidc_ring_buf_count_iris5,
	.scale_clocks = msm_vidc_scale_clocks_iris5,
	.calc_bw = msm_vidc_calc_bw_iris5,
	.decide_work_route = msm_vidc_decide_work_route_iris5p,
	.decide_work_mode = msm_vidc_decide_work_mode_iris5p,
	.decide_quality_mode = msm_vidc_decide_quality_mode_iris5p,
	.decide_scaling = msm_vidc_decide_scaling_iris5p,
};

int msm_vidc_init_iris5(struct msm_vidc_core *core)
{
	d_vpr_h("%s()\n", __func__);
	core->venus_ops = &iris5_ops;
	core->session_ops = &msm_session_ops;

	return 0;
}
