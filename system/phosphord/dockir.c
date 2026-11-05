/* phosphord: the dock's IR remote receiver: an RP2040 on the dock's USB hub running Retro-Remake's DockIR
 * ("TinyUSB pico_ir_keyboard", 1c4f:0002). With Phosphor's patch (firmware/dockir) it can be updated from
 * here: a HID feature report "DIRBOOT!" to its keyboard interface restarts it into the RP2040's ROM
 * bootloader, whose drive (RPI RP2) appears on the SS1; the .uf2 is copied onto it and it restarts with the
 * new firmware. A feature report read answers its id ("DIRUP001"). The first flash of a dock is by hand
 * (BOOTSEL + MCU RST in the NVMe slot, from a PC): its own firmware can't be told to restart.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *   PHOS_USB_SYS, PHOS_HIDRAW_SYS: /sys/bus/usb/devices and /sys/class/hidraw, for tests */
#include "dockir.h"
#include "log.h"
#include "paths.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/hidraw.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define BOOT_MAGIC "DIRBOOT!"
#define UPDATABLE  "DIRUP"

static const char *env_or(const char *k, const char *d) { const char *v = getenv(k); return v && *v ? v : d; }
static const char *usb_sys(void) { return env_or("PHOS_USB_SYS", "/sys/bus/usb/devices"); }
static const char *hid_sys(void) { return env_or("PHOS_HIDRAW_SYS", "/sys/class/hidraw"); }

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static struct { int busy, ok; char state[24], msg[200]; } job;

static int read_line(const char *path, char *out, size_t n)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	if (!fgets(out, (int)n, f)) out[0] = 0;
	fclose(f);
	out[strcspn(out, "\r\n")] = 0;
	return 0;
}

/* the USB device whose product is <product>: its sysfs folder name (e.g. 1-1.1.7), 0 found */
static int usb_find(const char *product, char *dev, size_t n, char *bcd, size_t bn)
{
	DIR *d = opendir(usb_sys());
	if (!d) return -1;
	struct dirent *e;
	int found = -1;
	while ((e = readdir(d)) && found) {
		if (e->d_name[0] == '.') continue;
		char p[512], v[96];
		snprintf(p, sizeof(p), "%s/%s/product", usb_sys(), e->d_name);
		if (read_line(p, v, sizeof(v)) || strcmp(v, product)) continue;
		snprintf(dev, n, "%s", e->d_name);
		if (bcd) { snprintf(p, sizeof(p), "%s/%s/bcdDevice", usb_sys(), e->d_name); if (read_line(p, bcd, bn)) bcd[0] = 0; }
		found = 0;
	}
	closedir(d);
	return found;
}

/* /dev/hidrawN of the receiver's keyboard interface (<dev>:1.0) */
static int hidraw_of(const char *dev, char *out, size_t n)
{
	DIR *d = opendir(hid_sys());
	if (!d) return -1;
	struct dirent *e;
	char want[64];
	snprintf(want, sizeof(want), "%s:1.0", dev);
	int found = -1;
	while ((e = readdir(d)) && found) {
		if (strncmp(e->d_name, "hidraw", 6)) continue;
		char p[512], real[PATH_MAX];
		snprintf(p, sizeof(p), "%s/%s/device", hid_sys(), e->d_name);
		if (!realpath(p, real)) continue;
		char *last = strrchr(real, '/');
		if (!last) continue;
		*last = 0;                                   /* .../<dev>:1.0/0003:1C4F:0002.0002 -> .../<dev>:1.0 */
		const char *itf = strrchr(real, '/');
		if (itf && !strcmp(itf + 1, want)) { snprintf(out, n, "/dev/%s", e->d_name); found = 0; }
	}
	closedir(d);
	return found;
}

/* the receiver's id ("DIRUP001"), or "" when its firmware doesn't answer one */
static void read_id(const char *dev, char *id, size_t n)
{
	id[0] = 0;
	char hr[64];
	if (hidraw_of(dev, hr, sizeof(hr))) return;
	int fd = open(hr, O_RDWR | O_CLOEXEC);
	if (fd < 0) return;
	unsigned char buf[9] = { 0 };
	int r = ioctl(fd, HIDIOCGFEATURE(sizeof(buf)), buf);
	close(fd);
	if (r <= 1) return;
	size_t j = 0;
	for (int i = 1; i < r && i < 9 && j + 1 < n; i++) if (buf[i] >= 32 && buf[i] < 127) id[j++] = (char)buf[i];
	id[j] = 0;
}

