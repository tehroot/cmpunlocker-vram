/*
 * ga100-bar0-dump — read-only BAR0 dump for GA100-class parts.
 *
 * Purpose: capture the register state of an *uncrippled* GA100 (A100 20b0/20b5/
 * 20f1, A30 20b7, or 20bb) so it can be diffed against a CMP 170HX (20c2).
 *
 * Read-only. Performs no writes, no config-space access beyond sysfs reads,
 * and does not touch the nvidia driver. Safe to run on a rented instance.
 *
 * Build:  cc -O2 -o ga100-bar0-dump ga100-bar0-dump.c
 * Run:    sudo ./ga100-bar0-dump [BDF] > a100.txt
 *
 * Requires iomem=relaxed on the kernel cmdline (sysfs resource0 mmap is
 * refused by iomem_is_exclusive() otherwise). If you cannot set it, use the
 * kmod fallback in recon/kmod/.
 *
 * Run with the nvidia driver loaded and after `nvidia-smi` has run, so the
 * capture reflects post-GSP-init state — that is what the 170HX values were
 * taken under.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/mman.h>

#define BAR0_LEN 0x1000000u /* 16 MiB */

static volatile uint32_t *bar0;

static uint32_t rd(uint32_t off) { return bar0[off >> 2]; }

/* PRI returns 0xBADFxxxx on priv-level / decode failures; 0xFFFFFFFF means the
 * cycle never reached the GPU. Mark both so they are not mistaken for data. */
static const char *note(uint32_t v)
{
	if (v == 0xFFFFFFFFu)      return "  <no-decode>";
	if ((v >> 16) == 0xBADFu)  return "  <priv-err>";
	return "";
}

struct reg { uint32_t off; const char *name; };

static const struct reg named[] = {
	/* --- the decisive one: gate input for the 0xcb00 path --- */
	{ 0x118f78, "PGC6_AON_?           (bit30 = 0xcb00 gate)" },
	{ 0x11823c, "PGC6_AON_?           (bits[11:10] == 2 expected)" },
	{ 0x0012e0, "?                    (bit0 == 0 expected)" },

	/* --- PCIe advertisement / training --- */
	{ 0x088084, "XVE_LINK_CAPABILITIES" },
	{ 0x088088, "XVE_LINK_CONTROL_STATUS" },
	{ 0x0880a4, "XVE_LINK_CAPABILITIES_2  (bits[7:1] = speed vector)" },
	{ 0x0880a8, "XVE_LINK_CONTROL_2       (bits[3:0] = target speed)" },
	{ 0x08841c, "XVE_PRIV_MISC_1" },
	{ 0x08860c, "XVE_VSEC_DEVICE" },
	{ 0x08872c, "XVE_?                (publish trigger)" },
	{ 0x08c040, "XP_?                 (bits[19:18] = MAX_RATE)" },
	{ 0x08c1c0, "XP_PL_LINK_RATE" },
	{ 0x08c2c0, "XP_CYA_0             (bit2 = DIS_G2)" },

	/* --- fuse shadows --- */
	{ 0x0204d8, "FUSE_?               (device-ID related)" },
	{ 0x08204d8,"FUSE_OPT_?           (device-ID related)" },
	{ 0x082056c,"FUSE_OPT_?" },
	{ 0x082057c,"FUSE_OPT_?           (gen fuse candidate)" },
	{ 0x0820580,"FUSE_OPT_?           (gen fuse candidate)" },
	{ 0x0820584,"FUSE_OPT_?           (unidentified)" },
	{ 0x08207d4,"FUSE_OPT_?           (unidentified group)" },
	{ 0x08207d8,"FUSE_OPT_?" },
	{ 0x08207dc,"FUSE_OPT_?" },
	{ 0x08207e0,"FUSE_OPT_?" },
	{ 0x08207e4,"FUSE_OPT_?" },
	{ 0x08207e8,"FUSE_OPT_?" },
	{ 0x08207ec,"FUSE_OPT_?" },

	/* --- UPHY / per-lane --- */
	{ 0x118e80, "UPHY_?" },
	{ 0x118e90, "UPHY_?               (0x00068c00 == 0xcb00 ran)" },
	{ 0x118bb4, "UPHY_?" },
	{ 0x137678, "PER_LANE_?" },
	{ 0x1376f8, "PER_LANE_?" },

	/* --- the packed lane-map group written by the ROM packer --- */
	{ 0x132a00, "LANEMAP_0" },
	{ 0x132a04, "LANEMAP_1" },
	{ 0x132a08, "LANEMAP_2" },
	{ 0x132a0c, "LANEMAP_3" },
	{ 0x132a10, "LANEMAP_4" },
	{ 0x132a14, "LANEMAP_5" },

	/* --- identity --- */
	{ 0x000000, "PMC_BOOT_0" },
	{ 0x0000a00,"PMC_BOOT_42" },
};

