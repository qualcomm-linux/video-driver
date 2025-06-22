// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/delay.h>
#include <linux/reset.h>
#include <media/videobuf2-core.h>

#include "msm_vidc_iris4.h"
#include "msm_vidc_buffer_iris4.h"
#include "msm_vidc_power_iris4.h"
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
#define WRAPPER_APV_BASE_OFFS_IRIS4                   0x00040000
#define WRAPPER_IRIS_APV_TOP_IRQ_STATUS_IRIS4        (WRAPPER_APV_BASE_OFFS_IRIS4 + 0x00)
#define WRAPPER_IRIS_APV_TOP_IRQ_MASK_IRIS4          (WRAPPER_APV_BASE_OFFS_IRIS4 + 0x04)
#define WRAPPER_IRIS_APV_TOP_IRQ_CLEAR_IRIS4         (WRAPPER_APV_BASE_OFFS_IRIS4 + 0x08)
#define WRAPPER_IRIS_APV_TOP_CLK_HALT_IRIS4          (WRAPPER_APV_BASE_OFFS_IRIS4 + 0x10)
#define WRAPPER_IRIS_APV_TOP_IDLE_STATUS_IRIS4       (WRAPPER_APV_BASE_OFFS_IRIS4 + 0x70)
#define WRAPPER_IRIS_APV_TOP_VPP_START_IRIS4         (WRAPPER_APV_BASE_OFFS_IRIS4 + 0xE0)
#define WRAPPER_IRIS_APV_TOP_VPP_CONFIG_IRIS4        (WRAPPER_APV_BASE_OFFS_IRIS4 + 0xEC)
#define WRAPPER_IRIS_APV_TOP_ENC_CONFIG_IRIS4        (WRAPPER_APV_BASE_OFFS_IRIS4 + 0xF0)

/*
 * --------------------------------------------------------------------------
 * MODULE: VCODEC_CPU_CS
 * --------------------------------------------------------------------------
 */
#define VCODEC_CPU_CS_IRIS4                            0x000A0000
#define CPU_CS_A2HSOFTINTCLR_IRIS4                     (VCODEC_CPU_CS_IRIS4 + 0x1C)
#define VCODEC_VPU_CPU_CS_VCICMDARG0_IRIS4             (VCODEC_CPU_CS_IRIS4 + 0x24)
#define VCODEC_VPU_CPU_CS_VCICMDARG1_IRIS4             (VCODEC_CPU_CS_IRIS4 + 0x28)
#define VCODEC_VPU_CPU_CS_SCIACMD_IRIS4                (VCODEC_CPU_CS_IRIS4 + 0x48)
#define VCODEC_VPU_CPU_CS_SCIACMDARG0_IRIS4            (VCODEC_CPU_CS_IRIS4 + 0x4C)
#define VCODEC_VPU_CPU_CS_SCIACMDARG1_IRIS4            (VCODEC_CPU_CS_IRIS4 + 0x50)
#define VCODEC_VPU_CPU_CS_SCIACMDARG2_IRIS4            (VCODEC_CPU_CS_IRIS4 + 0x54)
#define VCODEC_VPU_CPU_CS_SCIBCMD_IRIS4                (VCODEC_CPU_CS_IRIS4 + 0x5C)
#define VCODEC_VPU_CPU_CS_SCIBCMDARG0_IRIS4            (VCODEC_CPU_CS_IRIS4 + 0x60)
#define VCODEC_VPU_CPU_CS_SCIBARG1_IRIS4               (VCODEC_CPU_CS_IRIS4 + 0x64)
#define VCODEC_VPU_CPU_CS_SCIBARG2_IRIS4               (VCODEC_CPU_CS_IRIS4 + 0x68)
#define CPU_CS_H2XSOFTINTEN_IRIS4                      (VCODEC_CPU_CS_IRIS4 + 0x148)
#define CPU_IC_SOFTINT_IRIS4                           (VCODEC_CPU_CS_IRIS4 + 0x150)
#define CPU_CS_AHB_BRIDGE_SYNC_RESET_IRIS4             (VCODEC_CPU_CS_IRIS4 + 0x160)
#define CPU_CS_X2RPMh_IRIS4                            (VCODEC_CPU_CS_IRIS4 + 0x168)
#define VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_IRIS4  (VCODEC_CPU_CS_IRIS4 + 0x174)
#define VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_STATUS_IRIS4 (VCODEC_CPU_CS_IRIS4 + 0x178)
#define CPU_IC_SOFTINT_H2A_SHFT_IRIS4                  0x0

#define HFI_CTRL_INIT_IRIS4                            VCODEC_VPU_CPU_CS_SCIACMD_IRIS4
#define HFI_CTRL_STATUS_IRIS4                          VCODEC_VPU_CPU_CS_SCIACMDARG0_IRIS4
typedef enum {
    HFI_CTRL_NOT_INIT                   = 0x0,
    HFI_CTRL_READY                      = 0x1,
    HFI_CTRL_ERROR_FATAL                = 0x2,
    HFI_CTRL_ERROR_UC_REGION_NOT_SET    = 0x4,
    HFI_CTRL_ERROR_HW_FENCE_QUEUE       = 0x8,
    HFI_CTRL_PC_READY                   = 0x100,
    HFI_CTRL_VCODEC_IDLE                = 0x40000000
} hfi_ctrl_status_type;

