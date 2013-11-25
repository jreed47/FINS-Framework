/**
 * @file icmpHandling.c
 *
 *  @date Nov 28, 2010
 *  @author Abdallah Abdallah
 */

#include "icmpHandling.h"
#include <finstypes.h>

struct daemon_socket_general_ops icmp_general_ops = { .proto = IPPROTO_ICMP, .socket_type_test = socket_icmp_test, .socket_out = socket_out_icmp,
		.daemon_in_fdf = daemon_in_fdf_icmp, .daemon_in_error = daemon_in_error_icmp, };
static struct daemon_socket_out_ops icmp_out_ops = { .socket_out = socket_out_icmp, .bind_out = bind_out_icmp, .listen_out = listen_out_icmp, .connect_out =
		connect_out_icmp, .accept_out = accept_out_icmp, .getname_out = getname_out_icmp, .ioctl_out = ioctl_out_icmp, .sendmsg_out = sendmsg_out_icmp,
		.recvmsg_out = recvmsg_out_icmp, .getsockopt_out = getsockopt_out_icmp, .setsockopt_out = setsockopt_out_icmp, .release_out = release_out_icmp,
		.poll_out = poll_out_icmp, .mmap_out = mmap_out_icmp, .socketpair_out = socketpair_out_icmp, .shutdown_out = shutdown_out_icmp, .close_out =
				close_out_icmp, .sendpage_out = sendpage_out_icmp, };
static struct daemon_socket_in_ops icmp_in_ops = { };
static struct daemon_socket_other_ops icmp_other_ops = { .recvmsg_timeout = recvmsg_timeout_icmp, };

int socket_icmp_test(int domain, int type, int protocol) {
	return type == SOCK_RAW && (protocol == IPPROTO_ICMP || protocol == IPPROTO_IP);
}

void socket_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int domain) {
	PRINT_DEBUG("Entered: hdr=%p, domain=%d", hdr, domain);

	int ret = daemon_sockets_insert(module, hdr->sock_id, hdr->sock_index, SOCK_RAW, IPPROTO_ICMP, &icmp_out_ops, &icmp_in_ops, &icmp_other_ops);
	PRINT_DEBUG("sock_index=%d, ret=%d", hdr->sock_index, ret);

	if (ret) {
		ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
	} else {
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void bind_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, struct sockaddr_storage *addr) {
	PRINT_DEBUG("Entered: hdr=%p", hdr);
	struct daemon_data *md = (struct daemon_data *) module->data;

	if (md->sockets[hdr->sock_index].family != AF_UNSPEC) {
		PRINT_WARN("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EINVAL); //22
		return;
	}

	if (addr->ss_family == AF_INET) {
		uint32_t host_ip = ntohl(addr4_get_ip(addr));
		PRINT_DEBUG("bind address: family=%u, host_ip=%u", AF_INET, host_ip);

		md->sockets[hdr->sock_index].family = AF_INET;
		addr4_set_ip(&md->sockets[hdr->sock_index].host_addr, host_ip);
		PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u, host=%u, rem=%u",
				md->sockets[hdr->sock_index].sock_id, hdr->sock_index, md->sockets[hdr->sock_index].state, addr4_get_ip(&md->sockets[hdr->sock_index].host_addr), addr4_get_ip(&md->sockets[hdr->sock_index].rem_addr));
	} else if (addr->ss_family == AF_INET6) {
		//TODO

		md->sockets[hdr->sock_index].family = AF_INET6;
	} else {
		PRINT_ERROR("Wrong address family=%d", addr->ss_family);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
}

void listen_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int backlog) {
	PRINT_DEBUG("Entered: hdr=%p, backlog=%d", hdr, backlog);
	struct daemon_data *md = (struct daemon_data *) module->data;

	if (md->sockets[hdr->sock_index].family == AF_UNSPEC) {
		PRINT_WARN("todo");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	md->sockets[hdr->sock_index].listening = 1;
	md->sockets[hdr->sock_index].backlog = backlog;

	ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
}

void connect_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, struct sockaddr_storage *addr, int flags) {
	PRINT_DEBUG("Entered: hdr=%p, flags=%d", hdr, flags);
	struct daemon_data *md = (struct daemon_data *) module->data;

	PRINT_DEBUG("SOCK_NONBLOCK=%d (%d), SOCK_CLOEXEC=%d (%d), O_NONBLOCK=%d (%d), O_ASYNC=%d (%d)",
			SOCK_NONBLOCK & flags, SOCK_NONBLOCK, SOCK_CLOEXEC & flags, SOCK_CLOEXEC, O_NONBLOCK & flags, O_NONBLOCK, O_ASYNC & flags, O_ASYNC);

	/** TODO connect for UDP means that this address will be the default address to send
	 * to. BUT IT WILL BE ALSO THE ONLY ADDRESS TO RECEIVER FROM NOTICE THAT the relation
	 **/

	if (addr->ss_family == AF_INET) {
		if (md->sockets[hdr->sock_index].family != AF_UNSPEC && md->sockets[hdr->sock_index].family != AF_INET) {
			PRINT_WARN("todo error");
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EAFNOSUPPORT);
			return;
		}

		uint32_t rem_ip = ntohl(addr4_get_ip(addr));

		if (md->sockets[hdr->sock_index].state > SS_UNCONNECTED) {
			PRINT_DEBUG("old rem=%u", addr4_get_ip(&md->sockets[hdr->sock_index].host_addr));
		}
		PRINT_DEBUG("dest address: family=%u, rem_ip=%u", AF_INET, rem_ip);

		md->sockets[hdr->sock_index].state = SS_CONNECTING;
		md->sockets[hdr->sock_index].listening = 0;
		addr4_set_ip(&md->sockets[hdr->sock_index].rem_addr, rem_ip);

		//uint32_t host_ip;

		if (md->sockets[hdr->sock_index].family == AF_UNSPEC) {
			md->sockets[hdr->sock_index].family = AF_INET;

			//auto bind
			/*
			 struct addr_record *address = (struct addr_record *) list_find(md->if_main->addr_list, addr_is_v4);
			 if (address != NULL) {
			 //host_ip = addr4_get_ip(&address->ip);
			 } else {
			 PRINT_WARN("todo error");
			 }
			 */
		}
		PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u, host=%u, rem=%u",
				md->sockets[hdr->sock_index].sock_id, hdr->sock_index, md->sockets[hdr->sock_index].state, addr4_get_ip(&md->sockets[hdr->sock_index].host_addr), addr4_get_ip(&md->sockets[hdr->sock_index].rem_addr));
	} else if (addr->ss_family == AF_INET6) {
		if (md->sockets[hdr->sock_index].family != AF_UNSPEC && md->sockets[hdr->sock_index].family != AF_INET6) {
			PRINT_WARN("todo error");
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EAFNOSUPPORT);
			return;
		}
		PRINT_WARN("todo");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	} else {
		if (md->sockets[hdr->sock_index].state > SS_UNCONNECTED) {
			md->sockets[hdr->sock_index].state = SS_UNCONNECTED;

			if (md->sockets[hdr->sock_index].family == AF_INET) {
				addr4_set_ip(&md->sockets[hdr->sock_index].rem_addr, 0);
			} else if (md->sockets[hdr->sock_index].family == AF_INET6) {
				PRINT_WARN("todo");
				nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
				return;
			} else {
				//TODO find out? do nothing?
			}
		} else {
			PRINT_ERROR("Wrong address family=%d", addr->ss_family);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EAFNOSUPPORT);
			return;
		}
	}

	ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
}

