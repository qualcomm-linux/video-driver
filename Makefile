# SPDX-License-Identifier: GPL-2.0-only

$(info CONFIG_MSM_VIDC_ANDROID=$(CONFIG_MSM_VIDC_ANDROID))

ifneq ($(CONFIG_MSM_VIDC_ANDROID), m)
M := $(shell pwd)
KBUILD_OPTIONS += VIDEO_ROOT=$(M)
VIDEO_ROOT := $(shell pwd)
endif

VIDEO_COMPILE_TIME = $(shell date)
VIDEO_COMPILE_BY = $(shell echo $(USER))
VIDEO_COMPILE_HOST = $(shell uname -n)
VIDEO_GEN_PATH = $(VIDEO_ROOT)/driver/vidc/inc/video_generated_h

all: modules

$(VIDEO_GEN_PATH): $(shell find . -type f \( -iname \*.c -o -iname \*.h -o -iname \*.mk \))
	echo '#define VIDEO_COMPILE_TIME "$(VIDEO_COMPILE_TIME)"' > $(VIDEO_GEN_PATH)
	echo '#define VIDEO_COMPILE_BY "$(VIDEO_COMPILE_BY)"' >> $(VIDEO_GEN_PATH)
	echo '#define VIDEO_COMPILE_HOST "$(VIDEO_COMPILE_HOST)"' >> $(VIDEO_GEN_PATH)

modules: $(VIDEO_GEN_PATH)
ifeq ($(CONFIG_MSM_VIDC_ANDROID), m)
	ln -sf $(VIDEO_ROOT)/driver $(VIDEO_ROOT)/msm_video/driver
else
	ln -sf $(VIDEO_ROOT)/driver $(VIDEO_ROOT)/video/driver
endif
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)
ifeq ($(CONFIG_MSM_VIDC_ANDROID), m)
	rm $(VIDEO_ROOT)/msm_video/driver
else
	rm $(VIDEO_ROOT)/video/driver
endif

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -f video/conftest/*.log
	rm -rf .tmp_versions
