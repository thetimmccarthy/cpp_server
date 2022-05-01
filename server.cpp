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

#define PORT "8080"
#define MAXDATASIZE 100
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

int main (int argc, char *argv[]) {
    int numbytes;
    char buf[MAXDATASIZE];
    int sockfd, new_fd; // sockfd listens, new_fd gets new connections
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; //connector's address info
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];

    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // could be AF_INET or AF_INET6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // system gets IP address for server

    // puts info into servinfo, a pointer to the headnode of a linked list - servinfo
    // first param is domain name, but we set to passive
    // gives a linkedlist bc name may have several IP addresses
    rv = getaddrinfo(NULL, PORT, &hints, &servinfo);

    if (rv != 0) {
        fprintf(stderr, "getaddringo: %s\n", gai_strerror(rv));
    }
    
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (sockfd == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break; // if you get here, socket is bound properly
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchild_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("Server: waiting for connection...\n");

    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

        printf("server: got connection from %s\n", s);

        // if (!fork()) { // this is the child process
        //     close(sockfd); // child doesnt need the listener
        //     if (send(new_fd, "Hello world!\n", sizeof("Hello world!\n"), 0) == -1 ) {
        //         perror("send");
        //     } 
        //     close(new_fd);
        //     exit(0);
            
        // }
    numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0);
    if (numbytes == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';

    printf("server: received %s\n", buf);    

    if (!fork()) { // this is the child process
            close(sockfd); // child doesnt need the listener
            if (send(new_fd, buf, sizeof(buf), 0) == -1 ) {
                perror("send");
            } 
            close(new_fd);
            exit(0);
            
        }

    }
    return 0;
    

};