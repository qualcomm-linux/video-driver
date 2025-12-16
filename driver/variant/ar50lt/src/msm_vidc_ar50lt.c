/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/delay.h>
#include <linux/clk.h>

#include "msm_vidc_ar50lt.h"
#include "msm_vidc_buffer_ar50lt.h"
#include "msm_vidc_power_ar50lt.h"
#include "msm_vidc_memory.h"
#include "msm_vidc_driver.h"
#include "venus_hfi.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_core.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_control.h"
#include "resources.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_buffer.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_variant.h"
#include "msm_vidc_platform.h"


#define VIDEO_ARCH_LX                       1
#define VIDC_CPU_BASE_OFFS_AR50_LT          0x000A0000
#define VIDEO_GCC_BASE_OFFS_AR50_LT         0x00000000
#define VIDEO_CC_BASE_OFFS_AR50_LT          0x00100000

#define VIDC_CPU_CS_BASE_OFFS_AR50_LT       (VIDC_CPU_BASE_OFFS_AR50_LT)
#define VIDC_CPU_IC_BASE_OFFS_AR50_LT       (VIDC_CPU_BASE_OFFS_AR50_LT)

#define VIDC_CPU_CS_A2HSOFTINTCLR_AR50_LT   (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x1C)
#define VIDC_CPU_CS_VMIMSG_AR50_LT          (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x34)
#define VIDC_CPU_CS_VMIMSGAG0_AR50_LT       (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x38)
#define VIDC_CPU_CS_VMIMSGAG1_AR50_LT       (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x3C)
#define VIDC_CPU_CS_VMIMSGAG2_AR50_LT       (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x40)
#define VIDC_CPU_CS_VMIMSGAG3_AR50_LT       (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x44)
#define VIDC_CPU_CS_SCIACMD_AR50_LT         (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x48)

/* HFI_CTRL_STATUS */
#define VIDC_CPU_CS_SCIACMDARG0_AR50_LT     (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x4C)
#define VIDC_CPU_CS_SCIACMDARG0_BMSK_AR50_LT                        0xff
#define VIDC_CPU_CS_SCIACMDARG0_SHFT_AR50_LT                        0x0
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_BMSK_AR50_LT  0xfe
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_SHFT_AR50_LT  0x1
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_STATUS_BMSK_AR50_LT   0x1
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_STATUS_SHFT_AR50_LT   0x0
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY_AR50_LT           0x100
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK_AR50_LT 0x40000000

/* HFI_QTBL_INFO */
#define VIDC_CPU_CS_SCIACMDARG1_AR50_LT     (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x50)

/* HFI_QTBL_ADDR */
#define VIDC_CPU_CS_SCIACMDARG2_AR50_LT     (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x54)

/* HFI_VERSION_INFO */
#define VIDC_CPU_CS_SCIACMDARG3_AR50_LT     (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x58)

/* VIDC_SFR_ADDR */
#define VIDC_CPU_CS_SCIBCMD_AR50_LT         (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x5C)

/* VIDC_MMAP_ADDR */
#define VIDC_CPU_CS_SCIBCMDARG0_AR50_LT     (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x60)

/* VIDC_UC_REGION_ADDR */
#define VIDC_CPU_CS_SCIBARG1_AR50_LT        (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x64)

/* VIDC_UC_REGION_ADDR */
#define VIDC_CPU_CS_SCIBARG2_AR50_LT        (VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x68)

#define VIDC_CPU_IC_SOFTINT_EN_AR50_LT      (VIDC_CPU_IC_BASE_OFFS_AR50_LT + 0x148)
#define VIDC_CPU_IC_SOFTINT_AR50_LT         (VIDC_CPU_IC_BASE_OFFS_AR50_LT + 0x150)
#define VIDC_CPU_IC_SOFTINT_H2A_BMSK_AR50_LT       0x8000
#define VIDC_CPU_IC_SOFTINT_H2A_SHFT_AR50_LT       0x1

/*
 * --------------------------------------------------------------------------
 * MODULE: vidc_wrapper
 * --------------------------------------------------------------------------
 */
#define VIDC_WRAPPER_BASE_OFFS_AR50_LT      0x000B0000

