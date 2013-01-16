/**
 * @file IP4.c
 * @brief FINS module - Internet Protocol version 4
 *
 * Main function of the module.
 */

#include "ipv4.h"
#include <queueModule.h>

#include <switch.h>
struct fins_proto_module ipv4_proto = { .module_id = IPV4_ID, .name = "ipv4", .running_flag = 1, }; //TODO make static?

IP4addr my_ip_addr;
IP4addr my_mask;
IP4addr loopback;
IP4addr loopback_mask;

/*
 IP4addr my_ip_addr;
 IP4addr loopback_ip_addr;
 IP4addr any_ip_addr;
 */

struct ip4_routing_table* routing_table;
struct ip4_packet *construct_packet_buffer;
struct ip4_stats stats;

struct ip4_store *store_list;
uint32_t store_num;

struct ipv4_interface *ipv4_interface_list;
uint32_t ipv4_interface_num;

struct ipv4_cache *ipv4_cache_list; //The list of current cache we have
uint32_t ipv4_cache_num;


int ipv4_to_switch(struct finsFrame *ff) {
	return module_to_switch(&ipv4_proto, ff);
}

void *switch_to_ipv4(void *local) {
	PRINT_CRITICAL("Entered");

	while (ipv4_proto.running_flag) {
		IP4_receive_fdf();
		PRINT_DEBUG("");
	}

	PRINT_CRITICAL("Exited");
	pthread_exit(NULL);
}

struct ip4_store *store_create(uint32_t serial_num, struct finsFrame *ff, uint8_t *pdu) {
	PRINT_DEBUG("Entered: serial_num=%u, ff=%p, pdu=%p", serial_num, ff, pdu);

	struct ip4_store *store = (struct ip4_store *) fins_malloc(sizeof(struct ip4_store));
	store->next = NULL;

	store->serial_num = serial_num;
	store->ff = ff;
	store->pdu = pdu;

	PRINT_DEBUG("Exited: serial_num=%u, ff=%p, pdu=%p, store=%p", serial_num, ff, pdu, store);
	return store;
}

void store_free(struct ip4_store *store) {
	PRINT_DEBUG("Entered: store=%p", store);

	if (store->pdu) {
		PRINT_DEBUG("Freeing pdu=%p", store->pdu);
		free(store->pdu);
	}

	if (store->ff)
		freeFinsFrame(store->ff);

	free(store);
}

int store_list_insert(struct ip4_store *store) {
	PRINT_DEBUG("Entered: store=%p", store);

	if (store_list == NULL) {
		store_list = store;
	} else {
		struct ip4_store *temp = store_list;

		while (temp->next != NULL) {
			temp = temp->next;
		}

		temp->next = store;
		store->next = NULL;
	}

	store_num++;
	PRINT_DEBUG("Exited: store=%p, store_num=%u", store, store_num);
	return 1;
}

struct ip4_store *store_list_find(uint32_t serial_num) {
	PRINT_DEBUG("Entered: serial_num=%u", serial_num);

	struct ip4_store *temp = store_list;

	while (temp != NULL && temp->serial_num != serial_num) {
		temp = temp->next;
	}

	PRINT_DEBUG("Exited: serial_num=%u, store=%p", serial_num, temp);
	return temp;
}

void store_list_remove(struct ip4_store *store) {
	PRINT_DEBUG("Entered: store=%p", store);

	if (store_list == NULL) {
		PRINT_DEBUG("Exited: store=%p, store_num=%u", store, store_num);
		return;
	}

	if (store_list == store) {
		store_list = store_list->next;
		store_num--;
		PRINT_DEBUG("Exited: store=%p, store_num=%u", store, store_num);
		return;
	}

	struct ip4_store *temp = store_list;
	while (temp->next != NULL) {
		if (temp->next == store) {
			temp->next = store->next;
			store_num--;
			PRINT_DEBUG("Exited: store=%p, store_num=%u", store, store_num);
			return;
		}
		temp = temp->next;
	}

	PRINT_DEBUG("Exited: store=%p, store_num=%u", store, store_num);
}

