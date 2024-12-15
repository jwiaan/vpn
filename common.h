#pragma once

#include <assert.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include <openssl/ssl.h>

struct vpn {
	int fd;
	SSL *ssl;
};

void loop(struct vpn *);
void server(const char *, const char *, void (*)(int));
int start(int (*)(int, const struct sockaddr *, socklen_t),
	  const char *, const char *, int);
