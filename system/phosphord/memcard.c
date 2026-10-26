/* phosphord: the SuperStation One's front ports (controllers, memory cards) through phosphor-main.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * phosphor-main relays the Phosphor menu core's front ports (SNAC) on its socket (ports, pads,
 * pad_analog, card_read, card_write; see phosphor-main's PHOSPHOR.md). phosphord keeps one
 * connection to it, owns the ports while something here needs them and turns them off after a
 * while without use (the user port then goes back to MT32-pi / MIDI).
 *
 * If the stock menu core is loaded and the Phosphor one is installed (<fat>/phosphor/cores/
 * menu_phosphor.rbf), ports_prepare loads it (only from the menu, never in a game). menu.rbf is
 * never replaced: any other core loaded afterwards brings the stock menu core back.
 *
 * Card jobs run in their own thread, one at a time:
 *   backup   the whole card (1024 sectors of 128 bytes, each read up to 4 times) to
 *            <fat>/phosphor/memcards/port<N>-YYYYMMDD-HHMMSS.mcd, the format MiSTer's PSX core uses
 *   restore  a backup back onto the card: backs the card up first (that backup stays), writes
 *            only the sectors that differ, reads every one of them back
 *   check    is there a card, and is it formatted ("MC" in sector 0)
 *   prepare  load the Phosphor menu core if needed and turn the ports on (Main restarts: seconds)
 * Reading the controllers never loads the menu core; only a job does. */
#include "memcard.h"
#include "mclib.h"
#include "cJSON.h"
#include "log.h"
#include "paths.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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

#define SECTORS 1024
#define SECTOR 128
#define CARD_BYTES (SECTORS * SECTOR)
#define TRIES 6     /* like SS1 MemCard (the core that backs cards up on its own): 6 tries a sector */
#define IDLE_OFF_S 20

static pthread_mutex_t mlk = PTHREAD_MUTEX_INITIALIZER;     /* the connection to Main */
static pthread_mutex_t slk = PTHREAD_MUTEX_INITIALIZER;     /* the job state */
static pthread_cond_t wake = PTHREAD_COND_INITIALIZER;
static int mfd = -1;
static int ports_on;
static volatile int loading;    /* the Phosphor menu core is being loaded: answer from what's known */
static time_t last_use;
static ports_state_t st;
static char want_job[16], want_name[128];
static int want_port;

static const char *main_sock(void)
{
	const char *p = getenv("PHOSPHOR_MAIN_SOCK");
	return p && *p ? p : "/tmp/phosphor-main.sock";
}

const char *memcard_dir(void) { return mclib_dir(); }

static void m_close(void)
{
	if (mfd >= 0) close(mfd);
	mfd = -1;
	ports_on = 0;          /* Main turns them off when the connection goes */
}

static int m_open(void)
{
	if (mfd >= 0) return 0;
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return -1;
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof(a.sun_path), "%s", main_sock());
	if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0) { close(fd); return -1; }
	mfd = fd;
	ports_on = 0;          /* a new connection (Main may have restarted) owns nothing yet */
	return 0;
}

