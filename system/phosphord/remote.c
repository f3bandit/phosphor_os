/* phosphord: remote access over the home network (phone app, PC app, Claude plugin).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Off until turned on in Settings > Remote access. Plain HTTP/1.1 on port 8767, one request per
 * connection, only from home-network addresses (10/8, 172.16/12, 192.168/16, 169.254/16, 127/8,
 * or the subnet of one of the SS1's own network connections).
 *
 *   GET  /info                       {name, version, api}                   no key needed
 *   POST /pair    {"code","name"}    {token, id}: the code shown on the TV; 5 tries, 2 minutes
 *   POST /api     {"cmd":...}        the same JSON commands as the local socket, a safe subset
 *   GET  /art/<game id>              the game's box art (JPEG)
 *   PUT  /art/<game id>   <image>    new box art (PNG/JPEG/..., up to 16 MB), scaled and stored
 *
 * Every request but /info and /pair needs "Authorization: Bearer <token>". Only a hash of each
 * token is kept (config/remote.ini). This thread doesn't touch the database: commands go through
 * phosphord's own socket like any local client, so they run one at a time in the main loop. */
#include "remote.h"
#include "cJSON.h"
#include "log.h"
#include "paths.h"
#include "sha256.h"
#include "version.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define API_VERSION 1
#define HEAD_MAX (16 * 1024)
#define API_BODY_MAX (1024 * 1024)
#define ART_BODY_MAX (16 * 1024 * 1024)
#define PAIR_SECONDS 120
#define PAIR_TRIES 5

/* commands a paired device may send: nothing that changes the network, pairing or remote access */
static const char *ALLOWED[] = {
	"status", "categories", "systems", "games", "game", "last", "launch", "favorite", "rescan",
	"link", "unlink", "candidates", "edit", "scrape", "rescrape", "scrape_sources",
	"video", "video_files", "diag", "cloud", "backup", "memcard", "pads", "pad_analog", "ports", "bluetooth", NULL,
};

typedef struct {
	char hash[65];
	char name[64];
	long created, last_seen;
} dev_t_;

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static int on, port = REMOTE_PORT, listening;
static char err_msg[128];
static dev_t_ devs[REMOTE_MAX_DEVICES];
static int ndevs;
static char code[8];
static long code_until;
static int code_tries;
static char paired_name[64];
static long saved_at;

/* ---------- settings file ---------- */

static void ini_path(char *out, size_t n) { snprintf(out, n, "%s/remote.ini", p_config()); }

static void load(void)
{
	char path[PATH_MAX], line[512];
	ini_path(path, sizeof(path));
	FILE *f = fopen(path, "r");
	if (!f) return;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		char *s = line;
		while (*s == ' ' || *s == '\t') s++;
		if (!*s || *s == ';' || *s == '#') continue;
		char *eq = strchr(s, '=');
		if (!eq) continue;
		*eq = 0;
		char *k = s, *v = eq + 1;
		for (char *e = eq - 1; e >= k && (*e == ' ' || *e == '\t'); e--) *e = 0;
		while (*v == ' ' || *v == '\t') v++;
		if (!strcmp(k, "on")) on = atoi(v) != 0;
		else if (!strcmp(k, "port")) { int p = atoi(v); if (p > 0 && p < 65536) port = p; }
		else if (!strcmp(k, "device") && ndevs < REMOTE_MAX_DEVICES) {
			dev_t_ *d = &devs[ndevs];
			int used = 0;
			if (sscanf(v, "%64s %ld %ld %n", d->hash, &d->created, &d->last_seen, &used) >= 3 && strlen(d->hash) == 64) {
				snprintf(d->name, sizeof(d->name), "%s", v + used);
				ndevs++;
			}
		}
	}
	fclose(f);
}

