#define _GNU_SOURCE
#include <stdio.h>
#include <netdb.h>
#include <string.h>

int main (int argc, char **argv){

    struct addrinfo *result, hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (argc < 2){
        printf("Usage: %s <hostname>", argv[0]);
        return 1;
    }

    int ret = getaddrinfo(argv[1], NULL, &hints, &result);
    if (ret != 0) {
        printf("getaddrinfo: %s", gai_strerror(ret));
        return 0;
    }

    struct addrinfo *entry;
    char host[NI_MAXHOST];
    for (entry = result; entry != NULL; entry = entry->ai_next) {
        getnameinfo(
          entry->ai_addr,
          entry->ai_addrlen,
          host,
          sizeof(host),
          NULL,
          0,
          NI_NUMERICHOST
        );
        printf("----\n");
        printf("Host: %s; Protocol: %d; SocketType: %d;\n", host, entry->ai_protocol, entry->ai_socktype);
    }
    printf("----\n");

    freeaddrinfo(entry);

    return 0;
}