void accept_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, uint64_t sock_id_new, int sock_index_new, int flags) {
	PRINT_DEBUG("Entered: hdr=%p, sock_id_new=%llu, index_new=%d, flags=%d", hdr, sock_id_new, sock_index_new, flags);

	PRINT_DEBUG("SOCK_NONBLOCK=%d (%d), SOCK_CLOEXEC=%d (%d), O_NONBLOCK=%d (%d), O_ASYNC=%d (%d)",
			SOCK_NONBLOCK & flags, SOCK_NONBLOCK, SOCK_CLOEXEC & flags, SOCK_CLOEXEC, O_NONBLOCK & flags, O_NONBLOCK, O_ASYNC & flags, O_ASYNC);

	//TODO: finish this

	ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
}

void getname_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int peer) {
	PRINT_DEBUG("Entered: hdr=%p, peer=%d", hdr, peer);
	struct daemon_data *md = (struct daemon_data *) module->data;

	int address_len;
	struct sockaddr_storage address;

	if (md->sockets[hdr->sock_index].family == AF_INET) {
		PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u, host=%u, rem=%u",
				md->sockets[hdr->sock_index].sock_id, hdr->sock_index, md->sockets[hdr->sock_index].state, addr4_get_ip(&md->sockets[hdr->sock_index].host_addr), addr4_get_ip(&md->sockets[hdr->sock_index].rem_addr));

		uint32_t addr_ip;
		if (peer == 0) { //getsockname
			addr_ip = addr4_get_ip(&md->sockets[hdr->sock_index].host_addr);

			if (addr_ip == DAEMON_ADDR_ANY) { //TODO change this when have multiple interfaces
				struct addr_record *addr = (struct addr_record *) list_find(md->if_main->addr_list, addr_is_v4);
				if (addr != NULL) {
					addr_ip = addr4_get_ip(&addr->ip);
				} else {
					PRINT_WARN("todo error");
				}
			}
		} else if (peer == 1) { //getpeername
			if (md->sockets[hdr->sock_index].state > SS_UNCONNECTED) {
				addr_ip = addr4_get_ip(&md->sockets[hdr->sock_index].rem_addr);
			} else {
				addr_ip = 0;
			}
		} else if (peer == 2) { //accept4 //TODO figure out supposed to do??
			if (md->sockets[hdr->sock_index].state > SS_UNCONNECTED) {
				addr_ip = addr4_get_ip(&md->sockets[hdr->sock_index].rem_addr);
			} else {
				addr_ip = 0;
			}
		} else {
			PRINT_WARN("todo error");
			addr_ip = 0;
		}

		address.ss_family = md->sockets[hdr->sock_index].family;
		address_len = sizeof(struct sockaddr_in);

		struct sockaddr_in *addr4 = (struct sockaddr_in *) &address;
		addr4->sin_addr.s_addr = htonl(addr_ip);
		addr4->sin_port = 0;
		PRINT_DEBUG("addr='%s' (%u)", inet_ntoa(addr4->sin_addr), addr4->sin_addr.s_addr);
	} else if (md->sockets[hdr->sock_index].family == AF_INET6) {
		PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u", md->sockets[hdr->sock_index].sock_id, hdr->sock_index, md->sockets[hdr->sock_index].state);
		address.ss_family = md->sockets[hdr->sock_index].family;
		address_len = sizeof(struct sockaddr_in6);

		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) &address;
		addr6->sin6_port = 0;
		PRINT_WARN("todo");
		//TODO finish
	} else {
		PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u", md->sockets[hdr->sock_index].sock_id, hdr->sock_index, md->sockets[hdr->sock_index].state);
		//AF_UNSPEC, only occurs when not bound
		//Observed returns struct sockaddr with just family filled out
		//Family defaults to AF_INET, probably because of the main address of main interface

		//TODO figure out correct response to this
		address.ss_family = AF_INET;
		address_len = sizeof(struct sockaddr_in);

		struct sockaddr_in *addr4 = (struct sockaddr_in *) &address;
		addr4->sin_addr.s_addr = 0;
		addr4->sin_port = 0;
		PRINT_DEBUG("addr='%s' netw=%u", inet_ntoa(addr4->sin_addr), addr4->sin_addr.s_addr);
	}

	//send msg to wedge
	int msg_len = sizeof(struct daemon_to_wedge_hdr) + sizeof(int) + address_len;
	uint8_t *msg = (uint8_t *) secure_malloc(msg_len);

	struct daemon_to_wedge_hdr *hdr_ret = (struct daemon_to_wedge_hdr *) msg;
	hdr_ret->call_type = hdr->call_type;
	hdr_ret->call_id = hdr->call_id;
	hdr_ret->call_index = hdr->call_index;
	hdr_ret->ret = ACK;
	hdr_ret->msg = 0;
	uint8_t *pt = msg + sizeof(struct daemon_to_wedge_hdr);

	*(int *) pt = address_len;
	pt += sizeof(int);

	memcpy(pt, &address, address_len);
	pt += address_len;

	if (pt - msg != msg_len) {
		PRINT_ERROR("write error: diff=%d, len=%d", pt - msg, msg_len);
		free(msg);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	PRINT_DEBUG("msg_len=%d, msg='%s'", msg_len, msg);
	if (send_wedge(module, msg, msg_len, 0)) {
		PRINT_ERROR("Exited: fail send_wedge: hdr=%p", hdr);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	} else {
		PRINT_DEBUG("Exited: normal: hdr=%p", hdr);
	}
	free(msg);
}

void ioctl_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, uint32_t cmd, uint8_t *buf, int buf_len) {
	PRINT_DEBUG("Entered: hdr=%p, cmd=%d, len=%d", hdr, cmd, buf_len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint32_t len;
	//uint8_t *val;
	int msg_len;
	uint8_t *msg = NULL;
	struct daemon_to_wedge_hdr *hdr_ret;
	uint8_t *pt;

	switch (cmd) {
	case FIONREAD:
		PRINT_DEBUG("FIONREAD cmd=%d", cmd);
		//figure out buffered data

		//send msg to wedge
		msg_len = sizeof(struct daemon_to_wedge_hdr) + sizeof(uint32_t);
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct daemon_to_wedge_hdr *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct daemon_to_wedge_hdr);

		*(uint32_t *) pt = md->sockets[hdr->sock_index].data_buf;
		pt += sizeof(uint32_t);

		if (pt - msg != msg_len) {
			PRINT_ERROR("write error: diff=%d, len=%d", pt - msg, msg_len);
			free(msg);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}
		break;
	case SIOCGSTAMP:
		PRINT_DEBUG("SIOCGSTAMP cmd=%d", cmd);

		len = sizeof(struct timeval);
		//val = &md->daemon_sockets[hdr->sock_index].latest;

		//send msg to wedge
		msg_len = sizeof(struct daemon_to_wedge_hdr) + sizeof(uint32_t) + len;
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct daemon_to_wedge_hdr *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct daemon_to_wedge_hdr);

		*(uint32_t *) pt = len;
		pt += sizeof(uint32_t);

		PRINT_DEBUG("stamp=%u.%u", (uint32_t)md->sockets[hdr->sock_index].stamp.tv_sec, (uint32_t) md->sockets[hdr->sock_index].stamp.tv_usec);

		memcpy(pt, &md->sockets[hdr->sock_index].stamp, len);
		pt += len;

		if (pt - msg != msg_len) {
			PRINT_ERROR("write error: diff=%d, len=%d", pt - msg, msg_len);
			free(msg);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}
		break;
	default:
		PRINT_ERROR("default cmd=%d", cmd);
		msg_len = 0;
		break;
	}

	PRINT_DEBUG("msg_len=%d, msg='%s'", msg_len, msg);
	if (msg_len == 0) {
		//nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1); //TODO uncomment
		ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
		return;
	}

	if (send_wedge(module, msg, msg_len, 0)) {
		PRINT_ERROR("Exited: fail send_wedge: hdr=%p", hdr);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
	free(msg);
}

