# SPDX-License-Identifier: GPL-2.0-only
TARGET_VIDC_ENABLE := false
ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
	ifeq ($(TARGET_KERNEL_DLKM_VIDEO_OVERRIDE), true)
		TARGET_VIDC_ENABLE := true
	endif
else
TARGET_VIDC_ENABLE := true
endif

ifeq ($(ENABLE_HYP), true)
ifneq ($(TARGET_BOARD_PLATFORM), gen5)
TARGET_VIDC_ENABLE := false
endif
endif

ifeq ($(TARGET_VIDC_ENABLE),true)
VIDEO_BLD_DIR := $(shell pwd)/vendor/qcom/opensource/video-driver
VIDEO_SELECT := CONFIG_MSM_VIDC_V4L2=m
VIDEO_SELECT += CONFIG_MSM_VIDC_ANDROID=m
VIDEO_SELECT += CONFIG_MSM_VIDC_LLCC=m

# Build msm_video.ko
###########################################################
# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := VIDEO_ROOT=$(VIDEO_BLD_DIR)

KBUILD_OPTIONS += $(VIDEO_SELECT)
KBUILD_OPTIONS += TARGET_BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)

ifneq ($(TARGET_BOARD_PLATFORM),canoe)
KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS+=$(shell pwd)/$(call intermediates-dir-for,DLKM,hw-fence-module-symvers)/Module.symvers
ifneq ($(TARGET_BOARD_PLATFORM), gen5)
KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS=$(shell pwd)/$(call intermediates-dir-for,DLKM,mmrm-module-symvers)/Module.symvers
KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS+=$(shell pwd)/$(call intermediates-dir-for,DLKM,synx-driver-symvers)/synx-driver-symvers
else
ifeq ($(ENABLE_HYP), true)
KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS=$(PWD)/$(call intermediates-dir-for,DLKM,virtio-video-symvers)/Module.symvers
endif
endif
endif
###########################################################

DLKM_DIR   := device/qcom/common/dlkm

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
# For incremental compilation
LOCAL_SRC_FILES           := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
LOCAL_MODULE              := msm_video.ko
LOCAL_MODULE_KBUILD_NAME  := msm_video/msm_video.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
LOCAL_MODULE_DDK_BUILD    := true
LOCAL_MODULE_DDK_SUBTARGET_REGEX := "video.*"
LOCAL_MODULE_KO_DIRS      := msm_video/msm_video.ko

ifneq ($(TARGET_BOARD_PLATFORM),canoe)
LOCAL_REQUIRED_MODULES    += hw-fence-module-symvers
ifneq ($(TARGET_BOARD_PLATFORM), gen5)
LOCAL_REQUIRED_MODULES    := mmrm-module-symvers
LOCAL_REQUIRED_MODULES    += synx-driver-symvers
LOCAL_ADDITIONAL_DEPENDENCIES := $(call intermediates-dir-for,DLKM,mmrm-module-symvers)/Module.symvers
LOCAL_ADDITIONAL_DEPENDENCIES += $(call intermediates-dir-for,DLKM,synx-driver-symvers)/synx-driver-symvers
else
ifeq ($(ENABLE_HYP), true)
LOCAL_REQUIRED_MODULES := virtio-video-symvers
LOCAL_ADDITIONAL_DEPENDENCIES += $(call intermediates-dir-for,DLKM,virtio-video-symvers)/Module.symvers
endif
endif
LOCAL_ADDITIONAL_DEPENDENCIES += $(call intermediates-dir-for,DLKM,hw-fence-module-symvers)/Module.symvers
endif

include $(DLKM_DIR)/Build_external_kernelmodule.mk
endif
