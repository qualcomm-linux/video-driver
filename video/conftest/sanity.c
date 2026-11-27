/*
 * Basic check that the conftest compile command works at all.
 * If this fails, still keep the default behaviour.
 */
#include <linux/kernel.h>

static __maybe_unused int msm_vidc_conftest(void)
{
	return 0;
}
