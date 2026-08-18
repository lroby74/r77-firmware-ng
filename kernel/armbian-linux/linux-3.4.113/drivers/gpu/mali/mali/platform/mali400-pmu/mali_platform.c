/*
 * Copyright (C) 2010-2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"
#include "mali_mem_validation.h"

#include <linux/mali/mali_utgard.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/clk/sunxi_name.h>
#include <linux/clk-private.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <mach/irqs.h>
#include <mach/sys_config.h>
#include <mach/platform.h>

static struct clk *mali_clk = NULL;
static struct clk *gpu_pll  = NULL;

/*
 * Manopola della frequenza GPU, aggiunta per la Retron 77.
 *
 * Il driver originale fissava 252 MHz e non esponeva niente: la frequenza
 * non si poteva ne' leggere ne' cambiare. Qui compare in
 *
 *     /sys/kernel/gpu/freq        (in MHz, leggibile e scrivibile)
 *
 * Il valore di avvio resta 252, quindi senza che nessuno scriva niente il
 * comportamento e' identico a prima.
 */
#define MALI_FREQ_BOOT  252
#define MALI_FREQ_MIN   120
#define MALI_FREQ_MAX   600

static int mali_freq_mhz = MALI_FREQ_BOOT;
static struct kobject *mali_gpu_kobj = NULL;

static int mali_apply_freq(int mhz)
{
	if (mhz < MALI_FREQ_MIN || mhz > MALI_FREQ_MAX)
		return -EINVAL;

	if (!gpu_pll || !mali_clk)
		return -ENODEV;

	if (clk_set_rate(gpu_pll, (unsigned long)mhz * 1000 * 1000)) {
		printk(KERN_ERR "mali: gpu pll a %d MHz rifiutata\n", mhz);
		return -EIO;
	}

	if (clk_set_rate(mali_clk, (unsigned long)mhz * 1000 * 1000)) {
		printk(KERN_ERR "mali: mali clk a %d MHz rifiutata\n", mhz);
		return -EIO;
	}

	mali_freq_mhz = mhz;
	pr_info("mali clk: %d MHz\n", mhz);
	return 0;
}

static ssize_t mali_freq_show(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", mali_freq_mhz);
}

static ssize_t mali_freq_store(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	int mhz, err;

	if (sscanf(buf, "%d", &mhz) != 1)
		return -EINVAL;

	err = mali_apply_freq(mhz);

	return err ? err : (ssize_t)count;
}

static struct kobj_attribute mali_freq_attr =
	__ATTR(freq, 0644, mali_freq_show, mali_freq_store);

_mali_osk_errcode_t mali_platform_init(void)
{
	int freq = MALI_FREQ_BOOT;

	gpu_pll = clk_get(NULL, PLL_GPU_CLK);

	if (!gpu_pll || IS_ERR(gpu_pll))	{
		printk(KERN_ERR "Failed to get gpu pll clock!\n");
		return -1;
	}

	mali_clk = clk_get(NULL, GPU_CLK);
	if (!mali_clk || IS_ERR(mali_clk)) {
		printk(KERN_ERR "Failed to get mali clock!\n");
		return -1;
	}

	if (clk_set_rate(gpu_pll, freq * 1000 * 1000)) {
		printk(KERN_ERR "Failed to set gpu pll clock!\n");
		return -1;
	}

	if (clk_set_rate(mali_clk, freq * 1000 * 1000)) {
		printk(KERN_ERR "Failed to set mali clock!\n");
		return -1;
	}
	
	if (mali_clk->enable_count == 0) {
		if (clk_prepare_enable(gpu_pll))
			printk(KERN_ERR "Failed to enable gpu pll!\n");

		if (clk_prepare_enable(mali_clk))
			printk(KERN_ERR "Failed to enable mali clock!\n");
	}

	pr_info("mali clk: %d MHz\n", freq);
	mali_freq_mhz = freq;

	/* Se la manopola non nasce, il driver funziona lo stesso: si perde
	   solo la possibilita' di cambiare frequenza a caldo. */
	mali_gpu_kobj = kobject_create_and_add("gpu", kernel_kobj);
	if (!mali_gpu_kobj)
		printk(KERN_ERR "mali: /sys/kernel/gpu non creato\n");
	else if (sysfs_create_file(mali_gpu_kobj, &mali_freq_attr.attr))
		printk(KERN_ERR "mali: /sys/kernel/gpu/freq non creato\n");

    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	if (mali_gpu_kobj) {
		sysfs_remove_file(mali_gpu_kobj, &mali_freq_attr.attr);
		kobject_put(mali_gpu_kobj);
		mali_gpu_kobj = NULL;
	}

	if (mali_clk->enable_count == 1) {
		clk_disable_unprepare(mali_clk);
		clk_disable_unprepare(gpu_pll);
	}

    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
    MALI_SUCCESS;
}

void mali_gpu_utilization_handler(u32 utilization)
{
}

void set_mali_parent_power_domain(void* dev)
{
}


