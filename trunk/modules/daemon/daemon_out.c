/*
 * handlers.c
 *
 *  Created on: May 3, 2013
 *      Author: Jonathan Reed
 */
#include "daemon_internal.h"

static call_out_type call_outs[] = { NULL, socket_out, bind_out, listen_out, connect_out, accept_out, getname_out, ioctl_out, sendmsg_out, recvmsg_out,
		getsockopt_out, setsockopt_out, release_out, poll_out, mmap_out, socketpair_out, shutdown_out, close_out, sendpage_out };

void daemon_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *msg_pt, int msg_len) {
	PRINT_DEBUG("Entered: hdr=%p, sock_id=%llu, sock_index=%d, call_pid=%d,  call_type=%u, call_id=%u, call_index=%d, len=%d",
			hdr, hdr->sock_id, hdr->sock_index, hdr->call_pid, hdr->call_type, hdr->call_id, hdr->call_index, msg_len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	//############################### Debug
#ifdef DEBUG
	uint8_t *temp;
	temp = (uint8_t *) secure_malloc(msg_len + 1);
	memcpy(temp, msg_pt, msg_len);
	temp[msg_len] = '\0';
	PRINT_DEBUG("msg='%s'", temp);
	free(temp);

	if (0) {
		print_hex(msg_len, msg_pt);
	}
#endif
	//###############################

	if (hdr->call_index < 0 || hdr->call_index > DAEMON_MAX_CALLS) {
		PRINT_ERROR("call_index out of range: call_index=%d", hdr->call_index);
	} else {
		if (hdr->call_type == 0 || MAX_CALL_TYPES <= hdr->call_type) {
			PRINT_ERROR("call_type out of range: call_type=%u", hdr->call_type);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		} else if (hdr->call_type == SOCKET_CALL) {
			PRINT_DEBUG("wait$$$$$$$$$$$$$$$");
			secure_sem_wait(&md->sockets_sem);

			socket_out(module, hdr, msg_pt, msg_len);

			PRINT_DEBUG("post$$$$$$$$$$$$$$$");
			sem_post(&md->sockets_sem);
		} else {
			if (hdr->sock_index < 0 || DAEMON_MAX_SOCKETS < hdr->sock_index) {
				PRINT_ERROR("sock_index out of range: sock_index=%d", hdr->sock_index);
				nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EBADF);
			} else {
				PRINT_DEBUG("wait$$$$$$$$$$$$$$$");
				secure_sem_wait(&md->sockets_sem);
				if (md->sockets[hdr->sock_index].sock_id != hdr->sock_id) {
					PRINT_DEBUG("invalid socket: sock_id=%llu, sock_index=%d, call_pid=%d,  call_type=%u, call_id=%u, call_index=%d",
							hdr->sock_id, hdr->sock_index, hdr->call_pid, hdr->call_type, hdr->call_id, hdr->call_index);
					PRINT_DEBUG("post$$$$$$$$$$$$$$$");
					sem_post(&md->sockets_sem);

					//TODO find error for synch issues between wedge/daemon
					nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
					return;
				}

				PRINT_DEBUG("sock_id=%llu, sock_index=%d, type=%d, proto=%d",
						md->calls[hdr->call_index].sock_id, md->calls[hdr->call_index].sock_index, md->sockets[hdr->sock_index].type, md->sockets[hdr->sock_index].protocol);
				(call_outs[hdr->call_type])(module, hdr, msg_pt, msg_len);

				PRINT_DEBUG("post$$$$$$$$$$$$$$$");
				sem_post(&md->sockets_sem);
			}
		}
	}
}

