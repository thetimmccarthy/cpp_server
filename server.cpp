#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>

#define PORT "8080"
#define MAXDATASIZE 256
#define BACKLOG 10

void sigchild_handler(int s){
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) { // if IPv4
        return &(((struct sockaddr_in*)sa)->sin_addr);
    } 

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int getListenerSocket() {
    int listener;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    rv = getaddrinfo(NULL, PORT, &hints, &servinfo);

    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener == -1) {
            perror("server: socket");
            continue;
        }

        
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes , sizeof(int)) == -1) {
            perror("setsockopt");
            continue;
        }

        if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) {
            close(listener);
            perror("server: bind");
            continue;
        }

        break; // if you get here, socket is bound properly

    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind");
        return -1;
    }

    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        return -1;
    }

    return listener;

}

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size) {
    if (*fd_count == *fd_size) {
        *fd_size *= 2;
        *pfds = (pollfd*) realloc(*pfds, sizeof(**pfds) * (*fd_size)); // explicit conversion necessary for c++
    }

    (*pfds)[*fd_count].fd = newfd; // add file descriptor to next open spot of array
    (*pfds)[*fd_count].events = POLLIN;

    (*fd_count)++;
}

void del_from_pdfs(struct pollfd *pfds, int i, int *fd_count) {
    pfds[i] = pfds[*fd_count - 1];
    (*fd_count)--;
}


int main (int argc, char *argv[]) {
    int numbytes;
    char buf[MAXDATASIZE];
    int sockfd, new_fd; // sockfd listens, new_fd gets new connections
    struct sockaddr_storage their_addr; //connector's address info
    socklen_t sin_size;
    struct sigaction sa;
    char s[INET6_ADDRSTRLEN];
    
    // not sure we need this still?
    sa.sa_handler = sigchild_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    int fd_count = 0;
    int fd_size = 5;
    struct pollfd* pfds = (pollfd*) malloc(sizeof(pollfd) * fd_size);

    if ((sockfd = getListenerSocket()) == -1) {
        perror("server: socket failed to connect");
        exit(1);
    };

    if (sockfd == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    printf("Server: waiting for connection...\n");

    pfds[0].fd = sockfd;
    pfds[0].events = POLLIN;
    fd_count = 1; // for sockfd, the listening socket
    

    while(1) {

        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }

        for (int i = 0; i < fd_count; i++) {
            if (pfds[i].revents & POLLIN) {
                
                if (pfds[i].fd == sockfd) {
                    sin_size = sizeof(their_addr);
                    new_fd = accept(sockfd,(struct sockaddr *)&their_addr, &sin_size);

                    if (new_fd == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&pfds, new_fd, &fd_count, &fd_size);

                        printf("pollserver: new connection from %s on socket %d\n", inet_ntop(their_addr.ss_family, 
                            get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s)), new_fd);
                    }


                } else {
                    int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);
                    int sender_fd = pfds[i].fd;

                    if (nbytes <= 0) {
                        if (nbytes == 0) {
                            printf("pollserver: socket %d hung up\n", sender_fd);
                        } else {
                            perror("recv");
                        }

                        close(pfds[i].fd);

                        del_from_pdfs(pfds, i, &fd_count);
                    } else {
                        if (nbytes > 0) {
                            for (int j = 0; j < fd_count; j++) {
                                
                                int dest_fd = pfds[j].fd;
                                // send to everyone except listener and sender (origin)
                                if (dest_fd != sockfd && dest_fd != sender_fd) {
                                    if(send(dest_fd, buf, nbytes, 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }
                    }

                }
            }
        }

    }
    return 0;
    

};