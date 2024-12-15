#include <assert.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/epoll.h>

enum state { HANDSHAKE, CONNECT, FORWARD, ERROR };

struct connection {
	int from, to;
	enum state state;
};

int call(void *p, int f, char *b, size_t n, const char *str)
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

void create(int from, int to, enum state state, int e)
{
	struct connection *co = malloc(sizeof(*co));
	co->from = from;
	co->to = to;
	co->state = state;
	struct epoll_event ee = {.events = EPOLLIN,.data.ptr = co };
	epoll_ctl(e, EPOLL_CTL_ADD, from, &ee);
}

void forward(struct connection *co)
{
	char b[1024 * 1024];
	ssize_t s = read(co->from, b, sizeof(b));
	if (s <= 0) {
		perror("read");
		goto error;
	}

	printf("read %ld - %d\n", s, co->from);
	int i = call(write, co->to, b, s, "write");
	if (i)
		goto error;

	return;
 error:
	co->state = ERROR;
}

void Connect(struct connection *co, int e)
{
	char b[10];
	int i = call(read, co->from, b, 5, "read");
	if (i)
		goto error;

	assert(b[1] == 1);
	assert(b[3] == 3);
	char host[256] = { };
	i = call(read, co->from, host, b[4], "read");
	if (i)
		goto error;

	char port[16];
	i = call(read, co->from, port, 2, "read");
	if (i)
		goto error;

	sprintf(port, "%hu", ntohs(*(uint16_t *) port));
	printf("%s:%s.\n", host, port);
	int c = start(connect, host, port, 0);
	if (c < 0)
		goto error;

	memset(&b[1], 0, 7);
	b[3] = 1;
	sprintf(&b[8], "%hu", htons(1116));
	i = call(write, co->from, b, 10, "write");
	if (i) {
		close(c);
		goto error;
	}

	create(c, co->from, FORWARD, e);
	co->to = c;
	co->state = FORWARD;
	printf("connect\n");
	return;
 error:
	co->state = ERROR;
}

void handshake(struct connection *co)
{
	char a[2];
	int i = call(read, co->from, a, 2, "read");
	if (i)
		goto error;

	char b[256];
	i = call(read, co->from, b, a[1], "read");
	if (i)
		goto error;

	a[1] = 0;
	i = call(write, co->from, a, 2, "write");
	if (i)
		goto error;

	co->state = CONNECT;
	return;
 error:
	co->state = ERROR;
}

void handle(struct connection *co, int e)
{
	switch (co->state) {
	case HANDSHAKE:
		handshake(co);
		break;
	case CONNECT:
		Connect(co, e);
		break;
	case FORWARD:
		forward(co);
		break;
	default:
		assert(0);
	}

	if (co->state == ERROR) {
		printf("close %d\n", co->from);
		close(co->from);
		free(co);
	}
}

void Accept(int s, int e)
{
	int c = accept(s, NULL, NULL);
	assert(c > 0);
	printf("accept %d\n", c);
	create(c, 0, HANDSHAKE, e);
}

void init(void)
{
	struct sigaction s = {.sa_handler = SIG_IGN };
	sigemptyset(&s.sa_mask);
	sigaction(SIGPIPE, &s, NULL);
}

int main(void)
{
	init();
	int s = start(bind, NULL, "1116", AI_PASSIVE);
	assert(s != -1);
	listen(s, 1024);

	int e = epoll_create1(0);
	struct epoll_event ee[1024] = { {.events = EPOLLIN,.data.fd = s} };
	epoll_ctl(e, EPOLL_CTL_ADD, s, ee);

	while (1) {
		int n = epoll_wait(e, ee, sizeof(ee) / sizeof(*ee), -1);
		assert(n > 0);

		for (int i = 0; i < n; ++i) {
			if (ee[i].events != EPOLLIN) {
				printf("events=%x\n", ee[i].events);
			}

			if (ee[i].data.fd == s)
				Accept(s, e);
			else
				handle(ee[i].data.ptr, e);
		}
	}
}