void sendmsg_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, uint32_t data_len, uint8_t *data, uint32_t flags,
		struct sockaddr_storage *addr, int addr_len) {
	PRINT_DEBUG("Entered: hdr=%p, data_len=%d, flags=%d, addr_len=%d", hdr, data_len, flags, addr_len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	PRINT_DEBUG("MSG_CONFIRM=%d (%d), MSG_DONTROUTE=%d (%d), MSG_DONTWAIT=%d (%d), MSG_EOR=%d (%d), MSG_MORE=%d (%d), MSG_NOSIGNAL=%d (%d), MSG_OOB=%d (%d)",
			MSG_CONFIRM & flags, MSG_CONFIRM, MSG_DONTROUTE & flags, MSG_DONTROUTE, MSG_DONTWAIT & flags, MSG_DONTWAIT, MSG_EOR & flags, MSG_EOR, MSG_MORE & flags, MSG_MORE, MSG_NOSIGNAL & flags, MSG_NOSIGNAL, MSG_OOB & flags, MSG_OOB);

	/** TODO handle flags cases */

	if (data_len == 0) { //TODO check this prob wrong!
		PRINT_ERROR("todo/redo");
		PRINT_DEBUG("data_len == 0, send ACK");
		ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
		return;
	}

	PRINT_DEBUG("family=%u", addr->ss_family);

	metadata *meta;
	if (addr->ss_family == AF_INET) {
		if (md->sockets[hdr->sock_index].family != AF_UNSPEC && md->sockets[hdr->sock_index].family != AF_INET) {
			PRINT_WARN("todo error");
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EAFNOSUPPORT);
			free(data);
			return;
		}

		uint32_t rem_ip;
		if (addr_len != 0) {
			rem_ip = ntohl(addr4_get_ip(addr));
		} else {
			rem_ip = addr4_get_ip(&md->sockets[hdr->sock_index].rem_addr);
		}

		uint32_t host_ip;
		if (md->sockets[hdr->sock_index].family != AF_UNSPEC) {
			host_ip = addr4_get_ip(&md->sockets[hdr->sock_index].host_addr);

			if (host_ip == DAEMON_ADDR_ANY) { //TODO change this when have multiple interfaces
				struct addr_record *address = (struct addr_record *) list_find(md->if_main->addr_list, addr_is_v4);
				if (address != NULL) {
					host_ip = addr4_get_ip(&address->ip);
				} else {
					PRINT_WARN("todo error");
				}
			}
		} else {
			md->sockets[hdr->sock_index].family = AF_INET;

			struct addr_record *address = (struct addr_record *) list_find(md->if_main->addr_list, addr_is_v4);
			if (address != NULL) {
				host_ip = addr4_get_ip(&address->ip);
			} else {
				PRINT_WARN("todo error");
			}
		}

		PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u, host=%u, rem=%u",
				md->sockets[hdr->sock_index].sock_id, hdr->sock_index, md->sockets[hdr->sock_index].state, addr4_get_ip(&md->sockets[hdr->sock_index].host_addr), addr4_get_ip(&md->sockets[hdr->sock_index].rem_addr));

		//########################
#ifdef DEBUG
		struct in_addr *temp = (struct in_addr *) malloc(sizeof(struct in_addr));
		temp->s_addr = htonl(host_ip);
		PRINT_DEBUG("index=%d, host='%s' (%u)", hdr->sock_index, inet_ntoa(*temp), host_ip);
		temp->s_addr = htonl(rem_ip);
		PRINT_DEBUG("index=%d, rem='%s' (%u)", hdr->sock_index, inet_ntoa(*temp), rem_ip);
		free(temp);
#endif
		//########################

		meta = (metadata *) secure_malloc(sizeof(metadata));
		metadata_create(meta);

		secure_metadata_writeToElement(meta, "send_src_ipv4", &host_ip, META_TYPE_INT32);
		secure_metadata_writeToElement(meta, "send_dst_ipv4", &rem_ip, META_TYPE_INT32);
	} else if (addr->ss_family == AF_INET6) {
		if (md->sockets[hdr->sock_index].family != AF_UNSPEC && md->sockets[hdr->sock_index].family != AF_INET6) {
			PRINT_WARN("todo error");
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EAFNOSUPPORT);
			free(data);
			return;
		}

		PRINT_WARN("todo");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		free(data);
		return;
	} else {
		if (md->sockets[hdr->sock_index].state > SS_UNCONNECTED) {
			if (md->sockets[hdr->sock_index].family == AF_INET) {
				uint32_t host_ip = addr4_get_ip(&md->sockets[hdr->sock_index].host_addr);

				struct addr_record *address = (struct addr_record *) list_find(md->if_main->addr_list, addr_is_v4);
				if (address != NULL) {
					host_ip = addr4_get_ip(&address->ip);
				} else {
					PRINT_WARN("todo error");
				}

				uint32_t rem_ip = addr4_get_ip(&md->sockets[hdr->sock_index].rem_addr);

				meta = (metadata *) secure_malloc(sizeof(metadata));
				metadata_create(meta);

				secure_metadata_writeToElement(meta, "send_src_ipv4", &host_ip, META_TYPE_INT32);
				secure_metadata_writeToElement(meta, "send_dst_ipv4", &rem_ip, META_TYPE_INT32);

				PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u, host=%u, rem=%u",
						md->sockets[hdr->sock_index].sock_id, hdr->sock_index, md->sockets[hdr->sock_index].state, host_ip, rem_ip);

				//########################
#ifdef DEBUG
				struct in_addr *temp = (struct in_addr *) malloc(sizeof(struct in_addr));
				temp->s_addr = htonl(host_ip);
				PRINT_DEBUG("index=%d, host='%s' (%u)", hdr->sock_index, inet_ntoa(*temp), host_ip);
				temp->s_addr = htonl(rem_ip);
				PRINT_DEBUG("index=%d, rem='%s' (%u)", hdr->sock_index, inet_ntoa(*temp), rem_ip);
				free(temp);
#endif
				//########################
			} else { //AF_INET6
				PRINT_WARN("todo error");
				nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
				free(data);
				return;
			}
		} else {
			PRINT_ERROR("Wrong address family=%d", addr->ss_family);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EAFNOSUPPORT);
			free(data);
			return;
		}
	}

	uint32_t family = md->sockets[hdr->sock_index].family;
	secure_metadata_writeToElement(meta, "send_family", &family, META_TYPE_INT32);

	uint32_t ttl = md->sockets[hdr->sock_index].sockopts.FIP_TTL;
	secure_metadata_writeToElement(meta, "send_ttl", &ttl, META_TYPE_INT32);
	uint32_t tos = md->sockets[hdr->sock_index].sockopts.FIP_TOS;
	secure_metadata_writeToElement(meta, "send_tos", &tos, META_TYPE_INT32);

	if (daemon_fdf_to_switch(module, DAEMON_FLOW_ICMP, data_len, data, meta)) {
		ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, data_len);
	} else {
		PRINT_ERROR("Exited: failed to send ff");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		metadata_destroy(meta);
		free(data);
	}
}

/**
 * @function recvfrom_icmp
 * @param symbol tells if an address has been passed from the application to get the sender address or not
 *	Note this method is coded to be thread safe since ICMPreadFrom_fins mimics blocking and needs to be threaded.
 *
 */