/* call with mu held */
static int save(void)
{
	char path[PATH_MAX], tmp[PATH_MAX + 8];
	ini_path(path, sizeof(path));
	snprintf(tmp, sizeof(tmp), "%s.new", path);
	mkdirs(p_config());
	FILE *f = fopen(tmp, "w");
	if (!f) return -1;
	fprintf(f, "; Remote access (Settings > Remote access). Devices: key hash, paired, last used, name.\n"
	           "; Only hashes of the keys are kept here; remove a line (or use Settings) to forget a device.\n");
	fprintf(f, "on = %d\nport = %d\n", on, port);
	for (int i = 0; i < ndevs; i++) fprintf(f, "device = %s %ld %ld %s\n", devs[i].hash, devs[i].created, devs[i].last_seen, devs[i].name);
	int bad = fflush(f) != 0 || fsync(fileno(f)) != 0;
	bad |= fclose(f) != 0;
	if (bad || rename(tmp, path) != 0) { unlink(tmp); return -1; }
	saved_at = time(NULL);
	return 0;
}

/* ---------- state for the API ---------- */

static void addresses(remote_info_t *ri)
{
	struct ifaddrs *ifa, *i;
	ri->naddr = 0;
	if (getifaddrs(&ifa) != 0) return;
	for (i = ifa; i && ri->naddr < 4; i = i->ifa_next) {
		if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP)) continue;
		inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, ri->addr[ri->naddr], sizeof(ri->addr[0]));
		ri->naddr++;
	}
	freeifaddrs(ifa);
}

void remote_info(remote_info_t *ri)
{
	memset(ri, 0, sizeof(*ri));
	addresses(ri);
	pthread_mutex_lock(&mu);
	long now = time(NULL);
	if (code[0] && now >= code_until) code[0] = 0;
	ri->on = on;
	ri->port = port;
	ri->listening = listening;
	snprintf(ri->error, sizeof(ri->error), "%s", on && !listening ? err_msg : "");
	snprintf(ri->code, sizeof(ri->code), "%s", code);
	ri->code_left = code[0] ? (int)(code_until - now) : 0;
	snprintf(ri->paired, sizeof(ri->paired), "%s", paired_name);
	for (int i = 0; i < ndevs; i++) {
		snprintf(ri->dev[i].id, sizeof(ri->dev[i].id), "%.16s", devs[i].hash);
		snprintf(ri->dev[i].name, sizeof(ri->dev[i].name), "%s", devs[i].name);
		ri->dev[i].created = devs[i].created;
		ri->dev[i].last_seen = devs[i].last_seen;
	}
	ri->ndev = ndevs;
	pthread_mutex_unlock(&mu);
}

int remote_set_on(int v)
{
	pthread_mutex_lock(&mu);
	on = v != 0;
	if (!on) code[0] = 0;
	int r = save();
	pthread_mutex_unlock(&mu);
	plog("remote access %s", v ? "on" : "off");
	return r;
}

static int random_bytes(void *buf, size_t n)
{
	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0) return -1;
	size_t got = 0;
	while (got < n) {
		ssize_t r = read(fd, (char *)buf + got, n - got);
		if (r <= 0) { if (r < 0 && errno == EINTR) continue; close(fd); return -1; }
		got += (size_t)r;
	}
	close(fd);
	return 0;
}

int remote_pair_start(void)
{
	uint32_t r;
	if (random_bytes(&r, sizeof(r))) return -1;
	pthread_mutex_lock(&mu);
	snprintf(code, sizeof(code), "%06u", r % 1000000u);
	code_until = time(NULL) + PAIR_SECONDS;
	code_tries = 0;
	paired_name[0] = 0;
	pthread_mutex_unlock(&mu);
	return 0;
}

void remote_pair_stop(void)
{
	pthread_mutex_lock(&mu);
	code[0] = 0;
	pthread_mutex_unlock(&mu);
}

int remote_forget(const char *id)
{
	int r = -1;
	size_t n = strlen(id);
	pthread_mutex_lock(&mu);
	for (int i = 0; n >= 8 && i < ndevs; i++)
		if (!strncmp(devs[i].hash, id, n)) {
			plog("remote access: forgot %s", devs[i].name);
			memmove(&devs[i], &devs[i + 1], (size_t)(ndevs - i - 1) * sizeof(devs[0]));
			ndevs--;
			save();
			r = 0;
			break;
		}
	pthread_mutex_unlock(&mu);
	return r;
}

