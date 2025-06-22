// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/bits.h>

#include "msm_vidc_power.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_core.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_buffer.h"
#include "venus_hfi.h"
#include "resources.h"
#include "msm_vidc_events.h"

/* Q16 Format */
#define MSM_VIDC_MIN_UBWC_COMPLEXITY_FACTOR (1 << 16)
#define MSM_VIDC_MAX_UBWC_COMPLEXITY_FACTOR (4 << 16)
#define MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO (1 << 16)
#define MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO (5 << 16)
#define PASSIVE_VOTE 1000

/*
 * The below table is a structural representation of the following table:
 *  Resolution |    Bitrate |              Compression Ratio          |
 * ............|............|.........................................|
 * Width Height|Average High|Avg_8bpc Worst_8bpc Avg_10bpc Worst_10bpc|
 *  1280    720|      7   14|    1.69       1.28      1.49        1.23|
 *  1920   1080|     20   40|    1.69       1.28      1.49        1.23|
 *  2560   1440|     32   64|     2.2       1.26      1.97        1.22|
 *  3840   2160|     42   84|     2.2       1.26      1.97        1.22|
 *  4096   2160|     44   88|     2.2       1.26      1.97        1.22|
 *  4096   2304|     48   96|     2.2       1.26      1.97        1.22|
 */
