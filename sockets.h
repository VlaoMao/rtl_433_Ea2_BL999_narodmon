#ifndef SOCKETS_H
#define SOCKETS_H

#ifdef __cplusplus
extern "C" {
#endif

int create_connect_socket(const char *host, const char *port);
int close_socket(int fd);
int send_to_socket(int fd, const char *data, size_t len);
int read_from_socket(int fd, char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // SOCKETS_H
