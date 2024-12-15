#pragma once

#include <assert.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>
#include <openssl/ssl.h>

int handshake(int);
int establish(int);
int readn(int, char *, size_t);
int writen(int, const char *, size_t);
int start(int (*)(int, const struct sockaddr *, socklen_t),
	  const char *, const char *, int);