#define HFI_QTBL_INFO_IRIS4                          VCODEC_VPU_CPU_CS_SCIACMDARG1_IRIS4
typedef enum {
    HFI_QTBL_DISABLED    = 0x00,
    HFI_QTBL_ENABLED     = 0x01,
} hfi_qtbl_status_type;

#define HFI_QTBL_ADDR_IRIS4                          VCODEC_VPU_CPU_CS_SCIACMDARG2_IRIS4
#define HFI_MMAP_ADDR_IRIS4                          VCODEC_VPU_CPU_CS_SCIBCMDARG0_IRIS4
#define HFI_UC_REGION_ADDR_IRIS4                     VCODEC_VPU_CPU_CS_SCIBARG1_IRIS4
#define HFI_UC_REGION_SIZE_IRIS4                     VCODEC_VPU_CPU_CS_SCIBARG2_IRIS4
#define HFI_DEVICE_REGION_ADDR_IRIS4                 VCODEC_VPU_CPU_CS_VCICMDARG0_IRIS4
#define HFI_DEVICE_REGION_SIZE_IRIS4                 VCODEC_VPU_CPU_CS_VCICMDARG1_IRIS4
#define HFI_SFR_ADDR_IRIS4                           VCODEC_VPU_CPU_CS_SCIBCMD_IRIS4

/*
 * --------------------------------------------------------------------------
 * MODULE: VCODEC_IRIS_WRAPPER_TOP
 * --------------------------------------------------------------------------
 */
#define WRAPPER_BASE_OFFS_IRIS4                      0x000B0000
#define WRAPPER_APV_HW_VERSION_IRIS4                 (WRAPPER_BASE_OFFS_IRIS4 + 0x04)
#define WRAPPER_EFUSE_MONITOR_IRIS4                  (WRAPPER_BASE_OFFS_IRIS4 + 0x08)
#define WRAPPER_INTR_STATUS_IRIS4                    (WRAPPER_BASE_OFFS_IRIS4 + 0x0C)
#define WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS4         0x8
#define WRAPPER_INTR_STATUS_A2H_BMSK_IRIS4           0x4

#define WRAPPER_INTR_MASK_IRIS4                      (WRAPPER_BASE_OFFS_IRIS4 + 0x10)
#define WRAPPER_INTR_MASK_A2HWD_BMSK_IRIS4           0x8
#define WRAPPER_INTR_MASK_A2HCPU_BMSK_IRIS4          0x4

#define WRAPPER_DEBUG_BRIDGE_LPI_CONTROL_IRIS4        (WRAPPER_BASE_OFFS_IRIS4 + 0x54)
#define WRAPPER_DEBUG_BRIDGE_LPI_STATUS_IRIS4         (WRAPPER_BASE_OFFS_IRIS4 + 0x58)
#define WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS4        (WRAPPER_BASE_OFFS_IRIS4 + 0x5C)
#define WRAPPER_IRIS_CPU_NOC_LPI_STATUS_IRIS4         (WRAPPER_BASE_OFFS_IRIS4 + 0x60)
#define WRAPPER_IRIS_VCODEC_VPU_WRAPPER_SPARE_0_IRIS4 (WRAPPER_BASE_OFFS_IRIS4 + 0x78)
#define WRAPPER_CORE_POWER_STATUS_IRIS4               (WRAPPER_BASE_OFFS_IRIS4 + 0x80)
#define WRAPPER_CORE_POWER_CONTROL_IRIS4              (WRAPPER_BASE_OFFS_IRIS4 + 0x84)
#define WRAPPER_CORE_CLOCK_CONFIG_IRIS4               (WRAPPER_BASE_OFFS_IRIS4 + 0x88)

/*
 * --------------------------------------------------------------------------
 * MODULE: TZ_WRAPPER
 * --------------------------------------------------------------------------
 */
#define WRAPPER_TSW_BASE_OFFS_IRIS4                   0x000C0000
#define WRAPPER_TSW_CPU_STATUS_IRIS4                  (WRAPPER_TSW_BASE_OFFS_IRIS4 + 0x10)
#define WRAPPER_TSW_CTL_AXI_CLOCK_CONFIG_IRIS4        (WRAPPER_TSW_BASE_OFFS_IRIS4 + 0x14)
#define WRAPPER_TSW_QNS4PDXFIFO_RESET_IRIS4           (WRAPPER_TSW_BASE_OFFS_IRIS4 + 0x18)

/*
 * --------------------------------------------------------------------------
 * MODULE: AON_WRAPPER
 * --------------------------------------------------------------------------
 */
#define AON_BASE_OFFS_IRIS4                             0x000E0000
#define AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4           (AON_BASE_OFFS_IRIS4)
#define AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS4            (AON_BASE_OFFS_IRIS4 + 0x4)
#define AON_WRAPPER_MVP_NOC_ARCG_CONTROL_IRIS4          (AON_BASE_OFFS_IRIS4 + 0x10)
#define AON_WRAPPER_MVP_NOC_CORE_SW_RESET_IRIS4         (AON_BASE_OFFS_IRIS4 + 0x18)
#define AON_WRAPPER_MVP_NOC_CORE_CLK_CONTROL_IRIS4      (AON_BASE_OFFS_IRIS4 + 0x20)
#define AON_WRAPPER_SPARE_IRIS4                         (AON_BASE_OFFS_IRIS4 + 0x28)
#define AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS4 (AON_BASE_OFFS_IRIS4 + 0x2C)
#define AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_STATUS_IRIS4  (AON_BASE_OFFS_IRIS4 + 0x30)

