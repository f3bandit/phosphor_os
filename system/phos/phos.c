/* phos: talk to phosphord from the command line.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 *   phos status | categories | rescan | import | last
 *   phos systems [category]
 *   phos games <system>
 *   phos game <id> | launch <id>
 *   phos '{"cmd":"...", ...}'          any request as JSON
 * Prints the reply. Exit status 0 when it says ok. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int is_num(const char *s)
{
	if (!*s) return 0;
	for (; *s; s++) if (*s < '0' || *s > '9') return 0;
	return 1;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: phos status|categories|systems [cat]|games SYS|game ID|launch ID|last|rescan|import|'{json}'\n");
		return 2;
	}
	char req[4096];
	const char *c = argv[1], *a = argc > 2 ? argv[2] : "";
	if (c[0] == '{') snprintf(req, sizeof(req), "%s", c);
	else if (!strcmp(c, "systems") && *a) snprintf(req, sizeof(req), "{\"cmd\":\"systems\",\"category\":%d,\"all\":true}", atoi(a));
	else if (!strcmp(c, "games") && *a) snprintf(req, sizeof(req), "{\"cmd\":\"games\",\"system\":\"%s\"}", a);
	else if ((!strcmp(c, "game") || !strcmp(c, "launch")) && is_num(a)) snprintf(req, sizeof(req), "{\"cmd\":\"%s\",\"id\":%s}", c, a);
	else snprintf(req, sizeof(req), "{\"cmd\":\"%s\"}", c);

	const char *tmp = getenv("PHOS_TMP");
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/phosphord.sock", tmp && *tmp ? tmp : "/tmp/phosphor");
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0 || connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "phosphord isn't running (%s)\n", addr.sun_path);
		return 1;
	}
	strcat(req, "\n");
	if (write(fd, req, strlen(req)) < 0) return 1;
	char buf[65536];
	size_t len = 0;
	ssize_t r;
	char *out = NULL;
	while ((r = read(fd, buf, sizeof(buf))) > 0) {
		out = realloc(out, len + (size_t)r + 1);
		memcpy(out + len, buf, (size_t)r);
		len += (size_t)r;
		out[len] = 0;
		if (memchr(buf, '\n', (size_t)r)) break;
	}
	close(fd);
	if (!out) return 1;
	fputs(out, stdout);
	int okay = strstr(out, "\"ok\":true") != NULL;
	free(out);
	return okay ? 0 : 1;
}