static const struct lut LUT[] = {
	{
		.frame_size = 1280 * 720,
		.frame_rate = 30,
		.bitrate = 14,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 1280 * 720,
		.frame_rate = 60,
		.bitrate = 22,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 1920 * 1088,
		.frame_rate = 30,
		.bitrate = 40,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 1920 * 1088,
		.frame_rate = 60,
		.bitrate = 64,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 2560 * 1440,
		.frame_rate = 30,
		.bitrate = 64,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 2560 * 1440,
		.frame_rate = 60,
		.bitrate = 102,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 3840 * 2160,
		.frame_rate = 30,
		.bitrate = 84,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 3840 * 2160,
		.frame_rate = 60,
		.bitrate = 134,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2160,
		.frame_rate = 30,
		.bitrate = 88,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2160,
		.frame_rate = 60,
		.bitrate = 141,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2304,
		.frame_rate = 30,
		.bitrate = 96,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2304,
		.frame_rate = 60,
		.bitrate = 154,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
};

/**
 * Utility function to enforce some of our assumptions.  Spam calls to this
 * in hotspots in code to double check some of the assumptions that we hold.
 */
struct lut const *__lut(int width, int height, int fps)
{
	int frame_size = height * width, c = 0;

	do {
		if (LUT[c].frame_size >= frame_size && LUT[c].frame_rate >= fps)
			return &LUT[c];
	} while (++c < ARRAY_SIZE(LUT));

	return &LUT[ARRAY_SIZE(LUT) - 1];
}

fp_t __compression_ratio(struct lut const *entry, int bpp)
{
	int c = 0;

	for (c = 0; c < COMPRESSION_RATIO_MAX; ++c) {
		if (entry->compression_ratio[c].bpp == bpp)
			return entry->compression_ratio[c].ratio;
	}

	WARN(true, "Shouldn't be here, LUT possibly corrupted?\n");
	return FP_ZERO; /* impossible */
}


void __dump(struct dump dump[], int len)
{
	int c = 0;

	for (c = 0; c < len; ++c) {
		char format_line[128] = "", formatted_line[128] = "";

		if (dump[c].val == DUMP_HEADER_MAGIC) {
			snprintf(formatted_line, sizeof(formatted_line), "%s\n",
					 dump[c].key);
		} else {
			bool fp_format = !strcmp(dump[c].format, DUMP_FP_FMT);

			if (!fp_format) {
				snprintf(format_line, sizeof(format_line),
						 "    %-35s: %s\n", dump[c].key,
						 dump[c].format);
				snprintf(formatted_line, sizeof(formatted_line),
						 format_line, dump[c].val);
			} else {
				size_t integer_part, fractional_part;

				integer_part = fp_int(dump[c].val);
				fractional_part = fp_frac(dump[c].val);
				snprintf(formatted_line, sizeof(formatted_line),
						 "    %-35s: %zd + %zd/%zd\n",
						 dump[c].key, integer_part,
						 fractional_part,
						 fp_frac_base());


			}
		}
		d_vpr_b("%s", formatted_line);
	}
}

static int fill_dynamic_stats(struct msm_vidc_inst *inst,
	struct vidc_bus_vote_data *vote_data)
{
	struct msm_vidc_input_cr_data *temp, *next;
	u32 cf = MSM_VIDC_MAX_UBWC_COMPLEXITY_FACTOR;
	u32 cr = MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO;
	u32 input_cr = MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO;
	u32 frame_size;

	if (inst->power.fw_cr)
		cr = inst->power.fw_cr;

	if (inst->power.fw_cf) {
		cf = inst->power.fw_cf;
		frame_size = (msm_vidc_get_mbs_per_frame(inst) / (32 * 8) * 3) / 2;
		if (frame_size)
			cf = cf / frame_size;
	}

	list_for_each_entry_safe(temp, next, &inst->enc_input_crs, list)
		input_cr = min(input_cr, temp->input_cr);

	vote_data->compression_ratio = cr;
	vote_data->complexity_factor = cf;
	vote_data->input_cr = input_cr;

	/* Sanitize CF values from HW */
	cf = clamp_t(u32, cf, MSM_VIDC_MIN_UBWC_COMPLEXITY_FACTOR,
			MSM_VIDC_MAX_UBWC_COMPLEXITY_FACTOR);
	cr = clamp_t(u32, cr, MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO,
			MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO);
	input_cr = clamp_t(u32, input_cr, MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO,
			MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO);

	/*
	 * CR = MIN means UBWC didn't compress at all,
	 * which is impossible unless the input yuv is pure while noise,
	 * so MIN means there is no valid CR info,
	 * set zero will let the bw calculation function use a predefined CR value instead,
	 * to avoid overvoting.
	 */
	if (input_cr == MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO) {
		i_vpr_h(inst, "%s: input_cr = %d is invalid value\n", __func__, input_cr);
		input_cr = 0;
	}
	vote_data->compression_ratio = cr;
	vote_data->complexity_factor = cf;
	vote_data->input_cr = input_cr;

	i_vpr_l(inst,
		"Input CR = %d Recon CR = %d Complexity Factor = %d\n",
		vote_data->input_cr, vote_data->compression_ratio,
		vote_data->complexity_factor);

	return 0;
}

static int msm_vidc_set_buses(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *temp;
	u64 total_bw_ddr = 0, total_bw_llcc = 0;

	core = inst->core;

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		/* skip for session where no input is there to process */
		if (!temp->max_input_data_size)
			continue;

		/* skip inactive session bus bandwidth */
		if (!temp->active)
			continue;

		if (temp->power.power_mode == VIDC_POWER_TURBO) {
			total_bw_ddr = total_bw_llcc = INT_MAX;
			break;
		}

		total_bw_ddr += temp->power.ddr_bw;
		total_bw_llcc += temp->power.sys_cache_bw;
	}
	mutex_unlock(&core->lock);

	/* Incase of no video frames to process ensure min passive voting for Tensilica */
	if (!total_bw_ddr)
		total_bw_ddr = PASSIVE_VOTE;

	if (msm_vidc_ddr_bw) {
		d_vpr_l("msm_vidc_ddr_bw %d\n", msm_vidc_ddr_bw);
		total_bw_ddr = msm_vidc_ddr_bw;
	}

	if (msm_vidc_llc_bw) {
		d_vpr_l("msm_vidc_llc_bw %d\n", msm_vidc_llc_bw);
		total_bw_llcc = msm_vidc_llc_bw;
	}

	rc = venus_hfi_scale_buses(inst, total_bw_ddr, total_bw_llcc);
	if (rc)
		return rc;

	return 0;
}

