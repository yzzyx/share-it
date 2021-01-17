//
// Created by elias on 2020-12-12.
//
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <zlib.h>
#include "shareit.h"
#include "framebuffer.h"
#include "packet.h"
#include "buf.h"

#define PORT "8999"
#define BACKLOG 5

typedef struct session session_t;

typedef struct {
    int sockfd;
    char remote_addr[INET6_ADDRSTRLEN];
    session_t *current_session;

    z_stream *input_stream;
    z_stream *output_stream;
} client_t;

struct session {
    char *session_name;
    char *password;

    client_t **clients;
    int client_sz;
    int client_count;
};

session_t *new_session(const char *name, const char *password);
void free_session(session_t *s);
void session_add_client(session_t *s, client_t *client);
void session_remove_client(session_t *s, client_t *client);

typedef struct {
    client_t **clients;
    struct pollfd *pfds;

    int client_count;
    int client_list_size;

    session_t *sessions[256];
    int session_count;
} client_manager_t;

client_manager_t *new_client_manager(int server_socket);
int client_manager_add_client(client_manager_t *mgr, int client_fd, char *remote_addr);
int client_manager_remove_client(client_manager_t *mgr, int idx);
int client_manager_poll(client_manager_t *mgr) ;
int client_manager_add_session(client_manager_t *mgr, session_t *session);
void client_manager_remove_session(client_manager_t *mgr, session_t *session);

void print_update(framebuffer_update_t *update) {
    if (update == NULL) {
        printf("update: NULL\n");
        return;
    }

    printf("update: %p\n", update);
    printf(" n-rects: %d\n", update->n_rects);
    int i;
    for (i = 0; i < update->n_rects; i++) {
        framebuffer_rect_t *rect = update->rects[i];
        printf("  [%d] @ %d,%d\n", i, rect->xpos, rect->ypos);
        printf("  [%d] w: %d h: %d\n", i, rect->width, rect->height);
        if (rect->encoding_type == framebuffer_encoding_type_solid) {
            printf("  [%d] encoding SOLID (%d)\n", i, rect->encoding_type);
            printf("  [%d] color: %d, %d, %d\n", i, rect->enc.solid.red, rect->enc.solid.green, rect->enc.solid.blue);
        } else if (rect->encoding_type == framebuffer_encoding_type_raw) {
            printf("  [%d] encoding RAW (%d)\n", i, rect->encoding_type);
        } else {
            printf("  [%d]-> unknown encoding (%d)\n", i, rect->encoding_type);
        }
    }
}