void socket_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);

	uint8_t *pt = buf;
	//convert to: struct socket_call_hdr *hdr = (struct socket_call_hdr *)buf;

	int domain = *(int *) pt;
	pt += sizeof(int);

	int type = *(int *) pt;
	pt += sizeof(int);

	int protocol = *(int *) pt;
	pt += sizeof(int);

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	PRINT_DEBUG("domain=%d, type=%u, protocol=%d", domain, type, protocol);
	if (domain != AF_INET && domain != AF_INET6) {
		PRINT_ERROR("Wrong domain, only AF_INET us supported");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	//change to list containing struct socket_type: has create(), equal() funcs?
	if (socket_icmp_test(domain, type, protocol)) {
		socket_out_icmp(module, hdr, domain);
	} else if (socket_tcp_test(domain, type, protocol)) {
		socket_out_tcp(module, hdr, domain);
	} else if (socket_udp_test(domain, type, protocol)) {
		socket_out_udp(module, hdr, domain);
	} else {
		PRINT_ERROR("non supported socket: type=%d, protocol=%d", type, protocol);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EACCES);
	}
}

void bind_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;
	uint8_t *pt = buf;

	int addr_len = *(int *) pt;
	pt += sizeof(int);

	if (addr_len <= 0) {
		PRINT_ERROR("READING ERROR! CRASH, addrlen=%d", addr_len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	} else {
		PRINT_DEBUG("addr_len=%d", addr_len);
	}

	struct sockaddr_storage addr;
	memcpy(&addr, pt, addr_len);
	pt += addr_len;

	int reuseaddr = *(int *) pt;
	pt += sizeof(int);

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	md->sockets[hdr->sock_index].sockopts.FSO_REUSEADDR |= reuseaddr; //TODO: when sockopts fully impelmented just set to '='

	if (md->sockets[hdr->sock_index].out_ops->bind_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->bind_out)(module, hdr, &addr);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void listen_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	int backlog;
	uint8_t *pt;

	pt = buf;

	backlog = *(int *) pt;
	pt += sizeof(int);

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	PRINT_DEBUG("backlog=%d", backlog);

	if (md->sockets[hdr->sock_index].out_ops->listen_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->listen_out)(module, hdr, backlog);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void connect_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	socklen_t addrlen;
	struct sockaddr_storage addr;
	int flags;
	uint8_t *pt;

	pt = buf;

	addrlen = *(int *) pt;
	pt += sizeof(int);

	if (addrlen <= 0) {
		PRINT_ERROR("READING ERROR! CRASH, addrlen=%d", addrlen);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	memcpy(&addr, pt, addrlen);
	pt += addrlen;

	flags = *(int *) pt;
	pt += sizeof(int);

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	//PRINT_DEBUG("addr=%u/%d, family=%d, flags=0x%x", (addr->sin_addr).s_addr, ntohs(addr->sin_port), addr->sin_family, flags);

	if (md->sockets[hdr->sock_index].out_ops->connect_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->connect_out)(module, hdr, &addr, flags);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void accept_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint64_t sock_id_new;
	int sock_index_new;
	int flags;
	uint8_t *pt;

	pt = buf;

	sock_id_new = *(uint64_t *) pt;
	pt += sizeof(uint64_t);

	sock_index_new = *(int *) pt;
	pt += sizeof(int);

	flags = *(int *) pt;
	pt += sizeof(int);

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	PRINT_DEBUG("");

	if (md->sockets[hdr->sock_index].out_ops->accept_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->accept_out)(module, hdr, sock_id_new, sock_index_new, flags);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void getname_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	int peer;
	uint8_t *pt;

	pt = buf;

	peer = *(int *) pt;
	pt += sizeof(int);

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	PRINT_DEBUG("");

	if (md->sockets[hdr->sock_index].out_ops->getname_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->getname_out)(module, hdr, peer);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void ifr_total_test(struct if_record *ifr, uint32_t *total) {
	*total = *total + ifr->addr_list->len;
}

void ioctl_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int buf_len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, buf_len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint8_t *temp;
	int len;
	int msg_len = 0;
	uint8_t *msg = NULL;
	struct nl_daemon_to_wedge *hdr_ret;

	int32_t total = 0;
	struct if_record *ifr;
	struct addr_record *addr;
	struct sockaddr_in *addr4;
	struct ifreq *if_req;

	uint8_t *pt = buf;

	uint32_t cmd = *(uint32_t *) pt;
	pt += sizeof(uint32_t);

	switch (cmd) {
	case SIOCGIFCONF:
		PRINT_DEBUG("SIOCGIFCONF=%d", cmd);
		//TODO implement: http://lxr.linux.no/linux+v2.6.39.4/net/core/dev.c#L3919, http://lxr.linux.no/linux+v2.6.39.4/net/ipv4/devinet.c#L926
		len = *(int *) pt;
		pt += sizeof(int);

		if (pt - buf != buf_len) {
			PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, buf_len);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}

		PRINT_DEBUG("cmd=%d (SIOCGIFCONF), len=%d", cmd, len);

		msg_len = sizeof(struct nl_daemon_to_wedge) + sizeof(int) + len;
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct nl_daemon_to_wedge *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct nl_daemon_to_wedge);

		temp = pt; //store ptr to where total should be stored
		pt += sizeof(int);

		//TODO implement a looped version of this that's taken from where interface/device info will be stored
		//keep if_list sorted, go through list add ifr->addr_list->len to get size
		total = 0;
		struct linked_list *running_list = list_find_all(md->if_list, ifr_running_test);
		while (list_is_empty(running_list)) {
			ifr = (struct if_record *) list_remove_front(running_list);
			addr = (struct addr_record *) list_find(ifr->addr_list, addr_is_v4);
			if (addr != NULL) {
				if (total + sizeof(struct ifreq) <= len) {
					if_req = (struct ifreq *) pt;
					strcpy(if_req->ifr_name, (char *) ifr->name);

					addr4 = (struct sockaddr_in *) &if_req->ifr_addr;
					addr4->sin_family = AF_INET;
					addr4->sin_addr.s_addr = htonl(addr4_get_ip(&addr->ip));
					addr4->sin_port = 0;

					pt += sizeof(struct ifreq);
					total += sizeof(struct ifreq);
				} else {
					total = len;
					break;
				}
			}
		}
		list_free(running_list, nop_func);

		msg_len = sizeof(struct nl_daemon_to_wedge) + sizeof(int) + total;
		*(int *) temp = total;
		PRINT_DEBUG("total=%d (%d)", total, total/sizeof(struct ifreq));
		break;
	case SIOCGIFADDR:
		PRINT_DEBUG("SIOCGIFADDR=%d", cmd);

		len = *(int *) pt;
		pt += sizeof(int);

		temp = (uint8_t *) secure_malloc(len);
		memcpy(temp, pt, len);
		pt += len;

		if (pt - buf != buf_len) {
			PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, buf_len);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}

		PRINT_DEBUG("cmd=%d (SIOCGIFADDR), len=%d, temp='%s'", cmd, len, temp);

		ifr = (struct if_record *) list_find1(md->if_list, ifr_name_test, temp);
		if (ifr == NULL) {
			PRINT_ERROR("temp='%s'", temp);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EADDRNOTAVAIL);
			return;
		}

		msg_len = sizeof(struct nl_daemon_to_wedge) + sizeof(struct sockaddr_in);
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct nl_daemon_to_wedge *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct nl_daemon_to_wedge);

		addr4 = (struct sockaddr_in *) pt;
		addr4->sin_family = AF_INET;
		addr4->sin_port = 0;

		addr = (struct addr_record *) list_find(ifr->addr_list, addr_is_v4);
		if (addr != NULL) {
			addr4->sin_addr.s_addr = htonl(addr4_get_ip(&addr->ip));
		} else {
			PRINT_ERROR("todo error");
			addr4->sin_addr.s_addr = 0;
		}
		pt += sizeof(struct sockaddr_in);

		PRINT_DEBUG("temp='%s', addr=%s/%d", temp, inet_ntoa(addr4->sin_addr), addr4->sin_port);

		free(temp);
		break;
	case SIOCGIFDSTADDR:
		PRINT_DEBUG("SIOCGIFDSTADDR=%d", cmd);
		len = *(int *) pt;
		pt += sizeof(int);

		temp = (uint8_t *) secure_malloc(len);
		memcpy(temp, pt, len);
		pt += len;

		if (pt - buf != buf_len) {
			PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, buf_len);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}

		PRINT_DEBUG("cmd=%d (SIOCGIFDSTADDR), len=%d, temp='%s'", cmd, len, temp);

		ifr = (struct if_record *) list_find1(md->if_list, ifr_name_test, temp);
		if (ifr == NULL) {
			PRINT_ERROR("temp='%s'", temp);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EADDRNOTAVAIL);
			return;
		}

		msg_len = sizeof(struct nl_daemon_to_wedge) + sizeof(struct sockaddr_in);
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct nl_daemon_to_wedge *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct nl_daemon_to_wedge);

		addr4 = (struct sockaddr_in *) pt;
		addr4->sin_family = AF_INET;
		addr4->sin_port = 0;

		addr = (struct addr_record *) list_find(ifr->addr_list, addr_is_v4);
		if (addr != NULL) {
			addr4->sin_addr.s_addr = htonl(addr4_get_ip(&addr->ip));
		} else {
			PRINT_ERROR("todo error");
			addr4->sin_addr.s_addr = 0;
		}
		pt += sizeof(struct sockaddr_in);

		PRINT_DEBUG("temp='%s', addr=%s/%d", temp, inet_ntoa(addr4->sin_addr), addr4->sin_port);

		free(temp);
		break;
	case SIOCGIFBRDADDR:
		PRINT_DEBUG("SIOCGIFBRDADDR=%d", cmd);
		len = *(int *) pt;
		pt += sizeof(int);

		temp = (uint8_t *) secure_malloc(len);
		memcpy(temp, pt, len);
		pt += len;

		if (pt - buf != buf_len) {
			PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, buf_len);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}

		PRINT_DEBUG("cmd=%d (SIOCGIFBRDADDR), len=%d, temp='%s'", cmd, len, temp);

		ifr = (struct if_record *) list_find1(md->if_list, ifr_name_test, temp);
		if (ifr == NULL) {
			PRINT_ERROR("temp='%s'", temp);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EADDRNOTAVAIL);
			return;
		}

		msg_len = sizeof(struct nl_daemon_to_wedge) + sizeof(struct sockaddr_in);
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct nl_daemon_to_wedge *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct nl_daemon_to_wedge);

		addr4 = (struct sockaddr_in *) pt;
		addr4->sin_family = AF_INET;
		addr4->sin_port = 0;

		addr = (struct addr_record *) list_find(ifr->addr_list, addr_is_v4);
		if (addr != NULL) {
			addr4->sin_addr.s_addr = htonl(addr4_get_ip(&addr->bdc));
		} else {
			PRINT_ERROR("todo error");
			addr4->sin_addr.s_addr = 0;
		}
		pt += sizeof(struct sockaddr_in);

		PRINT_DEBUG("temp='%s', addr=%s/%d", temp, inet_ntoa(addr4->sin_addr), addr4->sin_port);

		free(temp);
		break;
	case SIOCGIFNETMASK:
		PRINT_DEBUG("SIOCGIFNETMASK=%d", cmd);
		len = *(int *) pt;
		pt += sizeof(int);

		temp = (uint8_t *) secure_malloc(len);
		memcpy(temp, pt, len);
		pt += len;

		if (pt - buf != buf_len) {
			PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, buf_len);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}

		PRINT_DEBUG("cmd=%d (SIOCGIFNETMASK), len=%d, temp='%s'", cmd, len, temp);

		ifr = (struct if_record *) list_find1(md->if_list, ifr_name_test, temp);
		if (ifr == NULL) {
			PRINT_ERROR("temp='%s'", temp);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EADDRNOTAVAIL);
			return;
		}

		msg_len = sizeof(struct nl_daemon_to_wedge) + sizeof(struct sockaddr_in);
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct nl_daemon_to_wedge *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct nl_daemon_to_wedge);

		addr = (struct addr_record *) list_find(ifr->addr_list, addr_is_v4);
		if (addr != NULL) {
			addr4 = (struct sockaddr_in *) pt;
			addr4->sin_family = AF_INET;
			addr4->sin_addr.s_addr = htonl(addr4_get_ip(&addr->mask));
			addr4->sin_port = 0;
		} else {
			PRINT_ERROR("todo error");
		}
		pt += sizeof(struct sockaddr_in);

		PRINT_DEBUG("temp='%s', addr=%s/%d", temp, inet_ntoa(addr4->sin_addr), addr4->sin_port);

		free(temp);
		break;
	case FIONREAD:
		PRINT_DEBUG("FIONREAD=%d", cmd);
		msg_len = 0; //handle per socket/protocol

		PRINT_ERROR("todo");
		break;
	case TIOCOUTQ:
		PRINT_DEBUG("TIOCOUTQ=%d", cmd);
		PRINT_ERROR("todo");
		break;
		//case TIOCINQ: //equiv to FIONREAD??
	case SIOCGIFNAME:
		PRINT_DEBUG("SIOCGIFNAME=%d", cmd);
		len = *(int *) pt; //IFNAMSIZ
		pt += sizeof(int);

		total = *(int *) pt; //ifr_ifindex
		pt += sizeof(int);

		if (pt - buf != buf_len) {
			PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, buf_len);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}

		PRINT_DEBUG("cmd=%d (SIOCGIFNAME), index=%d", cmd, total);

		ifr = (struct if_record *) list_find1(md->if_list, ifr_index_test, &total);
		if (ifr == NULL) {
			PRINT_ERROR("index=%d", total);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EADDRNOTAVAIL);
			return;
		}

		msg_len = sizeof(struct nl_daemon_to_wedge) + len;
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct nl_daemon_to_wedge *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct nl_daemon_to_wedge);

		memcpy(pt, ifr->name, len);
		pt += len;

		PRINT_DEBUG("index=%d, name='%s'", total, ifr->name);
		break;
	case SIOCGIFFLAGS:
		PRINT_DEBUG("SIOCGIFFLAGS=%d", cmd);
		len = *(int *) pt;
		pt += sizeof(int);

		temp = (uint8_t *) secure_malloc(len);
		memcpy(temp, pt, len);
		pt += len;

		if (pt - buf != buf_len) {
			PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, buf_len);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}

		PRINT_DEBUG("cmd=%d (SIOCGIFFLAGS), len=%d, temp='%s'", cmd, len, temp);

		ifr = (struct if_record *) list_find1(md->if_list, ifr_name_test, temp);
		if (ifr == NULL) {
			PRINT_ERROR("temp='%s'", temp);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EADDRNOTAVAIL);
			return;
		}

		msg_len = sizeof(struct nl_daemon_to_wedge) + sizeof(int);
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct nl_daemon_to_wedge *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct nl_daemon_to_wedge);

		*(int *) pt = ifr->flags;
		pt += sizeof(int);

		PRINT_DEBUG("temp='%s', ifr_flags=0x%x", temp, ifr->flags);

		free(temp);
		break;
	case SIOCSIFFLAGS:
		PRINT_DEBUG("SIOCSIFFLAGS=%d", cmd);
		PRINT_ERROR("todo");
		break;
	case SIOCGIFMTU:
		PRINT_DEBUG("SIOCGIFMTU=%d", cmd);
		len = *(int *) pt;
		pt += sizeof(int);

		temp = (uint8_t *) secure_malloc(len);
		memcpy(temp, pt, len);
		pt += len;

		if (pt - buf != buf_len) {
			PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, buf_len);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}

		PRINT_DEBUG("cmd=%d (SIOCGIFMTU), len=%d, temp='%s'", cmd, len, temp);

		ifr = (struct if_record *) list_find1(md->if_list, ifr_name_test, temp);
		if (ifr == NULL) {
			PRINT_ERROR("temp='%s'", temp);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, EADDRNOTAVAIL);
			return;
		}

		msg_len = sizeof(struct nl_daemon_to_wedge) + sizeof(int);
		msg = (uint8_t *) secure_malloc(msg_len);

		hdr_ret = (struct nl_daemon_to_wedge *) msg;
		hdr_ret->call_type = hdr->call_type;
		hdr_ret->call_id = hdr->call_id;
		hdr_ret->call_index = hdr->call_index;
		hdr_ret->ret = ACK;
		hdr_ret->msg = 0;
		pt = msg + sizeof(struct nl_daemon_to_wedge);

		*(int *) pt = ifr->mtu;
		pt += sizeof(int);

		PRINT_DEBUG("temp='%s', ifr_mtu=%d", temp, ifr->mtu);

		free(temp);
		break;
	case SIOCADDRT:
		PRINT_DEBUG("SIOCADDRT=%d", cmd);
		PRINT_ERROR("todo");
		break;
	case SIOCDELRT:
		PRINT_DEBUG("SIOCDELRT=%d", cmd);
		PRINT_ERROR("todo");
		break;
	case SIOCSIFADDR:
		PRINT_DEBUG("SIOCSIFADDR=%d", cmd);
		PRINT_ERROR("todo");
		break;
		//case SIOCAIPXITFCRT:
		//case SIOCAIPXPRISLT:
		//case SIOCIPXCFGDATA:
		//case SIOCIPXNCPCONN:
	case SIOCGSTAMP:
		PRINT_DEBUG("SIOCGSTAMP=%d", cmd);
		PRINT_ERROR("todo");
		break;
	case SIOCSIFDSTADDR:
		PRINT_DEBUG("SIOCSIFDSTADDR=%d", cmd);
		PRINT_ERROR("todo");
		break;
	case SIOCSIFBRDADDR:
		PRINT_DEBUG("SIOCSIFBRDADDR=%d", cmd);
		PRINT_ERROR("todo");
		break;
	case SIOCSIFNETMASK:
		PRINT_DEBUG("SIOCSIFNETMASK=%d", cmd);
		PRINT_ERROR("todo");
		break;
	default:
		PRINT_ERROR("default: cmd=%d", cmd);
		break;
	}

	PRINT_DEBUG("msg_len=%d, msg='%s'", msg_len, msg);
	if (msg_len != 0) {
		if (pt - msg != msg_len) {
			PRINT_ERROR("write error: diff=%d, len=%d", pt - msg, msg_len);
			free(msg);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
			return;
		}

		if (send_wedge(module, msg, msg_len, 0)) {
			PRINT_ERROR("Exiting, fail send_wedge: sock_id=%llu", hdr->sock_id);
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		}
		free(msg);
	} else {
		if (md->sockets[hdr->sock_index].out_ops->ioctl_out != NULL) {
			(md->sockets[hdr->sock_index].out_ops->ioctl_out)(module, hdr, cmd, buf, buf_len);
		} else {
			PRINT_ERROR("todo error");
			nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		}
	}
}

