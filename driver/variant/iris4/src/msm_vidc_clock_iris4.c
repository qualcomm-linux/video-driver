// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "perf_static_model.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_power_iris4.h"

#define ENABLE_FINEBITRATE_SUBUHD60 0

static u32 codec_encoder_gop_complexity_table_fp[9][3];
static u32 input_bitrate_fp;

/*
 * Chipset Generation Technology: SW/FW overhead profiling
 * need update with new numbers
 */
static u32 frequency_table_iris4[2][7] = {
	/* //make lowsvs_D1 as invalid; */
	{ 800, 630, 533, 444, 420, 338, 240 }, //core clk
	{ 1360, 1260, 800, 666, 630, 507, 360 }, //Tensilica clk
};

static u32 apv_encoder_ppc[2][4] = {
	// ubwc  422, 422+rot (NA for K-T), 420, 420 + rot ; UBWC 420 includes both p010, tp10
	{800, 800, 681, 648 },
	// linear 422, 422+rot (NA for K-T), 420, 420 + rot ; linear 420 includes p010
	{800, 800, 604, 341 },
};

static u32 sw_overhead_iris4[7] = {47200, 37170, 31447, 26196, 24780, 19942, 14160 };

#define TENSILICA_CORE_RATIO_IRIS4                                                 (15)

#define DECODER_VPP_FW_OVERHEAD_IRIS4_AV1D                                        (42000)
#define DECODER_VPP_FW_OVERHEAD_IRIS4_NONAV1D                                     (32000)
#define DECODER_VPP_FW_OVERHEAD_IRIS4_APVD                                        (60000)
#define DECODER_VPPVSP1STAGE_FW_OVERHEAD_IRIS4_AV1D                               (65100)
#define DECODER_VPPVSP1STAGE_FW_OVERHEAD_IRIS4_NONAV1D                            (49600)

//28000 FW overhead; 4000 ARP overhead
#define ENCODER_VPP_FW_OVERHEAD_IRIS4                                              (28000+4000)
#define ENCODER_VPP_FW_OVERHEAD_IRIS4_APVE                                         (69000)

//205000 FW overhead; 4000 ARP overhead
#define ENCODER_VPPVSP1STAGE_FW_OVERHEAD_IRIS4                                     (49600)

// in VPP HW cycles
#define ENCODER_WORKER_FW_OVERHEAD_IRIS4                                           (205000 + 4000)
#define DECODER_WORKER_FW_OVERHEAD_AV1_IRIS4                                       (158000)
#define DECODER_WORKER_FW_OVERHEAD_NONAV1_IRIS4                                    (108000)

static u32 encoder_vpp_cycles_2pipe_iris4[3][8] = {
	//h264e LCU16: 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 141, 142, 142, 143, 144, 146, 147, 149},
	//h265e LCU32: 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 141, 142, 142, 144, 146, 148, 150, 152},
	 //vpss_m2m   : 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 156, 156, 156, 156, 156, 156, 156, 156},
};

static u32 encoder_vpp_cycles_1pipe_iris4[3][8] = {
	//h264e LCU16: 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 141, 141, 141, 141, 141, 143, 145, 145},
	//h265e LCU32: 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 141, 141, 141, 142, 143, 145, 146, 146},
	//vpss_m2m   : 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 156, 156, 156, 156, 156, 156, 156, 156},
};

static u32 decoder_vpp_cycles_2pipe_iris4[4][8] = {
	//h265/h264 LCU16/32: 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 204, 204, 204, 204, 203, 210, 217, 219},
	//h265/vp9/av1 LCU64: 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 204, 205, 205, 210, 215, 218, 221, 223},
	//av1          LCU128  : 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 205, 217, 217, 230, 242, 240, 238, 241},
	//vvc/200cycle LCU128  : 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 205, 217, 217, 230, 242, 240, 238, 241},
};