void recvmsg_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int buf_len, uint32_t msg_controllen, int flags) {
	PRINT_DEBUG("Entered: hdr=%p, data_len=%d, msg_controllen=%u, flags=%d", hdr, buf_len, msg_controllen, flags);
	struct daemon_data *md = (struct daemon_data *) module->data;

	PRINT_DEBUG("SOCK_NONBLOCK=%d, SOCK_CLOEXEC=%d, O_NONBLOCK=%d, O_ASYNC=%d",
			(SOCK_NONBLOCK & flags)>0, (SOCK_CLOEXEC & flags)>0, (O_NONBLOCK & flags)>0, (O_ASYNC & flags)>0);
	PRINT_DEBUG( "MSG_CMSG_CLOEXEC=%d, MSG_DONTWAIT=%d, MSG_ERRQUEUE=%d, MSG_OOB=%d, MSG_PEEK=%d, MSG_TRUNC=%d, MSG_WAITALL=%d",
			(MSG_CMSG_CLOEXEC & flags)>0, (MSG_DONTWAIT & flags)>0, (MSG_ERRQUEUE & flags)>0, (MSG_OOB & flags)>0, (MSG_PEEK & flags)>0, (MSG_TRUNC & flags)>0, (MSG_WAITALL & flags)>0);

	struct daemon_store *store = NULL;
	uint32_t addr_len;
	struct sockaddr_in *addr4;
	//struct sockaddr_in6 *addr6;
	uint32_t data_len = 0;
	uint8_t *data = NULL;

	if (flags & MSG_ERRQUEUE) {
		if (!list_is_empty(md->sockets[hdr->sock_index].error_list)) {
			store = (struct daemon_store *) list_remove_front(md->sockets[hdr->sock_index].error_list);
			data_len = store->ff->ctrlFrame.data_len;
			data = store->ff->ctrlFrame.data;
			PRINT_DEBUG("removed store: store=%p, ff=%p, data_len=%u, data=%p, pos=%u", store, store->ff, data_len, data, store->pos);
			PRINT_DEBUG("after: sock_index=%d, error_list->len=%d", hdr->sock_index, md->sockets[hdr->sock_index].error_list->len);

			if (store->addr->ss_family == AF_INET) {
				addr_len = (uint32_t) sizeof(struct sockaddr_in);
				addr4 = (struct sockaddr_in *) store->addr;

				uint32_t dst_ip = addr4->sin_addr.s_addr;
				addr4->sin_addr.s_addr = htonl(dst_ip);
				addr4->sin_port = 0;
				PRINT_DEBUG("address:'%s' (%u)", inet_ntoa(addr4->sin_addr), addr4->sin_addr.s_addr);
			} else { //AF_INET6
				addr_len = (uint32_t) sizeof(struct sockaddr_in6);
				//addr6 = (struct sockaddr_in6 *) store->addr;

				PRINT_WARN("todo");
				nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
				return;
			}
		} else {
			//NACK
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EAGAIN); //Resource temporarily unavailable
			return;
		}
	} else {
		PRINT_DEBUG("before: sock_index=%d, data_buf=%d", hdr->sock_index, md->sockets[hdr->sock_index].data_buf);
		if (md->sockets[hdr->sock_index].data_buf > 0) {
			store = (struct daemon_store *) list_remove_front(md->sockets[hdr->sock_index].data_list);
			data_len = store->ff->dataFrame.pduLength;
			data = store->ff->dataFrame.pdu;
			PRINT_DEBUG("removed store: store=%p, ff=%p, data_len=%u, data=%p, pos=%u", store, store->ff, data_len, data, store->pos);

			md->sockets[hdr->sock_index].data_buf -= data_len;
			PRINT_DEBUG("after: sock_index=%d, data_buf=%d", hdr->sock_index, md->sockets[hdr->sock_index].data_buf);

			if (store->addr->ss_family == AF_INET) {
				addr_len = (uint32_t) sizeof(struct sockaddr_in);
				addr4 = (struct sockaddr_in *) store->addr;

				uint32_t src_ip = addr4->sin_addr.s_addr;
				addr4->sin_addr.s_addr = htonl(src_ip);
				addr4->sin_port = 0;
				PRINT_DEBUG("address:'%s' (%u)", inet_ntoa(addr4->sin_addr), addr4->sin_addr.s_addr);
			} else { //AF_INET6
				addr_len = (uint32_t) sizeof(struct sockaddr_in6);
				//addr6 = (struct sockaddr_in6 *) store->addr;

				PRINT_WARN("todo");
				nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
				return;
			}
		}
	}

	if (store != NULL) {
		secure_metadata_readFromElement(store->ff->metaData, "recv_stamp", &md->sockets[hdr->sock_index].stamp);
		PRINT_DEBUG("stamp=%u.%u", (uint32_t)md->sockets[hdr->sock_index].stamp.tv_sec, (uint32_t)md->sockets[hdr->sock_index].stamp.tv_usec);

		uint32_t msg_flags = 0;
		if (flags & MSG_ERRQUEUE) {
			msg_flags |= MSG_ERRQUEUE;
		}

		uint32_t msg_len;
		if (buf_len < data_len) {
			msg_len = buf_len;
			msg_flags |= MSG_TRUNC;
		} else {
			msg_len = data_len;
		}

		//#######
#ifdef DEBUG
		uint8_t *temp = (uint8_t *) secure_malloc(msg_len + 1);
		memcpy(temp, data, msg_len);
		temp[msg_len] = '\0';
		PRINT_DEBUG("msg_len=%d, msg='%s'", msg_len, temp);
		free(temp);

		if (0) { //TODO change to func, print_hex
			print_hex(msg_len, data);
		}
#endif
		//#######

		int32_t control_len;
		uint8_t *control;
		int ret_val = recvmsg_control(module, hdr, &msg_flags, store->ff->metaData, msg_controllen, flags, &control_len, &control);

		int ret = send_wedge_recvmsg(module, hdr, msg_flags, addr_len, store->addr, msg_len, data, control_len, control);
		if (!ret) {
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		}
		daemon_store_free(store);

		if (ret_val != 0) {
			free(control);
		}
		return;
	}

	if (daemon_calls_insert(module, hdr->call_id, hdr->call_index, hdr->call_pid, hdr->call_type, hdr->sock_id, hdr->sock_index)) {
		PRINT_DEBUG("inserting call: hdr=%p", hdr);
		md->calls[hdr->call_index].flags = flags;
		md->calls[hdr->call_index].buf = buf_len;
		md->calls[hdr->call_index].ret = msg_controllen;

		struct linked_list *call_list = md->sockets[hdr->sock_index].call_list;
		if (list_has_space(call_list)) {
			list_append(call_list, &md->calls[hdr->call_index]);

			if (flags & (MSG_DONTWAIT)) {
				timer_once_start(md->calls[hdr->call_index].to_data->tid, DAEMON_BLOCK_DEFAULT);
			}
		} else {
			PRINT_ERROR("call_list full");
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		}
	} else {
		PRINT_ERROR("Insert fail: hdr=%p", hdr);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void release_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr) {
	PRINT_DEBUG("Entered: hdr=%p", hdr);
	struct daemon_data *md = (struct daemon_data *) module->data;

	if (md->sockets[hdr->sock_index].family == AF_INET) {
		PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u, host=%u, rem=%u",
				md->sockets[hdr->sock_index].sock_id, hdr->sock_index, md->sockets[hdr->sock_index].state, addr4_get_ip(&md->sockets[hdr->sock_index].host_addr), addr4_get_ip(&md->sockets[hdr->sock_index].rem_addr));

	} else if (md->sockets[hdr->sock_index].family == AF_INET6) {
	} else { //AF_UNSPEC
	}

	daemon_sockets_remove(module, hdr->sock_index);
	PRINT_DEBUG("");
	ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
}