/* ---------- HTTP ---------- */

typedef struct {
	int fd;
	long long deadline_ms;     /* the whole request must arrive by then: a slow client can't hold the server */
	char head[HEAD_MAX + 1];
	size_t hlen, body_off;     /* body bytes already read start at head + body_off */
	char method[8], path[256];
	long clen;
	int chunked, expect;
	char auth[160];
} req_t;

static long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* recv, but never past the request's deadline */
static ssize_t rx(req_t *r, void *buf, size_t n)
{
	for (;;) {
		long long left = r->deadline_ms - now_ms();
		if (left <= 0) { errno = ETIMEDOUT; return -1; }
		struct pollfd p = { r->fd, POLLIN, 0 };
		int k = poll(&p, 1, (int)(left > 5000 ? 5000 : left));
		if (k < 0) { if (errno == EINTR) continue; return -1; }
		if (k == 0) continue;
		ssize_t got = recv(r->fd, buf, n, 0);
		if (got < 0 && errno == EINTR) continue;
		return got;
	}
}

static int send_all(int fd, const void *p, size_t n)
{
	const char *s = p;
	while (n) {
		ssize_t w = send(fd, s, n, MSG_NOSIGNAL);
		if (w < 0) { if (errno == EINTR) continue; return -1; }
		s += w;
		n -= (size_t)w;
	}
	return 0;
}

static const char *status_text(int code_)
{
	switch (code_) {
	case 200: return "OK";
	case 400: return "Bad Request";
	case 401: return "Unauthorized";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 411: return "Length Required";
	case 413: return "Payload Too Large";
	case 429: return "Too Many Requests";
	case 500: return "Internal Server Error";
	default: return "Service Unavailable";
	}
}

static void respond(int fd, int st, const char *type, const void *body, size_t n)
{
	char h[300];
	int hl = snprintf(h, sizeof(h), "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
	                  "Cache-Control: no-store\r\nConnection: close\r\n\r\n", st, status_text(st), type, n);
	if (send_all(fd, h, (size_t)hl) == 0 && n) send_all(fd, body, n);
}

static void respond_json(int fd, int st, const char *json) { respond(fd, st, "application/json", json, strlen(json)); }

static void respond_error(int fd, int st, const char *msg)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddBoolToObject(o, "ok", 0);
	cJSON_AddStringToObject(o, "error", msg);
	char *s = cJSON_PrintUnformatted(o);
	cJSON_Delete(o);
	respond_json(fd, st, s ? s : "{}");
	free(s);
}

static int read_head(req_t *r)
{
	r->hlen = 0;
	for (;;) {
		if (r->hlen >= HEAD_MAX) return 413;
		ssize_t n = rx(r, r->head + r->hlen, HEAD_MAX - r->hlen);
		if (n <= 0) return -1;
		r->hlen += (size_t)n;
		r->head[r->hlen] = 0;
		char *e = strstr(r->head, "\r\n\r\n");
		if (e) { r->body_off = (size_t)(e - r->head) + 4; break; }
	}
	char *line = r->head, *eol = strstr(line, "\r\n");
	*eol = 0;
	if (sscanf(line, "%7s %255s", r->method, r->path) != 2) return 400;
	r->clen = -1;
	for (char *h = eol + 2; h < r->head + r->body_off - 2;) {
		char *e = strstr(h, "\r\n");
		if (!e) break;
		*e = 0;
		char *c = strchr(h, ':');
		if (c) {
			*c = 0;
			char *v = c + 1;
			while (*v == ' ' || *v == '\t') v++;
			if (!strcasecmp(h, "content-length")) {
				char *end;
				errno = 0;
				long l = strtol(v, &end, 10);
				if (errno || *end || l < 0) return 400;
				r->clen = l;
			} else if (!strcasecmp(h, "transfer-encoding")) r->chunked = strcasecmp(v, "identity") != 0;
			else if (!strcasecmp(h, "expect")) r->expect = !strcasecmp(v, "100-continue");
			else if (!strcasecmp(h, "authorization")) snprintf(r->auth, sizeof(r->auth), "%s", v);
		}
		h = e + 2;
	}
	return 0;
}