static u32 decoder_vpp_cycles_1pipe_iris4[4][8] = {
	//h265/h264 LCU16/32: 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 202, 202, 202, 202, 202, 202, 202, 202},
	//h265/vp9/av1 LCU64: 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 203, 203, 203, 203, 202, 209, 215, 215},
	//av1          LCU128  : 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 203, 203, 203, 209, 214, 216, 218, 218},
	//vvc/200cycle LCU128  : 8KUHD, middle, UHD, middle, 1080p, middle, 720p, end
	{ 203, 203, 203, 209, 214, 216, 218, 218},
};

//Video IP Core Technology: bitrate constraint
//HW limit bitrate table (these values are measured end to end fw/sw impacts are also considered)
static u32 bitrate_table_iris4_2stage_fp[5][10] = {
	{ 0, 220, 220, 220, 220, 220, 220, 220, 220, 220 },   //h264 cavlc
	{ 0, 140, 150, 160, 175, 190, 190, 190, 190, 190 },   //h264 cabac
	{ 95, 168, 200, 225, 237, 260, 260, 260, 260, 260 },   //h265
	{ 90, 90, 90, 90, 90, 90, 90, 90, 90, 90 },    //vp9
	{ 130, 130, 120, 120, 120, 120, 120, 120, 120, 120 },   //av1
};

//HW limit bitrate table (these values are measured end to end fw/sw impacts are also considered)
static u32 bitrate_table_iris4_1stage_fp[5][10] = {        //1-stage assume IPPP
	{ 0, 220, 220, 220, 220, 220, 220, 220, 220, 220 },   //h264 cavlc
	{ 0, 110, 150, 150, 150, 150, 150, 150, 150, 150 },   //h264 cabac
	{ 0, 168, 185, 185, 185, 185, 185, 185, 185, 185 },   //h265
	{ 0, 70, 70, 70, 70, 70, 70, 70, 70, 70 },   //vp9
	{ 0, 100, 100, 100, 100, 100, 100, 100, 100, 100 },	  //av1
};

/* 8KUHD60; UHD240; 1080p960  with B */
//static u32 fp_pixel_count_bar0 = 3840 * 2160 * 240;
/* 8KUHD60; UHD240; 1080p960  without B */
static u32 fp_pixel_count_bar1 = 3840 * 2160 * 240;
/* 1080p720 */
static u32 fp_pixel_count_bar2 = 3840 * 2160 * 180;
/* UHD120 */
static u32 fp_pixel_count_bar3 = 3840 * 2160 * 120;
/* UHD90 */
static u32 fp_pixel_count_bar4 = 3840 * 2160 * 90;
/* UHD60 */
static u32 fp_pixel_count_bar5 = 3840 * 2160 * 60;
/* UHD30; FHD120; HD240 */
static u32 fp_pixel_count_bar6 = 3840 * 2160 * 30;
/* FHD60 */
static u32 fp_pixel_count_bar7 = 1920 * 1080 * 60;
/* FHD30 */
static u32 fp_pixel_count_bar8 = 1920 * 1080 * 30;
/* HD30 */
static u32 fp_pixel_count_bar9 = 1280 * 720 * 30;

//Resolution to look up VPP Cycles: for Iris4 and future chipsets
#define CODEC_RESOLUTION_720P                     (1280 * 720)
#define CODEC_RESOLUTION_1080P                    (1920 * 1080)
#define CODEC_RESOLUTION_3840x2160                (3840 * 2160)
#define CODEC_RESOLUTION_4096x2160                (4096 * 2160)
#define CODEC_RESOLUTION_8KUHD                    (7680 * 4320)

static u32 calculate_number_mbs_iris4(u32 width, u32 height, u32 lcu_size)
{
	u32 mbs_width = (width % lcu_size) ?
		(width / lcu_size + 1) : (width / lcu_size);

	u32 mbs_height = (height % lcu_size) ?
		(height / lcu_size + 1) : (height / lcu_size);

	return mbs_width * mbs_height * (lcu_size / 16) * (lcu_size / 16);
}