void poll_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, uint32_t events) {
	PRINT_DEBUG("Entered: hdr=%p, events=0x%x", hdr, events);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint32_t mask = 0;

	/*
	 if (md->sockets[hdr->sock_index].sock_id != hdr->sock_id) {
	 PRINT_ERROR("Socket Mismatch: sock_index=%d, sock_id=%llu, hdr->sock_id=%llu", hdr->sock_index, md->sockets[hdr->sock_index].sock_id, hdr->sock_id);
	 nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, POLLNVAL);
	 return;
	 }
	 */
	if (events) { //initial
		PRINT_DEBUG(
				"POLLIN=0x%x, POLLPRI=0x%x, POLLOUT=0x%x, POLLERR=0x%x, POLLHUP=0x%x, POLLNVAL=0x%x, POLLRDNORM=0x%x, POLLRDBAND=0x%x, POLLWRNORM=0x%x, POLLWRBAND=0x%x",
				(events & POLLIN) > 0, (events & POLLPRI) > 0, (events & POLLOUT) > 0, (events & POLLERR) > 0, (events & POLLHUP) > 0, (events & POLLNVAL) > 0, (events & POLLRDNORM) > 0, (events & POLLRDBAND) > 0, (events & POLLWRNORM) > 0, (events & POLLWRBAND) > 0);

		if (events & (POLLERR)) {
			if (!list_is_empty(md->sockets[hdr->sock_index].error_list)) {
				mask |= POLLERR;
			}
		}

		if (events & (POLLIN | POLLRDNORM | POLLPRI | POLLRDBAND)) {
			if (md->sockets[hdr->sock_index].data_buf > 0) {
				mask |= POLLIN | POLLRDNORM | POLLPRI;
			}
		}

		if (events & (POLLHUP)) {
			//mask |= POLLHUP; //TODO implement
		}

		if (events & (POLLOUT | POLLWRNORM | POLLWRBAND)) {
			mask |= POLLOUT | POLLWRNORM | POLLWRBAND;
		}

		uint32_t ret_mask = events & mask;
		PRINT_DEBUG("events=0x%x, mask=0x%x, ret_mask=0x%x", events, mask, ret_mask);
		if (ret_mask) {
			ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, ret_mask);
		} else {
			struct daemon_call *call = daemon_call_create(hdr->call_id, hdr->call_index, hdr->call_pid, hdr->call_type, hdr->sock_id, hdr->sock_index);
			call->buf = events;
			call->ret = 0;

			struct linked_list *call_list = md->sockets[hdr->sock_index].call_list;
			if (list_has_space(call_list)) {
				list_append(call_list, call);
				PRINT_DEBUG("");
				ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
			} else {
				PRINT_ERROR("call_list full");
				nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			}
		}
	} else { //final
		struct daemon_call *call =
				(struct daemon_call *) list_find2(md->sockets[hdr->sock_index].call_list, daemon_call_pid_test, &hdr->call_pid, &hdr->call_type);
		if (call) {
			events = call->buf;
			mask = call->ret;

			list_remove(md->sockets[hdr->sock_index].call_list, call);
			if (call->alloc) {
				daemon_call_free(call);
			} else {
				PRINT_WARN("todo error");
			}

			uint32_t ret_mask = events & mask;
			PRINT_DEBUG("events=0x%x, mask=0x%x, ret_mask=0x%x", events, mask, ret_mask);
			if (ret_mask) {
				ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, ret_mask);
			} else {
				PRINT_DEBUG(
						"POLLIN=0x%x, POLLPRI=0x%x, POLLOUT=0x%x, POLLERR=0x%x, POLLHUP=0x%x, POLLNVAL=0x%x, POLLRDNORM=0x%x, POLLRDBAND=0x%x, POLLWRNORM=0x%x, POLLWRBAND=0x%x",
						(events & POLLIN) > 0, (events & POLLPRI) > 0, (events & POLLOUT) > 0, (events & POLLERR) > 0, (events & POLLHUP) > 0, (events & POLLNVAL) > 0, (events & POLLRDNORM) > 0, (events & POLLRDBAND) > 0, (events & POLLWRNORM) > 0, (events & POLLWRBAND) > 0);

				if (events & (POLLERR)) {
					if (!list_is_empty(md->sockets[hdr->sock_index].error_list)) {
						mask |= POLLERR;
					}
				}

				if (events & (POLLIN | POLLRDNORM | POLLPRI | POLLRDBAND)) {
					if (md->sockets[hdr->sock_index].data_buf > 0) {
						mask |= POLLIN | POLLRDNORM | POLLPRI;
					}
				}

				if (events & (POLLHUP)) {
					//mask |= POLLHUP; //TODO implement
				}

				if (events & (POLLOUT | POLLWRNORM | POLLWRBAND)) {
					mask |= POLLOUT | POLLWRNORM | POLLWRBAND;
				}

				ret_mask = events & mask;
				PRINT_DEBUG("events=0x%x, mask=0x%x, ret_mask=0x%x", events, mask, ret_mask);
				ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, ret_mask);
			}
		} else {
			PRINT_WARN("final: no corresponding call: sock_id=%llu, sock_index=%d, call_pid=%d,  call_type=%u, call_id=%u, call_index=%d",
					hdr->sock_id, hdr->sock_index, hdr->call_pid, hdr->call_type, hdr->call_id, hdr->call_index);

			if (!list_is_empty(md->sockets[hdr->sock_index].error_list)) {
				mask |= POLLERR;
			}

			if (md->sockets[hdr->sock_index].data_buf > 0) {
				mask |= POLLIN | POLLRDNORM | POLLPRI;
			}

			mask |= POLLOUT | POLLWRNORM | POLLWRBAND;

			//mask |= POLLHUP; //TODO implement

			PRINT_DEBUG("mask=0x%x", mask);
			ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, mask);
		}
	}
}

void mmap_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr) {
	PRINT_DEBUG("Entered: hdr=%p", hdr);
	PRINT_WARN("todo");
}

void socketpair_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr) {
	PRINT_DEBUG("Entered: hdr=%p", hdr);
	PRINT_WARN("todo");
}

void shutdown_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int how) {
	PRINT_DEBUG("Entered: hdr=%p, how=%d", hdr, how);

	/**
	 * TODO Implement the checking of the shut_RD, shut_RW flags before making any operations
	 * applied on a TCP socket
	 */
	ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
}

void close_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr) {
	PRINT_DEBUG("Entered: hdr=%p", hdr);
	PRINT_WARN("todo");
}

void sendpage_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr) {
	PRINT_DEBUG("Entered: hdr=%p", hdr);
	PRINT_WARN("todo");
}