/* reads the body into buf (n = Content-Length), or into fd out when out >= 0 */
static int read_body(req_t *r, char *buf, int out)
{
	size_t have = r->hlen - r->body_off, want = (size_t)r->clen;
	if (have > want) have = want;
	if (r->expect && have < want) send_all(r->fd, "HTTP/1.1 100 Continue\r\n\r\n", 25);
	if (out >= 0) { if (have && write(out, r->head + r->body_off, have) != (ssize_t)have) return -1; }
	else memcpy(buf, r->head + r->body_off, have);
	size_t got = have;
	char tmp[16384];
	while (got < want) {
		size_t chunk = want - got;
		if (out >= 0 && chunk > sizeof(tmp)) chunk = sizeof(tmp);
		ssize_t n = rx(r, out >= 0 ? tmp : buf + got, chunk);
		if (n <= 0) return -1;
		if (out >= 0 && write(out, tmp, (size_t)n) != n) return -1;
		got += (size_t)n;
	}
	if (buf) buf[want] = 0;
	return 0;
}

/* a home-network address: the private ranges, or the same subnet as one of the SS1's own
   network connections (some home networks use other ranges, e.g. 200.200.200.x). Only a directly
   attached subnet counts, and nothing wider than a /16. */
static int local_peer(const struct sockaddr_in *a)
{
	uint32_t ip = ntohl(a->sin_addr.s_addr);
	if ((ip >> 24) == 10 || (ip >> 24) == 127 || (ip >> 20) == (172u << 4 | 1) ||
	    (ip >> 16) == (192u << 8 | 168) || (ip >> 16) == (169u << 8 | 254))
		return 1;
	struct ifaddrs *ifs, *i;
	int ok = 0;
	if (getifaddrs(&ifs)) return 0;
	for (i = ifs; i && !ok; i = i->ifa_next) {
		if (!i->ifa_addr || !i->ifa_netmask || i->ifa_addr->sa_family != AF_INET) continue;
		if (!(i->ifa_flags & IFF_UP) || (i->ifa_flags & IFF_LOOPBACK)) continue;
		uint32_t me = ntohl(((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr);
		uint32_t m = ntohl(((struct sockaddr_in *)i->ifa_netmask)->sin_addr.s_addr);
		if (m >= 0xffff0000u && m != 0xffffffffu && (me & m) == (ip & m)) ok = 1;
	}
	freeifaddrs(ifs);
	return ok;
}

/* the device for this request's key; updates last used. Returns its index or -1. */
static int authorize(const req_t *r, char *name, size_t nn)
{
	if (strncasecmp(r->auth, "Bearer ", 7)) return -1;
	const char *tok = r->auth + 7;
	if (strlen(tok) != 64) return -1;
	char h[65];
	sha256_hex(tok, 64, h);
	int found = -1;
	pthread_mutex_lock(&mu);
	for (int i = 0; i < ndevs; i++) {
		int diff = 0;
		for (int k = 0; k < 64; k++) diff |= devs[i].hash[k] ^ h[k];
		if (!diff) {
			found = i;
			snprintf(name, nn, "%s", devs[i].name);
			devs[i].last_seen = time(NULL);
			if (devs[i].last_seen - saved_at > 600) save();
		}
	}
	pthread_mutex_unlock(&mu);
	return found;
}

/* one command through phosphord's own socket; malloc'd reply or NULL */
static char *forward(const char *json)
{
	int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (s < 0) return NULL;
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof(a.sun_path), "%s", p_socket());
	struct timeval tv = { 120, 0 };
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0 || send_all(s, json, strlen(json)) || send_all(s, "\n", 1)) {
		close(s);
		return NULL;
	}
	size_t cap = 65536, len = 0;
	char *buf = malloc(cap);
	for (;;) {
		if (len + 4096 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) break; buf = nb; }
		ssize_t n = recv(s, buf + len, cap - len - 1, 0);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) break;
		len += (size_t)n;
		buf[len] = 0;
		char *nl = memchr(buf, '\n', len);
		if (nl) { *nl = 0; close(s); return buf; }
	}
	close(s);
	free(buf);
	return NULL;
}

