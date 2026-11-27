/*
 * dma_buf_(un)map_attachment_unlocked() were renamed to
 * dma_buf_(un)map_sgt_unlocked() in commit 77e0b2d2f4f3.
 * Defines MSM_VIDC_HAS_DMA_BUF_MAP_SGT when this file compiles.
 */
#include <linux/dma-buf.h>

static __maybe_unused void msm_vidc_conftest(struct dma_buf_attachment *attach)
{
	struct sg_table *table;

	table = dma_buf_map_sgt_unlocked(attach, DMA_BIDIRECTIONAL);
	dma_buf_unmap_sgt_unlocked(attach, table, DMA_BIDIRECTIONAL);
}
