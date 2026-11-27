/*
 * dma_fence_init() got an extra flags argument in commit 9d0c96dc3839.
 * Defines MSM_VIDC_HAS_DMA_FENCE_INIT_FLAGS when this file compiles.
 */
#include <linux/dma-fence.h>

static __maybe_unused void msm_vidc_conftest(struct dma_fence *fence,
					     const struct dma_fence_ops *ops,
					     spinlock_t *lock)
{
	dma_fence_init(fence, ops, lock, 0, 0, 0);
}