static void do_pair(req_t *r, const char *body)
{
	cJSON *j = cJSON_Parse(body);
	const char *c = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "code")) : NULL;
	const char *nm = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "name")) : NULL;
	char name[64];
	snprintf(name, sizeof(name), "%s", nm && *nm ? nm : "Device");
	for (char *p = name; *p; p++) if ((unsigned char)*p < 0x20) *p = ' ';
	int st = 0;
	char token[65] = "", id[17] = "";
	pthread_mutex_lock(&mu);
	long now = time(NULL);
	if (!code[0] || now >= code_until) st = 403;
	else if (!c || strlen(c) != 6) st = 400;
	else {
		int diff = 0;
		for (int k = 0; k < 6; k++) diff |= c[k] ^ code[k];
		if (diff) {
			if (++code_tries >= PAIR_TRIES) code[0] = 0;
			st = 401;
		} else if (ndevs >= REMOTE_MAX_DEVICES) st = 429;
		else {
			uint8_t raw[32];
			if (random_bytes(raw, sizeof(raw))) st = 500;
			else {
				for (int k = 0; k < 32; k++) sprintf(token + k * 2, "%02x", raw[k]);
				dev_t_ *d = &devs[ndevs++];
				sha256_hex(token, 64, d->hash);
				snprintf(d->name, sizeof(d->name), "%s", name);
				d->created = d->last_seen = now;
				snprintf(id, sizeof(id), "%.16s", d->hash);
				snprintf(paired_name, sizeof(paired_name), "%s", name);
				code[0] = 0;          /* one device per code */
				if (save()) st = 500;
				else st = 200;
			}
		}
	}
	pthread_mutex_unlock(&mu);
	cJSON_Delete(j);
	if (st == 200) {
		plog("remote access: paired %s", name);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddBoolToObject(o, "ok", 1);
		cJSON_AddStringToObject(o, "token", token);
		cJSON_AddStringToObject(o, "id", id);
		char *s = cJSON_PrintUnformatted(o);
		cJSON_Delete(o);
		respond_json(r->fd, 200, s);
		free(s);
	} else {
		sleep(1);   /* slows down guessing */
		respond_error(r->fd, st, st == 403 ? "pairing isn't open: choose Pair a device on the MiSTer" :
		                         st == 401 ? "wrong code" : st == 429 ? "too many devices: forget one in Settings" :
		                         st == 400 ? "code and name needed" : "couldn't save the device");
	}
}

static void do_api(req_t *r, const char *body)
{
	cJSON *j = cJSON_Parse(body);
	const char *cmd = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "cmd")) : NULL;
	int allowed = 0;
	for (int i = 0; cmd && ALLOWED[i]; i++) if (!strcmp(cmd, ALLOWED[i])) allowed = 1;
	if (!j || !cmd) { cJSON_Delete(j); respond_error(r->fd, 400, "send a JSON command: {\"cmd\":...}"); return; }
	/* one "cmd" only (cJSON matches keys without case): no doubt about which one phosphord runs */
	int ncmd = 0;
	for (cJSON *it = j->child; it; it = it->next) if (it->string && !strcasecmp(it->string, "cmd")) ncmd++;
	if (ncmd != 1) { cJSON_Delete(j); respond_error(r->fd, 400, "one \"cmd\" only"); return; }
	if (!allowed) { cJSON_Delete(j); respond_error(r->fd, 403, "that command only works on the MiSTer itself"); return; }
	/* "diag" may look, not make support bundles (they hold system details); every "bundle" key, any case */
	while (cJSON_GetObjectItem(j, "bundle")) cJSON_DeleteItemFromObject(j, "bundle");
	/* "bluetooth" may look, not pair, forget or disconnect: pairing only from the MiSTer itself */
	if (!strcmp(cmd, "bluetooth")) while (cJSON_GetObjectItem(j, "do")) cJSON_DeleteItemFromObject(j, "do");
	char *s = cJSON_PrintUnformatted(j);
	cJSON_Delete(j);
	char *out = s ? forward(s) : NULL;
	free(s);
	if (!out) { respond_error(r->fd, 503, "phosphord didn't answer"); return; }
	respond_json(r->fd, 200, out);
	free(out);
}