static int msm_vidc_scale_buses(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct vidc_bus_vote_data *vote_data;
	struct v4l2_format *out_f;
	struct v4l2_format *inp_f;
	u32 operating_rate, frame_rate;

	core = inst->core;
	if (!core->resource) {
		i_vpr_e(inst, "%s: invalid resource params\n", __func__);
		return -EINVAL;
	}
	vote_data = &inst->bus_data;

	vote_data->power_mode = VIDC_POWER_NORMAL;
	if (inst->power.buffer_counter < DCVS_WINDOW || is_image_session(inst))
		vote_data->power_mode = VIDC_POWER_TURBO;

	if (vote_data->power_mode == VIDC_POWER_TURBO)
		goto set_buses;

	out_f = &inst->fmts[OUTPUT_PORT];
	inp_f = &inst->fmts[INPUT_PORT];

	vote_data->codec = inst->codec;
	vote_data->input_width = inp_f->fmt.pix_mp.width;
	vote_data->input_height = inp_f->fmt.pix_mp.height;
	vote_data->output_width = out_f->fmt.pix_mp.width;
	vote_data->output_height = out_f->fmt.pix_mp.height;
	vote_data->lcu_size = (inst->codec == MSM_VIDC_HEVC ||
			inst->codec == MSM_VIDC_VP9) ? 32 : 16;
	if (inst->codec == MSM_VIDC_AV1)
		vote_data->lcu_size =
			inst->capabilities[SUPER_BLOCK].value ? 128 : 64;
	vote_data->fps = inst->max_rate;

	if (is_encode_session(inst)) {
		vote_data->domain = MSM_VIDC_ENCODER;
		vote_data->bitrate = inst->capabilities[BIT_RATE].value;
		vote_data->rotation = inst->capabilities[ROTATION].value;
		vote_data->b_frames_enabled =
			inst->capabilities[B_FRAME].value > 0;

		/* scale bitrate if operating rate is larger than frame rate */
		frame_rate = msm_vidc_get_frame_rate(inst);
		operating_rate = inst->max_rate;
		if (frame_rate && operating_rate && operating_rate > frame_rate)
			vote_data->bitrate = (vote_data->bitrate / frame_rate) * operating_rate;

		vote_data->num_formats = 1;
		vote_data->color_formats[0] = v4l2_colorformat_to_driver(inst,
			inst->fmts[INPUT_PORT].fmt.pix_mp.pixelformat, __func__);
		vote_data->vpss_preprocessing_enabled =
			inst->capabilities[REQUEST_PREPROCESS].value;
	} else if (is_decode_session(inst)) {
		u32 color_format;

		vote_data->domain = MSM_VIDC_DECODER;
		vote_data->bitrate = inst->max_input_data_size * vote_data->fps * 8;
		color_format = v4l2_colorformat_to_driver(inst,
			inst->fmts[OUTPUT_PORT].fmt.pix_mp.pixelformat, __func__);
		if (is_linear_colorformat(color_format)) {
			vote_data->num_formats = 2;
			/*
			 * 0 index - dpb colorformat
			 * 1 index - opb colorformat
			 */
			if (is_10bit_colorformat(color_format))
				vote_data->color_formats[0] = MSM_VIDC_FMT_TP10C;
			else
				vote_data->color_formats[0] = MSM_VIDC_FMT_NV12;

			/* APV decoder supports only P210C for DPB buffers */
			if (inst->codec == MSM_VIDC_APV)
				vote_data->color_formats[0] = MSM_VIDC_FMT_P210C;

			vote_data->color_formats[1] = color_format;
		} else if (inst->codec == MSM_VIDC_AV1 &&
			inst->capabilities[FILM_GRAIN].value) {
			/*
			 * UBWC formats with AV1 film grain requires dpb-opb
			 * split mode
			 */
			vote_data->num_formats = 2;
			vote_data->color_formats[0] =
				vote_data->color_formats[1] = color_format;
		} else {
			vote_data->num_formats = 1;
			vote_data->color_formats[0] = color_format;
		}
	}
	vote_data->work_mode = inst->capabilities[STAGE].value;
	if (core->resource->subcache_set.set_to_fw)
		vote_data->use_sys_cache = true;
	vote_data->num_vpp_pipes = core->capabilities[NUM_VPP_PIPE].value;
	fill_dynamic_stats(inst, vote_data);

	call_session_op(core, calc_bw, inst, vote_data);

	inst->power.ddr_bw = vote_data->calc_bw_ddr;
	inst->power.sys_cache_bw = vote_data->calc_bw_llcc;

	if (!inst->stats.avg_bw_llcc)
		inst->stats.avg_bw_llcc = inst->power.sys_cache_bw;
	else
		inst->stats.avg_bw_llcc =
			(inst->stats.avg_bw_llcc + inst->power.sys_cache_bw) / 2;

	if (!inst->stats.avg_bw_ddr)
		inst->stats.avg_bw_ddr = inst->power.ddr_bw;
	else
		inst->stats.avg_bw_ddr =
			(inst->stats.avg_bw_ddr + inst->power.ddr_bw) / 2;

set_buses:
	inst->power.power_mode = vote_data->power_mode;
	rc = msm_vidc_set_buses(inst);
	if (rc)
		return rc;

	return 0;
}

