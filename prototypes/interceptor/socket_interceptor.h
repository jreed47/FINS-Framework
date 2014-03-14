/*
 * @file socket_interceptor.h
 *
 *  @date Oct 21, 2010
 *      @author Abdallah Abdallah
 */

#ifndef MYSOCKETSTUB_TEST1_H_
#define MYSOCKETSTUB_TEST1_H_


#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>

#include <signal.h>

//CHANGED mrd015 !!!!!
#ifndef BUILD_FOR_ANDROID
	#include <execinfo.h> 
#endif

#include <semaphore.h>

//ADDED mrd015 !!!!!
#ifdef BUILD_FOR_ANDROID
	#include <linux/sem.h> 
#else
	#include <sys/sem.h> 
#endif

#include <pthread.h>    /* POSIX Threads */

/*additional headers for testing */
#include "finsdebug.h"
//#include "arp.c"

/* to handle forking + pipes operations */
#include <unistd.h>
#include <sys/types.h>

#define MAX_sockets 100

struct socketUniqueID {

	int processID;
	int socketDesc;
	int pipeDesc;
};

struct socketIdentifier {

	pid_t processID;
	int socketDesc;
	int fakeID;
	char semaphore_name[30];
	char asemaphore_name[30];

	sem_t *s; /** Named semaphore to protect the client named pipe between interceptor and Jinni */
	sem_t *as; /** Additional named semaphore to force order between reading and writing to the pipe */

};

/** every global variables needs a semaphore to protect it
 * in case the library copu became shared among more than one application
 *
 * */

struct socketIdentifier FinsHistory[MAX_sockets];

//ADDED mrd015 !!!!!
#ifdef BUILD_FOR_ANDROID
	//#define FINS_TMP_ROOT "/data/data/fins"
	#define FINS_TMP_ROOT "/dev/fins"
#else
	#define FINS_TMP_ROOT "/tmp/fins"
#endif

#define MAIN_SOCKET_CHANNEL FINS_TMP_ROOT "/mainsocket_channel"
#define CLIENT_CHANNEL_TX FINS_TMP_ROOT "/processID_%d_TX_%d"
#define CLIENT_CHANNEL_RX FINS_TMP_ROOT "/processID_%d_RX_%d"

/** The Global socket channel descriptor is used to communicate between the socket
 * interceptor and the socket jinni until they exchange the socket UNIQUE ID, then a separate
 * named pipe gets opened for the newly created socket */
int socket_channel_desc;
sem_t *main_channel_semaphore1;
sem_t *main_channel_semaphore2;

sem_t FinsHistory_semaphore;
char main_sem_name1[] = "main_channel1";
char main_sem_name2[] = "main_channel2";
#define MAX_parallel_processes 10
/** Todo document the work on the differences between the use of processes level semaphores
 * and threads level semaphores! and how each one of them is important and where they were employed
 * in FINS code
 */

#define FINS_LOW_LIMIT	100
#define FINS_HIGH_LIMIT	FINS_LOW_LIMIT+MAX_sockets

struct board {

	int socketID;
	uint16_t srcport;
	uint16_t dstport;
	uint32_t host_IP_netformat;
	int input_fd[2];
	int output_fd[2];

};

typedef enum {
	SS_FREE = 0, /* not allocated                */
	SS_UNCONNECTED, /* unconnected to any socket    */
	SS_CONNECTING, /* in process of connecting     */
	SS_CONNECTED, /* connected to socket          */
	SS_DISCONNECTING
/* in process of disconnecting  */
} socket_state;

/*
 struct socket {
 socket_state   state;

 kmemcheck_bitfield_begin(type);
 short                   type;
 kmemcheck_bitfield_end(type);

 unsigned long           flags;

 struct socket_wq        *wq;

 struct file             *file;
 struct sock             *sk;
 const struct proto_ops  *ops;
 };


 */

/** Socket related calls and their codes */
#define socket_call 1
#define socketpair_call 2
#define bind_call 3
#define getsockname_call 4
#define connect_call 5
#define getpeername_call 6
#define send_call 7
#define recv_call 8
#define sendto_call 9
#define recvfrom_call 10
#define sendmsg_call 11
#define recvmsg_call 12
#define getsockopt_call 13
#define setsockopt_call 14
#define listen_call 15
#define accept_call 16
#define accept4_call 17
#define shutdown_call 18
/** Additional calls
 * To hande special cases
* overwriting the generic functions which write to a socket descriptor
 * in order to make sure that we cover as many applications as possible
 * This range of these functions will start from 30
 */
#define close_call 19
#define write_call 30



#define ACK 	200

void init_socketChannel();
int checkFinsHistory(pid_t target1, int target2);


int searchFinsHistory(pid_t target1, int target2);


int insertFinsHistory(pid_t value1, int value2, int value3);

int removeFinsHistory(pid_t target1, int target2);


ssize_t read_msghdr_from_pipe(int sockfd, struct msghdr *msg);
ssize_t write_msghdr_to_pipe(int sockfd, struct msghdr *msg);

/** The functions pointers related section *
 * Definitions of Functions pointers of the sockets related functions
 * */
int (*_socket)(int domain, int type, int protocol);
int (*_socketpair)(int __domain, int __type, int __protocol, int __fds[2]);
int (*_bind)(int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len);
int (*_getsockname)(int __fd, __SOCKADDR_ARG __addr,
		socklen_t *__restrict __len);
/** TODO Implement connect for UDP as well as send and recv */
int (*_connect)(int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len);
int (*_getpeername)(int __fd, __SOCKADDR_ARG __addr,
		socklen_t *__restrict __len);
ssize_t (*_send)(int __fd, __const void *__buf, size_t __n, int __flags);
ssize_t (*_recv)(int __fd, void *__buf, size_t __n, int __flags);

ssize_t (*_sendto)(int __fd, __const void *__buf, size_t __n, int __flags,
		__CONST_SOCKADDR_ARG __addr, socklen_t __addr_len);

ssize_t (*_recvfrom)(int __fd, void *__restrict __buf, size_t __n, int __flags,
		__SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len);

ssize_t (*_sendmsg)(int __fd, __const struct msghdr *__message, int __flags);

ssize_t (*_recvmsg)(int __fd, struct msghdr *__message, int __flags);

int (*_getsockopt)(int __fd, int __level, int __optname,
		void *__restrict __optval, socklen_t *__restrict __optlen);

int (*_setsockopt)(int __fd, int __level, int __optname,
		__const void *__optval, socklen_t __optlen);

int (*_listen)(int __fd, int __n);

int (*_accept)(int __fd, __SOCKADDR_ARG __addr,
		socklen_t *__restrict __addr_len);

int (*_accept4)(int __fd, __SOCKADDR_ARG __addr,
		socklen_t *__restrict __addr_len, int __flags);

int (*_shutdown)(int __fd, int __how);
/** --------------------------------------------------------------------*/
/**	Functions pointers related to non socket related calls
 *
 */
int (*_close)(int __fd);

ssize_t (*_write)(int __fd, const void *__buf, size_t __count);

/** --------------------------------------------------------------------*/



#ifdef __cplusplus
}
#endif


#endif /* MYSOCKETSTUB_TEST1_H_ */