/*
 * --------------------------------------------------------------------------
 * MODULE: IRIS_AON_MVP_NOC_RESET
 * --------------------------------------------------------------------------
 */
#define AON_MVP_NOC_RESET_BASE_OFFS_IRIS4              0x0001F000
#define AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS4            (AON_MVP_NOC_RESET_BASE_OFFS_IRIS4 + 0x0)
#define AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS4            (AON_MVP_NOC_RESET_BASE_OFFS_IRIS4 + 0x04)
#define AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS4        (AON_MVP_NOC_RESET_BASE_OFFS_IRIS4 + 0x08)
#define AON_WRAPPER_MVP_NOC_RESET_SPARE_IRIS4          (AON_MVP_NOC_RESET_BASE_OFFS_IRIS4 + 0x0C)

/*
 * --------------------------------------------------------------------------
 * MODULE: VCODEC_SS registers
 * --------------------------------------------------------------------------
 */
#define VCODEC_BASE_OFFS_IRIS4                       0x00000000
#define VCODEC_SS_IDLE_STATUSn_IRIS4                 (VCODEC_BASE_OFFS_IRIS4 + 0x70)

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

static int __interrupt_init_iris4(struct msm_vidc_core *core)
{
	u32 mask_val = 0;
	int rc = 0;

	/* All interrupts should be disabled initially 0x1F6 : Reset value */
	rc = __read_register(core, WRAPPER_INTR_MASK_IRIS4, &mask_val);
	if (rc)
		return rc;

	/* Write 0 to unmask CPU and WD interrupts */
	mask_val &= ~(WRAPPER_INTR_MASK_A2HWD_BMSK_IRIS4|
			WRAPPER_INTR_MASK_A2HCPU_BMSK_IRIS4);
	rc = __write_register(core, WRAPPER_INTR_MASK_IRIS4, mask_val);
	if (rc)
		return rc;

	return 0;
}

static int __raise_interrupt_iris4(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = __write_register(core, CPU_IC_SOFTINT_IRIS4, 1 << CPU_IC_SOFTINT_H2A_SHFT_IRIS4);
	if (rc)
		return rc;

	return 0;
}

static int __clear_interrupt_iris4(struct msm_vidc_core *core)
{
	u32 intr_status = 0, mask = 0;
	int rc = 0;

	rc = __read_register(core, WRAPPER_INTR_STATUS_IRIS4, &intr_status);
	if (rc)
		return rc;

	mask = (WRAPPER_INTR_STATUS_A2H_BMSK_IRIS4 |
		WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS4 |
		HFI_CTRL_VCODEC_IDLE);

	if (intr_status & mask) {
		core->intr_status |= intr_status;
		core->reg_count++;
		d_vpr_l("INTERRUPT: times: %d interrupt_status: %d\n",
			core->reg_count, intr_status);
	} else {
		core->spur_count++;
	}

	rc = __write_register(core, CPU_CS_A2HSOFTINTCLR_IRIS4, 1);
	if (rc)
		return rc;

	return 0;
}