void getsockopt_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int level, int optname, int optlen, uint8_t *optval) {
	PRINT_DEBUG("Entered: hdr=%p, level=%d, optname=%d, optlen=%d", hdr, level, optname, optlen);
	struct daemon_data *md = (struct daemon_data *) module->data;

	/*
	 * 7 levels+:
	 * SOL_SOCKET == 1
	 * IPPROTO_IP == SOL_IP == 0
	 * IPPROTO_IPV6 == SOL_IPV6 == 41
	 * IPPROTO_ICMP == 1
	 * IPPROTO_ICMPV6 == SOL_ICMPV6 == 58
	 * IPPROTO_RAW == SOL_RAW == 255
	 * IPPROTO_TCP == SOL_TCP == 6
	 */

	int len = 0;
	uint8_t *val = NULL;

	switch (level) {
	case SOL_IP:
		switch (optname) {
		case IP_TOS:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FIP_TOS;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case IP_RECVERR:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FIP_RECVERR;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case IP_MTU_DISCOVER:
			//TODO
			PRINT_WARN("todo");
			break;
		case IP_RECVTTL:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FIP_RECVTTL;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case IP_TTL:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FIP_TTL;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		default:
			break;
		}
		break;
	case SOL_RAW:
		switch (optname) {
		case ICMP_FILTER:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FICMP_FILTER;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		default:
			break;
		}
		break;
	case SOL_TCP:
		//TODO check whether this should throw an error
		switch (optname) {
		case TCP_NODELAY:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FTCP_NODELAY;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		default:
			break;
		}
		break;
	case SOL_SOCKET:
		switch (optname) {
		case SO_DEBUG:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FSO_DEBUG;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_REUSEADDR:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FSO_REUSEADDR;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_TYPE:
#ifndef BUILD_FOR_ANDROID
		case SO_PROTOCOL:
		case SO_DOMAIN:
#endif
		case SO_ERROR:
		case SO_DONTROUTE:
		case SO_BROADCAST:
			break;
		case SO_SNDBUF:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FSO_SNDBUF;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_SNDBUFFORCE:
			break;
		case SO_RCVBUF:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FSO_RCVBUF;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_RCVBUFFORCE:
			break;
		case SO_KEEPALIVE:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FSO_KEEPALIVE;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_OOBINLINE:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FSO_OOBINLINE;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_NO_CHECK:
			break;
		case SO_PRIORITY:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FSO_PRIORITY;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_LINGER:
		case SO_BSDCOMPAT:
			break;
		case SO_TIMESTAMP:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FSO_TIMESTAMP;
			} else {
				PRINT_WARN("todo error");
			}
			break;
#ifndef BUILD_FOR_ANDROID
		case SO_TIMESTAMPNS:
		case SO_TIMESTAMPING:
#endif
		case SO_RCVTIMEO:
			//TODO less - gets 8 byte value, timestamp??
		case SO_SNDTIMEO:
			//TODO less - gets 8 byte value, timestamp??
		case SO_RCVLOWAT:
		case SO_SNDLOWAT:
			break;
		case SO_PASSCRED:
			if (optlen >= sizeof(int)) {
				len = sizeof(int);
				val = (uint8_t *) &md->sockets[hdr->sock_index].sockopts.FSO_PASSCRED;
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_PEERCRED:
			//TODO trickier
		case SO_PEERNAME:
		case SO_ACCEPTCONN:
		case SO_PASSSEC:
		case SO_PEERSEC:
#ifndef BUILD_FOR_ANDROID
		case SO_MARK:
		case SO_RXQ_OVFL:
#endif
		case SO_ATTACH_FILTER:
		case SO_DETACH_FILTER:
			PRINT_WARN("todo");
			break;
		default:
			//nack?
			PRINT_ERROR("default=%d", optname);
			break;
		}
		break;
	default:
		break;
	}

	//if (len) {
	//send msg to wedge
	int msg_len = sizeof(struct daemon_to_wedge_hdr) + sizeof(int) + (len > 0 ? len : 0);
	uint8_t *msg = (uint8_t *) secure_malloc(msg_len);

	struct daemon_to_wedge_hdr *hdr_ret = (struct daemon_to_wedge_hdr *) msg;
	hdr_ret->call_type = hdr->call_type;
	hdr_ret->call_id = hdr->call_id;
	hdr_ret->call_index = hdr->call_index;
	hdr_ret->ret = ACK;
	hdr_ret->msg = 0;
	uint8_t *pt = msg + sizeof(struct daemon_to_wedge_hdr);

	*(int *) pt = len;
	pt += sizeof(int);

	if (len > 0) {
		memcpy(pt, val, len);
		pt += len;
	}

	if (pt - msg != msg_len) {
		PRINT_ERROR("write error: diff=%d, len=%d", pt - msg, msg_len);
		free(msg);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	PRINT_DEBUG("msg_len=%d, msg='%s'", msg_len, msg);
	if (send_wedge(module, msg, msg_len, 0)) {
		PRINT_ERROR("Exited: fail send_wedge: hdr=%p", hdr);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	} else {

	}
	free(msg);
	//} else {
	//	nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	//}
}

void setsockopt_out_icmp(struct fins_module *module, struct wedge_to_daemon_hdr *hdr, int level, int optname, int optlen, uint8_t *optval) {
	PRINT_DEBUG("Entered: hdr=%p, level=%d, optname=%d, optlen=%d", hdr, level, optname, optlen);
	struct daemon_data *md = (struct daemon_data *) module->data;

	/*
	 * 7 levels+:
	 * SOL_SOCKET == 1
	 * IPPROTO_IP == SOL_IP == 0
	 * IPPROTO_IPV6 == SOL_IPV6 == 41
	 * IPPROTO_ICMP == 1
	 * IPPROTO_ICMPV6 == SOL_ICMPV6 == 58
	 * IPPROTO_RAW == SOL_RAW == 255
	 * IPPROTO_TCP == SOL_TCP == 6
	 */

	switch (level) {
	case SOL_IP:
		switch (optname) {
		case IP_TOS:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FIP_TOS = *(int *) optval;
				PRINT_DEBUG("FIP_TOS=%d", md->sockets[hdr->sock_index].sockopts.FIP_TOS);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case IP_RECVERR:
			if (optlen >= sizeof(int)) {
				int old = md->sockets[hdr->sock_index].sockopts.FIP_RECVERR;
				md->sockets[hdr->sock_index].sockopts.FIP_RECVERR = *(int *) optval;
				PRINT_DEBUG("FIP_RECVERR=%d", md->sockets[hdr->sock_index].sockopts.FIP_RECVERR);

				if (!list_is_empty(md->sockets[hdr->sock_index].error_list)) {
					if (old && !md->sockets[hdr->sock_index].sockopts.FIP_RECVERR) { //FIP_RECVERR opt changed from 1 to 0
						struct daemon_store *old_store;
						while (md->sockets[hdr->sock_index].error_list->len > 1) {
							old_store = (struct daemon_store *) list_remove_front(md->sockets[hdr->sock_index].error_list);
							daemon_store_free(old_store);
						}
					}
				}
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case IP_MTU_DISCOVER:
			//TODO
			PRINT_ERROR("todo: IP_MTU_DISCOVER");
			break;
		case IP_RECVTTL:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FIP_RECVTTL = *(int *) optval;
				PRINT_DEBUG("FIP_RECVTTL=%d", md->sockets[hdr->sock_index].sockopts.FIP_RECVTTL);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case IP_TTL:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FIP_TTL = *(int *) optval;
				PRINT_DEBUG("FIP_TTL=%d", md->sockets[hdr->sock_index].sockopts.FIP_TTL);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		default:
			break;
		}
		break;
	case SOL_RAW:
		switch (optname) {
		case ICMP_FILTER:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FICMP_FILTER = *(int *) optval;
				PRINT_DEBUG("FICMP_FILTER=%d", md->sockets[hdr->sock_index].sockopts.FICMP_FILTER);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		default:
			break;
		}
		break;
	case SOL_TCP:
		//TODO check whether this should throw an error
		switch (optname) {
		case TCP_NODELAY:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FTCP_NODELAY = *(int *) optval;
				PRINT_DEBUG("FTCP_NODELAY=%d", md->sockets[hdr->sock_index].sockopts.FTCP_NODELAY);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		default:
			break;
		}
		break;
	case SOL_SOCKET:
		switch (optname) {
		case SO_DEBUG:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FSO_DEBUG = *(int *) optval;
				PRINT_DEBUG("FSO_DEBUG=%d", md->sockets[hdr->sock_index].sockopts.FSO_DEBUG);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_REUSEADDR:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FSO_REUSEADDR = *(int *) optval;
				PRINT_DEBUG("FSO_REUSEADDR=%d", md->sockets[hdr->sock_index].sockopts.FSO_REUSEADDR);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_TYPE:
#ifndef BUILD_FOR_ANDROID
		case SO_PROTOCOL:
		case SO_DOMAIN:
#endif
		case SO_ERROR:
		case SO_DONTROUTE:
		case SO_BROADCAST:
			break;
		case SO_SNDBUF:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FSO_SNDBUF = *(int *) optval;
				PRINT_DEBUG("FSO_SNDBUF=%d", md->sockets[hdr->sock_index].sockopts.FSO_SNDBUF);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_SNDBUFFORCE:
			break;
		case SO_RCVBUF:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FSO_RCVBUF = 2 * (*(int *) optval); //TODO add conditions //TODO check if this is right!
				PRINT_DEBUG("FSO_RCVBUF=%d", md->sockets[hdr->sock_index].sockopts.FSO_RCVBUF);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_RCVBUFFORCE:
			break;
		case SO_KEEPALIVE:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FSO_KEEPALIVE = *(int *) optval;
				PRINT_DEBUG("FSO_KEEPALIVE=%d", md->sockets[hdr->sock_index].sockopts.FSO_KEEPALIVE);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_OOBINLINE:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FSO_OOBINLINE = *(int *) optval;
				PRINT_DEBUG("FSO_OOBINLINE=%d", md->sockets[hdr->sock_index].sockopts.FSO_OOBINLINE);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_NO_CHECK:
			break;
		case SO_PRIORITY:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FSO_PRIORITY = *(int *) optval;
				PRINT_DEBUG("FSO_PRIORITY=%d", md->sockets[hdr->sock_index].sockopts.FSO_PRIORITY);
			} else {
				PRINT_WARN("todo error");
			}
			break;
		case SO_LINGER:
		case SO_BSDCOMPAT:
			break;
		case SO_TIMESTAMP:
			if (optlen >= sizeof(int)) {
				md->sockets[hdr->sock_index].sockopts.FSO_TIMESTAMP = *(int *) optval;
				PRINT_DEBUG("FSO_TIMESTAMP=%d", md->sockets[hdr->sock_index].sockopts.FSO_TIMESTAMP);
			} else {
				PRINT_WARN("todo error");
			}
			break;
#ifndef BUILD_FOR_ANDROID
		case SO_TIMESTAMPNS:
		case SO_TIMESTAMPING:
#endif
		case SO_RCVTIMEO:
			//TODO less - gets 8 byte value, timestamp??
		case SO_SNDTIMEO:
			//TODO less - gets 8 byte value, timestamp??
		case SO_RCVLOWAT:
		case SO_SNDLOWAT:
		case SO_PASSCRED:
			//TODO later
		case SO_PEERCRED:
			//TODO later
		case SO_PEERNAME:
		case SO_ACCEPTCONN:
		case SO_PASSSEC:
		case SO_PEERSEC:
#ifndef BUILD_FOR_ANDROID
		case SO_MARK:
		case SO_RXQ_OVFL:
#endif
		case SO_ATTACH_FILTER:
		case SO_DETACH_FILTER:
			PRINT_WARN("todo");
			break;
		default:
			//nack?
			PRINT_ERROR("default=%d", optname);
			break;
		}
		break;
	default:
		break;
	}

	ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);

	if (optlen > 0) {
		free(optval);
	}
}