struct region { uint32_t base, len; const char *name; };

static const struct region regions[] = {
	{ 0x0088000, 0x100,  "XVE config mirror" },
	{ 0x008c000, 0x400,  "XP" },
	{ 0x0118000, 0x1000, "PGC6 / AON island" },
	{ 0x0132900, 0x200,  "lane-map packer target" },
	{ 0x0137600, 0x100,  "per-lane" },
	{ 0x0820400, 0x400,  "fuse shadows" },
};

/* --wide: full windows around everything above. Costs a few seconds of output
 * and nothing else — worth taking while a rented reference part is live. */
static const struct region wide[] = {
	{ 0x0000000, 0x1000, "PMC" },
	{ 0x0088000, 0x1000, "XVE full" },
	{ 0x008c000, 0x1000, "XP full" },
	{ 0x0118000, 0x1000, "PGC6 / AON island" },
	{ 0x0132000, 0x1000, "lane-map / packer full" },
	{ 0x0137000, 0x1000, "per-lane full" },
	{ 0x0820000, 0x1000, "fuse region full" },
	{ 0x0021000, 0x1000, "fuse ctrl" },
	{ 0x0009000, 0x1000, "PTIMER / misc" },
};

static const uint16_t ga100_ids[] = {
	0x20b0, 0x20b1, 0x20b2, 0x20b3, 0x20b5, 0x20b6, 0x20b7, 0x20b8,
	0x20bb, 0x20bd, 0x20be, 0x20bf, 0x20c2, 0x20f0, 0x20f1, 0x20f2,
};

static int read_hex(const char *path, unsigned *out)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	int ok = fscanf(f, "%x", out) == 1;
	fclose(f);
	return ok ? 0 : -1;
}

/* Pick the first GA100-class device on the bus if the caller did not name one. */
static int find_bdf(char *out, size_t n)
{
	DIR *d = opendir("/sys/bus/pci/devices");
	struct dirent *e;
	char p[512];
	unsigned ven, dev;
	int found = 0;

	if (!d) return -1;
	while (!found && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		snprintf(p, sizeof p, "/sys/bus/pci/devices/%s/vendor", e->d_name);
		if (read_hex(p, &ven) || ven != 0x10de) continue;
		snprintf(p, sizeof p, "/sys/bus/pci/devices/%s/device", e->d_name);
		if (read_hex(p, &dev)) continue;
		for (size_t i = 0; i < sizeof ga100_ids / sizeof *ga100_ids; i++) {
			if (dev == ga100_ids[i]) {
				snprintf(out, n, "%s", e->d_name);
				found = 1;
				break;
			}
		}
	}
	closedir(d);
	return found ? 0 : -1;
}