static int __get_device_region_info(struct msm_vidc_core *core,
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

static int __program_bootup_registers_iris4(struct msm_vidc_core *core)
{
	u32 min_dev_reg_addr = 0, dev_reg_size = 0;
	struct device *dev = NULL;
	u32 value;
	int rc = 0;

	dev = &core->pdev->dev;

	value = (u32)core->iface_q_table.align_device_addr;
	rc = __write_register(core, HFI_UC_REGION_ADDR_IRIS4, value);
	if (rc)
		return rc;

	value = SHARED_QSIZE;
	rc = __write_register(core, HFI_UC_REGION_SIZE_IRIS4, value);
	if (rc)
		return rc;

	value = (u32)core->iface_q_table.align_device_addr;
	rc = __write_register(core, HFI_QTBL_ADDR_IRIS4, value);
	if (rc)
		return rc;

	rc = __write_register(core, HFI_QTBL_INFO_IRIS4, HFI_QTBL_ENABLED);
	if (rc)
		return rc;

	if (core->mmap_buf.align_device_addr) {
		value = (u32)core->mmap_buf.align_device_addr;
		rc = __write_register(core, HFI_MMAP_ADDR_IRIS4, value);
		if (rc)
			return rc;
	} else {
		d_vpr_e("%s: skip mmap buffer programming\n", __func__);
		/* ignore the error for now for backward compatibility */
		/* return -EINVAL; */
	}

	rc = __get_device_region_info(core, &min_dev_reg_addr, &dev_reg_size);
	if (rc)
		return rc;

	if (min_dev_reg_addr && dev_reg_size) {
		rc = __write_register(core, HFI_DEVICE_REGION_ADDR_IRIS4, min_dev_reg_addr);
		if (rc)
			return rc;

		rc = __write_register(core, HFI_DEVICE_REGION_SIZE_IRIS4, dev_reg_size);
		if (rc)
			return rc;
	} else {
		d_vpr_h("%s: skip device region programming\n", __func__);
		/* ignore the error for now for backward compatibility */
		/* return -EINVAL; */
	}

	if (core->sfr.align_device_addr) {
		value = (u32)core->sfr.align_device_addr + VIDEO_ARCH_LX;
		rc = __write_register(core, HFI_SFR_ADDR_IRIS4, value);
		if (rc)
			return rc;
	}

	/* Based on below register programming, firmware WA for canoe-v2 would be enabled */
	if (of_device_is_compatible(dev->of_node, "qcom,canoe-vidc-v2")) {
		rc = __write_register(core, WRAPPER_IRIS_VCODEC_VPU_WRAPPER_SPARE_0_IRIS4, 0x1);
		if (rc)
			return rc;
	} else {
		rc = __write_register(core, WRAPPER_IRIS_VCODEC_VPU_WRAPPER_SPARE_0_IRIS4, 0x0);
		if (rc)
			return rc;
	}

	return 0;
}

static int __boot_firmware_iris4(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 ctrl_init_val = 0, ctrl_status = 0, count = 0, max_tries = 1000;

	rc = __program_bootup_registers_iris4(core);
	if (rc)
		return rc;

	ctrl_init_val = BIT(0);

	rc = __write_register(core, HFI_CTRL_INIT_IRIS4, ctrl_init_val);
	if (rc)
		return rc;

	while (count < max_tries) {
		rc = __read_register(core, HFI_CTRL_STATUS_IRIS4, &ctrl_status);
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
		//d_vpr_e(FMT_STRING_BOOT_FIRMWARE_ERROR, ctrl_status, ctrl_init_val);
		return -ETIME;
	}

	/* Enable interrupt before sending commands to venus */
	rc = __write_register(core, CPU_CS_H2XSOFTINTEN_IRIS4, 0x1);
	if (rc)
		return rc;

	rc = __write_register(core, CPU_CS_X2RPMh_IRIS4, 0x0);
	if (rc)
		return rc;

	return rc;
}

static bool is_iris4_hw_power_collapsed(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 value = 0, pwr_status = 0;

	rc = __read_register(core, WRAPPER_CORE_POWER_STATUS_IRIS4, &value);
	if (rc)
		return false;

	/* if BIT(1) is 1 then video hw power is on else off */
	pwr_status = value & BIT(1);
	return pwr_status ? false : true;
}

static bool is_hw_enabled(struct msm_vidc_core *core, const char *name)
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

static bool is_vpu_iris4_1p(struct msm_vidc_core *core)
{
	return !!(core->platform->data.vpu_ver == VPU_VERSION_IRIS4_1P);
}

static int __power_off_iris4_apv(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 value = 0;
	u32 count = 0;

	if (!is_hw_enabled(core, "apv"))
		return 0;

	rc = __read_register(core, WRAPPER_EFUSE_MONITOR_IRIS4, &value);
	if (rc)
		goto fail_read_efuse;

	if (is_vpu_iris4_1p(core) || (value & BIT(27)))
		return 0;

	/*
	 * check to make sure core clock branch enabled else
	 * we cannot read apv top idle register
	 * BIT(1) is set implies APV system clock is disabled
	 */
	rc = __read_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS4, &value);
	if (rc)
		return rc;

	if ((value & BIT(1))) {
		d_vpr_e("%s: core clock config not enabled, enabling it to read apv registers\n",
			__func__);
		rc = __write_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS4, 0);
		if (rc)
			return rc;
	}

	/*
	 * add APV TOP IDLE STATUS check before collapsing APV per HPG update
	 * poll for APV TOP IDLE STATUS -> HPG 3.4.4.2
	 */
	//rc = __read_register_with_poll_timeout(core, WRAPPER_IRIS_APV_TOP_IDLE_STATUS_IRIS4,
	//		0x11F, 0x11F, 2000, 20000);
	//if (rc)
	//	d_vpr_e("%s: APV_TOP_IDLE_STATUS (%d) is not idle (%#x)\n",
	//		__func__, value);

	/* set MNoC to low power, set PD_NOC_QREQ (bit 0) */
	rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4,
					0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS4, &value);
	if (rc)
		return rc;

	while ((!(value & BIT(0))) && (value & BIT(2) || value & BIT(1))) {
		rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4,
					     0x0, BIT(0));
		if (rc)
			return rc;

		usleep_range(10, 20);

		rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4,
					     0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS4, &value);
		if (rc)
			return rc;

		++count;
		if (count >= 1000) {
			d_vpr_e("%s: AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4 failed\n", __func__);
			break;
		}
	}

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS4,
					       0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4 failed1\n", __func__);

	rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4,
					0x0, BIT(0));
	if (rc)
		return rc;

	rc = __write_register(core,  AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS4 , 0x080200);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS4,
					       0xffffffff, 0x080200, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS4 failed\n", __func__);

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS4, 0x080200);
	if (rc)
		return rc;

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS4, 0);
	if (rc)
		return rc;

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS4, 0);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS4,
					       0xffffffff, 0x0, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS4 failed\n", __func__);

	/*
	 * Reset both sides of 2 ahb2ahb_bridges (TZ and non-TZ)
	 * do we need to check status register here?
	 */
	rc = __write_register(core, VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_IRIS4, 0x3);
	if (rc)
		return rc;
	rc = __write_register(core, VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_IRIS4, 0x2);
	if (rc)
		return rc;
	rc = __write_register(core, VCODEC_VPU_CPU_CS_APV_BRIDGE_SYNC_RESET_IRIS4, 0x0);
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