#define VIDC_WRAPPER_HW_VERSION_AR50_LT    (VIDC_WRAPPER_BASE_OFFS_AR50_LT + 0x00)
#define VIDC_WRAPPER_HW_VERSION_MAJOR_VERSION_MASK_AR50_LT    0x78000000
#define VIDC_WRAPPER_HW_VERSION_MAJOR_VERSION_SHIFT_AR50_LT   28
#define VIDC_WRAPPER_HW_VERSION_MINOR_VERSION_MASK_AR50_LT    0xFFF0000
#define VIDC_WRAPPER_HW_VERSION_MINOR_VERSION_SHIFT_AR50_LT   16
#define VIDC_WRAPPER_HW_VERSION_STEP_VERSION_MASK_AR50_LT     0xFFFF

#define VIDC_WRAPPER_CLOCK_CONFIG_AR50_LT	(VIDC_WRAPPER_BASE_OFFS_AR50_LT + 0x04)

#define VIDC_WRAPPER_INTR_STATUS_AR50_LT    (VIDC_WRAPPER_BASE_OFFS_AR50_LT + 0x0C)
#define VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK_AR50_LT 0x10
#define VIDC_WRAPPER_INTR_STATUS_A2HWD_SHFT_AR50_LT 0x4
#define VIDC_WRAPPER_INTR_STATUS_A2H_BMSK_AR50_LT   0x4
#define VIDC_WRAPPER_INTR_STATUS_A2H_SHFT_AR50_LT   0x2

#define VIDC_WRAPPER_INTR_MASK_AR50_LT      (VIDC_WRAPPER_BASE_OFFS_AR50_LT + 0x10)
#define VIDC_WRAPPER_INTR_MASK_A2HWD_BMSK_AR50_LT   0x10
#define VIDC_WRAPPER_INTR_MASK_A2HWD_SHFT_AR50_LT   0x4
#define VIDC_WRAPPER_INTR_MASK_A2HVCODEC_BMSK_AR50_LT  0x8
#define VIDC_WRAPPER_INTR_MASK_A2HCPU_BMSK_AR50_LT  0x4
#define VIDC_WRAPPER_INTR_MASK_A2HCPU_SHFT_AR50_LT  0x2

#define VIDC_WRAPPER_INTR_CLEAR_A2HWD_BMSK_AR50_LT  0x10
#define VIDC_WRAPPER_INTR_CLEAR_A2HWD_SHFT_AR50_LT  0x4
#define VIDC_WRAPPER_INTR_CLEAR_A2H_BMSK_AR50_LT    0x4
#define VIDC_WRAPPER_INTR_CLEAR_A2H_SHFT_AR50_LT    0x2

/*
 * --------------------------------------------------------------------------
 * MODULE: tz_wrapper
 * --------------------------------------------------------------------------
 */
#define VIDC_WRAPPER_TZ_BASE_OFFS           0x000C0000
#define VIDC_WRAPPER_TZ_CPU_CLOCK_CONFIG    (VIDC_WRAPPER_TZ_BASE_OFFS)
#define VIDC_WRAPPER_TZ_CPU_STATUS          (VIDC_WRAPPER_TZ_BASE_OFFS + 0x10)

#define VIDC_CTRL_INIT_AR50_LT              VIDC_CPU_CS_SCIACMD_AR50_LT

#define VIDC_CTRL_STATUS_AR50_LT            VIDC_CPU_CS_SCIACMDARG0_AR50_LT
#define VIDC_CTRL_ERROR_STATUS__M_AR50_LT \
		VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_BMSK_AR50_LT
#define VIDC_CTRL_INIT_IDLE_MSG_BMSK_AR50_LT \
		VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK_AR50_LT
#define VIDC_CTRL_STATUS_PC_READY_AR50_LT \
		VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY_AR50_LT

#define VIDC_QTBL_INFO_AR50_LT          VIDC_CPU_CS_SCIACMDARG1_AR50_LT
#define VIDC_QTBL_ADDR_AR50_LT          VIDC_CPU_CS_SCIACMDARG2_AR50_LT
#define VIDC_VERSION_INFO_AR50_LT       VIDC_CPU_CS_SCIACMDARG3_AR50_LT