static int initialize_encoder_complexity_table(void)
{
	/* Beging Calculate Encoder GOP Complexity Table and HW Floor numbers */
	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_Bb_ENTRY] = 0;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_P_ENTRY] = 10000;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_Bb_ENTRY] * 150 +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_P_ENTRY] * 100);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_FACTORY_ENTRY] +
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_P_ENTRY] - 1);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_FACTORY_ENTRY] /
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_LOSSLESS][CODEC_ENCODER_GOP_P_ENTRY]);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_Bb_ENTRY] = 0;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_P_ENTRY] = 10000;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_Bb_ENTRY] * 200 +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_P_ENTRY] * 100);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_FACTORY_ENTRY] +
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_P_ENTRY] - 1);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_FACTORY_ENTRY] /
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IONLY][CODEC_ENCODER_GOP_P_ENTRY]);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_Bb_ENTRY] = 70000;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_P_ENTRY] = 10000;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_Bb_ENTRY] * 200 +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_P_ENTRY] * 100);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] +
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_P_ENTRY] - 1);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] /
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I3B4b1P][CODEC_ENCODER_GOP_P_ENTRY]);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_Bb_ENTRY] = 30000;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_P_ENTRY] = 10000;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_Bb_ENTRY] * 200 +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_P_ENTRY] * 100);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] +
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_P_ENTRY] - 1);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_FACTORY_ENTRY] /
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_I1B2b1P][CODEC_ENCODER_GOP_P_ENTRY]);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_Bb_ENTRY] = 10000;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_P_ENTRY] = 10000;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_Bb_ENTRY] * 200 +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_P_ENTRY] * 100);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_FACTORY_ENTRY] +
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_P_ENTRY] - 1);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_FACTORY_ENTRY] /
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IbP][CODEC_ENCODER_GOP_P_ENTRY]);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_Bb_ENTRY] = 0;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_P_ENTRY] = 10000;

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_Bb_ENTRY] * 200 +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_P_ENTRY] * 100);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_FACTORY_ENTRY] +
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_P_ENTRY] - 1);

	codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_FACTORY_ENTRY] =
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_FACTORY_ENTRY] /
		(codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_Bb_ENTRY] +
		codec_encoder_gop_complexity_table_fp
		[CODEC_GOP_IPP][CODEC_ENCODER_GOP_P_ENTRY]);

	return 0;
}

static u32 get_bitrate_entry(u32 pixle_count)
{
	u32 bitrate_entry = 0;

	if (pixle_count >= fp_pixel_count_bar1)
		bitrate_entry = 1;
	else if (pixle_count >= fp_pixel_count_bar2)
		bitrate_entry = 2;
	else if (pixle_count >= fp_pixel_count_bar3)
		bitrate_entry = 3;
	else if (pixle_count >= fp_pixel_count_bar4)
		bitrate_entry = 4;
	else if (pixle_count >= fp_pixel_count_bar5)
		bitrate_entry = 5;
	else if (pixle_count >= fp_pixel_count_bar6)
		bitrate_entry = 6;
	else if (pixle_count >= fp_pixel_count_bar7)
		bitrate_entry = 7;
	else if (pixle_count >= fp_pixel_count_bar8)
		bitrate_entry = 8;
	else if (pixle_count >= fp_pixel_count_bar9)
		bitrate_entry = 9;
	else
		bitrate_entry = 9;

	return bitrate_entry;
}

