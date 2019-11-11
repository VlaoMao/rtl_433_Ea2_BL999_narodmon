#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sockets.h"


int create_connect_socket(const char *host, const char *port)
{
    struct hostent *hostnm;    /* server host name information        */
    struct sockaddr_in server; /* server address                      */
    int s;

    hostnm = gethostbyname(host);
    if (hostnm == (struct hostent *) 0)
    {
        fprintf(stderr, "Gethostbyname failed\n");
        return -1;
    }

    unsigned short port_num = (unsigned short) atoi(port);

    server.sin_family      = AF_INET;
    server.sin_port        = htons(port_num);
    server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Socket(): %m\n");
        return -1;
    }

    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        fprintf(stderr, "Connect(): %m\n");
        return -1;
    }

    return s;
}

int close_socket(int fd)
{
    shutdown(fd, SHUT_RDWR);
    return close(fd);
}

int send_to_socket(int fd, const char *data, size_t len)
{
    return send(fd, data, len, 0);
}

int read_from_socket(int fd, char *buf, size_t len)
{
    return recv(fd, buf, len, 0);
}