int main(int argc, char **argv)
{
	char bdf[512], path[1024];
	unsigned ven = 0, dev = 0;
	int fd, is_wide = 0, want_rom = 0, i;
	const struct region *rlist = regions;
	size_t rcount = sizeof regions / sizeof *regions;

	bdf[0] = 0;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--wide"))
			is_wide = 1;
		else if (!strcmp(argv[i], "--rom"))
			want_rom = 1;
		else
			snprintf(bdf, sizeof bdf, "%s", argv[i]);
	}
	if (is_wide) {
		rlist = wide;
		rcount = sizeof wide / sizeof *wide;
	}

	if (!bdf[0] && find_bdf(bdf, sizeof bdf)) {
		fprintf(stderr, "no GA100-class NVIDIA device found; pass a BDF\n");
		return 1;
	}

	snprintf(path, sizeof path, "/sys/bus/pci/devices/%s/vendor", bdf);
	read_hex(path, &ven);
	snprintf(path, sizeof path, "/sys/bus/pci/devices/%s/device", bdf);
	read_hex(path, &dev);

	snprintf(path, sizeof path, "/sys/bus/pci/devices/%s/resource0", bdf);
	fd = open(path, O_RDONLY | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", path, strerror(errno));
		return 1;
	}

	bar0 = mmap(NULL, BAR0_LEN, PROT_READ, MAP_SHARED, fd, 0);
	if (bar0 == MAP_FAILED) {
		fprintf(stderr,
			"mmap resource0: %s\n"
			"  EINVAL here almost always means iomem=relaxed is missing from the\n"
			"  kernel cmdline. Add it and reboot, or use the kmod in recon/kmod/.\n",
			strerror(errno));
		close(fd);
		return 1;
	}

	printf("# ga100-bar0-dump\n");
	printf("# bdf=%s id=%04x:%04x boot0=0x%08x\n", bdf, ven, dev, rd(0));
	printf("# read-only; diff two of these directly\n\n");

	/* SXM4 modules expose no PCI expansion ROM BAR, so sysfs .../rom does not
	 * exist. The PROM aperture in BAR0 is still readable, and reading it is
	 * not a write. If the ROM is shadowed/disabled this yields 0xff or 0x00
	 * fill rather than an image — check the signature and move on. */
	if (want_rom) {
		const uint32_t prom = 0x300000, len = 0x100000;
		unsigned char *buf = malloc(len);
		FILE *rf;

		for (uint32_t o = 0; o < len; o += 4) {
			uint32_t v = rd(prom + o);
			buf[o]     = v & 0xff;
			buf[o + 1] = (v >> 8) & 0xff;
			buf[o + 2] = (v >> 16) & 0xff;
			buf[o + 3] = (v >> 24) & 0xff;
		}
		rf = fopen("prom.bin", "wb");
		if (rf) { fwrite(buf, 1, len, rf); fclose(rf); }

		printf("# prom.bin: sig=%02x%02x %s\n", buf[0], buf[1],
		       (buf[0] == 0x55 && buf[1] == 0xaa) ? "VALID" : "no image");
		free(buf);
	}

	printf("=== named ===\n");
	for (size_t i = 0; i < sizeof named / sizeof *named; i++) {
		uint32_t v = rd(named[i].off);
		printf("0x%07x 0x%08x  %s%s\n",
		       named[i].off, v, named[i].name, note(v));
	}

	printf("\n=== decode ===\n");
	{
		uint32_t cap  = rd(0x88084);
		uint32_t cap2 = rd(0x880a4);
		uint32_t sta  = rd(0x88088);
		uint32_t ctl2 = rd(0x880a8);
		uint32_t f78  = rd(0x118f78);
		uint32_t f23c = rd(0x11823c);

		printf("LnkCap  max_speed      = %u\n", cap & 0xf);
		printf("LnkCap  max_width      = %u\n", (cap >> 4) & 0x3f);
		printf("LnkCap2 speed_vector   = 0x%02x  (2.5=%u 5.0=%u 8.0=%u 16.0=%u)\n",
		       (cap2 >> 1) & 0x7f,
		       (cap2 >> 1) & 1, (cap2 >> 2) & 1,
		       (cap2 >> 3) & 1, (cap2 >> 4) & 1);
		printf("LnkCtl2 target_speed   = %u\n", ctl2 & 0xf);
		printf("LnkSta  cur_speed      = %u  width = %u\n",
		       sta >> 16 & 0xf, sta >> 20 & 0x3f);
		printf("0x118f78 bit30         = %u   <-- DECISIVE\n", (f78 >> 30) & 1);
		printf("0x11823c bits[11:10]   = %u\n", (f23c >> 10) & 3);
		printf("0x12e0   bit0          = %u\n", rd(0x12e0) & 1);
		printf("0x118e90 == 0x00068c00 = %s\n",
		       rd(0x118e90) == 0x00068c00 ? "yes (0xcb00 ran)" : "no");
	}

	for (size_t i = 0; i < rcount; i++) {
		printf("\n=== region 0x%07x +0x%x  %s ===\n",
		       rlist[i].base, rlist[i].len, rlist[i].name);
		for (uint32_t o = 0; o < rlist[i].len; o += 4) {
			uint32_t off = rlist[i].base + o;
			uint32_t v = rd(off);
			printf("0x%07x 0x%08x%s\n", off, v, note(v));
		}
	}

	munmap((void *)bar0, BAR0_LEN);
	close(fd);
	return 0;
}