int store_list_is_empty(void) {
	return store_num == 0;
}

int store_list_has_space(void) {
	return store_num < IP4_STORE_LIST_MAX;
}

//################ ARP/interface stuff //TODO move to common?
struct ipv4_interface *ipv4_interface_create(uint64_t mac_addr, uint32_t ip_addr) {
	PRINT_DEBUG("Entered: mac=%llx, ip=%u", mac_addr, ip_addr);

	struct ipv4_interface *interface = (struct ipv4_interface *) fins_malloc(sizeof(struct ipv4_interface));
	interface->next = NULL;

	interface->mac_addr = mac_addr;
	interface->ip_addr = ip_addr;

	PRINT_DEBUG("Exited: mac=%llx, ip=%u, interface=%p", mac_addr, ip_addr, interface);
	return interface;
}

void ipv4_interface_free(struct ipv4_interface *interface) {
	PRINT_DEBUG("Entered: interface=%p", interface);

	free(interface);
}

int ipv4_interface_list_insert(struct ipv4_interface *interface) {
	PRINT_DEBUG("Entered: interface=%p", interface);

	interface->next = ipv4_interface_list;
	ipv4_interface_list = interface;

	ipv4_interface_num++;
	return 1;
}

struct ipv4_interface *ipv4_interface_list_find(uint32_t ip_addr) {
	PRINT_DEBUG("Entered: ip=%u", ip_addr);

	struct ipv4_interface *interface = ipv4_interface_list;

	while (interface != NULL && interface->ip_addr != ip_addr) {
		interface = interface->next;
	}

	PRINT_DEBUG("Exited: ip=%u, interface=%p", ip_addr, interface);
	return interface;
}

void ipv4_interface_list_remove(struct ipv4_interface *interface) {
	PRINT_DEBUG("Entered: interface=%p", interface);

	if (ipv4_interface_list == NULL) {
		return;
	}

	if (ipv4_interface_list == interface) {
		ipv4_interface_list = ipv4_interface_list->next;
		ipv4_interface_num--;
		return;
	}

	struct ipv4_interface *temp = ipv4_interface_list;
	while (temp->next != NULL) {
		if (temp->next == interface) {
			temp->next = interface->next;
			ipv4_interface_num--;
			return;
		}
		temp = temp->next;
	}
}

int ipv4_interface_list_is_empty(void) {
	return ipv4_interface_num == 0;
}

int ipv4_interface_list_has_space(void) {
	return ipv4_interface_num < IPV4_INTERFACE_LIST_MAX;
}

struct ipv4_request *ipv4_request_create(struct finsFrame *ff, uint64_t src_mac, uint32_t src_ip) {
	PRINT_DEBUG("Entered: ff=%p, mac=%llx, ip=%u", ff, src_mac, src_ip);

	struct ipv4_request *request = (struct ipv4_request *) fins_malloc(sizeof(struct ipv4_request));
	request->next = NULL;

	request->ff = ff;
	request->src_mac = src_mac;
	request->src_ip = src_ip;

	PRINT_DEBUG("Exited: ff=%p, mac=%llx, ip=%u, request=%p", ff, src_mac, src_ip, request);
	return request;
}

void ipv4_request_free(struct ipv4_request *request) {
	PRINT_DEBUG("Entered: request=%p", request);

	if (request->ff) {
		freeFinsFrame(request->ff);
	}

	free(request);
}

struct ipv4_request_list *ipv4_request_list_create(uint32_t max) {
	PRINT_DEBUG("Entered: max=%u", max);

	struct ipv4_request_list *request_list = (struct ipv4_request_list *) fins_malloc(sizeof(struct ipv4_request_list));
	request_list->max = max;
	request_list->len = 0;

	request_list->front = NULL;
	request_list->end = NULL;

	PRINT_DEBUG("Exited: max=%u, request_list=%p", max, request_list);
	return request_list;
}

