/* phosphord: SSH for Settings > Remote access (Off, Password, Keys only), the SSH keys that may log in,
 * and the copy of the remote access setup kept on another drive (NVMe or USB) so a freshly flashed SD
 * card gets it back. The system side (mounts, sshd) is phosphor/bin/phos-ssh.sh.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Keys only come in two ways: typed on the MiSTer, or offered by a paired device ("ssh_key_offer")
 * and then accepted on the TV. Nothing from another device changes the mode, the password or removes
 * keys: remote access never changes remote access. */
#include "sshacc.h"
#include "log.h"
#include "paths.h"
#include "sha256.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_KEYS 32
#define MAX_PENDING 4

typedef struct { char line[1200], from[64]; long at; } pending_t;

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pending_t pend[MAX_PENDING];
static int npend;
static int dirty;              /* the copy on the other drive needs saving */
static long dirty_at;
static char last_err[200];

static void conf(char *out, size_t n, const char *name) { snprintf(out, n, "%s/ssh/%s", p_config(), name); }
static void script(char *out, size_t n) { snprintf(out, n, "%s/phosphor/bin/phos-ssh.sh", p_fat()); }

static char *read_all(const char *path, size_t max)
{
	FILE *f = fopen(path, "r");
	if (!f) return NULL;
	char *b = malloc(max + 1);
	size_t n = b ? fread(b, 1, max, f) : 0;
	fclose(f);
	if (b) b[n] = 0;
	return b;
}

static int write_all(const char *path, const char *text)
{
	char tmp[PATH_MAX + 8];
	snprintf(tmp, sizeof(tmp), "%s.new", path);
	char dir[PATH_MAX];
	snprintf(dir, sizeof(dir), "%s/ssh", p_config());
	mkdirs(dir);
	FILE *f = fopen(tmp, "w");
	if (!f) return -1;
	int bad = fputs(text, f) < 0;
	bad |= fflush(f) != 0 || fsync(fileno(f)) != 0;
	bad |= fclose(f) != 0;
	if (bad || rename(tmp, path)) { unlink(tmp); return -1; }
	return 0;
}

static void read_line(const char *name, char *out, size_t n)
{
	char p[PATH_MAX];
	conf(p, sizeof(p), name);
	char *s = read_all(p, n - 1);
	snprintf(out, n, "%s", s ? s : "");
	free(s);
	out[strcspn(out, "\r\n")] = 0;
}

/* the script, its output (first line) into out; exit code back */
static int run(const char *args, const char *stdin_text, char *out, size_t n)
{
	char sc[PATH_MAX], cmd[PATH_MAX * 2 + 64];
	script(sc, sizeof(sc));
	if (access(sc, X_OK)) { snprintf(out, n, "phos-ssh.sh is missing"); return -1; }
	char in[PATH_MAX] = "";
	if (stdin_text) {
		snprintf(in, sizeof(in), "%s/ssh-in.%d", p_tmp(), (int)getpid());
		FILE *f = fopen(in, "w");
		if (!f) return -1;
		chmod(in, 0600);
		fputs(stdin_text, f);
		fputc('\n', f);
		fclose(f);
		snprintf(cmd, sizeof(cmd), "'%s' %s < '%s' 2>&1", sc, args, in);
	} else snprintf(cmd, sizeof(cmd), "'%s' %s 2>&1", sc, args);
	FILE *p = popen(cmd, "r");
	if (!p) { if (in[0]) unlink(in); return -1; }
	char line[300], first[300] = "", err[300] = "";
	while (fgets(line, sizeof(line), p)) {
		line[strcspn(line, "\r\n")] = 0;
		if (!first[0]) snprintf(first, sizeof(first), "%s", line);
		if (strchr(line, ' ') && !strchr(line, '=')) snprintf(err, sizeof(err), "%s", line);
	}
	int rc = pclose(p);
	if (in[0]) unlink(in);
	snprintf(out, n, "%s", err[0] ? err : first);
	return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}

/* ---------- keys ---------- */

static const char *KEY_TYPES[] = { "ssh-ed25519", "ssh-rsa", "ecdsa-sha2-nistp256", "ecdsa-sha2-nistp384",
	"ecdsa-sha2-nistp521", "sk-ssh-ed25519@openssh.com", "sk-ecdsa-sha2-nistp256@openssh.com", NULL };