int client_manager_handle_client(client_manager_t *mgr, client_t *c) {
    // Read packet-type first
    size_t sz;
    uint8_t type;
    int err;
    framebuffer_update_t *update = NULL;

    sz = recv(c->sockfd, &type, sizeof(type), 0);
    if (sz <= 0) {
        return -1;
    }

    if (type == packet_type_session_join_request) {
        char *session_name, *password;
        if (pkt_recv_session_join_request(c->sockfd, &session_name, &password) == -1) {
            perror("pkt_recv_session_join_request");
            return -1;
        }

        // Remove client from their current session if they have one
        if (c->current_session != NULL) {
            session_t *s = c->current_session;
            session_remove_client(s, c);
            if (s->client_count == 0) {
                client_manager_remove_session(mgr, s);
            }
        }

        int has_session = 0;
        for (int i = 0; i < mgr->session_count; i ++) {
            if (strcmp(mgr->sessions[i]->session_name, session_name) == 0) {
                printf("[%p] joining existing session %s\n", c, session_name);
                session_add_client(mgr->sessions[i], c);
                has_session = 1;
                break;
            }
        }

        if (!has_session) {
            printf("[%p] creating new session %s\n", c, session_name);
            session_t *s = new_session(session_name, password);
            if (s == NULL) {
                return -1;
            }
            // Add current client to session
            session_add_client(s, c);
            client_manager_add_session(mgr, s);
        }

        pkt_session_join_response_t response_pkt = {
            .status = SESSION_JOIN_OK,
        };

        if (pkt_send_session_join_response(c->sockfd, &response_pkt) == -1) {
            perror("pkt_send_session_join_response");
            return -1;
        }

        // send list of currently joined clients if we have any...

    } else if (type == packet_type_session_screenshare_start) {
        uint16_t width, height;
        if (pkt_recv_session_screenshare_start_request(c->sockfd, &width, &height) != 0) {
            perror("pkt_recv_session_screenshare_start_request");
            return -1;
        }

        if (c->current_session != NULL) {
            session_t *s = c->current_session;
            for (int i = 0; i < s->client_count; i ++) {
                if (s->clients[i] == c) {
                    continue;
                }
                pkt_send_session_screenshare_request(s->clients[i]->sockfd, width, height);
            }
        }
        printf("[%p] screenshare start: %d x %d\n", c, width, height);
    } else if (type == packet_type_framebuffer_update) {
        if ((err = pkt_recv_framebuffer_update(c->sockfd, &update)) != 0) {
            fprintf(stderr, "error while reading screendata: %s\n", strerror(err));
            return -1;
        }
        printf("[%p] update received\n", c);
        if (c->current_session != NULL) {
            session_t *s = c->current_session;
            for (int i = 0; i < s->client_count; i ++) {
                if (s->clients[i] == c) {
                    continue;
                }
                pkt_send_framebuffer_update(s->clients[i]->sockfd, update);
            }
        }
        free_framebuffer_update(update);
    } else if (type == packet_type_cursor_info) {
        uint16_t x, y;
        uint8_t cursor;
        err = pkt_recv_cursorinfo(c->sockfd, &x, &y, &cursor);
        if (err != 0) {
            fprintf(stderr, "error while reading cursor info: %s\n", strerror(err));
            return -1;
        }
        printf("[%p] cursor: %d, %d\n", c, x, y);
        if (c->current_session != NULL) {
            session_t *s = c->current_session;
            for (int i = 0; i < s->client_count; i ++) {
                if (s->clients[i] == c) {
                    continue;
                }
                pkt_send_cursorinfo(s->clients[i]->sockfd, x, y, cursor);
            }
        }
        free_framebuffer_update(update);
    } else {
        fprintf(stderr, "unknown packet type %d\n", type);
        return 1;
    }

    return 0;
}

client_manager_t *new_client_manager(int server_socket) {
    client_manager_t *mgr;

    mgr = calloc(1, sizeof(client_manager_t));
    if (mgr == NULL) {
        return NULL;
    }
    mgr->client_list_size = 5;
    mgr->pfds = calloc(mgr->client_list_size, sizeof(struct pollfd));
    mgr->clients = calloc(mgr->client_list_size, sizeof(client_t *));
    mgr->pfds[0].fd = server_socket;
    mgr->pfds[0].events = POLL_IN;

    return mgr;
}

int client_manager_add_client(client_manager_t *mgr, int client_fd, char *remote_addr) {
    if (mgr->client_count == mgr->client_list_size) {
        mgr->client_list_size *= 2;
        mgr->clients = realloc(mgr->clients, sizeof(client_t *) * mgr->client_list_size);
        if (mgr->clients == NULL) {
            return -1;
        }

        mgr->pfds = realloc(mgr->pfds, sizeof(struct pollfd) * (mgr->client_list_size+1));
        if (mgr->pfds == NULL) {
            return -1;
        }
    }

    client_t *c = malloc(sizeof (client_t));
    c->sockfd = client_fd;
    strncpy(c->remote_addr, remote_addr, sizeof(c->remote_addr));

    // Compression is not implemented
//    c->input_stream = calloc(1, sizeof(z_stream));
//    if (c->input_stream == NULL) {
//        return -1;
//    }
//
//    ret = inflateInit(c->input_stream);
//    if (ret != Z_OK) {
//        return -1;
//    }

    mgr->clients[mgr->client_count] = c;
    mgr->pfds[mgr->client_count+1].fd = client_fd;
    mgr->pfds[mgr->client_count+1].events = POLL_IN;
    mgr->client_count ++;

    printf("Adding client from %s\n", remote_addr);
    return 0;
}

int client_manager_remove_client(client_manager_t *mgr, int idx) {
    client_t *c = mgr->clients[idx];
    // Compression is not implemented
//    inflateEnd(c->input_stream);
//    deflateEnd(c->output_stream);
//    free(c->input_stream);
//    free(c->output_stream);
    free(c);

    mgr->clients[idx] = mgr->clients[mgr->client_count-1];
    mgr->pfds[idx+1] = mgr->pfds[mgr->client_count];
    mgr->client_count--;
    return 0;
}