#define VIDC_SFR_ADDR_AR50_LT           VIDC_CPU_CS_SCIBCMD_AR50_LT
#define VIDC_MMAP_ADDR_AR50_LT          VIDC_CPU_CS_SCIBCMDARG0_AR50_LT
#define VIDC_UC_REGION_ADDR_AR50_LT     VIDC_CPU_CS_SCIBARG1_AR50_LT
#define VIDC_UC_REGION_SIZE_AR50_LT     VIDC_CPU_CS_SCIBARG2_AR50_LT

static int __setup_ucregion_memory_map_ar50lt(struct msm_vidc_core *vidc_core)
{
	struct msm_vidc_core *core = vidc_core;
	int rc = 0;
	u32 value;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = __write_register(core, VIDC_UC_REGION_ADDR_AR50_LT,
			(u32)core->iface_q_table.align_device_addr);
	if (rc)
		return rc;

	rc = __write_register(core, VIDC_UC_REGION_SIZE_AR50_LT, SHARED_QSIZE);
	if (rc)
		return rc;

	rc = __write_register(core, VIDC_QTBL_ADDR_AR50_LT,
			(u32)core->iface_q_table.align_device_addr);
	if (rc)
		return rc;

	rc = __write_register(core, VIDC_QTBL_INFO_AR50_LT, 0x01);
	if (rc)
		return rc;

	if (core->sfr.align_device_addr) {
		value = (u32)core->sfr.align_device_addr + VIDEO_ARCH_LX;
		rc = __write_register(core, VIDC_SFR_ADDR_AR50_LT,
			value);
		if (rc)
			return rc;
	}
	return 0;
}

static int __boot_firmware_ar50lt(struct msm_vidc_core *vidc_core)
{
	int rc = 0;
	u32 ctrl_init_val = 0, ctrl_status = 0, count = 0, max_tries = 1000;
	struct msm_vidc_core *core = vidc_core;

	if (!core) {
		d_vpr_e("%s: NULL core\n", __func__);
		return 0;
	}

	rc = __setup_ucregion_memory_map_ar50lt(core);
	if (rc) {
		d_vpr_e("%s: failed to setup ucregion\n", __func__);
		return rc;
	}

	ctrl_init_val = BIT(0);

	rc = __write_register(core, VIDC_CTRL_INIT_AR50_LT, ctrl_init_val);
	if (rc)
		return rc;

	while (!ctrl_status && count < max_tries) {
		rc = __read_register(core, VIDC_CTRL_STATUS_AR50_LT, &ctrl_status);
		if (rc)
			return rc;

		if ((ctrl_status & VIDC_CTRL_ERROR_STATUS__M_AR50_LT) == 0x4) {
			d_vpr_e("invalid setting for UC_REGION\n");
			break;
		}
		usleep_range(50, 100);
		count++;
	}

	if (count >= max_tries) {
		d_vpr_e("Error booting up vidc firmware\n");
		rc = -ETIME;
	}

	/* Enable interrupt before sending commands to venus */
	__write_register(core, VIDC_CPU_IC_SOFTINT_EN_AR50_LT, 0x1);
	return rc;
}

static int __interrupt_init_ar50lt(struct msm_vidc_core *vidc_core)
{
	struct msm_vidc_core *core = vidc_core;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	rc = __write_register(core, VIDC_WRAPPER_INTR_MASK_AR50_LT,
		VIDC_WRAPPER_INTR_MASK_A2HVCODEC_BMSK_AR50_LT);
	if (rc)
		return rc;

	return 0;
}

static int __raise_interrupt_ar50lt(struct msm_vidc_core *vidc_core)
{
	struct msm_vidc_core *core = vidc_core;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = __write_register(core, VIDC_CPU_IC_SOFTINT_AR50_LT,
		VIDC_CPU_IC_SOFTINT_H2A_SHFT_AR50_LT);
	if (rc)
		return rc;

	return 0;
}

