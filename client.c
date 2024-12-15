#include "common.h"

SSL_CTX *ctx;

void *thread(void *p)
{
	struct vpn *vpn = p;
	int fd = start(connect, NULL, "2222", 0);
	SSL_set_fd(vpn->ssl, fd);
	SSL_connect(vpn->ssl);
	loop(vpn);
	return NULL;
}

void work(int fd)
{
	struct vpn *vpn = malloc(sizeof(*vpn));
	vpn->fd = fd;
	vpn->ssl = SSL_new(ctx);
	pthread_t t;
	pthread_create(&t, NULL, thread, vpn);
}

int main(void)
{
	ctx = SSL_CTX_new(TLS_client_method());
	server(NULL, "1111", work);
}