static int __power_off_iris4_hardware(struct msm_vidc_core *core)
{
	int rc = 0, i = 0;
	u32 value = 0;
	bool pwr_collapsed = false;
	u32 count = 0;

	/*
	 * Incase hw power control is enabled, for any error case
	 * CPU WD, video hw unresponsive cases, NOC error case etc,
	 * execute NOC reset sequence before disabling power. If there
	 * is no CPU WD and hw power control is enabled, fw is expected
	 * to power collapse video hw always.
	 */
	if (is_core_sub_state(core, CORE_SUBSTATE_FW_PWR_CTRL)) {
		pwr_collapsed = is_iris4_hw_power_collapsed(core);
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
	rc = __read_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS4, &value);
	if (rc)
		return rc;

	if ((value & BIT(0))) {
		d_vpr_e("%s: core clock config not enabled, enabling it to read vcodec registers\n",
			__func__);
		rc = __write_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS4, 0);
		if (rc)
			return rc;
	}

	/*
	 * add MNoC idle check before collapsing MVS0 per HPG update
	 * poll for VCODEC_SS_IDLE_STATUS -> HPG 3.4.4
	 */
	rc = __read_register_with_poll_timeout(core, VCODEC_SS_IDLE_STATUSn_IRIS4,
			0x7103, 0x7103, 2000, 20000);
	if (rc)
		d_vpr_e("%s: VCODEC_SS_IDLE_STATUS (%d) is not idle (%#x)\n",
			__func__, i, value);

	/* set MNoC to low power, set PD_NOC_QREQ (bit 0) */
	rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4,
					0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS4, &value);
	if (rc)
		return rc;

	while ((!(value & BIT(0))) && (value & BIT(2) || value & BIT(1))) {
		rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4,
					     0x0, BIT(0));
		if (rc)
			return rc;

		usleep_range(10, 20);

		rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4,
					     0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS4, &value);
		if (rc)
			return rc;

		++count;
		if (count >= 1000) {
			d_vpr_e("%s: AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4 failed\n", __func__);
			break;
		}
	}

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_LPI_STATUS_IRIS4,
					       0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4 failed1\n", __func__);

	rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL_IRIS4,
					0x0, BIT(0));
	if (rc)
		return rc;

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS4, 0x070103);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS4,
					       0xffffffff, 0x070103, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS4 failed1\n", __func__);

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS4 , 0x070103);
	if (rc)
		return rc;

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_SYNCRST_IRIS4 , 0x0);
	if (rc)
		return rc;

	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_REQ_IRIS4, 0x0);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS4,
					       0xffffffff, 0x0, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_NOC_RESET_ACK_IRIS4\n", __func__);

	/*
	 * Reset both sides of 2 ahb2ahb_bridges (TSW and non-TSW)
	 */
	rc = __write_register(core, CPU_CS_AHB_BRIDGE_SYNC_RESET_IRIS4, 0x3);
	if (rc)
		return rc;
	rc = __write_register(core, CPU_CS_AHB_BRIDGE_SYNC_RESET_IRIS4, 0x2);
	if (rc)
		return rc;
	rc = __write_register(core, CPU_CS_AHB_BRIDGE_SYNC_RESET_IRIS4, 0x0);
	if (rc)
		return rc;

disable_power:
	/* power down process */

	rc = __read_register(core, WRAPPER_EFUSE_MONITOR_IRIS4, &value);
	if (rc)
		return rc;

	/* VCODEC_VIDEO_CC_MVS0_VPP1_GDSCR --> "vpp1" - To be named as per dtsi*/
	if (is_hw_enabled(core, "vpp1") && (!is_vpu_iris4_1p(core) || !(value & BIT(28)))) {
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
	if (is_hw_enabled(core, "vpp0") && !(value & BIT(29))) {
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

	return rc;
}

static int __power_off_iris4_controller(struct msm_vidc_core *core)
{
	int rc = 0;
	int value = 0;
	u32 count = 0;

	/*
	 * mask fal10_veto QLPAC error since fal10_veto can go 1
	 * when pwwait == 0 and clamped to 0 -> HPG 3.7.4
	 */
	rc = __write_register(core, CPU_CS_X2RPMh_IRIS4, 0x3);
	if (rc)
		return rc;

	/* Set Iris CPU NoC to Low power */
	rc = __write_register_masked(core, WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS4,
			0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register(core, WRAPPER_IRIS_CPU_NOC_LPI_STATUS_IRIS4, &value);
	if (rc)
		return rc;

	while ((!(value & BIT(0))) && (value & BIT(1))) {
		rc = __write_register_masked(core, WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS4,
					     0x0, BIT(0));
		if (rc)
			return rc;

		usleep_range(10, 20);

		rc = __write_register_masked(core, WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS4,
					     0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, WRAPPER_IRIS_CPU_NOC_LPI_STATUS_IRIS4, &value);
		if (rc)
			return rc;

		++count;
		if (count >= 1000) {
			d_vpr_e("%s: WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS4 failed\n", __func__);
			break;
		}
	}

	rc = __read_register_with_poll_timeout(core, WRAPPER_IRIS_CPU_NOC_LPI_STATUS_IRIS4,
			0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_e("%s: WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS4 failed\n", __func__);

	rc = __write_register_masked(core, WRAPPER_IRIS_CPU_NOC_LPI_CONTROL_IRIS4,
				     0x0, BIT(0));
	if (rc)
		return rc;

	rc = __write_register_masked(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS4,
				     0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_STATUS_IRIS4, &value);
	if (rc)
		return rc;

	while ((!(value & BIT(0))) && (value & BIT(1) || value & BIT(2))) {
		rc = __write_register_masked(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS4,
					     0x0, BIT(0));
		if (rc)
			return rc;

		usleep_range(10, 20);

		rc = __write_register_masked(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS4,
					     0x1, BIT(0));
		if (rc)
			return rc;

		rc = __read_register(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_STATUS_IRIS4, &value);
		if (rc)
			return rc;

		++count;
		if (count >= 1000) {
			d_vpr_e("%s: AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS4 failed\n", __func__);
			break;
		}
	}

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_STATUS_IRIS4,
					       0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_e("%s: AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS4 failed\n", __func__);

	rc = __write_register_masked(core, AON_WRAPPER_MVP_VIDEO_CTL_NOC_LPI_CONTROL_IRIS4,
				     0x0, BIT(0));
	if (rc)
		return rc;

	/* Debug bridge LPI release */
	rc = __write_register(core, WRAPPER_DEBUG_BRIDGE_LPI_CONTROL_IRIS4, 0x0);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, WRAPPER_DEBUG_BRIDGE_LPI_STATUS_IRIS4,
					       0xffffffff, 0x0, 200, 2000);
	if (rc)
		d_vpr_e("%s: debug bridge release failed\n", __func__);

	/* power down process */
	rc = call_res_op(core, gdsc_off, core, "iris-ctl");
	if (rc) {
		d_vpr_e("%s: disable regulator iris-ctl failed\n", __func__);
		rc = 0;
	}

	rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_ARCG_CONTROL_IRIS4,
				     0x1, BIT(0));
	if (rc)
		return rc;

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
		d_vpr_e("%s: disable unprepare video_cc_mvs0c_freerun_clk failed\n", __func__);
		rc = 0;
	}

	rc = call_res_op(core, clk_disable, core, "video_cc_mvs0_freerun_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare video_cc_mvs0_freerun_clk failed\n", __func__);
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

	return rc;
}

