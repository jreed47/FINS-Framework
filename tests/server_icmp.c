/* udpserver.c */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>

//--------------------------------------------------- //temp stuff to cross compile, remove/implement better eventual?
#ifndef POLLRDNORM
#define POLLRDNORM POLLIN
#endif

#ifndef POLLRDBAND
#define POLLRDBAND POLLIN
#endif

#ifndef POLLWRNORM
#define POLLWRNORM POLLOUT
#endif

#ifndef POLLWRBAND
#define POLLWRBAND POLLOUT
#endif
//---------------------------------------------------

#define xxx(a,b,c,d) 	(16777216ul*(a) + (65536ul*(b)) + (256ul*(c)) + (d))

int i = 0;

typedef unsigned long IP4addr; /*  internet address			*/

struct ip4_packet {
	uint8_t ip_verlen; /* IP version & header length (in longs)*/
	uint8_t ip_dif; /* differentiated service			*/
	uint16_t ip_len; /* total packet length (in octets)	*/
	uint16_t ip_id; /* datagram id				*/
	uint16_t ip_fragoff; /* fragment offset (in 8-octet's)	*/
	uint8_t ip_ttl; /* time to live, in gateway hops	*/
	uint8_t ip_proto; /* IP protocol */
	uint16_t ip_cksum; /* header checksum 			*/
	IP4addr ip_src; /* IP address of source			*/
	IP4addr ip_dst; /* IP address of destination		*/
	uint8_t ip_data[1]; /* variable length data			*/
};

struct icmp_packet {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t param_1;
	uint16_t param_2;
	uint8_t data[1];
};

void termination_handler(int sig) {
	printf("\n**********Number of packers that have been received = %d *******\n", i);
	fflush(stdout);
	exit(2);
}

