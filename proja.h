//============================================================================
// Name        : proja.h
// Author      : Jiahao Liang
// Version     :
// Copyright   : Your copyright notice
// Description :
//============================================================================

#ifndef PROJA_H_
#define PROJA_H_

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

#define MAXNUMROUTERS 6
#define MAXDATASIZE 2048
#define MAX(x,y) ((x)>(y))?(x):(y)

//convert IP addr from string format to unsigned long number
//http://electrofriends.com/source-codes/software-programs/c/number-programs/c-program-to-convert-ip-address-to-32-bit-long-int/
unsigned long int ipConv(const char []);

int readConfig (char* filename, int *p_stage, int *p_num_routers);
int createSocket(char address[], int ai_socktype, const char* port, int* p_sockfd, in_port_t* p_sin_port);
int createRawSocket(char address[], int ai_socktype, int service, int* p_sockfd, in_port_t* p_sin_port);
bool isBelongSubnet(const char hostIP[],const char routerIP[], u_int8_t subnetMask);
//__sum16 chcksum(struct icmphdr *icmp_header, int len_in_bytes);
uint16_t ip_checksum(void* vdata,size_t length);

void outputIcmpMsg(char* buf, std::ofstream outfile);
void createIcmpRely(char* buf);



#endif /* PROJA_H_ */