static u32 get_vpp_cycles(struct api_calculation_input codec_input)
{
	u8 i = 0, j = 0;
	u32 ret = 0;

	if (codec_input.decoder_or_encoder == CODEC_DECODER) {
		if (codec_input.lcu_size <= 32)
			i = 0;
		else if (codec_input.lcu_size <= 64)
			i = 1;
		else
			i = 2;
	} else {
		if (codec_input.lcu_size <= 16)
			i = 0;
		else if (codec_input.lcu_size <= 32)
			i = 1;
		else
			i = 2;
	}

	if (codec_input.frame_width *
		codec_input.frame_height >= CODEC_RESOLUTION_8KUHD) {
		j = 0;
	} else if (codec_input.frame_width *
				codec_input.frame_height > CODEC_RESOLUTION_4096x2160) {
		j = 1;
	} else if (codec_input.frame_width *
				codec_input.frame_height > CODEC_RESOLUTION_3840x2160) {
		if (codec_input.decoder_or_encoder == CODEC_DECODER)
			j = 2;
		else
			j = 1;
	} else if (codec_input.frame_width *
				codec_input.frame_height == CODEC_RESOLUTION_3840x2160) {
		j = 2;
	} else if (codec_input.frame_width *
				codec_input.frame_height > CODEC_RESOLUTION_1080P) {
		j = 3;
	} else if (codec_input.frame_width *
				codec_input.frame_height == CODEC_RESOLUTION_1080P) {
		j = 4;
	} else if (codec_input.frame_width *
				codec_input.frame_height > CODEC_RESOLUTION_720P) {
		j = 5;
	} else if (codec_input.frame_width *
				codec_input.frame_height == CODEC_RESOLUTION_720P) {
		j = 6;
	} else {
		j = 7;
	}

	if (codec_input.decoder_or_encoder == CODEC_DECODER) {
		if (codec_input.vpu_ver == VPU_VERSION_IRIS4_1P) {
			ret = decoder_vpp_cycles_1pipe_iris4[i][j];
		} else {
			ret = decoder_vpp_cycles_2pipe_iris4[i][j];
		}
	} else {
		if (codec_input.vpu_ver == VPU_VERSION_IRIS4_1P) {
			ret = encoder_vpp_cycles_1pipe_iris4[i][j];
		} else {
			ret = encoder_vpp_cycles_2pipe_iris4[i][j];
		}
	}
	return ret;
}

static int calculate_vsp_min_freq(struct api_calculation_input codec_input,
		struct api_calculation_freq_output *codec_output)
{
	/*
	 * VSP calculation
	 * different methodology from Lahaina
	 */
	u32 vsp_hw_min_frequency = 0;
	u32 allintra_bitrate_533 = 343; //@533MHz  max UHD30 or UHD60 HDR10; HEVC ONLY
	u32 lossless_bitrate_533 = 720; //@533MHz  max 720p30 HDR10; HEVC only

	if (codec_input.codec == CODEC_APV) {
		codec_output->vsp_min_freq = 0;
		return 0;
	}

	/* UInt32 decoder_vsp_fw_overhead = 100 + 5; // amplified by 100x */
	u32 fw_sw_vsp_offset = 1000 + 55;      /* amplified by 1000x */
	/*
	 * Ignore fw_sw_vsp_offset, as this is baked into the reference bitrate tables.
	 *  As a consequence remove x1000 multipler as well.
	 */
	u32 codec = codec_input.codec;
	/* UInt32 *bitratetable; */
	u32 pixle_count = codec_input.frame_width *
		codec_input.frame_height * codec_input.frame_rate;

	u8 bitrate_entry = get_bitrate_entry(pixle_count); /* TODO EXTRACT */
	u32 freq_4bitrate = (codec_input.decoder_or_encoder == CODEC_DECODER) ?
					frequency_table_iris4[0][3] : frequency_table_iris4[0][5];

	input_bitrate_fp = ((u32)(codec_input.bitrate_mbps * 100 + 99)) / 100;

	/*
	 * bitrate was profiled at 444MHz for legacy codec
	 * bitrate was profiled at 533MHz for av1
	 */
	vsp_hw_min_frequency = freq_4bitrate * input_bitrate_fp * 1000;

	if (codec_input.codec == CODEC_AV1 && bitrate_entry == 1)
		vsp_hw_min_frequency = frequency_table_iris4[0][2] *
			input_bitrate_fp * 1000;

