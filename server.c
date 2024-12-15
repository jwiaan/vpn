#include "common.h"

SSL_CTX *ctx;

void *thread(void *p)
{
	struct vpn *vpn = p;
	SSL_accept(vpn->ssl);
	vpn->fd = start(connect, "www.baidu.com", "443", 0);
	loop(vpn);
	return NULL;
}

void work(int fd)
{
	struct vpn *vpn = malloc(sizeof(*vpn));
	vpn->ssl = SSL_new(ctx);
	SSL_set_fd(vpn->ssl, fd);
	pthread_t t;
	pthread_create(&t, NULL, thread, vpn);
}

int main(void)
{
	ctx = SSL_CTX_new(TLS_server_method());
	SSL_CTX_use_PrivateKey_file(ctx, "key", SSL_FILETYPE_PEM);
	SSL_CTX_use_certificate_file(ctx, "crt", SSL_FILETYPE_PEM);
	server(NULL, "2222", work);
}
