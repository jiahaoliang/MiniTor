Jiahao Liang

a)Reuse Code: checksum calculation function is from http://blog.csdn.net/qy532846454/article/details/5384086

b)Complete: No. 
	I completed the following parts:
		1. Communicates between tunnel and proxy
		2. Communicates between proxy and router
		3. extracts information from ip header and icmp header
	I failed to complete:
		1. For some reason, router is not able to sent the packet back to proxy after processing.
		2. Fail to create the correct checksum for ECHO_REPLY packet 

c)Portable: I believe my code is portable. Because most of the socket libraries and functions I used are compitible both on 32bit and 64bit systems.

d) Other: To compile, simply use the command make. In Stage 2, proxy will stop if no data from tunnel more than 20 sec, router will stop after receiving exactly 4 packets from proxy.