void sendmsg_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint32_t sk_flags;
	int timestamp;
	int addr_len;
	struct sockaddr_storage addr;
	uint32_t msg_flags;
	uint32_t msg_controllen;
	void *msg_control = NULL;
	uint32_t msg_len;
	uint8_t *msg = NULL;
	uint8_t *pt;

	pt = buf;

	sk_flags = *(uint32_t *) pt;
	pt += sizeof(uint32_t);
	timestamp = sk_flags & ((1 << SOCK_TIMESTAMP) | (1 << SOCK_RCVTSTAMP));

	addr_len = *(int *) pt;
	pt += sizeof(int);

	memcpy(&addr, pt, addr_len);
	pt += addr_len;

	msg_flags = *(uint32_t *) pt;
	pt += sizeof(uint32_t);

	msg_controllen = *(uint32_t *) pt;
	pt += sizeof(uint32_t);

	if (msg_controllen) {
		msg_control = secure_malloc(msg_controllen);
		memcpy(msg_control, pt, msg_controllen);
		pt += msg_controllen;
	}

	msg_len = *(uint32_t *) pt;
	pt += sizeof(uint32_t);

	if (msg_len) {
		msg = (uint8_t *) secure_malloc(msg_len);
		memcpy(msg, pt, msg_len);
		pt += msg_len;
	}

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		if (msg_controllen)
			free(msg_control);
		if (msg_len)
			free(msg);
		return;
	}

	PRINT_DEBUG("");
	md->sockets[hdr->sock_index].sockopts.FSO_TIMESTAMP |= timestamp;

	//#########################