int client_manager_poll(client_manager_t *mgr) {
    int poll_count = poll(mgr->pfds, mgr->client_count + 1, -1);
    if (poll_count == -1) {
        perror("poll");
        return 1;
    }

    for (int idx = 0; idx < mgr->client_count+1; idx++) {
        if (mgr->pfds[idx].revents & POLLIN) {
            if (idx == 0) { // Handle events on the server connection socket
                int client_fd = 0;
                char addr_buf[INET6_ADDRSTRLEN];
                struct sockaddr_storage remote_addr;
                socklen_t sin_size = sizeof remote_addr;

                client_fd = accept(mgr->pfds[0].fd, (struct sockaddr *) &remote_addr, &sin_size);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }

                void *addr;
                if (((struct sockaddr *) &remote_addr)->sa_family == AF_INET) {
                    addr = &(((struct sockaddr_in *) &remote_addr)->sin_addr);
                } else {
                    addr = &(((struct sockaddr_in6 *) &remote_addr)->sin6_addr);
                }
                inet_ntop(remote_addr.ss_family, addr, addr_buf, sizeof(addr_buf));
                if (client_manager_add_client(mgr, client_fd, addr_buf) != 0) {
                    return 1;
                }
            } else {
                if (client_manager_handle_client(mgr, mgr->clients[idx - 1]) < 0) {
                    close(mgr->pfds[idx].fd);
                    client_manager_remove_client(mgr, idx - 1);
                }
            }
        }
    }
    return 0;
}

int client_manager_add_session(client_manager_t *mgr, session_t *session) {
    if (mgr->session_count == sizeof(mgr->sessions) - 1) {
        printf("maximum number of sessions reached");
        return 1;
    }
    mgr->sessions[mgr->session_count] = session;
    mgr->session_count++;
    return 0;
}

void client_manager_remove_session(client_manager_t *mgr, session_t *session) {
    for (int i = 0; i < mgr->session_count; i ++) {
        if (mgr->sessions[i] == session) {
            mgr->sessions[i] = mgr->sessions[mgr->session_count-1];
            mgr->session_count --;
            return;
        }
    }
}

session_t *new_session(const char *name, const char *password) {
    session_t *s;
    s = calloc(1, sizeof(session_t));
    if (s == NULL) {
        return NULL;
    }

    s->session_name = strdup(name);
    if (password != NULL) {
        s->password = strdup(password);
    }
    return s;
}

void free_session(session_t *s) {
    if (s->session_name != NULL) {
        free(s->session_name);
    }

    if (s->password != NULL) {
        free(s->password);
    }

    if (s->clients != NULL) {
        for (int i = 0; i < s->client_count; i ++) {
            if (s->clients[i]->current_session == s) {
                s->clients[i]->current_session = NULL;
            }
        }
        free(s->clients);
    }
    free(s);
}

void session_add_client(session_t *s, client_t *client) {
    if (s->client_count == s->client_sz) {
        s->client_sz += 5;
        s->clients = realloc(s->clients, sizeof(client_t *) * s->client_sz);
    }
    s->clients[s->client_count] = client;
    client->current_session = s;
    s->client_count ++;
}

void session_remove_client(session_t *s, client_t *client) {
    for (int i = 0; i < s->client_count; i ++) {
        if (s->clients[i] == client) {
            s->clients[i] = s->clients[s->client_count-1];
            s->client_count --;
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    struct addrinfo hints, *servinfo, *p;
    int server_socket;
    int rv;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((server_socket = socket(p->ai_family, p->ai_socktype,
                                    p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            return 1;
        }

        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_socket);
            perror("server: bind");
            continue;
        }

        break;
    }
    freeaddrinfo(servinfo);


    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 1;
    }

    if (listen(server_socket, BACKLOG) == -1) {
        perror("listen");
        return 1;
    }

    client_manager_t *mgr = new_client_manager(server_socket);

    for (;;) {
        int ret = client_manager_poll(mgr);
        if (ret != 0) {
            return 1;
        }
    }
    return 0;
}