void ipv4_request_list_append(struct ipv4_request_list *request_list, struct ipv4_request *request) {
	PRINT_DEBUG("Entered: request_list=%p, request=%p", request_list, request);

	request->next = NULL;
	if (ipv4_request_list_is_empty(request_list)) {
		//queue empty
		request_list->front = request;
	} else {
		//node after end
		request_list->end->next = request;
	}
	request_list->end = request;
	request_list->len++;
}

struct ipv4_request *ipv4_request_list_find(struct ipv4_request_list *request_list, uint32_t src_ip) {
	PRINT_DEBUG("Entered: request_list=%p, ip=%u", request_list, src_ip);

	struct ipv4_request *request = request_list->front;
	while (request != NULL && request->src_ip != src_ip) {
		request = request->next;
	}

	PRINT_DEBUG("Exited: request_list=%p, ip=%u, request=%p", request_list, src_ip, request);
	return request;
}

struct ipv4_request *ipv4_request_list_remove_front(struct ipv4_request_list *request_list) {
	PRINT_DEBUG("Entered: request_list=%p", request_list);

	struct ipv4_request *request = request_list->front;

	request_list->front = request->next;
	request_list->len--;

	PRINT_DEBUG("Exited: request_list=%p, request=%p", request_list, request);
	return request;
}

int ipv4_request_list_is_empty(struct ipv4_request_list *request_list) {
	//return request_list->front == NULL;
	return request_list->len == 0;
}

int ipv4_request_list_has_space(struct ipv4_request_list *request_list) {
	return request_list->len < request_list->max;
}

void ipv4_request_list_free(struct ipv4_request_list *request_list) {
	PRINT_DEBUG("Entered: request_list=%p", request_list);

	struct ipv4_request *request = request_list->front;
	while (!ipv4_request_list_is_empty(request_list)) {
		request = ipv4_request_list_remove_front(request_list);
		ipv4_request_free(request);
	}

	free(request_list);
}

struct ipv4_cache *ipv4_cache_create(uint32_t ip_addr) {
	PRINT_DEBUG("Entered: ip=%u", ip_addr);

	struct ipv4_cache *cache = (struct ipv4_cache *) fins_malloc(sizeof(struct ipv4_cache));
	cache->next = NULL;

	cache->mac_addr = IPV4_MAC_NULL;
	cache->ip_addr = ip_addr;

	cache->request_list = ipv4_request_list_create(IPV4_REQUEST_LIST_MAX);

	cache->seeking = 0;
	memset(&cache->updated_stamp, 0, sizeof(struct timeval));

	PRINT_DEBUG("Exited: ip=%u, cache=%p", ip_addr, cache);
	return cache;
}

void ipv4_cache_free(struct ipv4_cache *cache) {
	PRINT_DEBUG("Entered: cache=%p", cache);

	if (cache->request_list) {
		ipv4_request_list_free(cache->request_list);
	}

	free(cache);
}
int ipv4_cache_list_insert(struct ipv4_cache *cache) {
	PRINT_DEBUG("Entered: cache=%p", cache);

	if (ipv4_cache_list == NULL) {
		ipv4_cache_list = cache;
	} else {
		struct ipv4_cache *temp = ipv4_cache_list;

		while (temp->next != NULL) {
			temp = temp->next;
		}

		temp->next = cache;
		cache->next = NULL;
	}

	ipv4_cache_num++;
	return 1;
}

struct ipv4_cache *ipv4_cache_list_find(uint32_t ip_addr) {
	PRINT_DEBUG("Entered: ip=%u", ip_addr);

	struct ipv4_cache *cache = ipv4_cache_list;
	while (cache != NULL && cache->ip_addr != ip_addr) {
		cache = cache->next;
	}

	PRINT_DEBUG("Exited: ip=%u, cache=%p", ip_addr, cache);
	return cache;
}

