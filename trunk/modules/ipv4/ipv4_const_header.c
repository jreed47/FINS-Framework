/*
 * IP4_const_prelim_header.c
 *
 *  Created on: Jun 24, 2010
 *      Author: rado
 */

#include "ipv4_internal.h"

void ipv4_const_header(struct fins_module *module, struct ip4_packet *packet, uint32_t source,
		uint32_t destination, uint8_t protocol) {
	struct ipv4_data *md = (struct ipv4_data *) module->data;

	packet->ip_verlen = IP4_VERSION << 4;
	packet->ip_verlen |= IP4_MIN_HLEN / 4;
	packet->ip_dif = 0;
	packet->ip_id = htons(md->unique_id++);
	packet->ip_ttl = IP4_INIT_TTL;
	packet->ip_proto = protocol;

	packet->ip_src = htonl(source);
	packet->ip_dst = htonl(destination);
	/** if destination has been found using gethostbyname
	 * no need to reverese because it will be already in net format
	 */
	//packet->ip_dst = (destination);
}