static long art_id(const char *path)
{
	char *end;
	long id = strtol(path + 5, &end, 10);
	return (*end || id <= 0) ? -1 : id;
}

static void do_art_get(req_t *r, long id)
{
	char q[96];
	snprintf(q, sizeof(q), "{\"cmd\":\"game\",\"id\":%ld}", id);
	char *out = forward(q);
	cJSON *j = out ? cJSON_Parse(out) : NULL;
	free(out);
	cJSON *g = j ? cJSON_GetObjectItem(j, "game") : NULL;
	const char *art = g ? cJSON_GetStringValue(cJSON_GetObjectItem(g, "art")) : NULL;
	char real[PATH_MAX], root[PATH_MAX], want[PATH_MAX];
	snprintf(want, sizeof(want), "%s/art", p_data());
	if (!art || !realpath(art, real) || !realpath(want, root) || strncmp(real, root, strlen(root)) || real[strlen(root)] != '/') {
		cJSON_Delete(j);
		respond_error(r->fd, 404, "no box art");
		return;
	}
	cJSON_Delete(j);
	int fd = open(real, O_RDONLY | O_CLOEXEC);
	struct stat sb;
	if (fd < 0 || fstat(fd, &sb) || sb.st_size > ART_BODY_MAX) { if (fd >= 0) close(fd); respond_error(r->fd, 404, "no box art"); return; }
	char *buf = malloc((size_t)sb.st_size + 1);
	ssize_t n = 0;
	while (buf && n < sb.st_size) {
		ssize_t k = read(fd, buf + n, (size_t)(sb.st_size - n));
		if (k < 0 && errno == EINTR) continue;
		if (k <= 0) break;
		n += k;
	}
	close(fd);
	if (!buf || n != sb.st_size) { free(buf); respond_error(r->fd, 500, "couldn't read the art"); return; }
	respond(r->fd, 200, "image/jpeg", buf, (size_t)n);
	free(buf);
}

static void do_art_put(req_t *r, long id)
{
	if (r->clen > ART_BODY_MAX) { respond_error(r->fd, 413, "image too big (16 MB at most)"); return; }
	r->deadline_ms = now_ms() + 30000 + r->clen / 50;   /* body: 30 s plus 20 s a MB (slow WiFi) */
	char tmp[PATH_MAX];
	snprintf(tmp, sizeof(tmp), "%s/remote-art.XXXXXX", p_tmp());
	int fd = mkstemp(tmp);
	if (fd < 0) { respond_error(r->fd, 500, "no room for the upload"); return; }
	int bad = read_body(r, NULL, fd);
	close(fd);
	if (bad) { unlink(tmp); return; }
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "set_art");
	cJSON_AddNumberToObject(q, "id", (double)id);
	cJSON_AddStringToObject(q, "file", tmp);
	char *s = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	char *out = forward(s);
	free(s);
	unlink(tmp);
	if (!out) { respond_error(r->fd, 503, "phosphord didn't answer"); return; }
	respond_json(r->fd, 200, out);
	free(out);
}

