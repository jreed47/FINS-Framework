/*
 * interface_internal.h
 *
 *  Created on: Apr 20, 2013
 *      Author: Jonathan Reed
 */

#ifndef INTERFACE_INTERNAL_H_
#define INTERFACE_INTERNAL_H_

#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <finsdebug.h>
#include <finstypes.h>
#include <finstime.h>
#include <metadata.h>
#include <finsqueue.h>

#include "interface.h"

/** Ethernet Stub Variables  */

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

//TODO these definitions need to be gathered
#ifdef BUILD_FOR_ANDROID
#define FINS_TMP_ROOT "/data/local/fins"
#else
#define FINS_TMP_ROOT "/tmp/fins"
#endif

#define INJECT_PATH FINS_TMP_ROOT "/fins_inject"
#define CAPTURE_PATH FINS_TMP_ROOT "/fins_capture"

/* ethernet headers are always exactly 14 bytes [1] */
#define SIZE_ETHERNET 14

/* Ethernet addresses are 6 bytes */
#define ETHER_ADDR_LEN	6

#define ETH_TYPE_IP4  0x0800
#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IP6  0x86dd

#define ETH_FRAME_LEN_MAX 10000 //1538
/* Ethernet header */
struct sniff_ethernet {
	uint8_t ether_dhost[ETHER_ADDR_LEN]; /* destination host address */
	uint8_t ether_shost[ETHER_ADDR_LEN]; /* source host address */
	u_short ether_type; /* IP? ARP? RARP? etc */
	uint8_t data[1];
};

//vvvvvvvvvvvvvvvvvv ARP/interface stuff
#define INTERFACE_IF_LIST_MAX 256
#define INTERFACE_REQUEST_LIST_MAX (2*65536) //TODO change back to 2^16?
#define INTERFACE_CACHE_TO_DEFAULT 15000
#define INTERFACE_MAC_NULL 0x0
#define INTERFACE_CACHE_LIST_MAX 8192
#define INTERFACE_STORE_LIST_MAX (2*65536)

struct interface_request {
	struct sockaddr_storage src_ip;
	uint64_t src_mac;
	struct finsFrame *ff;
};
int interface_request_ipv4_test(struct interface_request *request, uint32_t *src_ip);
void interface_request_free(struct interface_request *request);

struct interface_cache {
	struct sockaddr_storage ip; //unique id
	uint64_t mac;

	struct linked_list *request_list; //linked list of interface_request structs, representing requests made to resolve the MAC
	uint8_t seeking;
	struct timeval updated_stamp;
};
int interface_cache_ipv4_test(struct interface_cache *cache, uint32_t *ip);
int interface_cache_ipv6_test(struct interface_cache *cache, uint8_t *ip);
int interface_cache_non_seeking_test(struct interface_cache *cache);
void interface_cache_free(struct interface_cache *cache);

struct interface_store {
	uint32_t serial_num;
	uint32_t sent;
	struct interface_cache *cache;
	struct interface_request *request;
};
struct interface_store *interface_store_create(uint32_t serial_num, uint32_t sent, struct interface_cache *cache, struct interface_request *request);
int interface_store_serial_test(struct interface_store *store, uint32_t *serial_num);
int interface_store_request_test(struct interface_store *store, struct interface_request *request);
void interface_store_free(struct interface_store *store);
//^^^^^^^^^^^^^^^^^^ ARP/interface stuff

#define MAC_ADDR_LEN (6)
#define MAC_STR_LEN (6*2+5)

struct interface_interface_info {
	uint8_t name[IFNAMSIZ];
	uint8_t mac[2*MAC_ADDR_LEN];
	//uint64_t mac; //should work but doesn't
};

#define INTERFACE_INFO_MIN_SIZE (sizeof(uint32_t))
#define INTERFACE_INFO_SIZE(ii_num) (sizeof(uint32_t) + ii_num * sizeof(struct interface_interface_info))

struct interface_to_inject_hdr {
	uint32_t ii_num;
	struct interface_interface_info iis[INTERFACE_IF_LIST_MAX];
};

#define INTERFACE_LIB "interface"
#define INTERFACE_MAX_FLOWS 3
#define INTERFACE_FLOW_IPV4 0
#define INTERFACE_FLOW_ARP 	1
#define INTERFACE_FLOW_IPV6	2 //TODO add eventually
struct interface_data {
	struct linked_list *link_list; //linked list of link_record structs, representing links for this module
	uint32_t flows_num;
	struct fins_module_flow flows[INTERFACE_MAX_FLOWS];

	pthread_t switch_to_interface_thread;
	pthread_t capturer_to_interface_thread;

	int capture_fd;
	int inject_fd;

	struct linked_list *if_list; //linked list of if_record structs, representing all interfaces
	struct if_record *if_loopback;
	struct if_record *if_main;
	struct linked_list *cache_list; //linked list of interface_cache structs, representing internal cache for this module
	struct linked_list *store_list; //linked list of interface_store structs, holding FDF waiting on MAC addr to send
};

int interface_init(struct fins_module *module, metadata_element *params, struct envi_record *envi);
int interface_run(struct fins_module *module, pthread_attr_t *attr);
int interface_pause(struct fins_module *module);
int interface_unpause(struct fins_module *module);
int interface_shutdown(struct fins_module *module);
int interface_release(struct fins_module *module);

void interface_get_ff(struct fins_module *module);
void interface_fcf(struct fins_module *module, struct finsFrame *ff);
void interface_read_param(struct fins_module *module, struct finsFrame *ff);
void interface_set_param(struct fins_module *module, struct finsFrame *ff);
void interface_exec(struct fins_module *module, struct finsFrame *ff);
void interface_exec_reply(struct fins_module *module, struct finsFrame *ff);
void interface_exec_reply_get_addr(struct fins_module *module, struct finsFrame *ff);

void interface_out_fdf(struct fins_module *module, struct finsFrame *ff);
void interface_out_ipv4(struct fins_module *module, struct finsFrame *ff);
void interface_out_arp(struct fins_module *module, struct finsFrame *ff);
void interface_out_ipv6(struct fins_module *module, struct finsFrame *ff);
int interface_inject_pdu(int fd, uint32_t pduLength, uint8_t *pdu, uint64_t dst_mac, uint64_t src_mac, uint32_t ether_type);
int interface_send_request(struct fins_module *module, uint32_t src_ip, uint32_t dst_ip, uint32_t serial_num);

#define INTERFACE_EXEC_GET_ADDR 0

//don't use 0
#define INTERFACE_READ_PARAM_FLOWS MOD_READ_PARAM_FLOWS
#define INTERFACE_READ_PARAM_LINKS MOD_READ_PARAM_LINKS
#define INTERFACE_READ_PARAM_DUAL MOD_READ_PARAM_DUAL

#define INTERFACE_SET_PARAM_FLOWS MOD_SET_PARAM_FLOWS
#define INTERFACE_SET_PARAM_LINKS MOD_SET_PARAM_LINKS
#define INTERFACE_SET_PARAM_DUAL MOD_SET_PARAM_DUAL

#define INTERFACE_ERROR_TTL 0
#define INTERFACE_ERROR_DEST_UNREACH 1
#define INTERFACE_ERROR_GET_ADDR 2

#endif /* INTERFACE_INTERNAL_H_ */
