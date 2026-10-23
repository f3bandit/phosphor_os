#define _GNU_SOURCE
/* Phosphor shell: a terminal window for scripts (Update All, MiSTer's downloader, a Patreon core's
 * sign-in script...), so they run inside Phosphor instead of on MiSTer's own full-screen terminal.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * The script runs on a pseudo-terminal; libvterm (MIT) keeps the screen, which is drawn on a canvas,
 * only the cells that changed. Everything it prints also goes to a log file. */
#include "term.h"
#include "log.h"
#include "theme.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vterm.h>

#define MAX_COLS 132
#define MAX_ROWS 50

static struct {
	lv_obj_t *canvas;
	lv_draw_buf_t *buf;
	const lv_font_t *font;
	int cw, ch, cols, rows;
	VTerm *vt;
	VTermScreen *vs;
	int pty;
	pid_t pid;
	int running, status;
	FILE *log;
	unsigned char dirty[MAX_ROWS][MAX_COLS];
	int any_dirty;
	VTermPos cursor, drawn_cursor;
	int cursor_visible;
} t = { .pty = -1, .pid = -1 };

static int damage(VTermRect r, void *u)
{
	(void)u;
	for (int y = r.start_row; y < r.end_row && y < t.rows; y++)
		for (int x = r.start_col; x < r.end_col && x < t.cols; x++) t.dirty[y][x] = 1;
	t.any_dirty = 1;
	return 1;
}

static int moverect(VTermRect dst, VTermRect src, void *u)
{
	(void)src;
	return damage(dst, u);
}

static int movecursor(VTermPos pos, VTermPos old, int visible, void *u)
{
	(void)old;
	(void)u;
	t.cursor = pos;
	t.cursor_visible = visible;
	t.any_dirty = 1;
	return 1;
}

static int settermprop(VTermProp prop, VTermValue *val, void *u)
{
	(void)u;
	if (prop == VTERM_PROP_CURSORVISIBLE) { t.cursor_visible = val->boolean; t.any_dirty = 1; }
	return 1;
}

static const VTermScreenCallbacks CB = { .damage = damage, .moverect = moverect, .movecursor = movecursor, .settermprop = settermprop };

/* what the terminal sends back (keys, answers to queries) goes to the script */
static void output(const char *s, size_t len, void *u)
{
	(void)u;
	while (len && t.pty >= 0) {
		ssize_t w = write(t.pty, s, len);
		if (w < 0) { if (errno == EINTR) continue; break; }
		s += w;
		len -= (size_t)w;
	}
}

static lv_color_t col(VTermColor *c, int is_fg)
{
	if (is_fg && VTERM_COLOR_IS_DEFAULT_FG(c)) return PAL.text;
	if (!is_fg && VTERM_COLOR_IS_DEFAULT_BG(c)) return lv_color_hex(0x000000);
	vterm_screen_convert_color_to_rgb(t.vs, c);
	return lv_color_make(c->rgb.red, c->rgb.green, c->rgb.blue);
}

static int utf8(uint32_t c, char *o)
{
	if (c < 0x80) { o[0] = (char)c; return 1; }
	if (c < 0x800) { o[0] = (char)(0xC0 | (c >> 6)); o[1] = (char)(0x80 | (c & 0x3F)); return 2; }
	if (c < 0x10000) { o[0] = (char)(0xE0 | (c >> 12)); o[1] = (char)(0x80 | ((c >> 6) & 0x3F)); o[2] = (char)(0x80 | (c & 0x3F)); return 3; }
	o[0] = (char)(0xF0 | (c >> 18)); o[1] = (char)(0x80 | ((c >> 12) & 0x3F)); o[2] = (char)(0x80 | ((c >> 6) & 0x3F)); o[3] = (char)(0x80 | (c & 0x3F));
	return 4;
}