	if (codec_input.vsp_vpp_mode == CODEC_VSPVPP_MODE_2S) {
		u32 corner_bitrate = bitrate_table_iris4_2stage_fp[codec][bitrate_entry];

		if (codec_input.codec == CODEC_HEVC) {
			if (codec_input.hierachical_layer == CODEC_GOP_LOSSLESS) {
				vsp_hw_min_frequency = frequency_table_iris4[0][2] *
					input_bitrate_fp * 1000;
				corner_bitrate = lossless_bitrate_533;
			} else if (codec_input.hierachical_layer == CODEC_GOP_IONLY) {
				vsp_hw_min_frequency = frequency_table_iris4[0][2] *
					input_bitrate_fp * 1000;
				corner_bitrate = allintra_bitrate_533;
			}
		}
		vsp_hw_min_frequency = vsp_hw_min_frequency +
			(corner_bitrate * fw_sw_vsp_offset - 1);
		vsp_hw_min_frequency = DIV_ROUND_UP(vsp_hw_min_frequency,
			corner_bitrate * fw_sw_vsp_offset);
	} else {
		vsp_hw_min_frequency = vsp_hw_min_frequency +
			(bitrate_table_iris4_1stage_fp[codec][bitrate_entry] *
			fw_sw_vsp_offset - 1);
		vsp_hw_min_frequency = DIV_ROUND_UP(vsp_hw_min_frequency,
			(bitrate_table_iris4_1stage_fp[codec][bitrate_entry]) *
				fw_sw_vsp_offset);
	}

	codec_output->vsp_min_freq = vsp_hw_min_frequency;
	return 0;
}

static int calculate_apv_freq(struct api_calculation_input codec_input,
		struct api_calculation_freq_output *codec_output)
{
	u32 vsp_hw_min_frequency = 0;
	u64 vpp_hw_min_frequency = 0;
	u32 tensilica_min_frequency = 0;
	u32 fmin = 0, worker_Fmin = 0;
	u32 index = 0, temp = 0, len = 0;
	u32 ppc_index = 0;
	u16 ppc = (codec_input.decoder_or_encoder == CODEC_DECODER) ? 6 : 8;

	if (codec_input.decoder_or_encoder == CODEC_DECODER) {
		vpp_hw_min_frequency =
			(codec_input.frame_width *
			 codec_input.frame_height * 2 *
			 codec_input.frame_rate + ppc - 1) / ppc;
	} else {
		ppc = 800;
		ppc_index = codec_input.linear_ipb;

		if (codec_input.video_adv_feature == 0) {
			if (codec_input.format_10bpp == 3)
				ppc = apv_encoder_ppc[ppc_index][0];
			else if (codec_input.format_10bpp <= 1)
				ppc = apv_encoder_ppc[ppc_index][2];
			else
				d_vpr_e("invalid format selected for APVe\n");
		} else if (codec_input.video_adv_feature == 1) {
			if (codec_input.format_10bpp == 3)
				ppc = apv_encoder_ppc[ppc_index][1];
			else if (codec_input.format_10bpp <= 1)
				ppc = apv_encoder_ppc[ppc_index][3];
			else
				d_vpr_e("invalid format selected for APVe\n");
		}
		vpp_hw_min_frequency =
			(codec_input.frame_width *
			 codec_input.frame_height * 2 *
			 codec_input.frame_rate + ppc - 1) / ppc * 100;
	}
	vpp_hw_min_frequency = (vpp_hw_min_frequency * 104 + 99) / 100;

	if (codec_input.decoder_or_encoder == CODEC_DECODER) {
		tensilica_min_frequency =
			(DECODER_VPP_FW_OVERHEAD_IRIS4_APVD *
			 codec_input.frame_rate * TENSILICA_CORE_RATIO_IRIS4 + 9) / 10;
		vpp_hw_min_frequency +=
			DECODER_VPP_FW_OVERHEAD_IRIS4_APVD * codec_input.frame_rate;
		//small resolution (<960*540) consideration
		worker_Fmin = DECODER_WORKER_FW_OVERHEAD_NONAV1_IRIS4 *
						codec_input.frame_rate / 1000 / 1000;
	} else {
		tensilica_min_frequency =
			(ENCODER_VPP_FW_OVERHEAD_IRIS4_APVE *
			 codec_input.frame_rate * TENSILICA_CORE_RATIO_IRIS4 + 9) / 10;
		vpp_hw_min_frequency +=
			ENCODER_VPP_FW_OVERHEAD_IRIS4_APVE * codec_input.frame_rate;
		//small resolution (<960*540) consideration
		worker_Fmin =
			ENCODER_WORKER_FW_OVERHEAD_IRIS4 * codec_input.frame_rate / 1000 / 1000;
	}