static int __clear_interrupt_ar50lt(struct msm_vidc_core *vidc_core)
{
	struct msm_vidc_core *core = vidc_core;
	u32 intr_status = 0, mask = 0;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: NULL core\n", __func__);
		return 0;
	}

	rc = __read_register(core, VIDC_WRAPPER_INTR_STATUS_AR50_LT, &intr_status);
	if (rc)
		return rc;

	mask = (VIDC_WRAPPER_INTR_STATUS_A2H_BMSK_AR50_LT |
		VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK_AR50_LT |
		VIDC_CTRL_INIT_IDLE_MSG_BMSK_AR50_LT);

	if (intr_status & mask) {
		core->intr_status |= intr_status;
		core->reg_count++;
		d_vpr_l(
			"INTERRUPT for device: %pK: times: %d interrupt_status: %d\n",
			core, core->reg_count, intr_status);
	} else {
		core->spur_count++;
	}

	rc = __write_register(core, VIDC_CPU_CS_A2HSOFTINTCLR_AR50_LT, 1);
	if (rc)
		return rc;

	return 0;
}

static int __power_on_ar50lt_controller(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = call_res_op(core, gdsc_on, core, "venus");
	if (rc)
		goto fail_regulator;

	rc = call_res_op(core, clk_enable, core, "core_clk");
	if (rc)
		goto fail_clk_controller;

	if (core->platform->data.vpu_ver != VENUS_VERSION_AR50LT_V2) {
		rc = call_res_op(core, clk_enable, core, "iface_clk");
		if (rc)
			goto fail_clk_ahb;
	}

	rc = call_res_op(core, clk_enable, core, "bus_clk");
	if (rc)
		goto fail_clk_axi;

	return 0;

fail_clk_axi:
	if (core->platform->data.vpu_ver != VENUS_VERSION_AR50LT_V2)
		call_res_op(core, clk_disable, core, "iface_clk");
fail_clk_ahb:
	call_res_op(core, clk_disable, core, "core_clk");
fail_clk_controller:
	call_res_op(core, gdsc_off, core, "venus");
fail_regulator:
	return rc;
}

static int __power_on_ar50lt_hardware(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = call_res_op(core, gdsc_on, core, "venus-core0");
	if (rc)
		goto fail_regulator;

	rc = call_res_op(core, clk_enable, core, "core0_clk");
	if (rc)
		goto fail_clk_controller;

	rc = call_res_op(core, clk_enable, core, "core0_bus_clk");
	if (rc)
		goto fail_clk_axi;

#ifndef CONFIG_ARCH_VIENNA
	rc = call_res_op(core, clk_enable, core, "throttle_clk");
	if (rc)
		goto fail_clk_throttle;
#endif
	return 0;

#ifndef CONFIG_ARCH_VIENNA
fail_clk_throttle:
	call_res_op(core, clk_disable, core, "core0_bus_clk");
#endif

fail_clk_axi:
	call_res_op(core, clk_disable, core, "core0_clk");
fail_clk_controller:
	call_res_op(core, gdsc_off, core, "venus-core0");
fail_regulator:
	return rc;
}

static int __power_off_ar50lt_hardware(struct msm_vidc_core *core)
{
	int rc = 0;

	/* power down process */
	rc = call_res_op(core, gdsc_off, core, "venus-core0");
	if (rc) {
		d_vpr_e("%s: disable regulator vcodec failed\n", __func__);
		rc = 0;
	}

	rc = call_res_op(core, clk_disable, core, "core0_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare core0_clk failed\n", __func__);
		rc = 0;
	}

	rc = call_res_op(core, clk_disable, core, "core0_bus_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare core0_bus_clk failed\n", __func__);
		rc = 0;
	}

#ifndef CONFIG_ARCH_VIENNA
	rc = call_res_op(core, clk_disable, core, "throttle_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare throttle_clk failed\n", __func__);
		rc = 0;
	}
#endif

	return rc;
}