static int __power_off_iris4(struct msm_vidc_core *core)
{
	int rc = 0;

	if (!is_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE))
		return 0;

	/**
	 * Reset video_cc_mvs0_clk_src value to resolve MMRM high video
	 * clock projection issue.
	 */
	rc = call_res_op(core, set_clks, core, get_min_clock_index(core));
	if (rc)
		d_vpr_e("%s: resetting core clocks failed\n", __func__);

	rc = call_res_op(core, gdsc_sw_ctrl, core);
	if (rc)
		d_vpr_e("%s: gdsc_sw_ctrl failed\n", __func__);

	if (__power_off_iris4_apv(core))
		d_vpr_e("%s: failed to power off apv\n", __func__);

	if (__power_off_iris4_hardware(core))
		d_vpr_e("%s: failed to power off hardware\n", __func__);

	if (__power_off_iris4_controller(core))
		d_vpr_e("%s: failed to power off controller\n", __func__);

	rc = call_res_op(core, set_bw, core, 0, 0);
	if (rc)
		d_vpr_e("%s: failed to unvote buses\n", __func__);

	if (!call_venus_op(core, watchdog, core, core->intr_status))
		disable_irq_nosync(core->resource->irq);

	msm_vidc_change_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE, 0, __func__);

	return rc;
}

static int __power_on_iris4_controller(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = call_res_op(core, gdsc_on, core, "iris-ctl");
	if (rc)
		goto fail_regulator;

	rc = call_res_op(core, clk_enable, core, "gcc_video_axi1_clk");
	if (rc)
		goto fail_clk_axi;

	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0c_freerun_clk");
	if (rc)
		goto fail_clk_freerun;

	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0c_clk");
	if (rc)
		goto fail_clk_controller;

	return 0;

fail_clk_controller:
	call_res_op(core, clk_disable, core, "video_cc_mvs0c_freerun_clk");
fail_clk_freerun:
	call_res_op(core, clk_disable, core, "gcc_video_axi1_clk");
fail_clk_axi:
	call_res_op(core, gdsc_off, core, "iris-ctl");
fail_regulator:
	return rc;
}