static void handle(int fd)
{
	req_t *r = calloc(1, sizeof(*r));
	if (!r) { close(fd); return; }
	r->fd = fd;
	r->deadline_ms = now_ms() + 10000;                   /* headers: 10 s */
	struct timeval tv = { 15, 0 };
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	int e = read_head(r);
	if (e) { if (e > 0) respond_error(fd, e, "bad request"); goto done; }
	if (r->chunked) { respond_error(fd, 411, "send Content-Length"); goto done; }

	if (!strcmp(r->path, "/info")) {
		if (strcmp(r->method, "GET")) { respond_error(fd, 405, "GET"); goto done; }
		char s[160];
		snprintf(s, sizeof(s), "{\"ok\":true,\"name\":\"Phosphor\",\"version\":\"%s\",\"api\":%d}", PHOSD_VERSION, API_VERSION);
		respond_json(fd, 200, s);
		goto done;
	}
	int is_api = !strcmp(r->path, "/api"), is_pair = !strcmp(r->path, "/pair"), is_art = !strncmp(r->path, "/art/", 5);
	if (!is_api && !is_pair && !is_art) { respond_error(fd, 404, "no such path"); goto done; }
	if ((is_api || is_pair) && strcmp(r->method, "POST")) { respond_error(fd, 405, "POST"); goto done; }
	if (is_art && strcmp(r->method, "GET") && strcmp(r->method, "PUT")) { respond_error(fd, 405, "GET or PUT"); goto done; }

	char who[64] = "";
	if (!is_pair && authorize(r, who, sizeof(who)) < 0) {
		sleep(1);
		respond_error(fd, 401, "pair this device first (Settings > Remote access on the MiSTer)");
		goto done;
	}
	if (is_art) {
		long id = art_id(r->path);
		if (id < 0) { respond_error(fd, 404, "no such game"); goto done; }
		if (!strcmp(r->method, "GET")) do_art_get(r, id);
		else if (r->clen < 0) respond_error(fd, 411, "send Content-Length");
		else do_art_put(r, id);
		goto done;
	}
	if (r->clen < 0) { respond_error(fd, 411, "send Content-Length"); goto done; }
	if (r->clen > API_BODY_MAX) { respond_error(fd, 413, "request too big"); goto done; }
	r->deadline_ms = now_ms() + 30000;                   /* body: 30 s */
	char *body = malloc((size_t)r->clen + 1);
	if (!body || read_body(r, body, -1)) { free(body); goto done; }
	if (is_pair) do_pair(r, body);
	else do_api(r, body);
	free(body);
done:
	free(r);
	close(fd);
}

/* ---------- the thread ---------- */

static int open_listener(int p)
{
	int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (s < 0) return -1;
	int one = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons((uint16_t)p), .sin_addr.s_addr = htonl(INADDR_ANY) };
	if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(s, 8) < 0) {
		pthread_mutex_lock(&mu);
		snprintf(err_msg, sizeof(err_msg), "port %d: %s", p, strerror(errno));
		pthread_mutex_unlock(&mu);
		close(s);
		return -1;
	}
	return s;
}

static void *run(void *arg)
{
	(void)arg;
	int lfd = -1, lport = 0;
	long retry_at = 0;
	for (;;) {
		pthread_mutex_lock(&mu);
		int want = on, p = port;
		pthread_mutex_unlock(&mu);
		if ((!want || p != lport) && lfd >= 0) {
			close(lfd);
			lfd = -1;
			plog("remote access: stopped listening");
		}
		if (want && lfd < 0 && time(NULL) >= retry_at) {
			lfd = open_listener(p);
			if (lfd >= 0) { lport = p; plog("remote access: listening on port %d", p); }
			else retry_at = time(NULL) + 10;
		}
		pthread_mutex_lock(&mu);
		listening = lfd >= 0;
		pthread_mutex_unlock(&mu);
		if (lfd < 0) { poll(NULL, 0, 1000); continue; }
		struct pollfd pf = { lfd, POLLIN, 0 };
		if (poll(&pf, 1, 1000) <= 0) continue;
		struct sockaddr_in peer;
		socklen_t pl = sizeof(peer);
		int fd = accept4(lfd, (struct sockaddr *)&peer, &pl, SOCK_CLOEXEC);
		if (fd < 0) continue;
		if (peer.sin_family != AF_INET || !local_peer(&peer)) {
			char ip[32];
			inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
			plog("remote access: refused %s (not on the home network)", ip);
			close(fd);
			continue;
		}
		handle(fd);
	}
	return NULL;
}

void remote_start(void)
{
	load();
	pthread_t t;
	if (pthread_create(&t, NULL, run, NULL) == 0) pthread_detach(t);
}
