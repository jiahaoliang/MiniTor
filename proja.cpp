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
#include <sys/time.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <fcntl.h>
#include <linux/icmp.h>
#include <linux/ip.h>
#include <signal.h>



#include "sample_tunnel.h"

#define MAXDATASIZE 2048

#define MAX(x,y) ((x)>(y))?(x):(y)

using namespace std;

#define DEBUG(a,a_should_be) {if((a) != (a_should_be) )  \
									printf("What the fuck!/n"); \
				              else \
									printf("Thanks God!/n");}

int readConfig (char* filename, int *p_stage, int *p_num_routers){
    std::ifstream infile(filename);
    char buf[64];
    char *field=buf, *val=buf;
    if (!infile.is_open()) return 1;

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
//const char* port: destination port number
//in_port_t* p_sin_port: local port number
int createSocket(int ai_socktype, const char* port, int* p_sockfd, in_port_t* p_sin_port){
	//network variables
	struct addrinfo hints, *res, *p;
	int rv;
	struct sockaddr_in sa;	//store local address
	unsigned int sa_len = sizeof(sa);
//	char buf[64];


	memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_UNSPEC;	//ipv4 or ipv6
    hints.ai_socktype=ai_socktype;	//SOCK_DGRAM or SOCK_STREAM
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

    freeaddrinfo(res); // free the linked-list

    return 0;
}


/*checksum calculation function is
 * from http://blog.csdn.net/qy532846454/article/details/5384086
 */
__sum16 chcksum(struct icmphdr *icmp_header, int len_in_bytes){
	   int sum = 0;
	   int nleft = len_in_bytes;
	    unsigned short *p;
	    unsigned short tmp = 0;
	   while( nleft > 1)
	   {
	       sum += *p++;
	       nleft -= 2;
	    }
	    sum += (sum >> 16) + (sum & 0xffff);
	    sum += sum >> 16;
	    tmp = ~sum;
	    return tmp;

}

int main(int argc, char **argv) {
    std::cout << "Start...." << std::endl;

    int stage = 0,num_routers = 0;
    int sockfd = 0, cpid = 0;
	sockaddr sa;
	unsigned int sa_len = sizeof sa;
    in_port_t port_num_proxy = 0, port_num_router = 0;

    //for stage 2
    char tun_name[IFNAMSIZ];
	strcpy(tun_name, "tun1");
    int tun_fd;
    int maxfd;
    fd_set rfds;
    int retval;

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
//
//    switch (stage){
//    case 1:
//    	goto STAGE1;
//    	break;
//    case 2:
//    default:
//    	return 0;
//
//    }


//STAGE1:
    std::ofstream outfile;
    if(stage==1){
    	outfile.open("stage1.proxy.out");
    }else if(stage==2){
    	outfile.open("stage2.proxy.out");
    }else return 0;

	outfile<<"proxy port: "<<port_num_proxy<<std::endl;

	getsockname(sockfd, &sa, &sa_len);	//get proxy socket address, store at router(child) as globle variable
    cpid = fork();
    if(cpid){
    	//child process (router) here...

    	outfile.close();
    	close(sockfd);

		if(createSocket(SOCK_DGRAM, "0", &sockfd, &port_num_router)){
			perror("createSocket");
			exit(1);
		}
		std::cout<<"router 1, "<<"pid: "<<
				cpid<<", "<< "port: "<<port_num_router<<std::endl;

		if(stage==1){
			outfile.open("stage1.router1.out");
		}else if(stage==2){
			outfile.open("stage2.router1.out");
		}
		outfile<<"router 1, "<<"pid: "<<
				cpid<<", "<< "port: "<<port_num_router<<std::endl;

		sprintf(buf, "I am up#%d#%d", cpid, port_num_router);

		if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
				&sa, sa_len)) == -1) {
			perror("talker: sendto");
			exit(1);
		}
		if(stage==1){
			//stage 1 ends, exit
			outfile.close();
			close(sockfd);
			return 0;
		}else if(stage==2){
			//stage 2 goes here...

			//run forever
			while(1){
				if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
						&sa, &sa_len)) == -1) {
				    				perror("recvfrom");
				    				exit(1);
				    	}
				buf[numbytes] = '\0';
				struct iphdr *ip_header = (struct iphdr *)buf;
				struct in_addr sender_addr; sender_addr.s_addr=ip_header->saddr;
				struct in_addr dest_addr; dest_addr.s_addr=ip_header->daddr;
				outfile<<"ICMP from port: "<<((struct sockaddr_in*)&sa)->sin_port
						<<", src: "	<<inet_ntoa(sender_addr);
				outfile<<", dst: "	<<inet_ntoa(dest_addr)
						<<", ";
				struct icmphdr *icmp_header = (struct icmphdr *)(buf+20);
				int typ = icmp_header->type;
				outfile<<"type: "<< typ <<endl;

				//create ECHO_REPLY packet
				//1.switch IP address
				ip_header->daddr=sender_addr.s_addr;
				ip_header->saddr=dest_addr.s_addr;
				//2.change icmp message type
				icmp_header->type=0;
				//3.calculate icmp checksum
				icmp_header->checksum=0;
				icmp_header->checksum=chcksum(icmp_header, 8);

				//send it back to proxy
				if ((numbytes = sendto(sockfd, buf, MAXDATASIZE-1, 0,
						&sa, sa_len)) == -1) {
					perror("talker: sendto");
					exit(1);
				}

			}
			outfile.close();
			close(sockfd);
			exit(0);

		}


    }else{
    	//parent process (proxy) here...
    	char* p;
    	char router_port[6];
//    	unsigned short int port_num_router;

    	if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
    			&sa, &sa_len)) == -1) {
    				perror("recvfrom");
    				exit(1);
    	}
		buf[numbytes] = '\0';

		p = strtok(buf, "#");
		if(!strcmp(p, "I am up")){
			p = strtok(NULL, "#");
			outfile<<"router 1, "<<"pid: "<<atoi(p)<<", ";
			p = strtok(NULL, "#");
			strcpy(router_port,p);
			outfile<<"port: "<<atoi(p)<<std::endl;
		}


		if(stage==1){
			//stage 1 ends, exit
			outfile.close();
			close(sockfd);
			return 0;
		}else if(stage==2){
			//stage 2 goes here...

			//sockfd1 send data to router(child)
//			int sockfd1;
//			if(createSocket(SOCK_DGRAM, router_port, &sockfd1, &port_num_router)){
//			        perror("createSocket");
//			        exit(1);
//			    }

		    /* Connect to the tunnel interface (make sure you create the tunnel interface first) */
		    tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
		    maxfd = (tun_fd > sockfd)?tun_fd:sockfd;

		    /* Watch tun_fd and sockfd to see when they have input. */
		    FD_ZERO(&rfds);
		    FD_SET(tun_fd, &rfds);
		    FD_SET(sockfd, &rfds);

		    while(1){
//		    	struct timeval tv;
//		    	/* Wait up to 20 seconds. */
//			    tv.tv_sec = 20;
//			    tv.tv_usec = 0;

			    //select() will change the fd_set each time it is called successfully
			    FD_SET(tun_fd, &rfds);
			    FD_SET(sockfd, &rfds);

				retval = select(maxfd+1, &rfds, NULL, NULL, NULL);
				if (retval < 0)
					perror("select()");
				else if(retval){
					printf("Data is available now.\n");

					//reading the tunnel
					if(FD_ISSET(tun_fd, &rfds)){
						int nread;
						nread = tunnel_reader(tun_fd, buf);
						struct iphdr *ip_header = (struct iphdr *)buf;
						struct in_addr sender_addr; sender_addr.s_addr=ip_header->saddr;
						struct in_addr dest_addr; dest_addr.s_addr=ip_header->daddr;
						struct icmphdr *icmp_header = (struct icmphdr *)(buf+sizeof(struct iphdr));
						int typ = icmp_header->type;

						outfile<<"ICMP from tunnel, src: "<<inet_ntoa(sender_addr);
						outfile<<", dst: "	<<inet_ntoa(dest_addr)<<", ";
						outfile<<"type: "<< typ <<endl;

						//forward tunnel's packets to router
						buf[nread]='\0';
						if ((numbytes = sendto(sockfd, buf, nread, 0,
							&sa, sa_len)) == -1) {
							perror("talker: sendto");
							exit(1);
						}
					}
					//reading from router's UDP socket
					if(FD_ISSET(sockfd, &rfds)){
						if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
								&sa, &sa_len)) == -1) {
									perror("recvfrom");
									exit(1);
						}
						buf[numbytes]='\0';
						struct iphdr *ip_header = (struct iphdr *)buf;
						struct in_addr sender_addr; sender_addr.s_addr=ip_header->saddr;
						struct in_addr dest_addr; dest_addr.s_addr=ip_header->daddr;
						struct icmphdr *icmp_header = (struct icmphdr *)(buf+sizeof(struct iphdr));
						int typ = icmp_header->type;

						cout<<"get from router!"<<endl;
						outfile<<"ICMP from port: "<<((struct sockaddr_in*)&sa)->sin_port
								<<", src: "	<<inet_ntoa(sender_addr);
						outfile<<", dst: "	<<inet_ntoa(dest_addr)
								<<", ";
						outfile<<"type: "<< typ <<endl;
						//send it back to tunnel
						write(tun_fd,buf,numbytes);

					}

				}else{
					//will never come here...
					outfile.close();
					close(sockfd);
					close(tun_fd);
					exit(0);
					/*stage 2 ends*/
				}

		    }
		}
    }


    return 0;
}
