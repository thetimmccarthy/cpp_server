#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <arpa/inet.h>
#include <string>
#include <poll.h>
#include <sys/time.h>
#include <sys/types.h>

#define PORT "8080"
#define MAXDATASIZE 100
#define BACKLOG 10
#define STDIN 0

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family ==  AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    } 

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int getListenerSocket() {
    int listener;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    rv = getaddrinfo(NULL, PORT, &hints, &servinfo);

    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener == -1) {
            perror("client: socket");
            continue;
        }

        
        // if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes , sizeof(int)) == -1) {
        //     perror("setsockopt");
        //     continue;
        // }

        // if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) {
        //     close(listener);
        //     perror("client: bind");
        //     continue;
        // }    
        if (connect(listener, p->ai_addr, p->ai_addrlen) == -1) {
            close(listener);
            perror("client: connect");
            continue;
        }
        break; // if you get here, socket is bound properly
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect");
        return -1;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo);

    return listener;
}

int main(int argc, char* argv[] ) {
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];

    if (argc != 2) {
        fprintf(stderr, "usage: client hostname");
        exit(1);
    }
    sockfd = getListenerSocket();
    
    if(sockfd == -1) {
        perror("client: failed to connect");
    }

    struct timeval tv;
    fd_set readfds;
    tv.tv_sec = 2;
    tv.tv_usec = 500000;
   
    while(1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN, &readfds);
        FD_SET(sockfd, &readfds);
        select(sockfd+1, &readfds, NULL, NULL, NULL);

        // read from server
        if (FD_ISSET(sockfd, &readfds)){ 
            
            char buf[1204];
            memset(buf, 0, sizeof(buf));
            int lastbit;    

            lastbit = recv(sockfd, buf, sizeof(buf),0);
            // if (lastbit > 0 && lastbit < 1024) {
            //     buf[lastbit] = '\0';
            //     printf(">%s\n",buf);
            // }
            buf[lastbit] = '\0';
            printf(">%s\n",buf);

        }
        if(FD_ISSET(STDIN, &readfds)){
            // printf("HERE\n");
            char msg[1024];
            memset(msg, 0, sizeof(msg));
            read(STDIN, msg, sizeof(msg));
            int bytessent;
            bytessent = send(sockfd, msg, sizeof(msg),0);
            if( bytessent == -1) {
                perror("client send");
                exit(1);
            }
    
            

        }
    }
    
    close(sockfd);
    return 0;
}