	fmin = (u32)vpp_hw_min_frequency;

	if (fmin < worker_Fmin)
		fmin = worker_Fmin;

	temp = fmin / 1000 / 1000;
	len = ARRAY_SIZE(frequency_table_iris4[0]);
	while (index < len - 1) {
		if (temp >= frequency_table_iris4[0][index])
			break;
		index++;
	}
	fmin += sw_overhead_iris4[index] * codec_input.frame_rate;

	fmin = (fmin + 99999) / 1000 / 1000;

	codec_output->apv_min_freq = (u32)((vpp_hw_min_frequency + 99999) / 1000 / 1000);
	codec_output->vsp_min_freq = vsp_hw_min_frequency / 1000 / 1000;
	codec_output->tensilica_min_freq = (tensilica_min_frequency + 99999) / 1000 / 1000;
	codec_output->vpp_min_freq = 0;
	codec_output->hw_min_freq = fmin;

	return 0;
}

static int calculate_vpp_min_freq(struct api_calculation_input codec_input,
		struct api_calculation_freq_output *codec_output)
{
	u32 vpp_hw_min_frequency = 0;
	u32 fmin = 0;
	u32 tensilica_min_frequency = 0;
	u32 codec_mbspersession;
	u32 vpp_fw_overhead = DECODER_VPP_FW_OVERHEAD_IRIS4_NONAV1D;
	u32 vppvsp1stage_fw_overhead = DECODER_VPPVSP1STAGE_FW_OVERHEAD_IRIS4_NONAV1D;
	u32 worker_fw_overhead = DECODER_WORKER_FW_OVERHEAD_NONAV1_IRIS4;
	u32 vsp_hw_min_frequency = codec_output->vsp_min_freq * 1000 * 1000;
	u32 worker_Fmin = 0;
	u32 index = 0, temp = 0, len = 0;
	u32 vpp_target_clk_per_mb = get_vpp_cycles(codec_input);
	u32 hier_layer = 0;
	u32 gop_complexity = 0;


	if (codec_input.codec == CODEC_APV) {
		calculate_apv_freq(codec_input, codec_output);
		return 0;
	}

	codec_mbspersession =
		calculate_number_mbs_iris4(codec_input.frame_width,
		codec_input.frame_height, codec_input.lcu_size) *
		codec_input.frame_rate;

