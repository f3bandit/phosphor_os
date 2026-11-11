/* phosphord: RetroSpy, installed through Update All and set up from Phosphor.
 * Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md).
 *
 * RetroSpy (retro-spy.com) shows the controller being played on a PC, for streams and recordings. Its
 * MiSTer part is one program, <fat>/retrospy/retrospy, from Update All's database
 * "retrospy/retrospy-MiSTer". RetroSpy on the PC logs in over SSH (MiSTer's root / 1) and runs
 * "retrospy /dev/input/jsN" while it watches. Nothing of it runs otherwise, and Phosphor starts nothing.
 *
 * The "retrospy" command: whether it's installed; whether RetroSpy is watching, and which controller; the
 * SS1's address; the controllers as RetroSpy numbers them (jsN), each with its MiSTer player (Main's
 * "players": jsN and eventN are matched through sysfs). Installing and uninstalling go through Update All
 * (the "ua" command: install / uninstall of that database).
 *
 * For RetroSpy's Phosphor edition (f3bandit/RetroSpy, which asks with `phos '{"cmd":"retrospy"}'` over its SSH
 * login): each controller also has "phosphor", the layout Phosphor draws it with and which of its jsN buttons and
 * axes each control is (ctrl.c); "active" is the js number of the controller to show (Player 1's, else the first);
 * "layouts" has the drawings of the layouts in use, from the table the TV's controller test draws. */
#include "retrospy.h"
#include "ctrl.h"
#include "paths.h"
#include "../../frontend/src/padlayouts.h"      /* the drawings the TV's controller test uses (RetroSpy draws the same) */
#include <arpa/inet.h>
#include <dirent.h>
#include <glob.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define RS_DB "retrospy/retrospy-MiSTer"

/* Main's reply to one line (malloc'd), or NULL */
static char *main_req(const char *line, int timeout_ms)
{
	const char *sp = getenv("PHOSPHOR_MAIN_SOCK");
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return NULL;
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof(a.sun_path), "%s", sp && *sp ? sp : "/tmp/phosphor-main.sock");
	char msg[64];
	int n = snprintf(msg, sizeof(msg), "%s\n", line);
	if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0 || send(fd, msg, (size_t)n, MSG_NOSIGNAL) != n) { close(fd); return NULL; }
	size_t cap = 4096, len = 0;
	char *buf = calloc(1, cap);
	while (buf && len + 1 < cap) {
		struct pollfd p = { fd, POLLIN, 0 };
		if (poll(&p, 1, timeout_ms) <= 0) break;
		ssize_t r = read(fd, buf + len, 1);
		if (r <= 0 || buf[len] == '\n') break;
		len++;
	}
	close(fd);
	if (buf) buf[len] = 0;
	if (!buf || !len) { free(buf); return NULL; }
	return buf;
}

/* the jsN RetroSpy is reading (its command line), -1 none */
static int watching(void)
{
	DIR *d = opendir("/proc");
	if (!d) return -1;
	struct dirent *e;
	int js = -1;
	while (js < 0 && (e = readdir(d))) {
		if (e->d_name[0] < '1' || e->d_name[0] > '9') continue;
		char p[64], cmd[512];
		snprintf(p, sizeof(p), "/proc/%s/cmdline", e->d_name);
		FILE *f = fopen(p, "r");
		if (!f) continue;
		size_t n = fread(cmd, 1, sizeof(cmd) - 1, f);
		fclose(f);
		cmd[n] = 0;
		size_t l = strlen(cmd);                            /* argv[0], then the device */
		if (l < 9 || strcmp(cmd + l - 9, "/retrospy") || l + 1 >= n) continue;
		const char *dev = cmd + l + 1, *j = strstr(dev, "/js");
		js = j ? atoi(j + 3) : 0;
	}
	closedir(d);
	return js;
}

static void my_ip(char *out, size_t n)
{
	struct ifaddrs *ifs, *i;
	out[0] = 0;
	if (getifaddrs(&ifs)) return;
	for (i = ifs; i; i = i->ifa_next) {
		if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK)) continue;
		inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, out, (socklen_t)n);
		break;
	}
	freeifaddrs(ifs);
}

static void read_line(const char *path, char *out, size_t n)
{
	out[0] = 0;
	FILE *f = fopen(path, "r");
	if (!f) return;
	if (fgets(out, (int)n, f)) out[strcspn(out, "\r\n")] = 0;
	fclose(f);
}