static int b64v(int c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

/* the first bytes of the key blob: a 4-byte length and the type again (what a real key holds) */
static int blob_type_ok(const char *b64, const char *type)
{
	unsigned char out[64];
	size_t o = 0;
	int acc = 0, bits = 0;
	for (const char *p = b64; *p && *p != '=' && o < sizeof(out); p++) {
		int v = b64v((unsigned char)*p);
		if (v < 0) return 0;
		acc = (acc << 6) | v;
		bits += 6;
		if (bits >= 8) { bits -= 8; out[o++] = (unsigned char)((acc >> bits) & 0xff); }
	}
	size_t tl = strlen(type);
	if (o < 4 + tl) return 0;
	size_t len = ((size_t)out[0] << 24) | ((size_t)out[1] << 16) | ((size_t)out[2] << 8) | out[3];
	return len == tl && !memcmp(out + 4, type, tl);
}

/* "type base64 [comment]" → a clean authorized_keys line with the given name as its comment; 0 ok */
static int clean_key(const char *in, const char *name, char *out, size_t n)
{
	char type[64], b64[1100];
	if (!in || sscanf(in, " %63s %1099s", type, b64) != 2) return -1;
	int known = 0;
	for (int i = 0; KEY_TYPES[i]; i++) if (!strcmp(type, KEY_TYPES[i])) known = 1;
	if (!known || strlen(b64) < 40 || strlen(b64) >= 1099) return -1;
	for (const char *p = b64; *p; p++) if (b64v((unsigned char)*p) < 0 && *p != '=') return -1;
	if (!blob_type_ok(b64, type)) return -1;
	char nm[80];
	snprintf(nm, sizeof(nm), "%s", name && *name ? name : "key");
	for (char *p = nm; *p; p++) if ((unsigned char)*p < 0x20 || *p == '"') *p = ' ';
	char day[16];
	time_t t = time(NULL);
	strftime(day, sizeof(day), "%Y-%m-%d", localtime(&t));
	snprintf(out, n, "%s %s %s (added %s)", type, b64, nm, day);
	return 0;
}

static int load_keys(char lines[][1200], int max)
{
	char p[PATH_MAX];
	conf(p, sizeof(p), "authorized_keys");
	FILE *f = fopen(p, "r");
	if (!f) return 0;
	int n = 0;
	char l[1300];
	while (n < max && fgets(l, sizeof(l), f)) {
		l[strcspn(l, "\r\n")] = 0;
		if (!l[0] || l[0] == '#') continue;
		snprintf(lines[n++], 1200, "%s", l);
	}
	fclose(f);
	return n;
}

static int save_keys(char lines[][1200], int n)
{
	size_t cap = (size_t)n * 1201 + 200;
	char *b = malloc(cap);
	if (!b) return -1;
	size_t k = (size_t)snprintf(b, cap, "# SSH keys that may log in to this MiSTer as root (Settings > Remote access)\n");
	for (int i = 0; i < n; i++) k += (size_t)snprintf(b + k, cap - k, "%s\n", lines[i]);
	char p[PATH_MAX];
	conf(p, sizeof(p), "authorized_keys");
	int r = write_all(p, b);
	free(b);
	return r;
}

/* the blob part, to compare keys */
static void key_blob(const char *line, char *out, size_t n)
{
	char t[64], b[1100] = "";
	if (sscanf(line, "%63s %1099s", t, b) < 2) b[0] = 0;
	snprintf(out, n, "%s", b);
}

static void key_id(const char *line, char out[13])
{
	char b[1100], h[65];
	key_blob(line, b, sizeof(b));
	sha256_hex(b, strlen(b), h);
	snprintf(out, 13, "%.12s", h);
}

/* ---------- state ---------- */

static void mark(void) { pthread_mutex_lock(&mu); dirty = 1; dirty_at = time(NULL); pthread_mutex_unlock(&mu); }
void sshacc_changed(void) { mark(); }

static void signal_shell(void)
{
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/ssh-key-request", p_tmp());
	if (npend) { FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%d\n", npend); fclose(f); } }
	else unlink(p);
}

static int apply(char *err, size_t n)
{
	char out[300];
	int rc = run("apply", NULL, out, sizeof(out));
	if (rc) { snprintf(err, n, "%s", out[0] ? out : "couldn't apply the SSH setup"); return -1; }
	if (strstr(out, "only needs a key") || strstr(out, "didn't accept")) snprintf(err, n, "%s", out);
	return 0;
}

static void keep_now(void)
{
	char dir[PATH_MAX], out[300];
	read_line("keep", dir, sizeof(dir));
	if (!dir[0]) return;
	char a[PATH_MAX + 8];
	snprintf(a, sizeof(a), "keep '%s'", dir);
	if (run(a, NULL, out, sizeof(out))) { plog("remote access: couldn't keep a copy on %s: %s", dir, out); snprintf(last_err, sizeof(last_err), "Couldn't save the copy on %s", dir); }
	else last_err[0] = 0;
}

void sshacc_tick(void)
{
	pthread_mutex_lock(&mu);
	int go = dirty && time(NULL) - dirty_at >= 3;
	if (go) dirty = 0;
	pthread_mutex_unlock(&mu);
	if (go) keep_now();
}

static int fresh_card(void)
{
	char a[PATH_MAX], b[PATH_MAX], c[PATH_MAX];
	snprintf(a, sizeof(a), "%s/remote.ini", p_config());
	conf(b, sizeof(b), "mode");
	conf(c, sizeof(c), "authorized_keys");
	return access(a, F_OK) && access(b, F_OK) && access(c, F_OK);
}

void sshacc_boot(void)
{
	if (fresh_card()) {
		char out[600];
		char sc[PATH_MAX], cmd[PATH_MAX + 32];
		script(sc, sizeof(sc));
		snprintf(cmd, sizeof(cmd), "'%s' find 2>/dev/null", sc);
		FILE *p = access(sc, X_OK) ? NULL : popen(cmd, "r");
		out[0] = 0;
		if (p) { if (!fgets(out, sizeof(out), p)) out[0] = 0; pclose(p); }
		out[strcspn(out, "\r\n")] = 0;
		char *bar = strchr(out, '|');
		if (bar) {
			*bar = 0;
			char a[PATH_MAX + 16], r[300];
			snprintf(a, sizeof(a), "bring '%s'", out);
			if (!run(a, NULL, r, sizeof(r))) {
				char note[800], path[PATH_MAX];
				snprintf(note, sizeof(note), "%s|%s\n", out, bar + 1);
				conf(path, sizeof(path), "restored");
				write_all(path, note);
				conf(path, sizeof(path), "keep");
				char k[PATH_MAX + 2];
				snprintf(k, sizeof(k), "%s\n", out);
				write_all(path, k);
				plog("remote access: a fresh SD card: the setup brought back from %s", out);
			}
		}
	}
	char err[200] = "";
	if (apply(err, sizeof(err))) plog("remote access: ssh: %s", err);
}

static const char *mode_now(char *buf, size_t n)
{
	read_line("mode", buf, n);
	if (strcmp(buf, "off") && strcmp(buf, "keys")) snprintf(buf, n, "password");
	return buf;
}

/* drives a copy can go on: mounted USB drives (the NVMe is one, behind the dock's bridge) */
static void add_places(cJSON *o)
{
	cJSON *a = cJSON_AddArrayToObject(o, "places");
	FILE *f = fopen("/proc/mounts", "r");
	if (!f) return;
	char dev[256], mnt[256], fs[64], rest[512];
	while (fscanf(f, "%255s %255s %63s %511[^\n]", dev, mnt, fs, rest) == 4) {
		if (strncmp(mnt, "/media/usb", 10) || strncmp(dev, "/dev/sd", 7)) continue;
		if (strstr(rest, "ro,") == rest) continue;
		char blk[16] = "", path[300], model[128] = "", vendor[64] = "";
		sscanf(dev + 5, "%15[a-z]", blk);
		snprintf(path, sizeof(path), "/sys/block/%s/device/model", blk);
		char *m = read_all(path, 120);
		if (m) { snprintf(model, sizeof(model), "%s", m); free(m); }
		snprintf(path, sizeof(path), "/sys/block/%s/device/vendor", blk);
		m = read_all(path, 60);
		if (m) { snprintf(vendor, sizeof(vendor), "%s", m); free(m); }
		model[strcspn(model, "\n")] = 0;
		int nvme = strcasestr(model, "nvme") || strcasestr(vendor, "jmicron") || strcasestr(model, "jms58") || strcasestr(model, "rtl9210");
		cJSON *e = cJSON_CreateObject();
		cJSON_AddStringToObject(e, "dir", mnt);
		char label[80];
		snprintf(label, sizeof(label), "%s (%s)", nvme ? "NVMe" : "USB drive", mnt + 7);
		cJSON_AddStringToObject(e, "name", label);
		cJSON_AddItemToArray(a, e);
	}
	fclose(f);
}

cJSON *sshacc_json(int places)
{
	cJSON *o = cJSON_CreateObject();
	char m[16], keepdir[PATH_MAX], path[PATH_MAX];
	cJSON_AddStringToObject(o, "mode", mode_now(m, sizeof(m)));
	conf(path, sizeof(path), "root-hash");
	cJSON_AddBoolToObject(o, "own_password", access(path, F_OK) == 0);
	char sc[PATH_MAX];
	script(sc, sizeof(sc));
	cJSON_AddBoolToObject(o, "available", access(sc, X_OK) == 0);
	static char lines[MAX_KEYS][1200];
	pthread_mutex_lock(&mu);
	int n = load_keys(lines, MAX_KEYS);
	cJSON *ks = cJSON_AddArrayToObject(o, "keys");
	for (int i = 0; i < n; i++) {
		cJSON *k = cJSON_CreateObject();
		char id[13], t[64] = "", b[1100] = "", c[200] = "";
		key_id(lines[i], id);
		int used = 0;
		sscanf(lines[i], "%63s %1099s %n", t, b, &used);
		if (used) snprintf(c, sizeof(c), "%s", lines[i] + used);
		cJSON_AddStringToObject(k, "id", id);
		cJSON_AddStringToObject(k, "type", t);
		cJSON_AddStringToObject(k, "name", c[0] ? c : "key");
		size_t bl = strlen(b);
		cJSON_AddStringToObject(k, "tail", bl > 8 ? b + bl - 8 : b);
		cJSON_AddItemToArray(ks, k);
	}
	cJSON *ps = cJSON_AddArrayToObject(o, "pending");
	for (int i = 0; i < npend; i++) {
		cJSON *k = cJSON_CreateObject();
		char t[64] = "", b[1100] = "";
		sscanf(pend[i].line, "%63s %1099s", t, b);
		size_t bl = strlen(b);
		cJSON_AddNumberToObject(k, "n", i);
		cJSON_AddStringToObject(k, "from", pend[i].from);
		cJSON_AddStringToObject(k, "type", t);
		cJSON_AddStringToObject(k, "tail", bl > 8 ? b + bl - 8 : b);
		cJSON_AddItemToArray(ps, k);
	}
	pthread_mutex_unlock(&mu);
	read_line("keep", keepdir, sizeof(keepdir));
	cJSON *kp = cJSON_AddObjectToObject(o, "keep");
	cJSON_AddStringToObject(kp, "dir", keepdir);
	if (keepdir[0]) {
		snprintf(path, sizeof(path), "%s/phosphor-keep/remote/stamp", keepdir);
		char *s = read_all(path, 400);
		long saved = 0;
		if (s) { char *x = strstr(s, "saved="); if (x) saved = atol(x + 6); free(s); }
		cJSON_AddNumberToObject(kp, "saved", (double)saved);
		cJSON_AddBoolToObject(kp, "there", access(keepdir, W_OK) == 0);
	}
	cJSON_AddStringToObject(kp, "error", last_err);
	char r[PATH_MAX + 64];
	read_line("restored", r, sizeof(r));
	if (r[0]) {
		cJSON *x = cJSON_AddObjectToObject(o, "restored");
		char *bar = strchr(r, '|');
		if (bar) *bar = 0;
		cJSON_AddStringToObject(x, "from", r);
		cJSON_AddNumberToObject(x, "saved", bar ? atof(bar + 1) : 0);
	}
	if (places) add_places(o);
	return o;
}

/* a paired device (or a local tool) offers keys; they wait for a yes on the TV */
int sshacc_offer(cJSON *keys, const char *from, const char **err)
{
	int added = 0;
	cJSON *it;
	static char have[MAX_KEYS][1200];
	pthread_mutex_lock(&mu);
	int nh = load_keys(have, MAX_KEYS);
	cJSON_ArrayForEach(it, keys) {
		const char *k = cJSON_IsString(it) ? it->valuestring : cJSON_GetStringValue(cJSON_GetObjectItem(it, "key"));
		const char *nm = cJSON_IsObject(it) ? cJSON_GetStringValue(cJSON_GetObjectItem(it, "name")) : NULL;
		char line[1200], nmb[80], b[1100], hb[1100];
		snprintf(nmb, sizeof(nmb), "%s%s%s", from && *from ? from : "", nm && *nm ? ": " : "", nm && *nm ? nm : "");
		if (clean_key(k, nmb, line, sizeof(line))) { *err = "that isn't an SSH public key"; continue; }
		key_blob(line, b, sizeof(b));
		int dup = 0;
		for (int i = 0; i < nh && !dup; i++) { key_blob(have[i], hb, sizeof(hb)); dup = !strcmp(b, hb); }
		for (int i = 0; i < npend && !dup; i++) { key_blob(pend[i].line, hb, sizeof(hb)); dup = !strcmp(b, hb); }
		if (dup) { added++; continue; }              /* already there or waiting: fine */
		if (npend >= MAX_PENDING) { *err = "too many keys waiting: answer them on the TV first"; break; }
		pending_t *p = &pend[npend++];
		snprintf(p->line, sizeof(p->line), "%s", line);
		snprintf(p->from, sizeof(p->from), "%s", from && *from ? from : "a device");
		p->at = time(NULL);
		added++;
	}
	signal_shell();
	pthread_mutex_unlock(&mu);
	if (added) plog("remote access: %d SSH key(s) offered by %s, waiting for the TV", added, from && *from ? from : "a device");
	return added;
}

static int add_key_line(const char *line, const char **err)
{
	static char lines[MAX_KEYS][1200];
	int n = load_keys(lines, MAX_KEYS);
	char b[1100], hb[1100];
	key_blob(line, b, sizeof(b));
	for (int i = 0; i < n; i++) { key_blob(lines[i], hb, sizeof(hb)); if (!strcmp(b, hb)) return 0; }
	if (n >= MAX_KEYS) { *err = "32 keys already: remove one first"; return -1; }
	snprintf(lines[n++], 1200, "%s", line);
	if (save_keys(lines, n)) { *err = "couldn't save the keys"; return -1; }
	return 0;
}

const char *sshacc_do(cJSON *req, char *msg, size_t mn)
{
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	static char err[200];
	const char *e = NULL;
	char p[PATH_MAX];
	msg[0] = 0;
	err[0] = 0;
	if (!d) return "do missing";
	if (!strcmp(d, "ssh_mode")) {
		const char *m = cJSON_GetStringValue(cJSON_GetObjectItem(req, "mode"));
		if (!m || (strcmp(m, "off") && strcmp(m, "password") && strcmp(m, "keys"))) return "mode: off, password or keys";
		if (!strcmp(m, "keys")) {
			static char lines[MAX_KEYS][1200];
			pthread_mutex_lock(&mu);
			int n = load_keys(lines, MAX_KEYS);
			pthread_mutex_unlock(&mu);
			if (!n) return "Add a key first: keys only with no key would lock everyone out";
		}
		conf(p, sizeof(p), "mode");
		char t[16];
		snprintf(t, sizeof(t), "%s\n", m);
		if (write_all(p, t)) return "couldn't save the SSH mode";
		plog("remote access: ssh %s", m);
	} else if (!strcmp(d, "ssh_password")) {
		const char *pw = cJSON_GetStringValue(cJSON_GetObjectItem(req, "password"));
		if (!pw || strlen(pw) < 4) return "at least 4 characters";
		if (strlen(pw) > 72 || strpbrk(pw, "\r\n")) return "a password on one line, up to 72 characters";
		char out[300];
		if (run("setpw", pw, out, sizeof(out))) { snprintf(err, sizeof(err), "%s", out[0] ? out : "couldn't set the password"); return err; }
		plog("remote access: a new SSH password");
		mark();
		return NULL;                        /* (setpw applies it) */
	} else if (!strcmp(d, "ssh_password_reset")) {
		char out[300];
		if (run("resetpw", NULL, out, sizeof(out))) { snprintf(err, sizeof(err), "%s", out); return err; }
		plog("remote access: MiSTer's own SSH password again");
		mark();
		return NULL;
	} else if (!strcmp(d, "ssh_key_add")) {      /* typed or pasted on the MiSTer itself */
		char line[1200];
		const char *k = cJSON_GetStringValue(cJSON_GetObjectItem(req, "key"));
		const char *nm = cJSON_GetStringValue(cJSON_GetObjectItem(req, "name"));
		if (clean_key(k, nm ? nm : "added on the MiSTer", line, sizeof(line))) return "that isn't an SSH public key";
		pthread_mutex_lock(&mu);
		int r = add_key_line(line, &e);
		pthread_mutex_unlock(&mu);
		if (r) return e;
	} else if (!strcmp(d, "ssh_key_remove")) {
		const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(req, "id"));
		if (!id) return "id missing";
		static char lines[MAX_KEYS][1200];
		pthread_mutex_lock(&mu);
		int n = load_keys(lines, MAX_KEYS), w = 0, gone = 0;
		for (int i = 0; i < n; i++) {
			char k[13];
			key_id(lines[i], k);
			if (!strcmp(k, id)) { gone = 1; continue; }
			if (w != i) memcpy(lines[w], lines[i], sizeof(lines[0]));
			w++;
		}
		char m[16];
		int locked = gone && w == 0 && !strcmp(mode_now(m, sizeof(m)), "keys");
		int r = gone && !locked ? save_keys(lines, w) : 0;
		pthread_mutex_unlock(&mu);
		if (!gone) return "no such key";
		if (locked) return "That's the last key and SSH is keys only: change SSH to Password first";
		if (r) return "couldn't save the keys";
		plog("remote access: an SSH key removed");
	} else if (!strcmp(d, "ssh_key_accept") || !strcmp(d, "ssh_key_decline")) {
		cJSON *nn = cJSON_GetObjectItem(req, "n");
		int i = cJSON_IsNumber(nn) ? nn->valueint : -1, accept = !strcmp(d, "ssh_key_accept");
		pthread_mutex_lock(&mu);
		if (i < 0 || i >= npend) { pthread_mutex_unlock(&mu); return "no such key waiting"; }
		pending_t pk = pend[i];
		memmove(&pend[i], &pend[i + 1], (size_t)(npend - i - 1) * sizeof(pend[0]));
		npend--;
		signal_shell();
		int r = accept ? add_key_line(pk.line, &e) : 0;
		pthread_mutex_unlock(&mu);
		if (r) return e;
		plog("remote access: an SSH key from %s %s", pk.from, accept ? "added" : "declined");
		if (!accept) return NULL;
	} else if (!strcmp(d, "keep")) {
		const char *dir = cJSON_GetStringValue(cJSON_GetObjectItem(req, "dir"));
		if (!dir) return "dir missing";
		if (*dir && (strncmp(dir, "/media/usb", 10) || strstr(dir, "..") || strchr(dir + 10, '/') || access(dir, W_OK))) return "not a drive";
		conf(p, sizeof(p), "keep");
		char t[PATH_MAX + 2];
		snprintf(t, sizeof(t), "%s\n", dir);
		if (write_all(p, t)) return "couldn't save that";
		if (*dir) keep_now();
		plog("remote access: copy %s%s", *dir ? "kept on " : "off", dir);
		return NULL;
	} else if (!strcmp(d, "keep_now")) {
		keep_now();
		return last_err[0] ? last_err : NULL;
	} else if (!strcmp(d, "restored_seen")) {
		conf(p, sizeof(p), "restored");
		unlink(p);
		return NULL;
	} else return "do: ssh_mode, ssh_password, ssh_password_reset, ssh_key_add, ssh_key_remove, ssh_key_accept, ssh_key_decline, keep, keep_now, restored_seen";
	/* the change into effect */
	if (apply(err, sizeof(err))) return err;
	if (err[0]) snprintf(msg, mn, "%s", err);
	mark();
	return NULL;
}
