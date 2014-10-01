//============================================================================
// Name        : proja.cpp
// Author      : Jiahao Liang
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define MAXDATASIZE 64

int readConfig (char* filename, int *p_stage, int *p_num_routers){
    std::ifstream infile(filename);
    char buf[64];
    char *field=buf, *val=buf;
    while(infile.getline(buf,64,'\n')){
        if (buf[0] == '#') continue;	//ignore # comment
        else{
            field = strtok(buf, " ");
            val = strtok(NULL, " ");
            std::cout<<field<<std::endl;
            std::cout<<val<<std::endl;
            if(!strcmp(field,"stage")) *p_stage = atoi(val);
            else if(!strcmp(field,"num_routers")) *p_num_routers = atoi(val);
            else return 1;
        }
    }

    infile.close();
    return 0;
}

//ai_socktype = SOCK_DGRAM or SOCK_STREAM, dynamic port: port = "0"
int createSocket(int ai_socktype, const char* port, int* p_sockfd, in_port_t* p_sin_port){
	//network variables
	struct addrinfo hints, *res, *p;
	int rv;
	struct sockaddr_in sa;	//store local address
	unsigned int sa_len = sizeof(sa);
	char buf[64];


	memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_UNSPEC;	//ipv4 or ipv6
    hints.ai_socktype=ai_socktype;	//udp
    hints.ai_flags=AI_PASSIVE;	//set host IP as NULL
    if ((rv = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = res; p != NULL; p = p->ai_next) {
        if ((*p_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        if (bind(*p_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(*p_sockfd);
            perror("listener: bind");
            continue; }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    //get IP & port number
    getsockname(*p_sockfd, (struct sockaddr*)&sa, &sa_len);
//    inet_ntop(AF_INET, (void*)&(sa.sin_addr), buf, sizeof buf);

    *p_sin_port = sa.sin_port;

    return 0;
}
int main(int argc, char **argv) {
    std::cout << "!!!Hello World!!!" << std::endl; // prints !!!Hello World!!!
    int stage = 0,num_routers = 0;
    int sockfd = 0, cpid = 0;
    in_port_t port_num_proxy = 0, port_num_router = 0;

    char buf[MAXDATASIZE] = {0};
    int numbytes = 0;

    if(readConfig(*(++argv), &stage, &num_routers)){
        perror("readConfig");
        exit(1);
    }

    if(createSocket(SOCK_DGRAM, "0", &sockfd, &port_num_proxy)){
        perror("createSocket");
        exit(1);
    }

    std::cout<<port_num_proxy<<std::endl;

    std::ofstream outfile("stage1.proxy.out");
    outfile<<"proxy port: "<<port_num_proxy<<std::endl;

    cpid = fork();
    if(cpid){
    	//child process (router) here...
    	sockaddr sa;
    	unsigned int sa_len = sizeof sa;

    	getsockname(sockfd, &sa, &sa_len);
    	outfile.close();
    	close(sockfd);

		if(createSocket(SOCK_DGRAM, "0", &sockfd, &port_num_router)){
			perror("createSocket");
			exit(1);
		}
		std::cout<<"router 1, "<<"pid: "<<
				cpid<<", "<< "port: "<<port_num_router<<std::endl;

		std::ofstream outfile("stage1.router1.out");
		outfile<<"router 1, "<<"pid: "<<
				cpid<<", "<< "port: "<<port_num_router<<std::endl;

		sprintf(buf, "I am up#%d#%d", cpid, port_num_router);

		if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
				&sa, sa_len)) == -1) {
			perror("talker: sendto");
			exit(1);
		}
		outfile.close();
		close(sockfd);

    }else{
    	//parent process (proxy) here...
    	char* p;

    	if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
    					NULL, NULL)) == -1) {
    				perror("recvfrom");
    				exit(1);
    	}
		buf[numbytes] = '\0';

		p = strtok(buf, "#");
		if(!strcmp(p, "I am up")){
			p = strtok(NULL, "#");
			outfile<<"router 1, "<<"pid: "<<atoi(p)<<", ";
			p = strtok(NULL, "#");
			outfile<<"port: "<<atoi(p)<<std::endl;
		}

		outfile.close();
		close(sockfd);
    }


    return 0;
}
