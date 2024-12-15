#include "common.h"

int readn(int f, char *b, size_t n)
{
	while (n > 0) {
		ssize_t s = read(f, b, n);
		if (s > 0) {
			printf("read %ld from %d\n", s, f);
			b += s;
			n -= s;
			continue;
		}

		return -1;
	}

	return 0;
}

int writen(void *p, int f, char *b, size_t n, const char *str)
{
	while (n > 0) {
		ssize_t s = ((ssize_t(*)(int, char *, size_t))p) (f, b, n);
		if (s > 0) {
			printf("%s %ld - %d\n", str, s, f);
			b += s;
			n -= s;
			continue;
		}

		perror(str);
		return -1;
	}

	return 0;
}

void loop(struct vpn *vpn)
{
	struct pollfd p[2] = {
		{vpn->fd, POLLIN},
		{SSL_get_fd(vpn->ssl), POLLIN}
	};

	while (poll(p, 2, -1) > 0) {
		if (p[0].revents & POLLIN)
			fd2ssl(vpn);

		if (p[1].revents & POLLIN)
			ssl2fd(vpn);
	}

	perror("poll");
}

int start(int (*action)(int, const struct sockaddr *, socklen_t),
	  const char *host, const char *port, int flags)
{
	struct addrinfo *l, hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = flags
	};

	int e = getaddrinfo(host, port, &hints, &l);
	assert(!e);

	for (struct addrinfo * a = l; a; a = a->ai_next) {
		int s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (s < 0) {
			perror("socket");
			continue;
		}

		int i = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
		if (!action(s, a->ai_addr, a->ai_addrlen)) {
			freeaddrinfo(l);
			return s;
		}

		perror("action");
		close(s);
	}

	exit(1);
}

void server(const char *host, const char *port, void (*work)(int))
{
	int s = start(bind, host, port, AI_PASSIVE);
	listen(s, 10);
	while (1) {
		int c = accept(s, NULL, NULL);
		if (c < 0)
			perror("accept");

		work(c);
	}
}
