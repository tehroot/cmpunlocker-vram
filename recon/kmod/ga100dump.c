// SPDX-License-Identifier: GPL-2.0
/*
 * ga100dump — kmod fallback for ga100-bar0-dump.
 *
 * Use only when the sysfs resource0 mmap is unavailable (no iomem=relaxed on
 * the cmdline, e.g. a cloud image whose bootloader you cannot edit). In-kernel
 * ioremap is not subject to iomem_is_exclusive(), so this path always works.
 *
 * Read-only. Prints the same offset/value lines as the userspace tool to the
 * kernel log.
 *
 *   make -C recon/kmod
 *   sudo insmod recon/kmod/ga100dump.ko
 *   sudo dmesg | sed -n 's/^.*ga100dump: //p' > a100.txt
 *   sudo rmmod ga100dump
 *
 * Requires kernel headers and an unlocked kernel (Secure Boot / lockdown will
 * refuse an unsigned module).
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/io.h>

static const unsigned short ids[] = {
	0x20b0, 0x20b1, 0x20b2, 0x20b3, 0x20b5, 0x20b6, 0x20b7, 0x20b8,
	0x20bb, 0x20bd, 0x20be, 0x20bf, 0x20c2, 0x20f0, 0x20f1, 0x20f2,
};

static const struct { u32 off; const char *name; } named[] = {
	{ 0x118f78, "gate bit30" },
	{ 0x11823c, "gate bits[11:10]" },
	{ 0x0012e0, "gate bit0" },
	{ 0x088084, "LnkCap" },
	{ 0x088088, "LnkCtlSta" },
	{ 0x0880a4, "LnkCap2" },
	{ 0x0880a8, "LnkCtl2" },
	{ 0x08841c, "PRIV_MISC_1" },
	{ 0x08860c, "VSEC_DEVICE" },
	{ 0x08872c, "publish trigger" },
	{ 0x08c040, "MAX_RATE" },
	{ 0x08c1c0, "PL_LINK_RATE" },
	{ 0x08c2c0, "XP_CYA_0" },
	{ 0x8204d8, "fuse dev-id" },
	{ 0x82056c, "fuse dev-id" },
	{ 0x82057c, "fuse gen?" },
	{ 0x820580, "fuse gen?" },
	{ 0x820584, "fuse ?" },
	{ 0x118e80, "uphy" },
	{ 0x118e90, "uphy (0x00068c00 == ran)" },
	{ 0x118bb4, "uphy" },
	{ 0x137678, "per-lane" },
	{ 0x1376f8, "per-lane" },
	{ 0x132a00, "lanemap0" },
	{ 0x132a04, "lanemap1" },
	{ 0x132a08, "lanemap2" },
	{ 0x132a0c, "lanemap3" },
	{ 0x132a10, "lanemap4" },
	{ 0x132a14, "lanemap5" },
};

static const struct { u32 base, len; const char *name; } regions[] = {
	{ 0x0088000, 0x100,  "xve" },
	{ 0x008c000, 0x400,  "xp" },
	{ 0x0118000, 0x1000, "pgc6" },
	{ 0x0132900, 0x200,  "lanemap" },
	{ 0x0137600, 0x100,  "perlane" },
	{ 0x0820400, 0x400,  "fuse" },
};

static int __init ga100dump_init(void)
{
	struct pci_dev *pdev = NULL;
	void __iomem *bar0;
	int i, j, found = 0;
	u32 v;

	for (i = 0; i < ARRAY_SIZE(ids) && !found; i++) {
		pdev = pci_get_device(PCI_VENDOR_ID_NVIDIA, ids[i], NULL);
		if (pdev)
			found = 1;
	}
	if (!found) {
		pr_err("ga100dump: no GA100-class device found\n");
		return -ENODEV;
	}

	/* Map BAR0 without claiming the region — the nvidia driver owns it and
	 * we only ever read. */
	bar0 = ioremap(pci_resource_start(pdev, 0), 0x1000000);
	if (!bar0) {
		pr_err("ga100dump: ioremap failed\n");
		pci_dev_put(pdev);
		return -ENOMEM;
	}

	pr_info("ga100dump: # bdf=%s id=%04x:%04x boot0=0x%08x\n",
		pci_name(pdev), pdev->vendor, pdev->device, readl(bar0));

	pr_info("ga100dump: === named ===\n");
	for (i = 0; i < ARRAY_SIZE(named); i++)
		pr_info("ga100dump: 0x%07x 0x%08x  %s\n",
			named[i].off, readl(bar0 + named[i].off), named[i].name);

	v = readl(bar0 + 0x118f78);
	pr_info("ga100dump: DECISIVE 0x118f78 bit30 = %u\n", (v >> 30) & 1);

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		pr_info("ga100dump: === region 0x%07x +0x%x %s ===\n",
			regions[i].base, regions[i].len, regions[i].name);
		for (j = 0; j < regions[i].len; j += 4)
			pr_info("ga100dump: 0x%07x 0x%08x\n",
				regions[i].base + j,
				readl(bar0 + regions[i].base + j));
	}

	iounmap(bar0);
	pci_dev_put(pdev);
	pr_info("ga100dump: done -- rmmod ga100dump\n");
	return 0;
}

static void __exit ga100dump_exit(void) { }

module_init(ga100dump_init);
module_exit(ga100dump_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("read-only GA100 BAR0 register dump");