static int __power_off_ar50lt_controller(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = call_res_op(core, clk_disable, core, "core_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare core_clk failed\n", __func__);
		rc = 0;
	}

	if (core->platform->data.vpu_ver != VENUS_VERSION_AR50LT_V2) {
		rc = call_res_op(core, clk_disable, core, "iface_clk");
		if (rc) {
			d_vpr_e("%s: disable unprepare iface_clk failed\n", __func__);
			rc = 0;
		}
	}

	rc = call_res_op(core, clk_disable, core, "bus_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare bus_clk failed\n", __func__);
		rc = 0;
	}

	/* power down process */
	rc = call_res_op(core, gdsc_on, core, "venus");
	if (rc) {
		d_vpr_e("%s: disable regulator venus failed\n", __func__);
		rc = 0;
	}

	return rc;
}

static int __power_on_ar50lt(struct msm_vidc_core *core)
{
	u32 idx = 0;
	int rc = 0;

	if (is_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE))
		return 0;

	/* Vote for all hardware resources */
	rc = call_res_op(core, set_bw, core, INT_MAX, INT_MAX);
	if (rc) {
		d_vpr_e("%s: failed to vote buses, rc %d\n", __func__, rc);
		goto fail_vote_buses;
	}

	rc = __power_on_ar50lt_controller(core);
	if (rc) {
		d_vpr_e("%s: failed to power on ar50 controller\n", __func__);
		goto fail_power_on_controller;
	}

	rc = __power_on_ar50lt_hardware(core);
	if (rc) {
		d_vpr_e("%s: failed to power on ar50 hardware\n", __func__);
		goto fail_power_on_hardware;
	}
	/* video controller and hardware powered on successfully */
	msm_vidc_change_core_sub_state(core, 0, CORE_SUBSTATE_POWER_ENABLE, __func__);



	idx = core->power.clk_freq_idx ? core->power.clk_freq_idx : 0;
	rc = call_res_op(core, set_clks, core, idx);
	if (rc) {
		d_vpr_e("%s: failed to scale clocks\n", __func__);
		rc = 0;
	}

	/*
	 * Re-program all of the registers that get reset as a result of
	 * regulator_disable() and _enable()
	 */
	__set_registers(core);

	__interrupt_init_ar50lt(core);
	core->intr_status = 0;
	enable_irq(core->resource->irq);

	return rc;

fail_power_on_hardware:
	__power_off_ar50lt_controller(core);
fail_power_on_controller:
	call_res_op(core, set_bw, core, 0, 0);
fail_vote_buses:
	msm_vidc_change_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE, 0, __func__);
	return rc;
}

static int __power_off_ar50lt(struct msm_vidc_core *core)
{
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	if (!is_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE))
		return 0;

	if (__power_off_ar50lt_hardware(core))
		d_vpr_e("%s: failed to power off hardware\n", __func__);

	if (__power_off_ar50lt_controller(core))
		d_vpr_e("%s: failed to power off controller\n", __func__);

	if (call_res_op(core, set_bw, core, 0, 0))
		d_vpr_e("%s: failed to unvote buses\n", __func__);

	if (!(core->intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK_AR50_LT))
		disable_irq_nosync(core->resource->irq);
	core->intr_status = 0;

	msm_vidc_change_core_sub_state(core, CORE_SUBSTATE_POWER_ENABLE, 0, __func__);

	return rc;
}

static int __prepare_pc_ar50lt(struct msm_vidc_core *vidc_core)
{
	int rc = 0;
	u32 wfi_status = 0, idle_status = 0, pc_ready = 0;
	u32 ctrl_status = 0;
	struct msm_vidc_core *core = vidc_core;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = __read_register(core, VIDC_CTRL_STATUS_AR50_LT, &ctrl_status);
	if (rc)
		return rc;

	pc_ready = ctrl_status & VIDC_CTRL_STATUS_PC_READY_AR50_LT;
	idle_status = ctrl_status & BIT(30);

	if (pc_ready) {
		d_vpr_h("Already in pc_ready state\n");
		return 0;
	}
	rc = __read_register(core, VIDC_WRAPPER_TZ_CPU_STATUS, &wfi_status);
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

	rc = __read_register_with_poll_timeout(core, VIDC_CTRL_STATUS_AR50_LT,
			VIDC_CTRL_STATUS_PC_READY_AR50_LT, VIDC_CTRL_STATUS_PC_READY_AR50_LT,
			250, 2500);
	if (rc) {
		d_vpr_e("%s: Skip PC. Ctrl status not set\n", __func__);
		goto skip_power_off;
	}

	rc = __read_register_with_poll_timeout(core, VIDC_WRAPPER_TZ_CPU_STATUS,
			BIT(0), 0x1, 250, 2500);
	if (rc) {
		d_vpr_e("%s: Skip PC. Wfi status not set\n", __func__);
		goto skip_power_off;
	}
	return rc;

skip_power_off:
	rc = __read_register(core, VIDC_CTRL_STATUS_AR50_LT, &ctrl_status);
	if (rc)
		return rc;
	rc = __read_register(core, VIDC_WRAPPER_TZ_CPU_STATUS, &wfi_status);
	if (rc)
		return rc;
	wfi_status &= BIT(0);
	d_vpr_e("Skip PC, wfi=%#x, idle=%#x, pcr=%#x, ctrl=%#x)\n",
		wfi_status, idle_status, pc_ready, ctrl_status);
	return -EAGAIN;
}