static void draw_cell(lv_layer_t *layer, int row, int c)
{
	VTermScreenCell cell;
	VTermPos pos = { row, c };
	if (!vterm_screen_get_cell(t.vs, pos, &cell)) return;
	lv_color_t fg = col(&cell.fg, 1), bg = col(&cell.bg, 0);
	int inv = cell.attrs.reverse ^ (t.cursor_visible && row == t.cursor.row && c == t.cursor.col && t.running);
	if (inv) { lv_color_t x = fg; fg = bg; bg = x; }
	lv_area_t a = { c * t.cw, row * t.ch, (c + 1) * t.cw - 1, (row + 1) * t.ch - 1 };
	lv_draw_rect_dsc_t rd;
	lv_draw_rect_dsc_init(&rd);
	rd.bg_color = bg;
	rd.bg_opa = LV_OPA_COVER;
	lv_draw_rect(layer, &rd, &a);
	if (cell.chars[0] && cell.chars[0] != ' ') {
		char s[32];
		int k = 0;
		for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i] && k < 24; i++) k += utf8(cell.chars[i], s + k);
		s[k] = 0;
		lv_draw_label_dsc_t ld;
		lv_draw_label_dsc_init(&ld);
		ld.font = t.font;
		ld.color = fg;
		ld.text = s;
		ld.text_local = 1;
		lv_area_t ta = a;
		ta.x2 = ta.x1 + t.cw * (cell.width > 1 ? 2 : 1) + t.cw;
		lv_draw_label(layer, &ld, &ta);
	}
}

static void redraw(void)
{
	if (!t.canvas || !t.any_dirty) return;
	/* the cursor moves: its old and new cells */
	if (t.drawn_cursor.row < t.rows && t.drawn_cursor.col < t.cols) t.dirty[t.drawn_cursor.row][t.drawn_cursor.col] = 1;
	if (t.cursor.row < t.rows && t.cursor.col < t.cols) t.dirty[t.cursor.row][t.cursor.col] = 1;
	lv_layer_t layer;
	lv_canvas_init_layer(t.canvas, &layer);
	for (int y = 0; y < t.rows; y++)
		for (int x = 0; x < t.cols; x++)
			if (t.dirty[y][x]) { draw_cell(&layer, y, x); t.dirty[y][x] = 0; }
	lv_canvas_finish_layer(t.canvas, &layer);
	t.drawn_cursor = t.cursor;
	t.any_dirty = 0;
}

lv_obj_t *term_create(lv_obj_t *parent, int w, int h, const lv_font_t *font)
{
	t.font = font;
	t.cw = lv_font_get_glyph_width(font, 'M', 0);
	t.ch = lv_font_get_line_height(font);
	if (t.cw < 1) t.cw = 8;
	t.cols = LV_MIN(MAX_COLS, w / t.cw);
	t.rows = LV_MIN(MAX_ROWS, h / t.ch);
	if (t.buf) lv_draw_buf_destroy(t.buf);
	t.buf = lv_draw_buf_create(t.cols * t.cw, t.rows * t.ch, LV_COLOR_FORMAT_RGB565, 0);
	t.canvas = lv_canvas_create(parent);
	lv_canvas_set_draw_buf(t.canvas, t.buf);
	lv_canvas_fill_bg(t.canvas, lv_color_hex(0x000000), LV_OPA_COVER);
	if (t.vt) vterm_free(t.vt);
	t.vt = vterm_new(t.rows, t.cols);
	vterm_set_utf8(t.vt, 1);
	vterm_output_set_callback(t.vt, output, NULL);
	t.vs = vterm_obtain_screen(t.vt);
	vterm_screen_set_callbacks(t.vs, &CB, NULL);
	vterm_screen_enable_altscreen(t.vs, 1);
	vterm_screen_reset(t.vs, 1);
	memset(t.dirty, 0, sizeof(t.dirty));
	t.cursor_visible = 1;
	return t.canvas;
}

void term_forget(void) { t.canvas = NULL; }