int msm_vidc_apply_dcvs(struct msm_vidc_inst *inst)
{
	int rc = 0;
	int bufs_with_fw = 0;
	struct msm_vidc_power *power;

	/* skip dcvs */
	if (!inst->power.dcvs_mode)
		return 0;

	power = &inst->power;

	if (is_decode_session(inst)) {
		bufs_with_fw = msm_vidc_num_buffers(inst,
			MSM_VIDC_BUF_OUTPUT, MSM_VIDC_ATTR_QUEUED);
	} else {
		bufs_with_fw = msm_vidc_num_buffers(inst,
			MSM_VIDC_BUF_INPUT, MSM_VIDC_ATTR_QUEUED);
	}

	/* +1 as one buffer is going to be queued after the function */
	bufs_with_fw += 1;

	/*
	 * DCVS decides clock level based on below algorithm
	 *
	 * Limits :
	 * min_threshold : Buffers required for reference by FW.
	 * nom_threshold : Midpoint of Min and Max thresholds
	 * max_threshold : Min Threshold + DCVS extra buffers, allocated
	 *				   for smooth flow.
	 * 1) When buffers outside FW are reaching client's extra buffers,
	 *    FW is slow and will impact pipeline, Increase clock.
	 * 2) When pending buffers with FW are less than FW requested,
	 *    pipeline has cushion to absorb FW slowness, Decrease clocks.
	 * 3) When DCVS has engaged(Inc or Dec):
	 *    For decode:
	 *        - Pending buffers with FW transitions past the nom_threshold,
	 *        switch to calculated load, this smoothens the clock transitions.
	 *    For encode:
	 *        - Always switch to calculated load.
	 * 4) Otherwise maintain previous Load config.
	 */
	if (bufs_with_fw >= power->max_threshold) {
		power->dcvs_flags = MSM_VIDC_DCVS_INCR;
		goto exit;
	} else if (bufs_with_fw < power->min_threshold) {
		power->dcvs_flags = MSM_VIDC_DCVS_DECR;
		goto exit;
	}

	/* encoder: dcvs window handling */
	if (is_encode_session(inst)) {
		power->dcvs_flags = 0;
		goto exit;
	}

	/* decoder: dcvs window handling */
	if ((power->dcvs_flags & MSM_VIDC_DCVS_DECR && bufs_with_fw >= power->nom_threshold) ||
		(power->dcvs_flags & MSM_VIDC_DCVS_INCR && bufs_with_fw <= power->nom_threshold)) {
		power->dcvs_flags = 0;
	}

exit:
	i_vpr_p(inst, "dcvs: bufs_with_fw %d th[%d %d %d] flags %#x\n",
		bufs_with_fw, power->min_threshold,
		power->nom_threshold, power->max_threshold,
		power->dcvs_flags);

	return rc;
}