static int __watchdog_ar50lt(struct msm_vidc_core *vidc_core, u32 intr_status)
{
	int rc = 0;
	struct msm_vidc_core *core = vidc_core;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK_AR50_LT) {
		d_vpr_e("%s: received watchdog interrupt\n", __func__);
		rc = 1;
	}

	return rc;
}

static int __hw_ctrl_gdsc_ar50lt(struct msm_vidc_core *core)
{
	return call_res_op(core, gdsc_hw_ctrl, core);
}

static struct msm_vidc_venus_ops ar50lt_ops = {
	.boot_firmware = __boot_firmware_ar50lt,
	.raise_interrupt = __raise_interrupt_ar50lt,
	.clear_interrupt = __clear_interrupt_ar50lt,
	.setup_ucregion_memmap = __setup_ucregion_memory_map_ar50lt,
	.power_on = __power_on_ar50lt,
	.power_off = __power_off_ar50lt,
	.prepare_pc = __prepare_pc_ar50lt,
	.watchdog = __watchdog_ar50lt,
	.noc_error_info = NULL, //TODO Pavan
	.hw_ctrl_gdsc = __hw_ctrl_gdsc_ar50lt,
	.scm_mem_protect = msm_vidc_mem_protect_video_regions_v1,
};

int msm_vidc_decide_work_mode_ar50lt(struct msm_vidc_inst  *inst)
{
	u32 work_mode;
	struct v4l2_format *inp_f;
	u32 width, height, bitrate_mode;
	bool res_ok = false;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	work_mode = MSM_VIDC_STAGE_2;
	inp_f = &inst->fmts[INPUT_PORT];

	if (is_decode_session(inst)) {
		work_mode = MSM_VIDC_STAGE_2;
		height = inp_f->fmt.pix_mp.height;
		width = inp_f->fmt.pix_mp.width;
		res_ok = res_is_less_than_or_equal_to(width, height, 1280, 720);
		if (inst->capabilities[LOWLATENCY_MODE].value ||
				res_ok) {
			work_mode = MSM_VIDC_STAGE_1;
		}
	} else if (is_encode_session(inst)) {
		work_mode = MSM_VIDC_STAGE_1;
		bitrate_mode = inst->capabilities[BITRATE_MODE].value;
		if (bitrate_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR ||
			bitrate_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
			work_mode = MSM_VIDC_STAGE_2;
	} else {
		i_vpr_e(inst, "%s: invalid session type\n", __func__);
		return -EINVAL;
	}

	i_vpr_h(inst, "Configuring work mode = %u low latency = %lld\n",
		work_mode, inst->capabilities[LOWLATENCY_MODE].value);
	msm_vidc_update_cap_value(inst, STAGE, work_mode, __func__);

	return 0;
}

static inline bool is_crc_enabled(struct msm_vidc_core *core)
{
	return !!(core->hfi_debug_config & HFI_DEBUG_CONFIG_CRC);
}

u32 msm_vidc_buffer_region_ext_ar50lt(struct msm_vidc_inst *inst,
	enum msm_vidc_buffer_type buffer_type)
{
	struct msm_vidc_core *core = inst->core;
	u32 region = MSM_VIDC_NON_SECURE;

	if (!is_secure_session(inst)) {
		if (is_crc_enabled(core))
			return MSM_VIDC_NON_SECURE;

		switch (buffer_type) {
		case MSM_VIDC_BUF_ARP:
			region = MSM_VIDC_SECURE_NONPIXEL;
			break;
		case MSM_VIDC_BUF_INPUT:
			if (is_encode_session(inst))
				region = MSM_VIDC_NON_SECURE_PIXEL;
			else
				region = MSM_VIDC_NON_SECURE_BITSTREAM;
			break;
		case MSM_VIDC_BUF_OUTPUT:
			if (is_encode_session(inst))
				region = MSM_VIDC_NON_SECURE_BITSTREAM;
			else
				region = MSM_VIDC_NON_SECURE_PIXEL;
			break;
		case MSM_VIDC_BUF_DPB:
		case MSM_VIDC_BUF_VPSS:
		case MSM_VIDC_BUF_PARTIAL_DATA:
			region = MSM_VIDC_NON_SECURE_PIXEL;
			break;
		case MSM_VIDC_BUF_BIN:
			region = MSM_VIDC_NON_SECURE_BITSTREAM;
			break;
		case MSM_VIDC_BUF_INPUT_META:
		case MSM_VIDC_BUF_OUTPUT_META:
		case MSM_VIDC_BUF_COMV:
		case MSM_VIDC_BUF_NON_COMV:
		case MSM_VIDC_BUF_LINE:
		case MSM_VIDC_BUF_PERSIST:
			region = MSM_VIDC_NON_SECURE;
			break;
		default:
			i_vpr_e(inst, "%s: invalid driver buffer type %d\n",
				__func__, buffer_type);
		}
	} else {
		switch (buffer_type) {
		case MSM_VIDC_BUF_INPUT:
			if (is_encode_session(inst))
				region = MSM_VIDC_SECURE_PIXEL;
			else
				region = MSM_VIDC_SECURE_BITSTREAM;
			break;
		case MSM_VIDC_BUF_OUTPUT:
			if (is_encode_session(inst))
				region = MSM_VIDC_SECURE_BITSTREAM;
			else
				region = MSM_VIDC_SECURE_PIXEL;
			break;
		case MSM_VIDC_BUF_INPUT_META:
		case MSM_VIDC_BUF_OUTPUT_META:
			region = MSM_VIDC_NON_SECURE;
			break;
		case MSM_VIDC_BUF_DPB:
		case MSM_VIDC_BUF_VPSS:
		case MSM_VIDC_BUF_PARTIAL_DATA:
			region = MSM_VIDC_SECURE_PIXEL;
			break;
		case MSM_VIDC_BUF_BIN:
			region = MSM_VIDC_SECURE_BITSTREAM;
			break;
		case MSM_VIDC_BUF_ARP:
		case MSM_VIDC_BUF_COMV:
		case MSM_VIDC_BUF_NON_COMV:
		case MSM_VIDC_BUF_LINE:
		case MSM_VIDC_BUF_PERSIST:
			region = MSM_VIDC_SECURE_NONPIXEL;
			break;
		default:
			i_vpr_e(inst, "%s: invalid driver buffer type %d\n",
				__func__, buffer_type);
		}
	}

	return region;
}

static struct msm_vidc_session_ops msm_session_ops = {
	.buffer_size = msm_buffer_size_ar50lt,
	.min_count = msm_buffer_min_count_ar50lt,
	.extra_count = msm_buffer_extra_count_ar50lt,
	.ring_buf_count = NULL,
	.scale_clocks = msm_vidc_scale_clocks_ar50lt,
	.calc_bw = msm_vidc_calc_bw_ar50lt,
	.decide_work_route = NULL,
	.decide_work_mode = msm_vidc_decide_work_mode_ar50lt,
	.decide_quality_mode = NULL,
	.decide_scaling = NULL,
};

int msm_vidc_init_ar50lt(struct msm_vidc_core *core)
{
	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core->venus_ops = &ar50lt_ops;
	core->session_ops = &msm_session_ops;

	return 0;
}

int msm_vidc_deinit_ar50lt(struct msm_vidc_core *core)
{
	/* do nothing */
	return 0;
}