void poll_in_icmp(struct daemon_call *call, struct fins_module *module, uint32_t *flags) {
	if (call->type != POLL_CALL) {
		return;
	}

	PRINT_DEBUG("Entered: call=%p, flags=0x%x", call, *flags);
	uint32_t events = call->buf;

	PRINT_DEBUG(
			"POLLIN=0x%x, POLLPRI=0x%x, POLLOUT=0x%x, POLLERR=0x%x, POLLHUP=0x%x, POLLNVAL=0x%x, POLLRDNORM=0x%x, POLLRDBAND=0x%x, POLLWRNORM=0x%x, POLLWRBAND=0x%x",
			(events & POLLIN) > 0, (events & POLLPRI) > 0, (events & POLLOUT) > 0, (events & POLLERR) > 0, (events & POLLHUP) > 0, (events & POLLNVAL) > 0, (events & POLLRDNORM) > 0, (events & POLLRDBAND) > 0, (events & POLLWRNORM) > 0, (events & POLLWRBAND) > 0);

	uint32_t mask = 0;

	if (*flags & (POLLERR)) {
		if (events & (POLLERR)) {
			mask |= POLLERR;
		}
	}

	if (*flags & (POLLIN | POLLRDNORM | POLLPRI | POLLRDBAND)) {
		if (events & (POLLIN | POLLRDNORM | POLLPRI | POLLRDBAND)) {
			mask |= POLLIN | POLLRDNORM | POLLPRI;
		}
	}

	uint32_t ret_mask = events & mask;
	PRINT_DEBUG("events=0x%x, mask=0x%x, ret_mask=0x%x", events, mask, ret_mask);
	if (ret_mask) {
		//send msg to wedge
		int msg_len = sizeof(struct daemon_to_wedge_hdr);
		uint8_t *msg = (uint8_t *) secure_malloc(msg_len);

		struct daemon_to_wedge_hdr *hdr_ret = (struct daemon_to_wedge_hdr *) msg;
		hdr_ret->call_type = POLL_EVENT_CALL;
		hdr_ret->sock_id = call->sock_id;
		hdr_ret->sock_index = call->sock_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = ret_mask;
		uint8_t *pt = msg + sizeof(struct daemon_to_wedge_hdr);

		if (pt - msg != msg_len) {
			PRINT_ERROR("write error: diff=%d, len=%d", pt - msg, msg_len);
			free(msg);
			return;
		}

		PRINT_DEBUG("msg_len=%d, msg='%s'", msg_len, msg);
		if (send_wedge(module, msg, msg_len, 0)) {
			PRINT_ERROR("Exited: send_wedge error: call=%p", call);
		} else {

		}
		free(msg);

		call->ret |= ret_mask;
	}
}

uint32_t recvmsg_in_icmp(struct daemon_call *call, struct fins_module *module, metadata *meta, uint32_t data_len, uint8_t *data, struct sockaddr_storage *addr,
		uint32_t flags) {
	PRINT_DEBUG("Entered: call=%p, meta=%p, len=%u, data=%p, addr=%p, flags=0x%x", call, meta, data_len, data, addr, flags);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint32_t call_len = call->buf; //buffer size
	uint32_t msg_controllen = call->ret;

	secure_metadata_readFromElement(meta, "recv_stamp", &md->sockets[call->sock_index].stamp);
	PRINT_DEBUG("stamp=%u.%u", (uint32_t)md->sockets[call->sock_index].stamp.tv_sec, (uint32_t)md->sockets[call->sock_index].stamp.tv_usec);

	uint32_t msg_flags = 0;
	if (flags & MSG_ERRQUEUE) {
		msg_flags |= MSG_ERRQUEUE;
	}

	if (call_len < data_len) {
		data_len = call_len;
		msg_flags |= MSG_TRUNC;
	}

	uint32_t addr_len;
	if (addr->ss_family == AF_INET) {
		addr_len = (uint32_t) sizeof(struct sockaddr_in);
		struct sockaddr_in *addr4 = (struct sockaddr_in *) addr;
		addr4->sin_addr.s_addr = htonl(addr4->sin_addr.s_addr);
		addr4->sin_port = 0;
		PRINT_DEBUG("address:'%s' (%u)", inet_ntoa(addr4->sin_addr), addr4->sin_addr.s_addr);
	} else { //AF_INET6
		PRINT_WARN("todo");
		nack_send(module, call->id, call->index, call->type, 1);
		daemon_calls_remove(module, call->index);
		return data_len;
	}

	//#######
#ifdef DEBUG
	uint8_t *temp = (uint8_t *) secure_malloc(data_len + 1);
	memcpy(temp, data, data_len);
	temp[data_len] = '\0';
	PRINT_DEBUG("data_len=%d, data='%s'", data_len, temp);
	free(temp);

	if (0) { //TODO change to func, print_hex
		print_hex(data_len, data);
	}
#endif
	//#######

	int32_t control_len;
	uint8_t *control;
	int ret_val = recvmsg_control(module, (struct wedge_to_daemon_hdr *) call, &msg_flags, meta, msg_controllen, flags, &control_len, &control);

	int ret = send_wedge_recvmsg(module, (struct wedge_to_daemon_hdr *) call, msg_flags, addr_len, addr, data_len, data, control_len, control);
	if (!ret) {
		nack_send(module, call->id, call->index, call->type, 1);
	}
	daemon_calls_remove(module, call->index);

	if (ret_val != 0) {
		free(control);
	}
	return data_len;
}

