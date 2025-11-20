/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_VIDC_CORE_H_
#define _MSM_VIDC_CORE_H_

#include <media/v4l2-device.h>

#include "msm_vidc_internal.h"
#include "msm_vidc_state.h"
#include "venus_hfi_queue.h"
#include "msm_vidc_hw_virt.h"

#define MAX_EVENTS   30

#define call_venus_op(d, op, ...)			\
	(((d) && (d)->venus_ops && (d)->venus_ops->op) ? \
	((d)->venus_ops->op(__VA_ARGS__)) : 0)

#define call_md_op(d, op, ...)			\
	(((d) && (d)->md_ops && (d)->md_ops->op) ? \
	((d)->md_ops->op(__VA_ARGS__)) : 0)

struct msm_vidc_venus_ops {
	int (*boot_firmware)(struct msm_vidc_core *core);
	int (*raise_interrupt)(struct msm_vidc_core *core);
	int (*clear_interrupt)(struct msm_vidc_core *core);
	int (*prepare_pc)(struct msm_vidc_core *core);
	int (*power_on)(struct msm_vidc_core *core);
	int (*power_off)(struct msm_vidc_core *core);
	int (*watchdog)(struct msm_vidc_core *core, u32 intr_status);
	int (*noc_error_info)(struct msm_vidc_core *core);
	int (*switch_gdsc_mode)(struct msm_vidc_core *core, bool sw_mode);
	int (*hw_ctrl_gdsc)(struct msm_vidc_core *core);
	int (*sw_ctrl_gdsc)(struct msm_vidc_core *core);
	int (*scm_mem_protect)(struct msm_vidc_core *core);
	int (*enable_intr)(struct msm_vidc_core *core);
};

struct msm_vidc_md_ops {
	int (*md_register)(struct msm_vidc_core *core);
	int (*md_unregister)(struct msm_vidc_core *core);
	int (*md_dump_fw_region)(struct msm_vidc_core *core,
			const char *name, void *virt, u64 phys, u64 size);
	int (*md_dump_hfi_queues)(struct msm_vidc_core *core);
};

struct msm_vidc_synx_fence_data {
	u32                             client_id;
	void                           *session;
	u32                             client_flags; /* not used */
	struct msm_vidc_mem             queue;
};

struct msm_vidc_iface_q_info {
	void *q_hdr;
	struct msm_vidc_mem_addr q_array;
};

struct msm_video_device {
	enum msm_vidc_domain_type              type;
	struct video_device                    vdev;
	struct v4l2_m2m_dev                   *m2m_dev;
};

struct msm_vidc_core_power {
	u32 clk_freq_idx;
	u64 bw_ddr;
	u64 bw_llcc;
};

enum msm_vidc_hw_version {
	MSM_VIDC_HW_VERSION_V1 = 1, // chipset versions v1
	MSM_VIDC_HW_VERSION_V2 = 2, // chipset versions v2
};

struct msm_vidc_core {
	struct platform_device                *pdev;
	struct msm_video_device                vdev[2];
	struct v4l2_device                     v4l2_dev;
	struct media_device                    media_dev;
	struct list_head                       instances;
	struct list_head                       dangling_instances;
	struct dentry                         *debugfs_parent;
	struct dentry                         *debugfs_root;
	char                                   fw_version[MAX_MSM_VIDC_NAME_LENGTH];
	enum msm_vidc_core_state               state;
	int                                  (*state_handle)(struct msm_vidc_core *core,
							     enum msm_vidc_core_event_type type,
							     struct msm_vidc_event_data *data);
	enum msm_vidc_core_sub_state           sub_state;
	char                                   sub_state_name[MAX_MSM_VIDC_NAME_LENGTH];
	struct mutex                           lock;
	struct msm_vidc_resource              *resource;
	struct msm_vidc_platform              *platform;
	u32                                    intr_status;
	u32                                    spur_count;
	u32                                    reg_count;
	u32                                    enc_codecs_count;
	u32                                    dec_codecs_count;
	struct msm_vidc_core_capability        capabilities[CORE_CAP_MAX + 1];
	struct msm_vidc_full_virtualization    full_virtualization_data;
	struct msm_vidc_inst_capability       *inst_caps;
	struct msm_vidc_mem_addr               sfr;
	struct msm_vidc_mem_addr               iface_q_table;
	struct msm_vidc_mem_addr               mmap_buf;
	struct msm_vidc_mem_addr               aon_reg;
	struct msm_vidc_mem_addr               fence_reg;
	struct msm_vidc_mem_addr               qtimer_reg;
	struct msm_vidc_iface_q_info           iface_queues[VIDC_IFACEQ_NUMQ];
	struct delayed_work                    pm_work;
	struct workqueue_struct               *pm_workq;
	struct workqueue_struct               *batch_workq;
	struct delayed_work                    fw_unload_work;
	struct work_struct                     ssr_work;
	struct work_struct                     full_virt_ssr_work;
	u32                                    ssr_dev;
	struct msm_vidc_core_power             power;
	struct msm_vidc_ssr                    ssr;
	bool                                   debug_enable_crc;
	u32                                    hfi_debug_config;
	u32                                    skip_pc_count;
	u32                                    last_packet_type;
	u8                                    *packet;
	u32                                    packet_size;
	u8                                    *response_packet;
	const struct v4l2_file_operations     *v4l2_file_ops;
	const struct v4l2_ioctl_ops           *v4l2_ioctl_ops_enc;
	const struct v4l2_ioctl_ops           *v4l2_ioctl_ops_dec;
	const struct v4l2_ctrl_ops            *v4l2_ctrl_ops;
	const struct media_device_ops         *media_device_ops;
	const struct v4l2_m2m_ops             *v4l2_m2m_ops;
	const struct vb2_ops                  *vb2_ops;
	const struct vb2_mem_ops              *vb2_mem_ops;
	struct msm_vidc_venus_ops             *venus_ops;
	const struct msm_vidc_resources_ops   *res_ops;
	struct msm_vidc_session_ops           *session_ops;
	const struct msm_vidc_memory_ops      *mem_ops;
	const struct msm_vidc_fence_ops       *fence_ops;
	const struct msm_vidc_md_ops          *md_ops;
	u32                                    header_id;
	u32                                    packet_id;
	u32                                    sys_init_id;
	struct msm_vidc_synx_fence_data        synx_fence_data;
	/* hw_version: distinguish chip versions: v1, v2 */
	enum msm_vidc_hw_version               hw_version;
	int                                    cb_count;
};

#endif // _MSM_VIDC_CORE_H_