	/* Section 2. 0  VPP/VSP calculation */
	if (codec_input.decoder_or_encoder == CODEC_DECODER) { /* decoder */
		vpp_hw_min_frequency =
			(vpp_target_clk_per_mb * codec_mbspersession +
			 codec_input.pipe_num - 1) / (codec_input.pipe_num);
		if (codec_input.codec == CODEC_AV1) {
			vpp_fw_overhead = DECODER_VPP_FW_OVERHEAD_IRIS4_AV1D;
			vppvsp1stage_fw_overhead = DECODER_VPPVSP1STAGE_FW_OVERHEAD_IRIS4_AV1D;
			worker_fw_overhead = DECODER_WORKER_FW_OVERHEAD_AV1_IRIS4;
		}
	} else { /* encoder */
		/* Decide LP/HQ */
		u8 hq_mode = 0;

		vpp_fw_overhead = ENCODER_VPP_FW_OVERHEAD_IRIS4;
		vppvsp1stage_fw_overhead = ENCODER_VPPVSP1STAGE_FW_OVERHEAD_IRIS4;
		worker_fw_overhead = ENCODER_WORKER_FW_OVERHEAD_IRIS4;

		if (codec_input.frame_width * codec_input.frame_height <= 1920 * 1088) {
			if (codec_input.pipe_num > 1) {
				if (codec_input.frame_width * codec_input.frame_height *
					codec_input.frame_rate <= 1920 * 1088 * 60)
					hq_mode = 1;
			} else {
				if (codec_input.frame_width * codec_input.frame_height *
					codec_input.frame_rate <= 1920 * 1088 * 30)
					hq_mode = 1;
			}
		}

		codec_output->enc_hqmode = hq_mode;

		/* Section 1. 0 */
		/* TODO ONETIME call, should be in another place. */
		initialize_encoder_complexity_table();
		hier_layer = codec_input.hierachical_layer;
		gop_complexity =
		codec_encoder_gop_complexity_table_fp[hier_layer][CODEC_ENCODER_GOP_FACTORY_ENTRY];
		if (hq_mode)
			vpp_target_clk_per_mb = vpp_target_clk_per_mb << 1;
		else
			vpp_target_clk_per_mb =
				(vpp_target_clk_per_mb * gop_complexity + 99) / 100;

		vpp_hw_min_frequency =
			(vpp_target_clk_per_mb * codec_mbspersession +
			 codec_input.pipe_num - 1) / (codec_input.pipe_num);
		if (codec_input.video_adv_feature == FEATURE_LOOKAHEAD_ENCODE) {
			if (codec_input.hierachical_layer == CODEC_GOP_IPP)
				vpp_hw_min_frequency = vpp_hw_min_frequency << 1;
			else /* for IBP, IBBP, IBBBP, IBBBBP etc cases */
				vpp_hw_min_frequency = (vpp_hw_min_frequency * 3) >> 1;
		}
	}

	if (codec_input.vsp_vpp_mode == CODEC_VSPVPP_MODE_2S) {
		tensilica_min_frequency =
			(vpp_fw_overhead * codec_input.frame_rate *
			 TENSILICA_CORE_RATIO_IRIS4 + 9) / 10;
		vpp_hw_min_frequency += vpp_fw_overhead * codec_input.frame_rate;
	} else {
		tensilica_min_frequency =
			(vppvsp1stage_fw_overhead * codec_input.frame_rate *
			 TENSILICA_CORE_RATIO_IRIS4 + 9) / 10;
		vpp_hw_min_frequency += vppvsp1stage_fw_overhead * codec_input.frame_rate;
	}

	fmin = (vpp_hw_min_frequency > vsp_hw_min_frequency) ?
				vpp_hw_min_frequency : vsp_hw_min_frequency;

	//small resolution (<960*540) consideration
	worker_Fmin = worker_fw_overhead * codec_input.frame_rate / 1000 / 1000;
	if (fmin < worker_Fmin)
		fmin = worker_Fmin;

	temp = fmin / 1000 / 1000;
	len = ARRAY_SIZE(frequency_table_iris4[0]);
	while (index < len - 1) {
		if (temp >= frequency_table_iris4[0][index])
			break;
		index++;
	}
	fmin += sw_overhead_iris4[index] * codec_input.frame_rate;

	fmin = (fmin + 99999) / 1000 / 1000;

	codec_output->vpp_min_freq = (vpp_hw_min_frequency + 99999) / 1000 / 1000;
	codec_output->vsp_min_freq = vsp_hw_min_frequency / 1000 / 1000;
	codec_output->tensilica_min_freq = (tensilica_min_frequency + 99999) / 1000 / 1000;
	codec_output->apv_min_freq = 0;
	codec_output->hw_min_freq = fmin;

	return 0;
}

int msm_vidc_calculate_frequency_iris4(struct api_calculation_input codec_input,
		struct api_calculation_freq_output *codec_output)
{
	int rc = 0;

	rc = calculate_vsp_min_freq(codec_input, codec_output);
	if (rc)
		return rc;

	rc = calculate_vpp_min_freq(codec_input, codec_output);
	if (rc)
		return rc;

	return rc;
}