cJSON *retrospy_cmd(cJSON *req)
{
	(void)req;
	cJSON *o = cJSON_CreateObject();
	char exe[600], ip[64];
	snprintf(exe, sizeof(exe), "%s/retrospy/retrospy", p_fat());
	struct stat sb;
	int inst = !stat(exe, &sb) && S_ISREG(sb.st_mode);
	cJSON_AddBoolToObject(o, "installed", inst);
	if (inst) cJSON_AddNumberToObject(o, "at", (double)sb.st_mtime);
	cJSON_AddStringToObject(o, "db", RS_DB);
	int js = watching();
	cJSON_AddBoolToObject(o, "watching", js >= 0);
	if (js >= 0) cJSON_AddNumberToObject(o, "watching_js", js);
	my_ip(ip, sizeof(ip));
	cJSON_AddStringToObject(o, "host", ip);
	cJSON_AddStringToObject(o, "user", "root");
	cJSON_AddStringToObject(o, "password", "1");     /* MiSTer's own SSH login (Phosphor doesn't change it) */

	/* Main's players by eventN */
	cJSON *pl = NULL;
	char *r = main_req("players", 1500);
	if (r) { pl = cJSON_Parse(r); free(r); }
	cJSON *ps = cJSON_GetObjectItem(pl, "players");

	cJSON *arr = cJSON_AddArrayToObject(o, "controllers");
	cJSON *layouts = cJSON_CreateObject();
	int active = -1, first_js = -1;
	glob_t g;
	if (!glob("/sys/class/input/js*", 0, NULL, &g)) {
		for (size_t i = 0; i < g.gl_pathc; i++) {
			const char *base = strrchr(g.gl_pathv[i], '/') + 1;
			char p[600], name[160] = "";
			snprintf(p, sizeof(p), "%s/device/name", g.gl_pathv[i]);
			read_line(p, name, sizeof(name));
			/* its eventN: the other handler of the same input device */
			int player = 0;
			char ev[32] = "";
			snprintf(p, sizeof(p), "%s/device", g.gl_pathv[i]);
			DIR *d = opendir(p);
			if (d) {
				struct dirent *e;
				while ((e = readdir(d)))
					if (!strncmp(e->d_name, "event", 5)) { snprintf(ev, sizeof(ev), "%s", e->d_name); break; }
				closedir(d);
			}
			cJSON *it;
			cJSON_ArrayForEach(it, ps) {
				const cJSON *dv = cJSON_GetObjectItem(it, "dev");
				if (ev[0] && cJSON_IsString(dv) && !strcmp(dv->valuestring, ev)) {
					const cJSON *pn = cJSON_GetObjectItem(it, "player");
					if (cJSON_IsNumber(pn)) player = pn->valueint;
				}
			}
			cJSON *c = cJSON_CreateObject();
			int jsn = atoi(base + 2);
			cJSON_AddNumberToObject(c, "js", jsn);
			cJSON_AddStringToObject(c, "name", name);
			if (ev[0]) cJSON_AddStringToObject(c, "event", ev);
			cJSON_AddNumberToObject(c, "player", player);
			cJSON *ph = ev[0] ? ctrl_retrospy(ev) : NULL;
			if (ph) {
				const char *lk = cJSON_GetStringValue(cJSON_GetObjectItem(ph, "layout"));
				for (int li = 0; lk && li < PL_COUNT; li++) if (!strcmp(PLAYOUTS[li].key, lk)) cJSON_AddStringToObject(ph, "layout_name", PLAYOUTS[li].name);
				if (lk && !cJSON_GetObjectItem(layouts, lk)) {
					for (int li = 0; li < PL_COUNT; li++) {
						if (strcmp(PLAYOUTS[li].key, lk)) continue;
						cJSON *L = cJSON_AddObjectToObject(layouts, lk), *its = cJSON_AddArrayToObject(L, "items");
						cJSON_AddStringToObject(L, "name", PLAYOUTS[li].name);
						cJSON_AddNumberToObject(L, "w", 600);
						cJSON_AddNumberToObject(L, "h", 345);
						for (int k = 0; k < PLAYOUTS[li].n; k++) {
							const pl_item_t *it = &PLAYOUTS[li].it[k];
							cJSON *e = cJSON_CreateObject();
							char t[2] = { it->t, 0 };
							cJSON_AddStringToObject(e, "t", t);
							cJSON_AddStringToObject(e, "id", it->id);
							cJSON_AddNumberToObject(e, "x", it->x);
							cJSON_AddNumberToObject(e, "y", it->y);
							cJSON_AddNumberToObject(e, "a", it->a);
							cJSON_AddNumberToObject(e, "b", it->b);
							cJSON_AddStringToObject(e, "lab", it->lab);
							cJSON_AddItemToArray(its, e);
						}
					}
				}
				cJSON_AddItemToObject(c, "phosphor", ph);
			}
			if (player == 1) active = jsn;
			if (first_js < 0 || jsn < first_js) first_js = jsn;
			cJSON_AddItemToArray(arr, c);
		}
		globfree(&g);
	}
	cJSON_Delete(pl);
	if (active < 0) active = first_js;
	cJSON_AddNumberToObject(o, "active", active);
	cJSON_AddItemToObject(o, "layouts", layouts);
	return o;
}