void ipv4_cache_list_remove(struct ipv4_cache *cache) {
	PRINT_DEBUG("Entered: cache=%p", cache);

	if (ipv4_cache_list == NULL) {
		return;
	}

	if (ipv4_cache_list == cache) {
		ipv4_cache_list = ipv4_cache_list->next;
		ipv4_cache_num--;
		return;
	}

	struct ipv4_cache *temp = ipv4_cache_list;
	while (temp->next != NULL) {
		if (temp->next == cache) {
			temp->next = cache->next;
			ipv4_cache_num--;
			return;
		}
		temp = temp->next;
	}
}

struct ipv4_cache *ipv4_cache_list_remove_first_non_seeking(void) {
	PRINT_DEBUG("Entered");

	if (ipv4_cache_list == NULL) {
		return NULL;
	}

	struct ipv4_cache *cache = ipv4_cache_list;
	if (cache->seeking) {
		struct ipv4_cache *next;

		while (cache->next != NULL) {
			if (cache->next->seeking) {
				cache = cache->next;
			} else {
				next = cache->next;
				cache->next = next->next;

				ipv4_cache_num--;
				PRINT_DEBUG("Exited: cache=%p", next);
				return next;
			}
		}

		PRINT_DEBUG("Exited: cache=%p", NULL);
		return NULL; //TODO change to head?
	} else {
		ipv4_cache_list = cache->next;
	}

	ipv4_cache_num--;
	PRINT_DEBUG("Exited: cache=%p", cache);
	return cache;
}

int ipv4_cache_list_is_empty(void) {
	return ipv4_cache_num == 0;
}

int ipv4_cache_list_has_space(void) {
	return ipv4_cache_num < IPV4_CACHE_LIST_MAX;
}
//################


void ipv4_init(void) {
	PRINT_CRITICAL("Entered");
	ipv4_proto.running_flag = 1;

	module_create_ops(&ipv4_proto);
	module_register(&ipv4_proto);

	store_list = NULL;
	store_num = 0;

	/* find a way to get the IP of the desired interface automatically from the system
	 * or from a configuration file
	 */

	//my_ip_addr = IP4_ADR_P2H(192, 168, 1, 20);
	//my_ip_addr = IP4_ADR_P2H(172,31,50,160);
	//my_ip_addr = IP4_ADR_P2H(127, 0, 0, 1);
	//my_ip_addr = IP4_ADR_P2H(172, 31, 63, 231);
	//my_ip_addr = IP4_ADR_P2H(172, 31, 53, 114);
	//PRINT_DEBUG("%lu", my_ip_addr);
	//my_mask = IP4_ADR_P2H(255, 255, 255, 0); //TODO move to core/central place
	//ADDED mrd015 !!!!!
#ifndef BUILD_FOR_ANDROID
	IP4_init();
#endif
}

void ipv4_set_interface(uint32_t IP_address, uint32_t mask) {
	my_ip_addr = IP_address;
	my_mask = mask;
}

void ipv4_set_loopback(uint32_t IP_address, uint32_t mask) {
	loopback = IP_address;
	loopback_mask = mask;
}

void ipv4_run(pthread_attr_t *fins_pthread_attr) {
	PRINT_CRITICAL("Entered");

	pthread_create(&switch_to_ipv4_thread, fins_pthread_attr, switch_to_ipv4, fins_pthread_attr);
}

void ipv4_shutdown(void) {
	PRINT_CRITICAL("Entered");
	ipv4_proto.running_flag = 0;
	sem_post(ipv4_proto.event_sem);

	//TODO expand this

	PRINT_CRITICAL("Joining switch_to_ipv4_thread");
	pthread_join(switch_to_ipv4_thread, NULL);
}

void ipv4_release(void) {
	PRINT_CRITICAL("Entered");
	module_unregister(ipv4_proto.module_id);

	//TODO free all module related mem

	struct ip4_store *store;
	while (!store_list_is_empty()) {
		store = store_list;
		store_list_remove(store);
		store_free(store);
	}

	struct ip4_routing_table *table;
	while (routing_table) {
		table = routing_table;
		routing_table = routing_table->next_entry;

		free(table);
	}

	module_destroy_ops(&ipv4_proto);
}