const char *dockir_file(void)
{
	static char p[600];
	snprintf(p, sizeof(p), "%s/phosphor/updates/dockir.uf2", p_fat());
	return p;
}

int dockir_check_uf2(const char *path, char *err, size_t n)
{
	FILE *f = fopen(path, "rb");
	if (!f) { snprintf(err, n, "no firmware file"); return -1; }
	unsigned char b[512];
	long blocks = 0;
	int bad = 0;
	while (fread(b, 1, 512, f) == 512) {
		unsigned m0 = b[0] | b[1] << 8 | b[2] << 16 | (unsigned)b[3] << 24, m1 = b[4] | b[5] << 8 | b[6] << 16 | (unsigned)b[7] << 24;
		unsigned fam = b[28] | b[29] << 8 | b[30] << 16 | (unsigned)b[31] << 24, me = b[508] | b[509] << 8 | b[510] << 16 | (unsigned)b[511] << 24;
		if (m0 != 0x0A324655 || m1 != 0x9E5D5157 || me != 0x0AB16F30 || fam != 0xE48BFF56) { bad = 1; break; }
		blocks++;
	}
	fclose(f);
	if (bad || blocks == 0) { snprintf(err, n, "that isn't firmware for the dock's IR receiver (an RP2040 .uf2)"); return -1; }
	return 0;
}

cJSON *dockir_json(void)
{
	cJSON *o = cJSON_CreateObject();
	char dev[64], bcd[16] = "", id[16] = "";
	int present = usb_find("pico_ir_keyboard", dev, sizeof(dev), bcd, sizeof(bcd)) == 0;
	if (present) read_id(dev, id, sizeof(id));
	cJSON_AddBoolToObject(o, "present", present);
	cJSON_AddStringToObject(o, "firmware", id);
	cJSON_AddBoolToObject(o, "updatable", !strncmp(id, UPDATABLE, strlen(UPDATABLE)));
	cJSON_AddStringToObject(o, "version", bcd);
	struct stat sb;
	cJSON_AddBoolToObject(o, "file", stat(dockir_file(), &sb) == 0);
	cJSON *j = cJSON_AddObjectToObject(o, "job");
	pthread_mutex_lock(&lk);
	cJSON_AddBoolToObject(j, "busy", job.busy);
	cJSON_AddBoolToObject(j, "ok", job.ok);
	cJSON_AddStringToObject(j, "state", job.state);
	cJSON_AddStringToObject(j, "message", job.msg);
	pthread_mutex_unlock(&lk);
	return o;
}

static void say(const char *state, const char *msg)
{
	pthread_mutex_lock(&lk);
	snprintf(job.state, sizeof(job.state), "%s", state);
	snprintf(job.msg, sizeof(job.msg), "%s", msg);
	pthread_mutex_unlock(&lk);
	plog("dock IR: %s", msg);
}

/* the bootloader's drive: the block device of the USB device "RP2 Boot", its first partition */
static int boot_drive(char *blk, size_t n)
{
	char dev[64];
	if (usb_find("RP2 Boot", dev, sizeof(dev), NULL, 0)) return -1;
	char p[512];
	snprintf(p, sizeof(p), "%s/%s", usb_sys(), dev);
	/* .../<dev>/<dev>:1.0/hostN/targetN:0:0/N:0:0:0/block/sdX */
	DIR *b = NULL;
	char base[512];
	snprintf(base, sizeof(base), "/sys/block");
	b = opendir(base);
	if (!b) return -1;
	struct dirent *e;
	int found = -1;
	while ((e = readdir(b)) && found) {
		if (strncmp(e->d_name, "sd", 2)) continue;
		char q[512], real[PATH_MAX];
		snprintf(q, sizeof(q), "/sys/block/%s/device", e->d_name);
		if (!realpath(q, real)) continue;
		char want[96];
		snprintf(want, sizeof(want), "/%s:1.0/", dev);
		if (strstr(real, want)) { snprintf(blk, n, "/dev/%s1", e->d_name); found = 0; }
	}
	closedir(b);
	return found;
}

/* where <blk> is mounted ("" none) */
static void mounted_at(const char *blk, char *mnt, size_t n)
{
	mnt[0] = 0;
	FILE *f = fopen("/proc/mounts", "r");
	if (!f) return;
	char l[1024], dv[512], mp[512];
	while (fgets(l, sizeof(l), f)) if (sscanf(l, "%511s %511s", dv, mp) == 2 && !strcmp(dv, blk)) { snprintf(mnt, n, "%s", mp); break; }
	fclose(f);
}

