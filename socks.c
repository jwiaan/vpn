#include "common.h"

struct connection {
	int fd;
};

int forward(int from, int to)
{
	char b[4096];
	ssize_t r = read(from, b, sizeof(b));
	if (r <= 0) {
		perror("read");
		return -1;
	}

	return writen(to, b, r);
}

void loop(int x, int y)
{
	struct pollfd p[2] = { {x, POLLIN}, {y, POLLIN} };
	while (poll(p, 2, -1) > 0) {
		for (int i = 0; i < 2; i++) {
			if (p[i].revents & POLLIN) {
				if (forward(p[i].fd, p[(i + 1) % 2].fd))
					return;
			}
		}
	}

	perror("poll");
}

void *thread(void *p)
{
	struct connection *co = p;
	if (handshake(co->fd))
		goto error;

	int fd = establish(co->fd);
	if (fd < 0)
		goto error;

	loop(co->fd, fd);
	close(fd);

 error:
	close(co->fd);
	free(co);
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

		struct connection *co = malloc(sizeof(*co));
		co->fd = c;
		pthread_t t;
		pthread_create(&t, NULL, thread, co);
	}
}
