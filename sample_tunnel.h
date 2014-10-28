/*
 * sample_tunnel.h
 *
 *  Created on: Oct 1, 2014
 *      Author: csci551
 */

#ifndef SAMPLE_TUNNEL_H_
#define SAMPLE_TUNNEL_H_

int tun_alloc(char *dev, int flags);
int tunnel_reader(int tun_fd, char* buffer);

#endif /* SAMPLE_TUNNEL_H_ */