/* one line to Main, one line back (call with mlk held); NULL if Main isn't there */
static char *m_req_locked(const char *line, int timeout_ms)
{
	for (int attempt = 0; attempt < 2; attempt++) {
		if (m_open()) return NULL;
		size_t n = strlen(line);
		char *msg = malloc(n + 2);
		if (!msg) return NULL;
		memcpy(msg, line, n);
		msg[n] = '\n';
		ssize_t w = send(mfd, msg, n + 1, MSG_NOSIGNAL);
		free(msg);
		if (w != (ssize_t)(n + 1)) { m_close(); continue; }     /* Main restarted: once more */
		size_t cap = 1024, len = 0;
		char *buf = malloc(cap);
		if (!buf) return NULL;
		struct timespec t0;
		clock_gettime(CLOCK_MONOTONIC, &t0);
		for (;;) {
			struct timespec t1;
			clock_gettime(CLOCK_MONOTONIC, &t1);
			long left = timeout_ms - ((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000);
			struct pollfd p = { mfd, POLLIN, 0 };
			if (left <= 0 || poll(&p, 1, (int)left) <= 0) { free(buf); m_close(); return NULL; }
			/* byte by byte up to the newline: anything after it belongs to the next reply */
			char c;
			ssize_t r = recv(mfd, &c, 1, 0);
			if (r <= 0) { free(buf); m_close(); buf = NULL; break; }
			if (c == '\n') { buf[len] = 0; return buf; }
			if (len + 2 > cap) {
				if (cap >= 8192) { free(buf); m_close(); return NULL; }
				char *nb = realloc(buf, cap *= 2);
				if (!nb) { free(buf); m_close(); return NULL; }
				buf = nb;
			}
			buf[len++] = c;
		}
		if (attempt) return NULL;
	}
	return NULL;
}

static cJSON *m_json_locked(const char *line, int timeout_ms)
{
	char *r = m_req_locked(line, timeout_ms);
	if (!r) return NULL;
	cJSON *j = cJSON_Parse(r);
	free(r);
	return j;
}

static int jbool(cJSON *j, const char *k) { return cJSON_IsTrue(cJSON_GetObjectItem(j, k)); }

static int load_failed;     /* the menu core was loaded and its front ports didn't answer */

static void rbf_path(char *out, size_t n) { snprintf(out, n, "%s/phosphor/cores/menu_phosphor.rbf", p_fat()); }

/* what Main says (mlk held) */
static void look_locked(ports_state_t *s)
{
	s->main_api = s->available = s->can_load = s->in_game = 0;
	s->on = ports_on;
	cJSON *j = m_json_locked("status", 1500);
	if (!j) j = m_json_locked("status", 1500);       /* once more, on a new connection */
	if (!j) return;
	s->main_api = 1;
	s->in_game = !jbool(j, "menu");
	cJSON_Delete(j);
	j = m_json_locked("ports", 1500);
	if (!j) j = m_json_locked("ports", 1500);
	if (!j) { s->main_api = 0; return; }      /* no answer is not "the core isn't loaded": never load it on that */
	s->available = jbool(j, "available");
	s->on = jbool(j, "on") && ports_on;
	cJSON_Delete(j);
	char rbf[512];
	rbf_path(rbf, sizeof(rbf));
	s->can_load = !s->available && !s->in_game && !load_failed && file_exists(rbf);
}

static int prepare_locked(char *err, size_t n, int may_load)
{
	if (ports_on && mfd >= 0) { last_use = time(NULL); return 0; }   /* on, same Main as before */
	ports_state_t s;
	look_locked(&s);
	if (!s.main_api) { snprintf(err, n, "Phosphor's Main didn't answer: try again in a moment"); return -1; }
	if (s.in_game) { snprintf(err, n, "The front ports are only read here from the menu, not during a game"); return -1; }
	if (!s.available) {
		if (!s.can_load) { snprintf(err, n, "The Phosphor menu core isn't installed (phosphor/cores/menu_phosphor.rbf)"); return -1; }
		if (!may_load) { snprintf(err, n, "The front ports need the Phosphor menu core: open Controllers & memory cards"); return -1; }
		/* loading it restarts Main (the screen goes away for seconds): once, and never again after it
		   didn't answer, until phosphord restarts */
		if (load_failed) { snprintf(err, n, "The front ports aren't answering in the Phosphor menu core"); return -1; }
		/* Main asks the core again a second later (just after it starts, the core may not answer yet):
		   wait for that before loading it, which restarts Main */
		for (int i = 0; i < 3 && !s.available && s.main_api; i++) {
			pthread_mutex_unlock(&mlk);
			usleep(600000);
			pthread_mutex_lock(&mlk);
			look_locked(&s);
		}
		if (s.available) goto ready;
		if (!s.main_api) { snprintf(err, n, "Phosphor's Main didn't answer: try again in a moment"); return -1; }
		char rbf[512], line[600];
		rbf_path(rbf, sizeof(rbf));
		snprintf(line, sizeof(line), "launch %s", rbf);
		plog("front ports: loading the Phosphor menu core");
		cJSON *j = m_json_locked(line, 1500);
		cJSON_Delete(j);
		m_close();                                /* Main restarts with the new core */
		/* the connection isn't held while waiting: the API keeps answering (from what's known) */
		loading = 1;
		for (int i = 0; i < 40 && !s.available; i++) {
			pthread_mutex_unlock(&mlk);
			usleep(250000);
			pthread_mutex_lock(&mlk);
			look_locked(&s);
		}
		loading = 0;
		if (!s.available) {
			load_failed = 1;
			plog("front ports: the menu core didn't answer; not loading it again");
			snprintf(err, n, "The front ports aren't answering in the Phosphor menu core");
			return -1;
		}
	}
ready:
	if (!ports_on) {
		cJSON *j = m_json_locked("ports on", 1500);
		int ok = j && jbool(j, "ok");
		if (!ok) {
			cJSON *e = j ? cJSON_GetObjectItem(j, "error") : NULL;
			snprintf(err, n, "%s", cJSON_IsString(e) ? e->valuestring : "Main didn't answer");
			cJSON_Delete(j);
			return -1;
		}
		cJSON_Delete(j);
		ports_on = 1;
		usleep(30000);       /* the first controller reads */
	}
	last_use = time(NULL);
	return 0;
}

/* the trip wire: the worker sleeps with no timer while the ports are off, and is woken only when
 * something asks for the ports (the memory cards screen opening, a card job, a pads read) */
static void wake_worker(void)
{
	pthread_mutex_lock(&slk);
	pthread_cond_signal(&wake);
	pthread_mutex_unlock(&slk);
}

int ports_prepare(char *err, size_t n)
{
	pthread_mutex_lock(&mlk);
	int r = prepare_locked(err, n, 1);
	pthread_mutex_unlock(&mlk);
	wake_worker();                        /* it turns them off again once unused */
	return r;
}

char *ports_pads(char *err, size_t n)
{
	if (loading) { snprintf(err, n, "Loading the Phosphor menu core"); return NULL; }
	pthread_mutex_lock(&mlk);
	char *r = NULL;
	if (!prepare_locked(err, n, 0)) {
		r = m_req_locked("pads", 1500);
		if (!r) snprintf(err, n, "Main didn't answer");
	}
	pthread_mutex_unlock(&mlk);
	wake_worker();
	return r;
}

int ports_pad_analog(int port, char *err, size_t n)
{
	if (port != 1 && port != 2) { snprintf(err, n, "port is 1 or 2"); return -1; }
	if (loading) { snprintf(err, n, "Loading the Phosphor menu core"); return -1; }
	pthread_mutex_lock(&mlk);
	int r = prepare_locked(err, n, 0);
	if (!r) {
		char line[32];
		snprintf(line, sizeof(line), "pad_analog %d", port);
		cJSON *j = m_json_locked(line, 1500);
		if (!j || !jbool(j, "ok")) { snprintf(err, n, "Main didn't answer"); r = -1; }
		cJSON_Delete(j);
	}
	pthread_mutex_unlock(&mlk);
	wake_worker();
	return r;
}

/* ---- card sectors ---- */

/* 0 ok, else 1 no card, 2 bad reply, 3 out of range, 4 Main gone, 5 ports off; one try */
static int card_io(int port, int sec, unsigned char *data, int write)
{
	char line[400];
	int k = snprintf(line, sizeof(line), "card_%s %d %d", write ? "write" : "read", port, sec);
	if (write) {
		line[k++] = ' ';
		for (int i = 0; i < SECTOR; i++) k += snprintf(line + k, sizeof(line) - k, "%02x", data[i]);
	}
	pthread_mutex_lock(&mlk);
	last_use = time(NULL);
	cJSON *j = m_json_locked(line, 2000);
	pthread_mutex_unlock(&mlk);
	if (!j) return 4;
	int r = 0;
	if (!jbool(j, "ok")) {
		const char *e = cJSON_GetStringValue(cJSON_GetObjectItem(j, "error"));
		r = (e && !strcmp(e, "no card")) ? 1 : (e && !strcmp(e, "out of range")) ? 3
			: (e && !strncmp(e, "front ports", 11)) ? 5 : 2;
	} else if (!write) {
		const char *hex = cJSON_GetStringValue(cJSON_GetObjectItem(j, "data"));
		if (!hex || strlen(hex) != SECTOR * 2) r = 2;
		else for (int i = 0; i < SECTOR; i++) {
			unsigned v;
			if (sscanf(hex + i * 2, "%2x", &v) != 1) { r = 2; break; }
			data[i] = (unsigned char)v;
		}
	}
	cJSON_Delete(j);
	return r;
}

static int tries_used;       /* how many tries the last card_io_retry took (job thread only) */

static void mark(int sec, char v)
{
	pthread_mutex_lock(&slk);
	if (sec >= 0 && sec < SECTORS) st.map[sec] = v;
	pthread_mutex_unlock(&slk);
}

static int card_io_retry(int port, int sec, unsigned char *data, int write)
{
	int r = 0;
	for (int t = 0; t < TRIES; t++) {
		tries_used = t + 1;
		r = card_io(port, sec, data, write);
		if (r == 5) {                            /* Main restarted: the ports again */
			char err[200];
			pthread_mutex_lock(&mlk);
			ports_on = 0;
			int p = prepare_locked(err, sizeof(err), 1);
			pthread_mutex_unlock(&mlk);
			if (p) return 4;
			continue;
		}
		/* every failure is tried again, "no card" too: on the SS1's front ports a card now and then
		   misses a command (SS1 MemCard retries any failed sector the same way, 1 ms apart) */
		if (r == 0 || r == 3 || r == 4) return r;
		usleep(t < 2 ? 2000 : 20000);
	}
	return r;
}

static void progress(int done, const char *msg)
{
	pthread_mutex_lock(&slk);
	st.done = done;
	if (msg) snprintf(st.message, sizeof(st.message), "%s", msg);
	pthread_mutex_unlock(&slk);
}

static const char *io_error(int r)
{
	return r == 1 ? "No memory card in that port" : r == 4 ? "Phosphor's Main stopped answering"
		: "The memory card didn't answer properly (try reinserting it)";
}

/* the whole card into buf; 0 ok, else fills msg */
static int read_card(int port, unsigned char *buf, char *msg, size_t n, int base, const char *what)
{
	for (int s = 0; s < SECTORS; s++) {
		int r = card_io_retry(port, s, buf + s * SECTOR, 0);
		mark(s, r ? '3' : tries_used > 1 ? '2' : '1');
		if (r) { snprintf(msg, n, "%s (sector %d)", io_error(r), s); return -1; }
		if (s % 32 == 0) progress(base + s, what);
	}
	return 0;
}

/* the card a backup belongs to ("card/version.mcd"), for the card that's about to be overwritten when
   it isn't recognised; an old loose file: "Memory card" */
static const char *restore_card(const char *rel, char *out, size_t n)
{
	const char *sl = strchr(rel, '/');
	if (sl) snprintf(out, n, "%.*s", (int)(sl - rel), rel);
	else snprintf(out, n, "Memory card");
	return out;
}

/* a card that wasn't recognised waits here for its name (memcard_name) */
static unsigned char *pending;
static int ask_name;
static char suggest[64];

/* the card read into buf goes into the library: the card it's recognised as, else fallback (NULL: ask
   for a name). 0 saved (rel, msg), 1 waiting for a name, -1 error (msg) */
static int keep_card(unsigned char *buf, const char *fallback, char *rel, size_t rn, char *msg, size_t n, int force)
{
	char card[128];
	double score = force && fallback && *fallback ? 1.0 : mclib_recognise(buf, card, sizeof(card));
	if (force && fallback && *fallback) snprintf(card, sizeof(card), "%s", fallback);   /* "keep the backup as" */
	else if (score < 0.5) {
		if (fallback && *fallback) snprintf(card, sizeof(card), "%s", fallback);
		else {
			pthread_mutex_lock(&slk);
			free(pending);
			pending = malloc(CARD_BYTES);
			if (pending) memcpy(pending, buf, CARD_BYTES);
			ask_name = pending != NULL;
			mclib_suggest(suggest, sizeof(suggest));
			pthread_mutex_unlock(&slk);
			if (!pending) { snprintf(msg, n, "Out of memory"); return -1; }
			snprintf(msg, n, "A memory card Phosphor hasn't seen before: give it a name");
			return 1;
		}
	}
	int same = 0;
	if (mclib_store(card, buf, rel, rn, &same)) { snprintf(msg, n, "Can't write to %.150s", mclib_dir()); return -1; }
	if (same) snprintf(msg, n, "%.100s: the same as its last backup, nothing new to keep", card);
	else snprintf(msg, n, "Backed up as %.100s", card);
	return 0;
}

static void finish(int ok, const char *msg, const char *file)
{
	pthread_mutex_lock(&slk);
	st.busy = 0;
	st.ok = ok;
	if (ok) st.done = st.total;
	snprintf(st.message, sizeof(st.message), "%s", msg);
	if (file) snprintf(st.file, sizeof(st.file), "%s", file);
	char job[16];
	snprintf(job, sizeof(job), "%s", st.job);
	int port = st.port;
	pthread_mutex_unlock(&slk);
	plog("memory card %s, port %d: %s", job, port, msg);
}

static void run_job(const char *job, int port, const char *name)
{
	char err[200], msg[200], file[128] = "";
	if (ports_prepare(err, sizeof(err))) { finish(0, err, NULL); return; }
	unsigned char *card = malloc(CARD_BYTES), *want = NULL;
	if (!card) { finish(0, "Out of memory", NULL); return; }

	if (!strcmp(job, "prepare")) finish(1, "The front ports are on", NULL);
	else if (!strcmp(job, "check")) {
		unsigned char s0[SECTOR];
		int r = card_io_retry(port, 0, s0, 0);
		if (r) finish(0, io_error(r), NULL);
		else if (s0[0] == 'M' && s0[1] == 'C') finish(1, "Memory card found (formatted)", NULL);
		else finish(1, "Memory card found (not formatted)", NULL);
	}
	else if (!strcmp(job, "backup")) {
		if (read_card(port, card, msg, sizeof(msg), 0, "Reading the card")) finish(0, msg, NULL);
		else {
			/* name: keep it as that library card (the app's "Keep the backup as"); else recognised by its saves */
			char nm[128] = "";
			if (name && *name && mclib_safe_name(name, nm, sizeof(nm))) nm[0] = 0;
			int k = keep_card(card, nm, file, sizeof(file), msg, sizeof(msg), nm[0] != 0);
			finish(k >= 0, msg, k == 0 ? file : NULL);
		}
	}
	else if (!strcmp(job, "restore")) {
		char path[700], fb[128];
		snprintf(path, sizeof(path), "%s/%s", mclib_dir(), name);
		struct stat sb;
		FILE *f = NULL;
		want = malloc(CARD_BYTES);
		if (!want || stat(path, &sb) || sb.st_size != CARD_BYTES || !(f = fopen(path, "rb")) || fread(want, 1, CARD_BYTES, f) != CARD_BYTES) {
			if (f) fclose(f);
			finish(0, "That backup can't be read (it has to be a 128 KB .mcd file)", NULL);
		}
		else {
			fclose(f);
			/* 1: what's on the card now, kept as a backup */
			if (read_card(port, card, msg, sizeof(msg), 0, "Backing up the card first")) finish(0, msg, NULL);
			else if (keep_card(card, restore_card(name, fb, sizeof(fb)), file, sizeof(file), msg, sizeof(msg), 0) != 0) finish(0, msg, NULL);
			else {
				pthread_mutex_lock(&slk);
				snprintf(st.file, sizeof(st.file), "%s", file);
				pthread_mutex_unlock(&slk);
				/* 2: the sectors that differ, each read back. The saves' data first and the directory
				   (block 0, sectors 0-63) last, so a write that stops partway never leaves a directory
				   describing data that isn't there yet */
				int bad = -1, r = 0, written = 0;
				for (int i = 0; i < SECTORS && bad < 0; i++) {
					int s = (i + 64) % SECTORS;
					if (i % 32 == 0) progress(SECTORS + i, "Writing");
					if (!memcmp(card + s * SECTOR, want + s * SECTOR, SECTOR)) continue;
					unsigned char back[SECTOR];
					for (int t = 0; t < 2; t++) {
						r = card_io_retry(port, s, want + s * SECTOR, 1);
						if (!r) usleep(5000);      /* a card is busy for a moment after a write (SS1 MemCard waits 5 ms) */
						if (!r) r = card_io_retry(port, s, back, 0);
						if (!r && memcmp(back, want + s * SECTOR, SECTOR)) r = 2;
						if (!r) break;
					}
					mark(s, r ? '3' : '5');
					if (r) bad = s;
					else written++;
				}
				if (bad >= 0) {
					snprintf(msg, sizeof(msg), "Writing stopped at sector %d: %s. What was on the card is in %s", bad, io_error(r), file);
					finish(0, msg, file);
				} else {
					snprintf(msg, sizeof(msg), "Restored (%d sectors changed). What was on the card before is in %s", written, file);
					finish(1, msg, file);
				}
			}
		}
	}
	free(card);
	free(want);
}

static void *worker(void *arg)
{
	(void)arg;
	for (;;) {
		char job[16], name[128];
		int port;
		pthread_mutex_lock(&slk);
		while (!want_job[0]) {
			/* ports off: sleep until woken (wake_worker); on: look every 2 s, to turn them off once unused */
			if (!ports_on) { pthread_cond_wait(&wake, &slk); continue; }
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 2;
			pthread_cond_timedwait(&wake, &slk, &ts);
			if (want_job[0]) break;
			pthread_mutex_unlock(&slk);
			/* nothing needs the ports for a while: back to MT32-pi / MIDI */
			pthread_mutex_lock(&mlk);
			if (ports_on && time(NULL) - last_use > IDLE_OFF_S) {
				cJSON *j = m_json_locked("ports off", 1500);
				cJSON_Delete(j);
				ports_on = 0;
			}
			pthread_mutex_unlock(&mlk);
			pthread_mutex_lock(&slk);
		}
		snprintf(job, sizeof(job), "%s", want_job);
		snprintf(name, sizeof(name), "%s", want_name);
		port = want_port;
		want_job[0] = 0;
		pthread_mutex_unlock(&slk);
		run_job(job, port, name);
	}
	return NULL;
}

void memcard_start(void)
{
	pthread_t t;
	if (pthread_create(&t, NULL, worker, NULL) == 0) pthread_detach(t);
}

int memcard_job(const char *what, int port, const char *name, char *err, size_t n)
{
	if (!what || (strcmp(what, "backup") && strcmp(what, "restore") && strcmp(what, "check") && strcmp(what, "prepare"))) { snprintf(err, n, "do: backup, restore, check or prepare"); return -1; }
	if (!strcmp(what, "prepare") && !port) port = 1;
	if (port != 1 && port != 2) { snprintf(err, n, "port is 1 or 2"); return -1; }
	if (!strcmp(what, "restore") && !mclib_valid(name)) { snprintf(err, n, "name: one of the backups (.mcd)"); return -1; }
	pthread_mutex_lock(&slk);
	if (st.busy || want_job[0]) { pthread_mutex_unlock(&slk); return 1; }
	snprintf(want_job, sizeof(want_job), "%s", what);
	snprintf(want_name, sizeof(want_name), "%s", name ? name : "");
	want_port = port;
	st.busy = 1;
	snprintf(st.job, sizeof(st.job), "%s", what);
	st.port = port;
	st.done = 0;
	if (!strcmp(what, "backup") || !strcmp(what, "restore")) {     /* a new map for jobs that read the card */
		memset(st.map, '0', SECTORS);
		st.map[SECTORS] = 0;
	}
	st.total = (!strcmp(what, "check") || !strcmp(what, "prepare")) ? 1 : !strcmp(what, "backup") ? SECTORS : 2 * SECTORS;
	st.ok = 0;
	st.message[0] = st.file[0] = 0;
	pthread_cond_signal(&wake);
	pthread_mutex_unlock(&slk);
	return 0;
}

void memcard_state(ports_state_t *s)
{
	ports_state_t look = { 0 };
	static ports_state_t last;            /* while loading (or busy), the last answer */
	if (!loading && pthread_mutex_trylock(&mlk) == 0) {
		look_locked(&look);
		pthread_mutex_unlock(&mlk);
		last = look;
	} else look = last;
	pthread_mutex_lock(&slk);
	*s = st;
	s->ask_name = ask_name;
	snprintf(s->suggest, sizeof(s->suggest), "%s", suggest);
	pthread_mutex_unlock(&slk);
	s->main_api = look.main_api;
	s->available = look.available;
	s->can_load = look.can_load;
	s->in_game = look.in_game;
	s->on = look.on;
}

char *memcard_list(void) { return mclib_list(); }

int memcard_name(const char *name, char *rel, size_t rn, char *err, size_t n)
{
	char card[80];
	if (mclib_safe_name(name, card, sizeof(card))) { snprintf(err, n, "Give the card a name"); return -1; }
	pthread_mutex_lock(&slk);
	unsigned char *img = pending;
	pending = NULL;
	ask_name = 0;
	pthread_mutex_unlock(&slk);
	if (!img) { snprintf(err, n, "There's no card waiting for a name"); return -1; }
	int same = 0, r = mclib_store(card, img, rel, rn, &same);
	free(img);
	if (r) { snprintf(err, n, "Can't write to %.150s", mclib_dir()); return -1; }
	pthread_mutex_lock(&slk);
	snprintf(st.message, sizeof(st.message), "Backed up as %.100s", card);
	snprintf(st.file, sizeof(st.file), "%s", rel);
	pthread_mutex_unlock(&slk);
	plog("memory card named %s (%s)", card, rel);
	return 0;
}