int main(int argc, char *argv[]) {

	printf(
			"EACCES=%d EACCES=%d EPERM=%d EADDRINUSE=%d EAFNOSUPPORT=%d EAGAIN=%d EALREADY=%d EBADF=%d ECONNREFUSED=%d EFAULT=%d EINPROGRESS=%d EINTR=%d EISCONN=%d ENETUNREACH=%d ENOTSOCK=%d ETIMEDOUT=%d\n",
			EACCES, EACCES, EPERM, EADDRINUSE, EAFNOSUPPORT, EAGAIN, EALREADY, EBADF, ECONNREFUSED, EFAULT, EINPROGRESS, EINTR, EISCONN, ENETUNREACH, ENOTSOCK,
			ETIMEDOUT);

	uint16_t port;

	(void) signal(SIGINT, termination_handler);

	int sock;
	socklen_t addr_len = sizeof(struct sockaddr);
	int bytes_read;
	char recv_data[4000];
	int ret;
	pid_t pID = 0;

	struct sockaddr_in server_addr;
	struct sockaddr_in *client_addr;

	if (argc > 1) {
		port = atoi(argv[1]);
	} else {
		port = 45454;
	}

	client_addr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	//if ((sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0)) == -1) {
	if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) {
		//if ((sock = socket(39, SOCK_DGRAM, 0)) == -1) {
		perror("Socket");
		exit(1);
	}
	printf("Provided with sock=%d\n", sock);

	int val = 0;
	setsockopt(sock, SOL_IP, IP_MTU_DISCOVER, &val, sizeof(val));
	val = 1;
	setsockopt(sock, SOL_SOCKET, SO_TIMESTAMP, &val, sizeof(val));
	val = 1;
	setsockopt(sock, SOL_SOCKET, SO_TIMESTAMP, &val, sizeof(val));
	val = 1;
	setsockopt(sock, SOL_IP, IP_RECVTTL, &val, sizeof(val));
	val = 0;
	setsockopt(sock, SOL_IP, IP_RECVERR, &val, sizeof(val));

	//fcntl64(3, F_SETFL, O_RDONLY|O_NONBLOCK) = 0
	//fstat64(1, {st_dev=makedev(0, 11), st_ino=3, st_mode=S_IFCHR|0620, st_nlink=1, st_uid=1000, st_gid=5, st_blksize=1024, st_blocks=0, st_rdev=makedev(136, 0), st_atime=2012/10/16-22:31:09, st_mtime=2012/10/16-22:31:09, st_ctime=2012/10/16-19:33:02}) = 0
	//val = 3;
	//setsockopt(sock, SOL_IP, IP_TTL, &val, sizeof(val));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	//server_addr.sin_addr.s_addr = xxx(127,0,0,1);
	//server_addr.sin_addr.s_addr = xxx(114,53,31,172);
	//server_addr.sin_addr.s_addr = xxx(172,31,54,87);
	//server_addr.sin_addr.s_addr = xxx(192,168,1,20);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	//server_addr.sin_addr.s_addr = INADDR_LOOPBACK;
	server_addr.sin_addr.s_addr = htonl(server_addr.sin_addr.s_addr);
	bzero(&(server_addr.sin_zero), 8);

	printf("Binding to server: pID=%d addr=%s:%d, netw=%u\n", pID, inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port), server_addr.sin_addr.s_addr);
	if (bind(sock, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1) {
		perror("Bind");
		printf("Failure");
		exit(1);
	}

	addr_len = sizeof(struct sockaddr);

	printf("\n UDPServer Waiting for client at server_addr=%s/%d, netw=%u", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port),
			server_addr.sin_addr.s_addr);
	fflush(stdout);

	i = 0;

	int nfds = 2;
	struct pollfd fds[nfds];
	fds[0].fd = -1;
	fds[0].events = POLLIN | POLLPRI | POLLRDNORM;
	fds[1].fd = sock;
	fds[1].events = POLLIN | POLLPRI | POLLRDNORM;
	//fds[1].events = POLLIN | POLLPRI | POLLOUT | POLLERR | POLLHUP | POLLNVAL | POLLRDNORM | POLLRDBAND | POLLWRNORM | POLLWRBAND;
	printf("\n fd: sock=%d, events=%x", sock, fds[1].events);
	int time = 10000;

	printf("\n POLLIN=%x POLLPRI=%x POLLOUT=%x POLLERR=%x POLLHUP=%x POLLNVAL=%x POLLRDNORM=%x POLLRDBAND=%x POLLWRNORM=%x POLLWRBAND=%x", POLLIN, POLLPRI,
			POLLOUT, POLLERR, POLLHUP, POLLNVAL, POLLRDNORM, POLLRDBAND, POLLWRNORM, POLLWRBAND);
	fflush(stdout);

	int temp = fds[1].events;
	printf("\n POLLIN=%x POLLPRI=%x POLLOUT=%x POLLERR=%x POLLHUP=%x POLLNVAL=%x POLLRDNORM=%x POLLRDBAND=%x POLLWRNORM=%x POLLWRBAND=%x val=%d (%x)",
			(temp & POLLIN) > 0, (temp & POLLPRI) > 0, (temp & POLLOUT) > 0, (temp & POLLERR) > 0, (temp & POLLHUP) > 0, (temp & POLLNVAL) > 0,
			(temp & POLLRDNORM) > 0, (temp & POLLRDBAND) > 0, (temp & POLLWRNORM) > 0, (temp & POLLWRBAND) > 0, temp, temp);

	struct timeval tv;

	tv.tv_sec = 0; /* 30 Secs Timeout */
	tv.tv_usec = 0; // Not init'ing this can cause strange errors

	//ret = getsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(struct timeval));
	//printf("\n ret=%d tv.tv_sec=%d tv.tv_usec=%d", ret, tv.tv_sec, tv.tv_usec);
	//fflush(stdout);

	int j = 0;
	while (++j <= 3) {
		//pID = fork();
		if (pID == 0) { // child -- Capture process
			continue;
		} else if (pID < 0) { // failed to fork
			printf("Failed to Fork \n");
			fflush(stdout);
			exit(1);
		} else { // parent
			//port += j - 1;
			break;
		}
	}

	if (pID == 0) {
		//while (1);
	}

	if (pID == 0) {
		j = 0;
		int k = 0;
		while (1) {
			//printf("\n pID=%d poll before", pID);
			//fflush(stdout);
			ret = poll(fds, nfds, time);
			printf("\n poll: pID=%d, ret=%d, revents=%x", pID, ret, fds[ret].revents);
			fflush(stdout);

			if (ret || 0) {
				if (1) {
					printf("\n poll: ret=%d, revents=%x", ret, fds[ret].revents);
					printf("\n POLLIN=%d POLLPRI=%d POLLOUT=%d POLLERR=%d POLLHUP=%d POLLNVAL=%d POLLRDNORM=%d POLLRDBAND=%d POLLWRNORM=%d POLLWRBAND=%d ",
							(fds[ret].revents & POLLIN) > 0, (fds[ret].revents & POLLPRI) > 0, (fds[ret].revents & POLLOUT) > 0,
							(fds[ret].revents & POLLERR) > 0, (fds[ret].revents & POLLHUP) > 0, (fds[ret].revents & POLLNVAL) > 0,
							(fds[ret].revents & POLLRDNORM) > 0, (fds[ret].revents & POLLRDBAND) > 0, (fds[ret].revents & POLLWRNORM) > 0,
							(fds[ret].revents & POLLWRBAND) > 0);
					fflush(stdout);
				}

				if ((fds[ret].revents & (POLLIN | POLLRDNORM)) || 0) {
					bytes_read = recvfrom(sock, recv_data, 4000, 0, (struct sockaddr *) client_addr, &addr_len);
					//bytes_read = recvfrom(sock,recv_data,1024,0,NULL, NULL);
					//bytes_read = recv(sock,recv_data,1024,0);
					if (bytes_read > 0) {
						recv_data[bytes_read] = '\0';
						printf("\n frame=%d, pID=%d, client=%s:%u: len=%d str='%s'", ++k, pID, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port),
								bytes_read, recv_data);
						fflush(stdout);

						if (bytes_read > 28) {
							struct ip4_packet *ipv4_pkt = (struct ip4_packet *) recv_data;
							printf("\n proto=%d", ipv4_pkt->ip_proto);
							fflush(stdout);
							if (ipv4_pkt->ip_proto == IPPROTO_ICMP) {
								struct icmp_packet *icmp_pkt = (struct icmp_packet *) ipv4_pkt->ip_data;
								printf("\n type=%d, code=%d", icmp_pkt->type, icmp_pkt->code);
								fflush(stdout);
								printf("\n data_len=%d, data='%s'", bytes_read - 28, icmp_pkt->data);
								fflush(stdout);
							} else if (ipv4_pkt->ip_proto == IPPROTO_UDP) {

							} else if (ipv4_pkt->ip_proto == IPPROTO_TCP) {

							}
						}

						//bytes_read = sendto(sock, recv_data, 1, 0, (struct sockaddr *) client_addr, sizeof(struct sockaddr_in));

						if ((strcmp(recv_data, "q") == 0) || strcmp(recv_data, "Q") == 0) {
							break;
						}
					} else if (errno != EWOULDBLOCK && errno != EAGAIN) {
						printf("\n Error recv at the Server: ret=%d errno='%s' (%d)\n", bytes_read, strerror(errno), errno);
						perror("Error:");
						fflush(stdout);
						break;
					}
				}
			}
			j++;
			//break;
		}
	} else {
		i = 0;
		while (i < 5) {
			//printf("\n pID=%d recvfrom before", pID);
			//fflush(stdout);
			bytes_read = recvfrom(sock, recv_data, 2000, 0, (struct sockaddr *) client_addr, &addr_len);
			//printf("\n pID=%d recvfrom after", pID);
			//fflush(stdout);
			//bytes_read = recvfrom(sock,recv_data,1024,0,NULL, NULL);
			//bytes_read = recv(sock,recv_data,1024,0);
			if (bytes_read > 0) {
				recv_data[bytes_read] = '\0';
				printf("\n frame=%d, pID=%d, client=%s:%u: said='%s'\n", ++i, pID, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), recv_data);
				fflush(stdout);

				//bytes_read = sendto(sock, recv_data, 1, 0, (struct sockaddr *) client_addr, sizeof(struct sockaddr_in));

				if ((strcmp(recv_data, "q") == 0) || strcmp(recv_data, "Q") == 0) {
					break;
				}
			} else if (errno != EWOULDBLOCK && errno != EAGAIN) {
				printf("\n Error recv at the Server: ret=%d errno='%s' (%d)\n", bytes_read, strerror(errno), errno);
				perror("Error:");
				fflush(stdout);
				break;
			}
		}
	}

	printf("\n After");
	fflush(stdout);
	while (1)
		;

	printf("\n Closing server socket");
	fflush(stdout);
	close(sock);

	printf("\n FIN");
	fflush(stdout);

	while (1)
		;

	return 0;
}

