#include "common.h"

int readn(int f, char *b, size_t n)
{
	while (n) {
		ssize_t s = read(f, b, n);
		if (s <= 0) {
			perror("read");
			return -1;
		}

		b += s;
		n -= s;
	}

	return 0;
}

int writen(int f, const char *b, size_t n)
{
	while (n) {
		ssize_t s = write(f, b, n);
		if (s <= 0) {
			perror("write");
			return -1;
		}

		b += s;
		n -= s;
	}

	return 0;
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
	if (e) {
		fprintf(stderr, "%s\n", gai_strerror(e));
		return -1;
	}

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

	return -1;
}

int handshake(int f)
{
	char a[2];
	if (readn(f, a, 2))
		return -1;

	char b[256];
	if (readn(f, b, a[1]))
		return -1;

	a[1] = 0;
	return writen(f, a, 2);
}

int establish(int f)
{
	char b[10];
	if (readn(f, b, 5))
		return -1;

	assert(b[1] == 1);
	assert(b[3] == 3);
	char host[256] = { };
	if (readn(f, host, b[4]))
		return -1;

	char port[16];
	if (readn(f, port, 2))
		return -1;

	sprintf(port, "%hu", ntohs(*(uint16_t *) port));
	printf("%s:%s.\n", host, port);
	int c = start(connect, host, port, 0);
	if (c < 0)
		return -1;

	memset(&b[1], 0, 7);
	b[3] = 1;
	sprintf(&b[8], "%hu", htons(1116));
	if (writen(f, b, 10)) {
		close(c);
		return -1;
	}

	return c;
}