int term_start(const char *script, const char *logpath)
{
	if (t.running) return -1;
	int m = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (m < 0 || grantpt(m) || unlockpt(m)) { if (m >= 0) close(m); return -1; }
	char *sn = ptsname(m);
	if (!sn) { close(m); return -1; }
	char slave[64];
	snprintf(slave, sizeof(slave), "%s", sn);
	struct winsize ws = { .ws_row = (unsigned short)t.rows, .ws_col = (unsigned short)t.cols };
	ioctl(m, TIOCSWINSZ, &ws);
	char dir[600];
	snprintf(dir, sizeof(dir), "%s", script);
	char *sl = strrchr(dir, '/');
	if (sl) *sl = 0;
	pid_t pid = fork();
	if (pid < 0) { close(m); return -1; }
	if (pid == 0) {
		setsid();
		int s = open(slave, O_RDWR);
		if (s < 0) _exit(126);
		ioctl(s, TIOCSCTTY, 0);
		ioctl(s, TIOCSWINSZ, &ws);
		dup2(s, 0); dup2(s, 1); dup2(s, 2);
		if (s > 2) close(s);
		for (int fd = 3; fd < 256; fd++) close(fd);
		signal(SIGPIPE, SIG_DFL);
		setenv("TERM", "xterm", 1);
		setenv("HOME", "/root", 1);
		setenv("LC_ALL", "en_US.UTF-8", 1);
		char c[16], r[16];
		snprintf(c, sizeof(c), "%d", t.cols);
		snprintf(r, sizeof(r), "%d", t.rows);
		setenv("COLUMNS", c, 1);
		setenv("LINES", r, 1);
		if (sl && chdir(dir)) {}
		execl("/bin/bash", "bash", script, (char *)NULL);
		_exit(127);
	}
	t.pty = m;
	fcntl(m, F_SETFL, O_NONBLOCK);
	t.pid = pid;
	t.running = 1;
	t.status = 0;
	if (t.log) fclose(t.log);
	t.log = logpath ? fopen(logpath, "w") : NULL;
	if (t.log) { setvbuf(t.log, NULL, _IOLBF, 0); fprintf(t.log, "== %s\n", script); }
	plog("terminal: running %s", script);
	return 0;
}

int term_poll(void)
{
	if (t.pty >= 0) {
		char buf[8192];
		for (int i = 0; i < 8; i++) {
			ssize_t r = read(t.pty, buf, sizeof(buf));
			if (r > 0) {
				vterm_input_write(t.vt, buf, (size_t)r);
				if (t.log) fwrite(buf, 1, (size_t)r, t.log);
				continue;
			}
			break;
		}
		vterm_screen_flush_damage(t.vs);
	}
	if (t.running) {
		int st;
		if (waitpid(t.pid, &st, WNOHANG) == t.pid) {
			/* what's still in the pty, then done */
			char buf[8192];
			ssize_t r;
			while (t.pty >= 0 && (r = read(t.pty, buf, sizeof(buf))) > 0) {
				vterm_input_write(t.vt, buf, (size_t)r);
				if (t.log) fwrite(buf, 1, (size_t)r, t.log);
			}
			vterm_screen_flush_damage(t.vs);
			t.running = 0;
			t.status = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
			if (t.pty >= 0) close(t.pty);
			t.pty = -1;
			if (t.log) { fprintf(t.log, "\n== exit %d\n", t.status); fclose(t.log); t.log = NULL; }
			t.any_dirty = 1;
			plog("terminal: done, exit %d", t.status);
		}
	}
	redraw();
	return t.running;
}

int term_running(void) { return t.running; }
int term_status(void) { return t.status; }

void term_key(int key)
{
	if (!t.running || !t.vt) return;
	vterm_keyboard_key(t.vt, (VTermKey)key, VTERM_MOD_NONE);
}

void term_text(const char *s)
{
	if (!t.running || !t.vt) return;
	for (; *s; s++) vterm_keyboard_unichar(t.vt, (uint32_t)(unsigned char)*s, VTERM_MOD_NONE);
}

void term_stop(void)
{
	if (!t.running || t.pid <= 0) return;
	kill(-t.pid, SIGHUP);
	kill(t.pid, SIGHUP);
	usleep(200000);
	kill(-t.pid, SIGKILL);
	kill(t.pid, SIGKILL);
}