#ifdef DEBUG
	uint8_t *temp = (uint8_t *) secure_malloc(msg_len + 1);
	memcpy(temp, msg, msg_len);
	temp[msg_len] = '\0';
	PRINT_DEBUG("msg='%s'", temp);
	free(temp);
	//#########################
	uint8_t *temp2 = (uint8_t *) secure_malloc(msg_controllen + 1);
	memcpy(temp2, msg_control, msg_controllen);
	temp2[msg_controllen] = '\0';
	PRINT_DEBUG("msg_control='%s'", temp2);
	free(temp2);
#endif
	//#########################

	if (md->sockets[hdr->sock_index].out_ops->sendmsg_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->sendmsg_out)(module, hdr, msg, msg_len, msg_flags, &addr, addr_len);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		if (msg_len) {
			free(msg);
		}
	}

	if (msg_controllen) {
		free(msg_control);
	}
}

void recvmsg_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint32_t sk_flags;
	int timestamp;
	int msg_len;
	uint32_t msg_controllen;
	int flags;
	uint8_t *pt;

	pt = buf;

	sk_flags = *(uint32_t *) pt;
	pt += sizeof(uint32_t);
	timestamp = sk_flags & ((1 << SOCK_TIMESTAMP) | (1 << SOCK_RCVTSTAMP)); //TODO remove rcvtstamp? or figure out/expand

	msg_len = *(int *) pt; //check on not in original socket_interceptor: recvmsg
	pt += sizeof(int);

	msg_controllen = *(uint32_t *) pt;
	pt += sizeof(uint32_t);

	flags = *(int *) pt;
	pt += sizeof(int);

	/*
	 msg_flags = *(uint32_t *) pt; //TODO remove, set when returning
	 pt += sizeof(uint32_t);

	 if (msg_controllen) {	//TODO send msg_controllen?
	 msg_control = (uint8_t *) secure_malloc(msg_controllen);
	 memcpy(msg_control, pt, msg_controllen);
	 pt += msg_controllen;
	 }
	 */

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		//if (msg_controllen) {
		//	free(msg_control);
		//}

		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	PRINT_DEBUG("flags=0x%x", flags);

	/** Notice that send is only used with tcp connections since
	 * the receiver is already known
	 */
	md->sockets[hdr->sock_index].sockopts.FSO_TIMESTAMP |= timestamp;

	if (md->sockets[hdr->sock_index].out_ops->recvmsg_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->recvmsg_out)(module, hdr, msg_len, msg_controllen, flags);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}

	//if (msg_controllen) {
	//	free(msg_control);
	//}
}

