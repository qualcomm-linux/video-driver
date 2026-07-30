#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

# Called by iris-vpu-dkms.service on every boot.
# Load iris_vpu module, handling potential errors gracefully.
lsmod | grep -q iris_vpu || modprobe iris_vpu
exit 0