static int __power_on_iris4_hardware(struct msm_vidc_core *core)
{
	int rc = 0;
	int value = 0;

	rc = call_res_op(core, gdsc_on, core, "vcodec");
	if (rc)
		goto fail_regulator;

	/* video controller and hardware powered on successfully */
	rc = msm_vidc_change_core_sub_state(core, 0, CORE_SUBSTATE_POWER_ENABLE, __func__);
	if (rc)
		goto fail_power_on_substate;

	rc = __read_register(core, WRAPPER_EFUSE_MONITOR_IRIS4, &value);
	if (rc)
		goto fail_read_efuse;

	/* VIDEO_CC_MVS0_VPP0_GDSCR --> vpp0 */
	if (is_hw_enabled(core, "vpp0") && !(value & BIT(29))) {
		rc = call_res_op(core, gdsc_on, core, "vpp0");
		if (rc)
			goto fail_regulator_vpp0;
	}

	/* VIDEO_CC_MVS0_VPP1_GDSCR --> vpp1 */
	if (is_hw_enabled(core, "vpp1") && (!is_vpu_iris4_1p(core) || !(value & BIT(28)))) {
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

	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0_freerun_clk");
	if (rc)
		goto fail_clk_freerun;

	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0_clk");
	if (rc)
		goto fail_clk_controller;

	rc = call_res_op(core, clk_enable, core, "video_cc_mvs0b_clk");
	if (rc)
		goto fail_clk_bse_controller;

	/* VIDEO_CC_MVS0_VPP0_GDSCR --> vpp0 */
	if (is_hw_enabled(core, "vpp0") && !(value & BIT(29))) {
		/*VIDEO_CC_MVS0_VPP0_CBCR --> video_cc_mvs0_vpp0_clk */
		rc = call_res_op(core, clk_enable, core, "video_cc_mvs0_vpp0_clk");
		if (rc)
			goto fail_clk_vpp0;
	}

	/* VIDEO_CC_MVS0_VPP1_GDSCR --> vpp1 */
	if (is_hw_enabled(core, "vpp1") && (!is_vpu_iris4_1p(core) || !(value & BIT(28)))) {
		/* VIDEO_CC_MVS0_VPP1_CBCR --> video_cc_mvs0_vpp1_clk */
		rc = call_res_op(core, clk_enable, core, "video_cc_mvs0_vpp1_clk");
		if (rc)
			goto fail_clk_vpp1;
	}

	return 0;

fail_clk_vpp1:
	if (is_hw_enabled(core, "vpp0") && !(value & BIT(29)))
		call_res_op(core, clk_disable, core, "video_cc_mvs0_vpp0_clk");
fail_clk_vpp0:
	call_res_op(core, clk_disable, core, "video_cc_mvs0b_clk");
fail_clk_bse_controller:
	call_res_op(core, clk_disable, core, "video_cc_mvs0_clk");
fail_clk_controller:
	call_res_op(core, clk_disable, core, "video_cc_mvs0_freerun_clk");
fail_clk_freerun:
	call_res_op(core, clk_disable, core, "gcc_video_axi0_clk");
fail_clk_axi:
fail_sw_ctrl:
	if (is_hw_enabled(core, "vpp1") && (!is_vpu_iris4_1p(core) || !(value & BIT(28))))
		call_res_op(core, gdsc_off, core, "vpp1");
fail_regulator_vpp1:
	if (is_hw_enabled(core, "vpp0") && !(value & BIT(29)))
		call_res_op(core, gdsc_off, core, "vpp0");
fail_regulator_vpp0:
fail_read_efuse:
fail_power_on_substate:
	call_res_op(core, gdsc_off, core, "vcodec");
fail_regulator:
	return rc;
}

static int __power_on_iris4_apv(struct msm_vidc_core *core)
{
	int rc = 0;
	int value = 0;

	if (!is_hw_enabled(core, "apv"))
		return 0;

	rc = __read_register(core, WRAPPER_EFUSE_MONITOR_IRIS4, &value);
	if (rc)
		goto fail_read_efuse;

	if (is_vpu_iris4_1p(core) || (value & BIT(27)))
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

static int __power_on_iris4(struct msm_vidc_core *core)
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

	rc = __power_on_iris4_controller(core);
	if (rc) {
		d_vpr_e("%s: failed to power on iris4 controller\n", __func__);
		goto fail_power_on_controller;
	}

	rc = __power_on_iris4_hardware(core);
	if (rc) {
		d_vpr_e("%s: failed to power on iris4 hardware\n", __func__);
		goto fail_power_on_hardware;
	}

	rc = __power_on_iris4_apv(core);
	if (rc) {
		d_vpr_e("%s: failed to power on iris4 apv\n", __func__);
		goto fail_power_on_apv;
	}

	idx = core->power.clk_freq_idx ? core->power.clk_freq_idx : 0;
	rc = call_res_op(core, set_clks, core, idx);
	if (rc) {
		d_vpr_e("%s: failed to scale clocks\n", __func__);
		rc = 0;
	}

	__set_registers(core);

	__interrupt_init_iris4(core);
	core->intr_status = 0;
	enable_irq(core->resource->irq);

	return rc;

fail_power_on_apv:
	__power_off_iris4_hardware(core);
fail_power_on_hardware:
	__power_off_iris4_controller(core);
fail_power_on_controller:
	call_res_op(core, set_bw, core, 0, 0);
fail_vote_buses:
	msm_vidc_change_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE, 0, __func__);

	return rc;
}

static int __prepare_pc_iris4(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 wfi_status = 0, idle_status = 0, pc_ready = 0;
	u32 ctrl_status = 0;

	rc = __read_register(core, HFI_CTRL_STATUS_IRIS4, &ctrl_status);
	if (rc)
		return rc;

	pc_ready = ctrl_status & HFI_CTRL_PC_READY;
	idle_status = ctrl_status & BIT(30);

	if (pc_ready) {
		d_vpr_h("Already in pc_ready state\n");
		return 0;
	}
	rc = __read_register(core, WRAPPER_TSW_CPU_STATUS_IRIS4, &wfi_status);
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

	rc = __read_register_with_poll_timeout(core, HFI_CTRL_STATUS_IRIS4,
			HFI_CTRL_PC_READY, HFI_CTRL_PC_READY, 250, 2500);
	if (rc) {
		d_vpr_e("%s: Skip PC. Ctrl status not set\n", __func__);
		goto skip_power_off;
	}

	rc = __read_register_with_poll_timeout(core, WRAPPER_TSW_CPU_STATUS_IRIS4,
			BIT(0), 0x1, 250, 2500);
	if (rc) {
		d_vpr_e("%s: Skip PC. Wfi status not set\n", __func__);
		goto skip_power_off;
	}
	return rc;

skip_power_off:
	rc = __read_register(core, HFI_CTRL_STATUS_IRIS4, &ctrl_status);
	if (rc)
		return rc;
	rc = __read_register(core, WRAPPER_TSW_CPU_STATUS_IRIS4, &wfi_status);
	if (rc)
		return rc;
	wfi_status &= BIT(0);
	d_vpr_e("Skip PC, wfi=%#x, idle=%#x, pcr=%#x, ctrl=%#x)\n",
		wfi_status, idle_status, pc_ready, ctrl_status);
	return -EAGAIN;
}