void getsockopt_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	int level;
	int optname;
	int optlen;
	uint8_t *optval = NULL;
	uint8_t *pt;

	pt = buf;

	level = *(int *) pt;
	pt += sizeof(int);

	optname = *(int *) pt;
	pt += sizeof(int);

	optlen = *(int *) pt;
	pt += sizeof(int);

	if (optlen > 0) { //TODO remove?
		optval = (uint8_t *) secure_malloc(optlen);
		memcpy(optval, pt, optlen);
		pt += optlen;
	}

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		if (optlen > 0) {
			free(optval);
		}
		return;
	}

	PRINT_DEBUG("");

	if (md->sockets[hdr->sock_index].out_ops->getsockopt_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->getsockopt_out)(module, hdr, level, optname, optlen, optval);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		if (optlen > 0) {
			free(optval);
		}
	}
}

void setsockopt_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	int level;
	int optname;
	int optlen;
	uint8_t *optval = NULL;
	uint8_t *pt;

	pt = buf;

	level = *(int *) pt;
	pt += sizeof(int);

	optname = *(int *) pt;
	pt += sizeof(int);

	optlen = (int) (*(uint32_t *) pt);
	pt += sizeof(uint32_t);

	if (optlen > 0) {
		optval = (uint8_t *) secure_malloc(optlen);
		memcpy(optval, pt, optlen);
		pt += optlen;
	}

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		if (optlen > 0) {
			free(optval);
		}
		return;
	}

	PRINT_DEBUG("");
	if (md->sockets[hdr->sock_index].out_ops->setsockopt_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->setsockopt_out)(module, hdr, level, optname, optlen, optval);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		if (optlen > 0) {
			free(optval);
		}
	}
}