static void *update_thread(void *u)
{
	(void)u;
	char dev[64], hr[64], blk[64], mnt[512], id[16];
	int ok = 0;
	if (usb_find("pico_ir_keyboard", dev, sizeof(dev), NULL, 0) || hidraw_of(dev, hr, sizeof(hr))) { say("failed", "The dock's IR receiver isn't there"); goto done; }
	say("bootloader", "Restarting the IR receiver into its bootloader");
	int fd = open(hr, O_RDWR | O_CLOEXEC);
	unsigned char m[9] = { 0 };
	memcpy(m + 1, BOOT_MAGIC, 8);
	if (fd < 0 || ioctl(fd, HIDIOCSFEATURE(sizeof(m)), m) < 0) { if (fd >= 0) close(fd); say("failed", "The IR receiver didn't take the update command"); goto done; }
	close(fd);
	int i = 0;
	for (; i < 100 && boot_drive(blk, sizeof(blk)); i++) usleep(100000);
	if (i == 100) { say("failed", "The IR receiver's bootloader drive didn't appear (BOOTSEL + MCU RST in the NVMe slot starts it by hand)"); goto done; }
	say("copying", "Writing the new firmware");
	/* MiSTer's usbmount mounts it as a /media/usbN: written there, or mounted here when it isn't */
	char own[600] = "";
	for (i = 0; i < 30; i++) { mounted_at(blk, mnt, sizeof(mnt)); if (mnt[0]) break; usleep(100000); }
	if (!mnt[0]) {
		snprintf(own, sizeof(own), "%s/dockir-boot", p_tmp());
		mkdir(own, 0700);
		if (mount(blk, own, "vfat", 0, NULL)) { say("failed", "Couldn't open the bootloader drive"); goto done; }
		snprintf(mnt, sizeof(mnt), "%s", own);
	}
	char dst[700];
	snprintf(dst, sizeof(dst), "%s/firmware.uf2", mnt);
	FILE *in = fopen(dockir_file(), "rb"), *out = fopen(dst, "wb");
	int bad = !in || !out;
	char buf[4096];
	size_t r;
	while (!bad && (r = fread(buf, 1, sizeof(buf), in)) > 0) if (fwrite(buf, 1, r, out) != r) bad = 1;
	if (in) fclose(in);
	if (out && fclose(out)) bad = 1;
	sync();
	if (own[0]) { umount2(own, MNT_DETACH); rmdir(own); }
	/* the RP2040 restarts by itself once it's written (its drive goes away); a write error is normal then */
	say("restarting", "Waiting for the IR receiver to come back");
	for (i = 0; i < 150 && usb_find("pico_ir_keyboard", dev, sizeof(dev), NULL, 0); i++) usleep(100000);
	if (i == 150) { say("failed", bad ? "The firmware couldn't be written" : "The IR receiver didn't come back: BOOTSEL + MCU RST and copy the firmware from a PC"); goto done; }
	usleep(500000);
	read_id(dev, id, sizeof(id));
	{
		char msg[200];
		snprintf(msg, sizeof(msg), "Updated: the IR receiver runs %s", id[0] ? id : "a firmware that can't be updated from here");
		say("done", msg);
	}
	unlink(dockir_file());
	ok = 1;
done:
	pthread_mutex_lock(&lk);
	job.busy = 0;
	job.ok = ok;
	pthread_mutex_unlock(&lk);
	return NULL;
}

int dockir_update_start(char *err, size_t n)
{
	char dev[64], id[16];
	if (usb_find("pico_ir_keyboard", dev, sizeof(dev), NULL, 0)) { snprintf(err, n, "The dock's IR receiver isn't there"); return -1; }
	read_id(dev, id, sizeof(id));
	if (strncmp(id, UPDATABLE, strlen(UPDATABLE))) { snprintf(err, n, "This IR receiver's firmware can't be updated from the SS1: flash it once from a PC (BOOTSEL + MCU RST in the dock's NVMe slot)"); return -1; }
	if (dockir_check_uf2(dockir_file(), err, n)) return -1;
	pthread_mutex_lock(&lk);
	if (job.busy) { pthread_mutex_unlock(&lk); snprintf(err, n, "The IR receiver is already being updated"); return -1; }
	job.busy = 1;
	job.ok = 0;
	pthread_mutex_unlock(&lk);
	pthread_t t;
	if (pthread_create(&t, NULL, update_thread, NULL)) { pthread_mutex_lock(&lk); job.busy = 0; pthread_mutex_unlock(&lk); snprintf(err, n, "couldn't start"); return -1; }
	pthread_detach(t);
	return 0;
}
