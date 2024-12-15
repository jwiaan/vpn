cc = gcc -Wall -o$@ $< common.c -lssl
.PHONY: all key clean
all: server client
server: server.c common.c common.h
	$(cc)
client: client.c common.c common.h
	$(cc)
key:
	openssl genrsa -out key
	openssl req -new -key key -out csr -subj /name=vpn
	openssl x509 -req -in csr -key key -out crt
clean:
	indent -linux *.c *.h
	rm -f *~ *.o server client key csr crt
