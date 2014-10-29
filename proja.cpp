//============================================================================
// Name        : proja.cpp
// Author      : Jiahao Liang
// Version     :
// Copyright   : Your copyright notice
// Description :
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
#include "proja.h"

using namespace std;

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

//convert IP addr from string format to unsigned long number
//http://electrofriends.com/source-codes/software-programs/c/number-programs/c-program-to-convert-ip-address-to-32-bit-long-int/
unsigned long int ipConv(const char ipadr[])
{
    unsigned long int num=0,val;
    char *tok,*ptr;
    char copy[16];
    strcpy(copy, ipadr);
    tok=strtok(copy,".");
    while( tok != NULL)
    {
        val=strtoul(tok,&ptr,0);
        num=(num << 8) + val;
        tok=strtok(NULL,".");
    }
    return(num);
}

/*******************************************************************************************************
Determine whether host is belong to the subnet defined by routerIP and subnetMask
input:
	char hostIP[], routerIP[]:	should be IP of host and router in decimal-dot, respectively. No longer than 16
  u_int8_t subnetMask:		a number from 0-31
output:
	true if match; otherwise, false
***********************************************************************************************************/
bool isBelongSubnet(const char hostIP[], const char routerIP[], u_int8_t subnetMask){
	unsigned long int hostIP_ul = ipConv(hostIP);
	unsigned long int routerIP_ul = ipConv(routerIP);
	unsigned long int subnetMask_ul = 0xffffffff;	//32 "1"s in bits
	unsigned long int temp = 0;

	//calculate subnetMask in 32-bits
	for(;subnetMask>0;subnetMask--){
		temp = (temp<<1) + 1;
	}
	subnetMask_ul^=temp;	//XOR

	if ((hostIP_ul&subnetMask_ul) == (routerIP_ul&subnetMask_ul))
		return true;
	else
		return false;
}