int msm_vidc_scale_power(struct msm_vidc_inst *inst, bool scale_buses)
{
	struct msm_vidc_core *core;
	struct msm_vidc_buffer *vbuf;
	u32 data_size = 0;
	u32 cnt = 0, idx;
	u32 fps;
	u32 frame_rate, operating_rate;
	u32 timestamp_rate = 0, input_rate = 0;
	struct msm_vidc_inst *temp;
	u64 curr_time_ns;

	core = inst->core;
	curr_time_ns = ktime_get_ns();

	/* skip scale_power when using hw virtualization */
	if (core->full_virtualization_data.virtualization_en)
		return 0;

	if (!inst->active) {
		/* scale buses for inactive -> active session */
		scale_buses = true;
		inst->active = true;
	}

	/*
	 * consider avg. filled length in decode batching case
	 * to avoid overvoting for the entire batch due to single
	 * frame with huge filled length
	 */
	if (inst->decode_batch.enable) {
		list_for_each_entry(vbuf, &inst->buffers.input.list, list) {
			if (vbuf->attr & MSM_VIDC_ATTR_DEFERRED ||
				vbuf->attr & MSM_VIDC_ATTR_QUEUED) {
				data_size += vbuf->data_size;
				cnt++;
			}
		}
		if (cnt)
			data_size /= cnt;
	} else {
		list_for_each_entry(vbuf, &inst->buffers.input.list, list)
			data_size = max(data_size, vbuf->data_size);
	}
	inst->max_input_data_size = data_size;

	frame_rate = msm_vidc_get_frame_rate(inst);
	operating_rate = msm_vidc_get_operating_rate(inst);
	fps = max(frame_rate, operating_rate);
	/*
	 * Consider input queuing rate power scaling in below scenarios
	 * decoder: non-realtime and realtime as well because client
	 *          may not set the frame rate / operating rate and
	 *          we need to rely on input queue rate
	 * encoder: non-realtime only, for realtime client is expected to
	 *          queue input buffers at the set frame rate / operating rate
	 */
	if (is_decode_session(inst) ||
		(is_encode_session(inst) && !is_realtime_session(inst))) {
		/*
		 * when buffer detected fps is more than client set value by 12.5%,
		 * utilize buffer detected fps to scale clock.
		 */
		timestamp_rate = msm_vidc_get_timestamp_rate(inst);
		input_rate = msm_vidc_get_input_rate(inst);
		if (timestamp_rate > (fps + fps / 8))
			fps = timestamp_rate;

		if (input_rate > fps) {
			fps = input_rate;
			/*
			 * add 6.25% more fps for NRT session to increase power to make
			 * firmware processing little faster than client queuing rate
			 */
			if (!is_realtime_session(inst))
				fps = fps + fps / 16;
		}
	}
	inst->max_rate = fps;

	/* update current session last active ts */
	inst->last_active_time_ns = curr_time_ns;

	/* no pending inputs - skip scale power */
	if (!inst->max_input_data_size)
		return 0;

	if (is_decode_session(inst) && is_slice_decode_enabled(inst))
		inst->max_input_data_size = max(inst->max_input_data_size,
				inst->slice_decode.frame_data_size);

	core_lock(core, __func__);
	/* detect inactive session */
	list_for_each_entry(temp, &core->instances, list) {
		/* skip current(active) session  */
		if (temp == inst)
			continue;

		if (!is_active_session(temp->last_active_time_ns, curr_time_ns))
			temp->active = false;
	}
	core_unlock(core, __func__);

	idx = call_session_op(core, scale_clocks, inst);
	if (venus_hfi_scale_clocks(inst, idx))
		i_vpr_e(inst, "failed to scale clock\n");

	if (scale_buses) {
		if (msm_vidc_scale_buses(inst))
			i_vpr_e(inst, "failed to scale bus\n");
	}

	i_vpr_hp(inst,
		"power: inst: clk %lld %lld %lld %lld %lld ddr %d llcc %d dcvs flags %#x fps %u (%u %u %u %u) core: clk_idx %d ddr %lld llcc %lld\n",
		inst->power.min_freq, inst->power.min_vpp_freq, inst->power.min_apv_freq,
		inst->power.min_bse_freq, inst->power.min_tensilica_freq,
		inst->power.ddr_bw, inst->power.sys_cache_bw, inst->power.dcvs_flags,
		inst->max_rate, frame_rate, operating_rate, timestamp_rate,
		input_rate, core->power.clk_freq_idx, core->power.bw_ddr, core->power.bw_llcc);

	trace_msm_vidc_perf_power_scale(inst, core->power.clk_freq_idx,
					core->power.bw_ddr,
					core->power.bw_llcc);

	return 0;
}

static void msm_vidc_dcvs_data_reset(struct msm_vidc_inst *inst)
{
	struct msm_vidc_power *dcvs;
	u32 min_count, actual_count, max_count;

	dcvs = &inst->power;
	if (is_encode_session(inst)) {
		min_count = inst->buffers.input.min_count;
		actual_count = inst->buffers.input.actual_count;
		max_count = min((min_count + DCVS_ENC_EXTRA_INPUT_BUFFERS), actual_count);
	} else if (is_decode_session(inst)) {
		min_count = inst->buffers.output.min_count;
		actual_count = inst->buffers.output.actual_count;
		max_count = min((min_count + DCVS_DEC_EXTRA_OUTPUT_BUFFERS), actual_count);
	} else {
		i_vpr_e(inst, "%s: invalid domain type %d\n",
			__func__, inst->domain);
		return;
	}

	dcvs->min_threshold = min_count;
	dcvs->max_threshold = max_count;
	dcvs->dcvs_window = min_count < max_count ? max_count - min_count : 0;
	dcvs->nom_threshold = dcvs->min_threshold + (dcvs->dcvs_window / 2);
	dcvs->dcvs_flags = 0;

	i_vpr_p(inst, "%s: dcvs: thresholds [%d %d %d] flags %#x\n",
		__func__, dcvs->min_threshold,
		dcvs->nom_threshold, dcvs->max_threshold,
		dcvs->dcvs_flags);
}

void msm_vidc_power_data_reset(struct msm_vidc_inst *inst)
{
	int rc = 0;

	i_vpr_hp(inst, "%s\n", __func__);

	msm_vidc_dcvs_data_reset(inst);

	inst->power.buffer_counter = 0;
	inst->power.fw_cr = 0;
	inst->power.fw_cf = INT_MAX;
	inst->power.fw_av1_tile_rows = 1;
	inst->power.fw_av1_tile_columns = 1;

	rc = msm_vidc_scale_power(inst, true);
	if (rc)
		i_vpr_e(inst, "%s: failed to scale power\n", __func__);
}
