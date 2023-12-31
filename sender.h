#ifndef __SENDER_H__
#define __SENDER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>
#include "common.h"
#include "util.h"  //util.h
#include "communicate.h"


void init_sender(Sender *, int);
void * run_sender(void *);


/// @brief 设置sender的最早到期时间
/// @param sender 要设置的sender
/// @param expiring_timeval 新时间
/// @return 若更改，则返回1，否则返回0
int set_sender_expiring_timeval(Sender *sender, struct timeval *expiring_timeval);

#endif