void release_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint8_t *pt;

	pt = buf;

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	if (md->sockets[hdr->sock_index].out_ops->release_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->release_out)(module, hdr);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void poll_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint8_t *pt;
	int events;

	pt = buf;

	events = *(int *) pt;
	pt += sizeof(int);

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, POLLERR);
		return;
	}

	//TODO remove!!! check if this is how it should be
	if (md->sockets[hdr->sock_index].sock_id != hdr->sock_id) {
		PRINT_DEBUG("invalid socket: sock_id=%llu, sock_index=%d, call_pid=%d,  call_type=%u, call_id=%u, call_index=%d",
				hdr->sock_id, hdr->sock_index, hdr->call_pid, hdr->call_type, hdr->call_id, hdr->call_index);
		ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, POLLNVAL); //TODO check value?
		return;
	}
	if (md->sockets[hdr->sock_index].out_ops->poll_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->poll_out)(module, hdr, events);
	} else {
		PRINT_ERROR("todo error");
		ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, POLLERR);
	}
}

void mmap_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	uint8_t *pt;
	pt = buf;

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	if (md->sockets[hdr->sock_index].out_ops->mmap_out != NULL) {
		//(md->sockets[hdr->sock_index].out_ops->mmap_out)(module, hdr);
		PRINT_DEBUG("implement mmap_icmp");
		ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void socketpair_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);

}

void shutdown_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);
	struct daemon_data *md = (struct daemon_data *) module->data;

	int how;
	uint8_t *pt;

	pt = buf;

	how = *(int *) pt;
	pt += sizeof(int);

	if (pt - buf != len) {
		PRINT_ERROR("READING ERROR! CRASH, diff=%d, len=%d", pt - buf, len);
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
		return;
	}

	if (md->sockets[hdr->sock_index].out_ops->shutdown_out != NULL) {
		(md->sockets[hdr->sock_index].out_ops->shutdown_out)(module, hdr, how);
	} else {
		PRINT_ERROR("todo error");
		nack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 1);
	}
}

void close_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {
	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);

	daemon_sockets_remove(module, hdr->sock_index);

	PRINT_DEBUG("");

	ack_send(module, hdr->call_id, hdr->call_index, hdr->call_type, 0);

	/**
	 * TODO Fix the problem with terminate queue which goes into infinite loop
	 * when close is called
	 */
}

void sendpage_out(struct fins_module *module, struct nl_wedge_to_daemon *hdr, uint8_t *buf, int len) {

	PRINT_DEBUG("Entered: hdr=%p, len=%d", hdr, len);

}