void daemon_in_fdf_icmp(struct fins_module *module, struct finsFrame *ff, uint32_t family, struct sockaddr_storage *src_addr, struct sockaddr_storage *dst_addr) {
	PRINT_DEBUG("Entered: ff=%p, family=%u, src_addr=%p, dst_addr=%p", ff, family, src_addr, dst_addr);
	struct daemon_data *md = (struct daemon_data *) module->data;

	int i;
	uint32_t flags;
	struct daemon_call *call;
	struct daemon_store *store;

	if (family == AF_INET) {
		//uint32_t src_ip = addr4_get_ip(src_addr);
		uint32_t dst_ip = addr4_get_ip(dst_addr);

		uint32_t host_ip;
		for (i = 0; i < DAEMON_MAX_SOCKETS; i++) {
			if (md->sockets[i].sock_id != -1 && md->sockets[i].protocol == IPPROTO_ICMP && md->sockets[i].family == AF_INET) {
				host_ip = addr4_get_ip(&md->sockets[i].host_addr);
				if (host_ip == DAEMON_ADDR_ANY || host_ip == dst_ip) {
					PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u, host=%u, rem=%u",
							md->sockets[i].sock_id, i, md->sockets[i].state, addr4_get_ip(&md->sockets[i].host_addr), addr4_get_ip(&md->sockets[i].rem_addr));

					//md->sockets[i].count++; //TODO remove, only for testing
					//PRINT_INFO("count=%d", md->sockets[i].count);

					//TODO check if this datagram comes from the address this socket has been previously connected to it (Only if the socket is already connected to certain address)
					flags = POLLIN;
					list_for_each2(md->sockets[i].call_list, poll_in_icmp, module, &flags);

					flags = 0;
					call = (struct daemon_call *) list_find1(md->sockets[i].call_list, daemon_call_recvmsg_test, &flags);
					if (call != NULL) {
						recvmsg_in_icmp(call, module, ff->metaData, ff->dataFrame.pduLength, ff->dataFrame.pdu, src_addr, 0);
						list_remove(md->sockets[i].call_list, call);
						continue;
					}

					store = (struct daemon_store *) secure_malloc(sizeof(struct daemon_store));
					store->addr = (struct sockaddr_storage *) secure_malloc(sizeof(struct sockaddr_storage));
					memcpy(store->addr, src_addr, sizeof(struct sockaddr_storage));
					store->ff = cloneFinsFrame(ff);
					store->pos = 0;

					if (list_has_space(md->sockets[i].data_list)) {
						PRINT_DEBUG("appending store: store=%p, ff=%p, data_len=%u, data=%p, pos=%u",
								store, store->ff, ff->dataFrame.pduLength, ff->dataFrame.pdu, store->pos);
						list_append(md->sockets[i].data_list, store);
						md->sockets[i].data_buf += ff->dataFrame.pduLength;
						PRINT_DEBUG("stored, sock_index=%d, ff=%p, meta=%p, data_buf=%d", i, store->ff, ff->metaData, md->sockets[i].data_buf);
					} else {
						PRINT_ERROR("data_list full: sock_index=%d, ff=%p", i, store->ff);
						daemon_store_free(store);
					}
				}
			}

		}

		freeFinsFrame(ff);
	} else { //AF_INET
		PRINT_WARN("todo");
		freeFinsFrame(ff);
	}
}

void daemon_in_error_icmp(struct fins_module *module, struct finsFrame *ff, uint32_t family, struct sockaddr_storage *src_addr,
		struct sockaddr_storage *dst_addr) {
	PRINT_DEBUG("Entered: ff=%p, family=%u, src_addr=%p, dst_addr=%p", ff, family, src_addr, dst_addr);
	struct daemon_data *md = (struct daemon_data *) module->data;

	int i;
	uint32_t flags;
	struct daemon_call *call;
	struct daemon_store *store;
	struct daemon_store *old_store;

	if (family == AF_INET) {
		uint32_t src_ip = addr4_get_ip(src_addr);
		//uint32_t dst_ip = addr4_get_ip(dst_addr);

		uint32_t host_ip;
		for (i = 0; i < DAEMON_MAX_SOCKETS; i++) {
			if (md->sockets[i].sock_id != -1 && md->sockets[i].protocol == IPPROTO_ICMP && md->sockets[i].family == AF_INET) {
				host_ip = addr4_get_ip(&md->sockets[i].host_addr);
				if (host_ip == DAEMON_ADDR_ANY || host_ip == src_ip) {
					PRINT_DEBUG("sock_id=%llu, sock_index=%d, state=%u, host=%u, rem=%u",
							md->sockets[i].sock_id, i, md->sockets[i].state, addr4_get_ip(&md->sockets[i].host_addr), addr4_get_ip(&md->sockets[i].rem_addr));

					flags = POLLERR;
					list_for_each2(md->sockets[i].call_list, poll_in_icmp, module, &flags);

					if (ff->ctrlFrame.param_id == DAEMON_ERROR_GET_ADDR) {
						flags = 0;
						call = (struct daemon_call *) list_find1(md->sockets[i].call_list, daemon_call_recvmsg_test, &flags);
						if (call != NULL) {
							nack_send(module, call->id, call->index, call->type, EAGAIN);
							daemon_calls_remove(module, call->index);
							list_remove(md->sockets[i].call_list, call);
						}
					}

					PRINT_DEBUG("FIP_RECVERR=%d", md->sockets[i].sockopts.FIP_RECVERR);
					if (md->sockets[i].sockopts.FIP_RECVERR) {
						flags = 1;
						call = (struct daemon_call *) list_find1(md->sockets[i].call_list, daemon_call_recvmsg_test, &flags);
						if (call != NULL) {
							recvmsg_in_icmp(call, module, ff->metaData, ff->dataFrame.pduLength, ff->dataFrame.pdu, dst_addr, MSG_ERRQUEUE);
							list_remove(md->sockets[i].call_list, call);
							continue;
						}
					}

					store = (struct daemon_store *) secure_malloc(sizeof(struct daemon_store));
					store->addr = (struct sockaddr_storage *) secure_malloc(sizeof(struct sockaddr_storage));
					memcpy(store->addr, src_addr, sizeof(struct sockaddr_storage));
					store->ff = cloneFinsFrame(ff);

					if (list_has_space(md->sockets[i].error_list)) {
						if (!md->sockets[i].sockopts.FIP_RECVERR) {
							while (!list_is_empty(md->sockets[i].error_list)) {
								old_store = (struct daemon_store *) list_remove_front(md->sockets[i].error_list);
								daemon_store_free(old_store);
							}
						}
						list_append(md->sockets[i].error_list, store);
						PRINT_DEBUG("stored, sock_index=%d, ff=%p, meta=%p, error_list->len=%d", i, store->ff, ff->metaData, md->sockets[i].error_list->len);
					} else {
						PRINT_ERROR("data_list full: sock_index=%d, ff=%p", i, store->ff);
						daemon_store_free(store);
					}
				}
			}

		}

		freeFinsFrame(ff);
	} else { //AF_INET
		PRINT_WARN("todo");
		freeFinsFrame(ff);
	}
}

void recvmsg_timeout_icmp(struct fins_module *module, struct daemon_call *call) {
	PRINT_DEBUG("Entered: call=%p", call);
	struct daemon_data *md = (struct daemon_data *) module->data;

	list_remove(md->sockets[call->sock_index].call_list, call);

	switch (md->sockets[call->sock_index].state) {
	case SS_UNCONNECTED:
		nack_send(module, call->id, call->index, call->type, EAGAIN); //nack EAGAIN or EWOULDBLOCK
		break;
	case SS_CONNECTING:
		nack_send(module, call->id, call->index, call->type, EAGAIN); //nack EAGAIN or EWOULDBLOCK
		break;
	default:
		PRINT_WARN("todo error");
		nack_send(module, call->id, call->index, call->type, 1);
		break;
	}

	daemon_calls_remove(module, call->index);
}
