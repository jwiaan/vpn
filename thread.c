#include <unistd.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

struct arg {
	int x;
};

int call(void *p, int f, char *b, size_t n)
{
	while (n > 0) {
		ssize_t s = ((ssize_t(*)(int, char *, size_t))p) (f, b, n);
		if (s > 0) {
			b += s;
			n -= s;
			continue;
		}

		perror("call");
		return -1;
	}

	return 0;
}

int io(int x, int y)
{
	char b[4096];
	ssize_t r = read(x, b, sizeof(b));
	if (r <= 0) {
		perror("read");
		return -1;
	}

	return call(write, y, b, r);
}

void loop(int x, int y)
{
	struct pollfd p[2] = { {x, POLLIN}, {y, POLLIN} };
	while (poll(p, 2, -1) > 0) {
		for (int i = 0; i < 2; i++) {
			if (p[i].revents & POLLIN) {
				if (io(p[i].fd, p[(i + 1) % 2].fd))
					return;
			}
		}
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

int init(int x)
{
	char b[4096];
	int i = call(read, x, b, 3);
	assert(i == 0);

	b[1] = 0;
	i = call(write, x, b, 2);
	assert(i == 0);

	i = call(read, x, b, 5);
	assert(i == 0);
	assert(b[1] == 1);
	assert(b[3] == 3);

	char host[260] = { };
	i = call(read, x, host, b[4]);
	assert(i == 0);

	char port[10];
	i = call(read, x, port, 2);
	assert(i == 0);
	sprintf(port, "%hu", ntohs(*(uint16_t *) port));

	printf("%s:%s.\n", host, port);
	int y = start(connect, host, port, 0);

	b[1] = 0;
	b[3] = 1;
	for (int i = 0; i < 4; i++)
		b[4] = 0;

	sprintf(&b[8], "%hu", htons(1116));
	i = call(write, x, b, 10);
	assert(i == 0);
	return y;
}

void *thread(void *p)
{
	int x = ((struct arg *)p)->x;
	int y = init(x);
	loop(x, y);
	close(x);
	close(y);
	free(p);
	printf("%d end\n", x);
	return NULL;
}

int main(void)
{
	int s = start(bind, NULL, "1116", AI_PASSIVE);
	listen(s, 10);
	while (1) {
		int c = accept(s, NULL, NULL);
		if (c < 0)
			perror("accept");

		printf("accept %d\n", c);
		struct arg *p = malloc(sizeof(*p));
		p->x = c;
		pthread_t t;
		pthread_create(&t, NULL, thread, p);
	}
}
