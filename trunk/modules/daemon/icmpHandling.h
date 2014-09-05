/**
 * @file icmpHandling.h
 *
 *  @date Nov 28, 2010
 *  @author Jonathan Reed
 */

#ifndef ICMPHANDLING_H_
#define ICMPHANDLING_H_

#include "daemon_internal.h"

#include <linux/icmp.h>

#define MAX_DATA_PER_ICMP 4096

int icmp_proto_register(struct fins_module *module);
int socket_icmp_test(int domain, int type, int protocol);
void socket_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int domain);
void daemon_in_fdf_icmp(struct fins_module *module, struct finsFrame *ff, uint32_t family, struct sockaddr_storage *src_addr, struct sockaddr_storage *dst_addr);
void daemon_in_error_icmp(struct fins_module *module, struct finsFrame *ff, uint32_t family, struct sockaddr_storage *src_addr,
		struct sockaddr_storage *dst_addr);

void bind_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, struct sockaddr_storage *addr);
void listen_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int backlog);
void connect_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, struct sockaddr_storage *addr, int flags);
void accept_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, uint64_t sock_id_new, int sock_index_new, int flags);
void getname_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int peer);
void ioctl_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, uint32_t cmd, int buf_len, uint8_t *buf);
void sendmsg_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, uint32_t data_len, uint8_t *data, uint32_t flags, int addr_len,
		struct sockaddr_storage *dest_addr);
void recvmsg_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int buf_len, uint32_t msg_controllen, int flags);
void getsockopt_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int level, int optname, int optlen, uint8_t *optval);
void setsockopt_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int level, int optname, int optlen, uint8_t *optval);
void release_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr);
void poll_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, uint32_t events);
void mmap_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr);
void socketpair_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr);
void shutdown_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int how);
void close_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr);
void sendpage_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr);

//TODO fix usage
void poll_in_icmp(struct daemon_call *call, struct fins_module *module, uint32_t *flags);
uint32_t recvmsg_in_icmp(struct daemon_call *call, struct fins_module *module, metadata *meta, uint32_t data_len, uint8_t *data, struct sockaddr_storage *addr,
		uint32_t flags);

void recvmsg_timeout_icmp(struct fins_module *module, struct daemon_call *call);

#endif /* ICMPHANDLING_H_ */
