/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _VENUS_HFI_H_
#define _VENUS_HFI_H_

#include "msm_vidc_internal.h"

struct msm_vidc_core;
struct msm_vidc_inst;

#define VIDC_MAX_PC_SKIP_COUNT	10

struct vidc_buffer_addr_info {
	enum msm_vidc_buffer_type buffer_type;
	u32 buffer_size;
	u32 num_buffers;
	u32 align_device_addr;
	u32 extradata_addr;
	u32 extradata_size;
	u32 response_required;
};

struct hfi_pending_packet {
	struct list_head       list;
	void                  *data;
};

int __strict_check(struct msm_vidc_core *core,
		   const char *function);
int venus_hfi_session_property(struct msm_vidc_inst *inst,
			       u32 pkt_type, u32 flags, u32 port,
			       u32 payload_type, void *payload,
			       u32 payload_size);
int venus_hfi_queue_input_buffer(struct msm_vidc_inst *inst,
			   struct msm_vidc_buffer *buffer,
			   struct msm_vidc_buffer *metabuf);
int venus_hfi_queue_output_buffer(struct msm_vidc_inst *inst,
			   struct msm_vidc_buffer *buffer,
			   struct msm_vidc_buffer *metabuf);
int venus_hfi_queue_internal_buffer(struct msm_vidc_inst *inst,
			   struct msm_vidc_buffer *buffer);
int venus_hfi_queue_super_buffer(struct msm_vidc_inst *inst,
				 struct msm_vidc_buffer *buffer,
				 struct msm_vidc_buffer *metabuf);
int venus_hfi_release_buffer(struct msm_vidc_inst *inst,
			     struct msm_vidc_buffer *buffer);
int venus_hfi_start(struct msm_vidc_inst *inst, enum msm_vidc_port_type port);
int venus_hfi_stop(struct msm_vidc_inst *inst, enum msm_vidc_port_type port);
int venus_hfi_session_close(struct msm_vidc_inst *inst);
int venus_hfi_session_command(struct msm_vidc_inst *inst,
			       u32 pkt_type, u32 flags,
			       u32 port, u32 session_id,
			       u32 payload_type, void *payload,
			       u32 payload_size,
			       const char *func);
int venus_hfi_session_open_locked(struct msm_vidc_inst *inst);
int venus_hfi_session_pause(struct msm_vidc_inst *inst, enum msm_vidc_port_type port);
int venus_hfi_session_resume(struct msm_vidc_inst *inst,
			     enum msm_vidc_port_type port, u32 payload);
int venus_hfi_session_drain(struct msm_vidc_inst *inst, enum msm_vidc_port_type port);
int venus_hfi_session_set_codec(struct msm_vidc_inst *inst);
int venus_hfi_session_set_secure_mode(struct msm_vidc_inst *inst);
int venus_hfi_core_init(struct msm_vidc_core *core);
int venus_hfi_core_deinit(struct msm_vidc_core *core, bool force);
int venus_hfi_noc_error_info(struct msm_vidc_core *core);
int venus_hfi_suspend(struct msm_vidc_core *core);
int venus_hfi_set_crc(struct msm_vidc_core *core);
int venus_hfi_trigger_ssr(struct msm_vidc_core *core, u32 type,
			  u32 client_id, u32 addr);
int venus_hfi_trigger_stability(struct msm_vidc_inst *inst, u32 type,
				u32 client_id, u32 val);
int venus_hfi_reserve_hardware(struct msm_vidc_inst *inst, u32 duration);
int venus_hfi_scale_clocks(struct msm_vidc_inst *inst, int idx);
int venus_hfi_scale_buses(struct msm_vidc_inst *inst, u64 bw_ddr, u64 bw_llcc);
int venus_hfi_set_ir_period(struct msm_vidc_inst *inst, u32 ir_type,
			    enum msm_vidc_inst_capability_type cap_id);
void venus_hfi_pm_work_handler(struct work_struct *work);
irqreturn_t venus_hfi_isr(int irq, void *data);
irqreturn_t venus_hfi_isr_handler(int irq, void *data);
int __prepare_pc(struct msm_vidc_core *core);
int __resume(struct msm_vidc_core *core);
struct device_region_info
	*venus_hfi_get_device_region_info(struct msm_vidc_core *core,
					  enum msm_vidc_device_region region);
struct subcache_info *get_session_level_subcache(
	struct msm_vidc_core *core, u32 llcc_type);
int venus_hfi_set_session_subcache(struct msm_vidc_inst *inst,
	u32 llcc_ucid, u32 dev_addr);
int venus_hfi_release_session_subcache(struct msm_vidc_inst *inst,
	u32 llcc_ucid, u32 dev_addr);
u64 venus_hfi_get_subcache_mask(struct msm_vidc_inst *inst);

#endif // _VENUS_HFI_H_