static int __watchdog_iris4(struct msm_vidc_core *core, u32 intr_status)
{
	int rc = 0;

	if (intr_status & WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS4) {
		d_vpr_e("%s: received watchdog interrupt\n", __func__);
		rc = 1;
		MSM_VIDC_FATAL(true);
	}

	return rc;
}

static int __noc_error_info_iris4(struct msm_vidc_core *core)
{
	u32 value;
	int rc = 0;

	if (is_iris4_hw_power_collapsed(core)) {
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

	MSM_VIDC_FATAL(true);
	return rc;
}

static int __hw_ctrl_gdsc_iris4(struct msm_vidc_core *core)
{
	return call_res_op(core, gdsc_hw_ctrl, core);
}

static int __sw_ctrl_gdsc_iris4(struct msm_vidc_core *core)
{
	return call_res_op(core, gdsc_sw_ctrl, core);
}

static int msm_vidc_decide_work_mode_iris4(struct msm_vidc_inst *inst)
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

static int msm_vidc_decide_work_route_iris4(struct msm_vidc_inst *inst)
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

		/*TODO Pipe=1 for legacy CBR*/
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

static int msm_vidc_decide_quality_mode_iris4(struct msm_vidc_inst *inst)
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
	max_hq_mbpf = core->capabilities[MAX_MBPF_HQ].value;;
	max_hq_mbps = core->capabilities[MAX_MBPS_HQ].value;;

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

static int msm_vidc_update_scaling_iris4(struct msm_vidc_inst *inst,
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

static int msm_vidc_decide_scaling_iris4(struct msm_vidc_inst *inst)
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

	if (msm_vidc_update_scaling_iris4(inst, aspect_ratio_w, aspect_ratio_h))
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

int msm_vidc_adjust_bitrate_boost_iris4(void *instance, struct v4l2_ctrl *ctrl)
{
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	s64 rc_type = -1;
	u32 width, height, frame_rate;
	struct v4l2_format *f;
	u32 max_bitrate = 0, bitrate = 0;

	adjusted_value = ctrl ? ctrl->val :
		inst->capabilities[BITRATE_BOOST].value;

	if (inst->bufq[OUTPUT_PORT].vb2q->streaming)
		return 0;

	if (msm_vidc_get_parent_value(inst, BITRATE_BOOST,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	/*
	 * Bitrate Boost are supported only for VBR rc type.
	 * Hence, do not adjust or set to firmware for non VBR rc's
	 */
	if (rc_type != HFI_RC_VBR_CFR) {
		adjusted_value = 0;
		goto adjust;
	}

	frame_rate = inst->capabilities[FRAME_RATE].value >> 16;
	f = &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	/*
	 * honor client set bitrate boost
	 * if client did not set, keep max bitrate boost upto 4k@60fps
	 * and remove bitrate boost after 4k@60fps
	 */
	if (inst->capabilities[BITRATE_BOOST].flags & CAP_FLAG_CLIENT_SET) {
		/* accept client set bitrate boost value as is */
	} else {
		if (res_is_less_than_or_equal_to(width, height, 4096, 2176) &&
			frame_rate <= 60)
			adjusted_value = MAX_BITRATE_BOOST;
		else
			adjusted_value = 0;
	}

	max_bitrate = msm_vidc_get_max_bitrate(inst);
	bitrate = inst->capabilities[BIT_RATE].value;
	if (adjusted_value) {
		if ((bitrate + bitrate / (100 / adjusted_value)) > max_bitrate) {
			i_vpr_h(inst,
				"%s: bitrate %d is beyond max bitrate %d, remove bitrate boost\n",
				__func__, max_bitrate, bitrate);
			adjusted_value = 0;
		}
	}
adjust:
	msm_vidc_update_cap_value(inst, BITRATE_BOOST, adjusted_value, __func__);

	return 0;
}

static struct msm_vidc_venus_ops iris4_ops = {
	.raise_interrupt = __raise_interrupt_iris4,
	.clear_interrupt = __clear_interrupt_iris4,
	.boot_firmware = __boot_firmware_iris4,
	.power_on = __power_on_iris4,
	.power_off = __power_off_iris4,
	.prepare_pc = __prepare_pc_iris4,
	.watchdog = __watchdog_iris4,
	.noc_error_info = __noc_error_info_iris4,
	.hw_ctrl_gdsc = __hw_ctrl_gdsc_iris4,
	.sw_ctrl_gdsc = __sw_ctrl_gdsc_iris4,
	.scm_mem_protect = msm_vidc_mem_protect_video_regions_v2,
};

static struct msm_vidc_session_ops msm_session_ops = {
	.buffer_size = msm_buffer_size_iris4,
	.min_count = msm_buffer_min_count_iris4,
	.extra_count = msm_buffer_extra_count_iris4,
	.ring_buf_count = msm_vidc_ring_buf_count_iris4,
	.scale_clocks = msm_vidc_scale_clocks_iris4,
	.calc_bw = msm_vidc_calc_bw_iris4,
	.decide_work_route = msm_vidc_decide_work_route_iris4,
	.decide_work_mode = msm_vidc_decide_work_mode_iris4,
	.decide_quality_mode = msm_vidc_decide_quality_mode_iris4,
	.decide_scaling = msm_vidc_decide_scaling_iris4,
};

int msm_vidc_init_iris4(struct msm_vidc_core *core)
{
	d_vpr_h("%s()\n", __func__);
	core->venus_ops = &iris4_ops;
	core->session_ops = &msm_session_ops;

	return 0;
}