//ai_socktype = SOCK_DGRAM or SOCK_STREAM or SOCK_RAW, dynamic port: port = "0"
//address: address socket want to bind to, set NULL to fill automatically
//const char* port: local port number that socket want to bind to
//in_port_t* p_sin_port: local port number in numeric form
int createSocket(char address[], int ai_socktype, const char* port, int* p_sockfd, in_port_t* p_sin_port){
	//network variables
	struct addrinfo hints, *res, *p;
	int rv;
	struct sockaddr_in sa;	//store local address
	unsigned int sa_len = sizeof(sa);
//	char buf[64];


	memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_UNSPEC;	//ipv4 or ipv6
    hints.ai_socktype=ai_socktype;	//SOCK_DGRAM or SOCK_STREAM
    if (address == NULL) hints.ai_flags=AI_PASSIVE;	//set host IP as NULL, to fill IP automatically
    if ((rv = getaddrinfo(address, port, &hints, &res)) != 0) {
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

    if(p_sin_port != NULL) *p_sin_port = sa.sin_port;

    freeaddrinfo(res); // free the linked-list

    return 0;
}

int createRawSocket(char address[], int ai_socktype, int service, int* p_sockfd, in_port_t* p_sin_port){
	struct addrinfo hints, *res;
	int rv;
	struct sockaddr_in sa;	//store local address
	unsigned int sa_len = sizeof(sa);

	memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_UNSPEC;	//ipv4 or ipv6
    hints.ai_socktype=ai_socktype;	//SOCK_DGRAM or SOCK_STREAM
    hints.ai_flags=AI_CANONNAME;

    if ((rv = getaddrinfo(address, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

	if((*p_sockfd = socket(res->ai_family, SOCK_RAW, service))==-1){
			perror("raw socket");
			return 1;
	}

	if (bind(*p_sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		close(*p_sockfd);
		perror("raw bind");
		return 2;
	}

	//get IP & port number
	getsockname(*p_sockfd, (struct sockaddr*)&sa, &sa_len);
	//    inet_ntop(AF_INET, (void*)&(sa.sin_addr), buf, sizeof buf);

	if(p_sin_port != NULL) *p_sin_port = sa.sin_port;

	freeaddrinfo(res); // free the linked-list

	return 0;
}

//read ICMP echo message, print infomation to outfile
void outputIcmpMsg(char* buf, std::ofstream outfile){
//	struct iphdr *ip_header = (struct iphdr *)buf;
//	struct in_addr sender_addr; sender_addr.s_addr=ip_header->saddr;
//	struct in_addr dest_addr; dest_addr.s_addr=ip_header->daddr;
//
//	outfile<<"ICMP from port: "<<((struct sockaddr_in*)&sa)->sin_port
//			<<", src: "	<<inet_ntoa(sender_addr);
//	outfile<<", dst: "	<<inet_ntoa(dest_addr)
//			<<", ";
//
//	struct icmphdr *icmp_header = (struct icmphdr *)(buf+20);
//	int typ = icmp_header->type;
//	outfile<<"type: "<< typ <<endl;
}

//modify the income ICMP request in buf, into the icmp reply message
void createIcmpRely(char* buf){
	struct iphdr *ip_header = (struct iphdr *)buf;
	struct in_addr sender_addr; sender_addr.s_addr=ip_header->saddr;
	struct in_addr dest_addr; dest_addr.s_addr=ip_header->daddr;
	struct icmphdr *icmp_header = (struct icmphdr *)(buf+20);

	//create ECHO_REPLY packet
	//1.switch IP address
	ip_header->daddr=sender_addr.s_addr;
	ip_header->saddr=dest_addr.s_addr;
	//2.change icmp message type
	icmp_header->type=0;
	//3.calculate icmp checksum
	icmp_header->checksum=0;
	icmp_header->checksum=ip_checksum(icmp_header, 8);
	ip_header->check = 0;
	ip_header->check=ip_checksum(ip_header, 20);
}
/*icmp checksum calculation function is
 * from http://blog.csdn.net/qy532846454/article/details/5384086
 */
//__sum16 chcksum(struct icmphdr *icmp_header, int len_in_bytes){
//	   int sum = 0;
//	   int nleft = len_in_bytes;
//	    unsigned short *p;
//	    unsigned short tmp = 0;
//	   while( nleft > 1)
//	   {
//	       sum += *p++;
//	       nleft -= 2;
//	    }
//	    sum += (sum >> 16) + (sum & 0xffff);
//	    sum += sum >> 16;
//	    tmp = ~sum;
//	    return tmp;
//
//}

//ip_checksum function from http://www.microhowto.info/howto/calculate_an_internet_protocol_checksum_in_c.html
//if ICMP header, length = 8
//if IP header, length = 20
uint16_t ip_checksum(void* vdata,size_t length) {
    // Cast the data pointer to one that can be indexed.
    char* data=(char*)vdata;

    // Initialise the accumulator.
    uint32_t acc=0xffff;

    // Handle complete 16-bit blocks.
    for (size_t i=0;i+1<length;i+=2) {
        uint16_t word;
        memcpy(&word,data+i,2);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Handle any partial block at the end of the data.
    if (length&1) {
        uint16_t word=0;
        memcpy(&word,data+length-1,1);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Return the checksum in network byte order.
    return htons(~acc);
}

int main(int argc, char **argv) {
    std::cout << "Start...." << std::endl;

    int stage = 0,num_routers = 0;
    int sockfd = 0, cpid = 0;
    int rawsockfd = 0;
    int routerID = 1;
    char routerIP[16];
    struct in_addr routerIP_in_addr;
    struct in_addr sourceIP_in_addr;	//the original source IP address of packets route to router
    struct sockaddr sa;
	unsigned int sa_len = sizeof sa;
	struct sockaddr sa_proxy;			//socket address of proxy
	unsigned int sa_proxy_len = sizeof sa_proxy;
	struct sockaddr sa_router[MAXNUMROUTERS];			//socket address of routers
	unsigned int sa_router_len[MAXNUMROUTERS];

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
    int i = 0;

    if(readConfig(*(++argv), &stage, &num_routers)){
        perror("readConfig");
        exit(1);
    }

    if(stage>4 || stage<1){
    	fprintf(stderr,"unavailable stage: %d\n",stage);
    	exit(1);
    }

    if(num_routers>6 || num_routers<1){
		fprintf(stderr,"unavailable # of routers: %d\n",num_routers);
		exit(1);
	}

    if(createSocket(NULL, SOCK_DGRAM, "0", &sockfd, &port_num_proxy)){
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

    //use buf[] denote the output filename
    sprintf(buf,"stage%d.proxy.out", stage);
    outfile.open(buf);
    buf[0]='\0';	//erase the buf

	outfile<<"proxy port: "<<port_num_proxy<<std::endl;

	getsockname(sockfd, &sa_proxy, &sa_proxy_len);	//get proxy socket address, store at router(child) as globle variable

	if(stage<4)
		cpid = fork();
    else{
    	//stage 4, fork multiple routers
    	for(i=0;i<num_routers;i++){
    		cpid = fork();
    		if(cpid) break;		//only the parent(proxy) do the forking..
    		routerID++;
    	}
    }
    if(cpid){
    	//child process (router) here...
    	//stage 1 begins
    	outfile.close();
    	close(sockfd);
    	//use buf[] denote the output filename
        sprintf(buf,"stage%d.router%d.out", stage, routerID);
        outfile.open(buf);
        buf[0]='\0';	//erase the buf

		if(createSocket(NULL, SOCK_DGRAM, "0", &sockfd, &port_num_router)){
			perror("createSocket");
			exit(1);
		}
		std::cout<<"router "<<routerID<<", "<<"pid: "<<
				cpid<<", "<< "port: "<<port_num_router<<std::endl;

		outfile<<"router "<<routerID<<", "<<"pid: "<<
				cpid<<", "<< "port: "<<port_num_router<<std::endl;

		//tell proxy this router is up
		sprintf(buf, "I am up#%d#%d#%d", routerID, cpid, port_num_router);
		if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
				&sa_proxy, sa_proxy_len)) == -1) {
			perror("talker: sendto");
			exit(1);
		}

		if(stage==1){
			//stage 1 ends, exit
			goto EXIT_CHILD;
		}

		if(stage>2){
			//stage 3 adds to 2 goes here...
			sprintf(routerIP, "192.168.20%d.2", routerID);
			inet_aton(routerIP, &routerIP_in_addr);

			//create raw socket communicate to the rest of the world
			if(createRawSocket(routerIP, SOCK_RAW, IPPROTO_ICMP, &rawsockfd, NULL)){
				perror("createSocket(SOCK_RAW)");
				exit(1);
			}

//			if((rawsockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP))==-1){
//				perror("raw socket");
//				exit(1);
//			}
//			struct sockaddr_in rawsockaddr;
//			memset(&rawsockaddr, 0, sizeof rawsockaddr);
//			rawsockaddr.sin_family = AF_INET;
//			rawsockaddr.sin_port = htons(1);
//			inet_aton(routerIP,&(rawsockaddr.sin_addr));
//			if (bind(rawsockfd, (struct sockaddr*)&rawsockaddr, (sizeof rawsockaddr)) == -1) {
//				close(rawsockfd);
//				perror("raw bind");
//				exit(1);
//			}

			cout<<"raw socket ID: "<<rawsockfd<<endl;

			maxfd = MAX(sockfd,rawsockfd);
			FD_ZERO(&rfds);
		}

		//run forever
		while(1){
			if(stage>2){
				//stage 3 adds to 2 goes here...
				/* Watch rawsockfd and sockfd to see when they have input. */
				FD_SET(rawsockfd, &rfds);
				FD_SET(sockfd, &rfds);
				retval = select(maxfd+1, &rfds, NULL, NULL, NULL);
				if (retval < 0){
					perror("select()");
				}else{
					//reading the raw socket
					if(FD_ISSET(rawsockfd, &rfds)){
						if ((numbytes = recvfrom(rawsockfd, buf, MAXDATASIZE-1 , 0,
								&sa, &sa_len)) == -1) {
											perror("recvfrom");
											exit(1);
						}
						buf[numbytes] = '\0';

						struct iphdr *ip_header = (struct iphdr *)buf;
						struct in_addr sender_addr; sender_addr.s_addr=ip_header->saddr;
						struct in_addr dest_addr; dest_addr.s_addr=ip_header->daddr;
						struct icmphdr *icmp_header = (struct icmphdr *)(buf+20);
						int typ = icmp_header->type;

						cout<<"router "<<routerID<<" raw socket get: "<<", src: "<<inet_ntoa(sender_addr);
						cout<<", dst: "	<<inet_ntoa(dest_addr)
								<<", ";
						cout<<"type: "<< typ <<endl;

						outfile<<"ICMP from raw sock"<<", src: "<<inet_ntoa(sender_addr);
						outfile<<", dst: "	<<inet_ntoa(dest_addr)<<", ";
						outfile<<"type: "<< typ <<endl;
						//if the packet from the real world, AND, not pinging router itself, send back to proxy
						if(!isBelongSubnet(inet_ntoa(sender_addr), routerIP, 24) && typ!=8){

							//change the destination IP back to the original source IP
							((struct iphdr *)buf)->daddr = sourceIP_in_addr.s_addr;
							((struct iphdr *)buf)->check = 0;
							((struct iphdr *)buf)->check = ip_checksum(buf, 20);
							//forward to proxy
							if ((numbytes = sendto(sockfd, buf, numbytes, 0,
								&sa_proxy, sa_proxy_len)) == -1) {
							perror("talker: sendto");
							exit(1);
							}
						}

					}

					//reading from proxy's UDP socket
					if(FD_ISSET(sockfd, &rfds))
						goto HANDLE_FROM_PROXY;
					else{
						cout<<"continue"<<endl;
						continue;
					}
				}
			}else{
				//stage 2: only deal with packet from proxy
				goto HANDLE_FROM_PROXY;
			}

HANDLE_FROM_PROXY:
			//receive packet from proxy
			//stage 2 goes here...
			if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
					&sa_proxy, &sa_proxy_len)) == -1) {
								perror("recvfrom");
								exit(1);
					}
			buf[numbytes] = '\0';

			for(int j=0;j<numbytes;j++) printf("%x",buf[j]);
			printf("\n");

			struct iphdr *ip_header = (struct iphdr *)buf;
			struct in_addr sender_addr; sender_addr.s_addr=ip_header->saddr;
			struct in_addr dest_addr; dest_addr.s_addr=ip_header->daddr;
			struct icmphdr *icmp_header = (struct icmphdr *)(buf+20);
			int typ = icmp_header->type;

			memcpy(&sourceIP_in_addr, &sender_addr, sizeof(sender_addr));	//store the source IP for further use

			outfile<<"ICMP from port: "<<((struct sockaddr_in*)&sa_proxy)->sin_port
					<<", src: "	<<inet_ntoa(sender_addr);
			outfile<<", dst: "	<<inet_ntoa(dest_addr)
					<<", ";
			outfile<<"type: "<< typ <<endl;

			if(stage>2){
				//stage 3 adds to 2 goes here...
				if(isBelongSubnet(inet_ntoa(dest_addr), routerIP, 24)){
					//if destination IP is in router subnet, reply
					cout<<"no way to get here!"<<endl;
				}else{
					//if in stage 3, destination IP is not in router subnet, routes to real world
					cout<<"not in the subnet:"<<"isBelongSubnet() = "
							<<isBelongSubnet(inet_ntoa(dest_addr), routerIP, 24)<<"dest_addr="<<inet_ntoa(dest_addr)
							<<"routerIP="<<routerIP<<endl;
//					1.switch IP address
//					ip_header->saddr=routerIP_in_addr.s_addr;
//					2.calculate ip checksum
//					ip_header->check = 0;
//					ip_header->check=ip_checksum(ip_header, 20);

					//send it to real world
					struct addrinfo hints, *res;
					int rv;
					memset(&hints, 0, sizeof hints);
				    hints.ai_family=AF_UNSPEC;	//ipv4 or ipv6
				    hints.ai_socktype=0;
				    hints.ai_flags=AI_CANONNAME;
				    if ((rv = getaddrinfo(inet_ntoa(dest_addr), NULL, &hints, &res)) != 0) {
				        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
				        return 1;
				    }
				    struct msghdr msg;
				    struct iovec iov[1];

				    iov[0].iov_base = buf+20;	//no ip header is needed
				    iov[0].iov_len = numbytes-20;
				    msg.msg_name = res->ai_addr;
				    msg.msg_namelen = res->ai_addrlen;
				    msg.msg_iov = iov;
				    msg.msg_iovlen = 1;
				    msg.msg_control = NULL;
				    msg.msg_controllen = 0;
				    msg.msg_flags = 0;

					for(int j=0;j<numbytes;j++) printf("%x",buf[j]);
					printf("\n");

					if ((numbytes = sendmsg(rawsockfd, &msg, 0)) == -1) {
					perror("rawsocket: sendmsg");
					exit(1);
					}
					cout<<"raw socket send "<<numbytes<<" bytes!"<<endl;
					freeaddrinfo(res); // free the linked-list
				}
			}else{
				//stage 2...
				//create ECHO_REPLY packet
				createIcmpRely(buf);
//				//1.switch IP address
//				ip_header->daddr=sender_addr.s_addr;
//				ip_header->saddr=dest_addr.s_addr;
//				//2.change icmp message type
//				icmp_header->type=0;
//				//3.calculate icmp checksum
//				icmp_header->checksum=0;
//				icmp_header->checksum=ip_checksum(icmp_header, 8);
//				ip_header->check = 0;
//				ip_header->check=ip_checksum(ip_header, 20);

//				//send it back to proxy
				if ((numbytes = sendto(sockfd, buf, MAXDATASIZE-1, 0,
					&sa_proxy, sa_proxy_len)) == -1) {
				perror("talker: sendto");
				exit(1);
				}
			}
		}

EXIT_CHILD:
			outfile.close();
			close(sockfd);
			exit(0);

    }else{
    	//parent process (proxy) here...
    	char* p;
    	char router_port[6];
//    	unsigned short int port_num_router;

    	//receive from routers, check if it's up
		printf("waiting for routers...\n");
    	for(i=0;i<num_routers;){
			if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
					&sa, &sa_len)) == -1) {
						perror("recvfrom");
						exit(1);
			}
			buf[numbytes] = '\0';
			//handleing the "up" msg
			p = strtok(buf, "#");
			if(!strcmp(p, "I am up")){
				p = strtok(NULL, "#");
				routerID = atoi(p);
				outfile<<"router "<<routerID<<", ";
				p = strtok(NULL, "#");
				outfile<<"pid: "<<atoi(p)<<", ";
				p = strtok(NULL, "#");
				strcpy(router_port,p);
				outfile<<"port: "<<atoi(p)<<std::endl;
				i++;

				memcpy(&sa_router[routerID-1], &sa, sizeof sa);
				sa_router_len[routerID-1] = sa_len;
			}
    	}


		//router is up


		if(stage==1){
			//stage 1 ends, exit
			outfile.close();
			close(sockfd);
			return 0;
		}else if(stage>1){
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

		    FD_ZERO(&rfds);

		    while(1){
//		    	struct timeval tv;
//		    	/* Wait up to 20 seconds. */
//			    tv.tv_sec = 20;
//			    tv.tv_usec = 0;

			    //select() will change the fd_set each time it is called successfully
		    	/* Watch tun_fd and sockfd to see when they have input. */
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
						buf[nread]='\0';
						struct iphdr *ip_header = (struct iphdr *)buf;
						struct in_addr sender_addr; sender_addr.s_addr=ip_header->saddr;
						struct in_addr dest_addr; dest_addr.s_addr=ip_header->daddr;
						struct icmphdr *icmp_header = (struct icmphdr *)(buf+sizeof(struct iphdr));
						int typ = icmp_header->type;

						outfile<<"ICMP from tunnel, src: "<<inet_ntoa(sender_addr);
						outfile<<", dst: "	<<inet_ntoa(dest_addr)<<", ";
						outfile<<"type: "<< typ <<endl;

						ip_header->check = 0;
						ip_header->check = ip_checksum(buf,10);

						//forward tunnel's packets to router
						/* hash the destination IP address across the routers that are provided
						 * treat the destination IP address as a unsigned 32-bit number and take it MOD the number of routers, then add one.
						 * (So with 4 routers, IP address 1.0.0.5 will map to 2, because 16,777,221 MOD 4 is 1.)
						 */
						routerID = ntohl(dest_addr.s_addr)%num_routers+1;

						if ((numbytes = sendto(sockfd, buf, nread, 0,
							&(sa_router[routerID-1]), sa_router_len[routerID-1])) == -1) {
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
