/**
 * @file tcpHandling.c
 *
 *  @date Nov 28, 2010
 *      @author Abdallah Abdallah
 */

#include "tcpHandling.h"
#include "finstypes.h"

extern sem_t jinniSockets_sem;
extern struct finssocket jinniSockets[MAX_sockets];

extern int thread_count;
extern sem_t thread_sem;

extern finsQueue Jinni_to_Switch_Queue;
extern finsQueue Switch_to_Jinni_Queue;
extern sem_t Jinni_to_Switch_Qsem;
extern sem_t Switch_to_Jinni_Qsem;

int serial_num = 0;

static struct finsFrame *get_fake_frame() {

	struct finsFrame *f = (struct finsFrame *) malloc(sizeof(struct finsFrame));
	PRINT_DEBUG("2.1");

	int linkvalue = 80211;
	char linkname[] = "linklayer";
	unsigned char *fakeData = (unsigned char *) malloc(10);
	strncpy(fakeData, "loloa7aa7a", 10);
	//fakeData = "loloa7aa7a";

	metadata *metaptr = (metadata *) malloc(sizeof(metadata));

	PRINT_DEBUG("2.2");
	//	metadata_create(metaptr);
	PRINT_DEBUG("2.3");
	//	metadata_addElement(metaptr,linkname,META_TYPE_INT);
	PRINT_DEBUG("2.4");
	//	metadata_writeToElement(metaptr,linkname,&linkvalue,META_TYPE_INT);
	PRINT_DEBUG("2.5");
	f->dataOrCtrl = DATA;
	f->destinationID.id = (unsigned char) SOCKETSTUBID;
	f->destinationID.next = NULL;

	(f->dataFrame).directionFlag = UP;
	//	(f->dataFrame).metaData		= metaptr;
	(f->dataFrame).metaData = NULL;
	(f->dataFrame).pdu = fakeData;
	(f->dataFrame).pduLength = 10;

	return (f);

}

/**
 *  Functions interfacing socketjinni_TCP with FINS core
 *
 */

int TCPreadFrom_fins(unsigned long long uniqueSockID, u_char *buf, int *buflen, int symbol, struct sockaddr_in *address, int block_flag) {

	/**TODO MUST BE FIXED LATER
	 * force symbol to become zero
	 */
	//symbol = 0;
	struct finsFrame *ff = NULL;
	int index;
	uint16_t srcport;
	uint32_t srcip;
	struct sockaddr_in * addr_in = (struct sockaddr_in *) address;

	PRINT_DEBUG("");
	sem_wait(&jinniSockets_sem);
	index = findjinniSocket(uniqueSockID);

	PRINT_DEBUG("index = %d", index);
	sem_post(&jinniSockets_sem);

	/**
	 * It keeps looping as a bad method to implement the blocking feature
	 * of recvfrom. In case it is not blocking then the while loop should
	 * be replaced with only a single trial !
	 *
	 */

	PRINT_DEBUG();
	if (block_flag == 1) {
		PRINT_DEBUG();
		/**
		 * WE Must FINS another way to emulate the blocking.
		 * The best suggestion is to use a pipeline to push the data in
		 * instead of the data queue
		 */
		do {
			PRINT_DEBUG("");
			sem_wait(&jinniSockets_sem);
			if (jinniSockets[index].uniqueSockID != uniqueSockID) {
				PRINT_DEBUG("Socket closed, canceling read block.");
				sem_post(&jinniSockets_sem);
				return (0);
			}
			sem_wait(&(jinniSockets[index].Qs));
			//		PRINT_DEBUG();

			ff = read_queue(jinniSockets[index].dataQueue);
			//	ff = get_fake_frame();
			//					PRINT_DEBUG();

			sem_post(&(jinniSockets[index].Qs));
			PRINT_DEBUG("");
			sem_post(&jinniSockets_sem);
		} while (ff == NULL);PRINT_DEBUG();

	} else {
		PRINT_DEBUG();

		sem_wait(&jinniSockets_sem);
		if (jinniSockets[index].uniqueSockID != uniqueSockID) {
			PRINT_DEBUG("Socket closed, canceling read block.");
			sem_post(&jinniSockets_sem);
			return (0);
		}
		sem_wait(&(jinniSockets[index].Qs));
		//ff= read_queue(jinniSockets[index].dataQueue);
		/**	ff = get_fake_frame();
		 print_finsFrame(ff); */
		ff = read_queue(jinniSockets[index].dataQueue);

		sem_post(&(jinniSockets[index].Qs));
		sem_post(&jinniSockets_sem);
	}

	if (ff == NULL) {
		//free(ff);
		return (0);
	}

	PRINT_DEBUG("PDU length %d", ff->dataFrame.pduLength);

	if (metadata_readFromElement(ff->dataFrame.metaData, "src_port", (uint16_t *) &srcport) == 0) {
		addr_in->sin_port = 0;

	}
	if (metadata_readFromElement(ff->dataFrame.metaData, "src_ip", (uint32_t *) &srcip) == 0) {
		addr_in->sin_addr.s_addr = 0;

	}

	PRINT_DEBUG("");
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("Socket closed, canceling read block.");
		sem_post(&jinniSockets_sem);
		return (0);
	}
	if (jinniSockets[index].connection_status > 0) {

		if ((srcport != jinniSockets[index].dstport) || (srcip != jinniSockets[index].dst_IP)) {

			PRINT_DEBUG("Wrong address, the socket is already connected to another destination");
			sem_post(&jinniSockets_sem);
			return (0);

		}
	}PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	//*buf = (u_char *)malloc(sizeof(ff->dataFrame.pduLength));
	//memcpy(*buf,ff->dataFrame.pdu,ff->dataFrame.pduLength);
	memcpy(buf, ff->dataFrame.pdu, ff->dataFrame.pduLength);
	*buflen = ff->dataFrame.pduLength;

	PRINT_DEBUG();

	if (symbol == 0) {
		//		address = NULL;
		PRINT_DEBUG();
		//	freeFinsFrame(ff);

		return (1);
	}PRINT_DEBUG();

	addr_in->sin_port = srcport;
	addr_in->sin_addr.s_addr = srcip;

	/**TODO Free the finsFrame
	 * This is the final consumer
	 * call finsFrame_free(Struct finsFrame** ff)
	 */
	PRINT_DEBUG();

	//freeFinsFrame(ff);

	/** Finally succeeded
	 *
	 */
	return (1);

} //end of readFrom_fins

int jinni_TCP_to_fins(u_char *dataLocal, int len, uint16_t dstport, uint32_t dst_IP_netformat, uint16_t hostport, uint32_t host_IP_netformat, int block_flag) {

	struct finsFrame *ff = (struct finsFrame *) malloc(sizeof(struct finsFrame));

	metadata *tcpout_meta = (metadata *) malloc(sizeof(metadata));

	PRINT_DEBUG();

	metadata_create(tcpout_meta);

	if (tcpout_meta == NULL) {
		PRINT_DEBUG("metadata creation failed");
		free(ff);
		return 0;
	}

	/** metadata_writeToElement() set the value of an element if it already exist
	 * or it creates the element and set its value in case it is new
	 */
	PRINT_DEBUG("%d, %d, %d, %d", dstport, dst_IP_netformat, hostport, host_IP_netformat);

	uint32_t dstprt = dstport;
	uint32_t hostprt = hostport;

	metadata_writeToElement(tcpout_meta, "dst_port", &dstprt, META_TYPE_INT);
	metadata_writeToElement(tcpout_meta, "src_port", &hostprt, META_TYPE_INT);
	metadata_writeToElement(tcpout_meta, "dst_ip", &dst_IP_netformat, META_TYPE_INT);
	metadata_writeToElement(tcpout_meta, "src_ip", &host_IP_netformat, META_TYPE_INT);
	metadata_writeToElement(tcpout_meta, "blockflag", &block_flag, META_TYPE_INT);

	ff->dataOrCtrl = DATA;
	/**TODO get the address automatically by searching the local copy of the
	 * switch table
	 */
	ff->destinationID.id = TCPID;
	ff->destinationID.next = NULL;
	(ff->dataFrame).directionFlag = DOWN;
	(ff->dataFrame).pduLength = len;
	(ff->dataFrame).pdu = dataLocal;
	(ff->dataFrame).metaData = tcpout_meta;

	/**TODO insert the frame into jinni_to_switch queue
	 * check if insertion succeeded or not then
	 * return 1 on success, or -1 on failure
	 * */
	PRINT_DEBUG("");
	sem_wait(&Jinni_to_Switch_Qsem);
	if (write_queue(ff, Jinni_to_Switch_Queue)) {

		sem_post(&Jinni_to_Switch_Qsem);
		PRINT_DEBUG("");
		return (1);
	}
	sem_post(&Jinni_to_Switch_Qsem);
	PRINT_DEBUG("");

	return (0);

}

int jinni_TCP_to_fins_cntrl(uint16_t opcode, metadata *params) {
	struct finsFrame *ff = (struct finsFrame *) malloc(sizeof(struct finsFrame));
	if (ff == NULL) {
		PRINT_DEBUG("ff creation failed");
		return 0;
	}

	ff->dataOrCtrl = CONTROL;
	ff->destinationID.id = TCPID; //TODO get the address from local copy of switch table
	ff->destinationID.next = NULL;
	ff->ctrlFrame.senderID = JINNIID;
	ff->ctrlFrame.serialNum = serial_num++;
	ff->ctrlFrame.opcode = opcode;
	ff->ctrlFrame.metaData = params;

	//ff->ctrlFrame.paramterID = command;
	//ff->ctrlFrame.paramterValue = data;
	//ff->ctrlFrame.paramterLen = len;

	PRINT_DEBUG("");
	sem_wait(&Jinni_to_Switch_Qsem);
	if (write_queue(ff, Jinni_to_Switch_Queue)) {
		sem_post(&Jinni_to_Switch_Qsem);
		PRINT_DEBUG("");

		return (1);
	} else {
		sem_post(&Jinni_to_Switch_Qsem);
		PRINT_DEBUG("");

		return (0);
	}
}

void socket_tcp(int domain, int type, int protocol, unsigned long long uniqueSockID) {
	int index;

	PRINT_DEBUG("socket_tcp CALL");
	sem_wait(&jinniSockets_sem);
	index = insertjinniSocket(uniqueSockID, type, protocol);
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	if (index < 0) {
		PRINT_DEBUG("incorrect index !! Crash");
		nack_send(uniqueSockID, socket_call);
		return;
	}PRINT_DEBUG("0000");

	ack_send(uniqueSockID, socket_call);
	PRINT_DEBUG("0003");
}

void bind_tcp(int index, unsigned long long uniqueSockID, struct sockaddr_in *addr) {

	uint32_t host_ip_netw;
	uint32_t host_ip;
	uint16_t host_port;

	PRINT_DEBUG("bind_TCP CALL");

	if (addr->sin_family != AF_INET) {
		PRINT_DEBUG("Wrong address family=%d", addr->sin_family);
		nack_send(uniqueSockID, bind_call);
		return;
	}

	/** TODO fix host port below, it is not initialized with any variable !!! */
	/** the check below is to make sure that the port is not previously allocated */
	host_ip_netw = addr->sin_addr.s_addr;
	host_ip = ntohl(addr->sin_addr.s_addr);
	host_port = ntohs(addr->sin_port);

	PRINT_DEBUG("bind address: host=%d(%s)/%d host_IP_netformat=%d", host_ip, inet_ntoa(addr->sin_addr), host_port, host_ip_netw);

	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("socket descriptor not found into jinni sockets");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, bind_call);
		return;
	}

	/** check if the same port and address have been both used earlier or not
	 * it returns (-1) in case they already exist, so that we should not reuse them
	 * */
	if (!checkjinniports(host_port, host_ip_netw) && !jinniSockets[index].sockopts.FSO_REUSEADDR) {
		PRINT_DEBUG("this port is not free");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, bind_call);
		free(addr);
		return;
	}

	/** TODO lock and unlock the protecting semaphores before making
	 * any modifications to the contents of the jinniSockets database
	 */

	//jinniSockets[index].host_IP = host_IP_netw;
	jinniSockets[index].host_IP = host_ip;
	jinniSockets[index].hostport = host_port;
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	/** Reverse again because it was reversed by the application itself
	 * In our example it is not reversed */
	//jinniSockets[index].host_IP.s_addr = ntohl(jinniSockets[index].host_IP.s_addr);
	/** TODO convert back to the network endian form before
	 * sending to the fins core
	 */

	ack_send(uniqueSockID, bind_call);

	free(addr);
	return;

}

void listen_tcp(int index, unsigned long long uniqueSockID, int backlog) {
	uint32_t host_ip;
	uint16_t host_port;

	PRINT_DEBUG("listen_TCP CALL");

	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("socket descriptor not found into jinni sockets");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, listen_call);
		return;
	}

	jinniSockets[index].listening = 1;
	jinniSockets[index].backlog = backlog;

	host_ip = jinniSockets[index].host_IP;
	host_port = jinniSockets[index].hostport;
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	PRINT_DEBUG("listen address: host=%u/%d", host_ip, host_port);

	/** Keep all ports and addresses in host order until later  action taken
	 * in IPv4 module
	 *  */
	/** addresses are in host format given that there are by default already filled
	 * host IP and host port. Otherwise, a port and IP has to be assigned explicitly below */
	metadata *params = (metadata *) malloc(sizeof(metadata));
	if (params == NULL) {
		PRINT_DEBUG("metadata creation failed");
		nack_send(uniqueSockID, listen_call);
		return;
	}
	metadata_create(params);

	int status = 0;
	metadata_writeToElement(params, "status", &status, META_TYPE_INT);

	uint32_t exec_call = EXEC_TCP_LISTEN;
	metadata_writeToElement(params, "exec_call", &exec_call, META_TYPE_INT);
	metadata_writeToElement(params, "host_ip", &host_ip, META_TYPE_INT);
	metadata_writeToElement(params, "host_port", &host_port, META_TYPE_INT);
	metadata_writeToElement(params, "backlog", &backlog, META_TYPE_INT);

	if (jinni_TCP_to_fins_cntrl(CTRL_EXEC, params)) {
		ack_send(uniqueSockID, listen_call);
	} else {
		PRINT_DEBUG("socketjinni failed to accomplish listen");
		nack_send(uniqueSockID, listen_call);
		metadata_destroy(params);
	}
}

void *connect_tcp_thread(void *local) {
	struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) local;
	int id = thread_data->id;
	int index = thread_data->index;
	unsigned long long uniqueSockID = thread_data->uniqueSockID;
	int flags = thread_data->flags;
	free(thread_data);

	int block_flag = 1; //TODO get from flags
	uint32_t exec_call = 0;
	uint32_t ret_val = 0;

	PRINT_DEBUG("connect_tcp_thread: Entered: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
	/*
	 sem_wait(&jinniSockets_sem);
	 if (jinniSockets[index].uniqueSockID != uniqueSockID) {
	 PRINT_DEBUG("recvfrom_udp_thread: Exiting, socket closed: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
	 sem_post(&jinniSockets_sem);
	 pthread_exit(NULL);
	 }

	 PRINT_DEBUG("");
	 sem_post(&jinniSockets_sem);
	 */

	PRINT_DEBUG();
	struct finsFrame *ff = get_fcf(index, uniqueSockID, block_flag);
	PRINT_DEBUG("connect_tcp_thread: after get_fdf: id=%d index=%d uniqueSockID=%llu", id, index, uniqueSockID);
	if (ff == NULL) {
		PRINT_DEBUG("connect_tcp_thread: Exiting, socket closed: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		nack_send(uniqueSockID, connect_call);
		pthread_exit(NULL);
	}

	if (ff->ctrlFrame.opcode != CTRL_EXEC_REPLY || ff->ctrlFrame.metaData == NULL) {
		PRINT_DEBUG("connect_tcp_thread: Exiting, fcf errors: id=%d, index=%d, uniqueSockID=%llu opcode=%d, metaData=%d",
				id, index, uniqueSockID, ff->ctrlFrame.opcode, ff->ctrlFrame.metaData==NULL);
		nack_send(uniqueSockID, connect_call);
		PRINT_DEBUG("freeing ff=%d", (int)ff);
		freeFinsFrame(ff);
		pthread_exit(NULL);
	}

	int ret = 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "exec_call", &exec_call) == 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "ret_val", &ret_val) == 0;

	if (ret || (exec_call != EXEC_TCP_CONNECT && exec_call != EXEC_TCP_ACCEPT) || ret_val == 0) {
		PRINT_DEBUG("connect_tcp_thread: Exiting, meta errors: id=%d, index=%d, uniqueSockID=%llu, ret=%d, exec_call=%d, ret_val=%d",
				id, index, uniqueSockID, ret, exec_call, ret_val);
		nack_send(uniqueSockID, connect_call);
	} else {
		PRINT_DEBUG("connect_tcp_thread: Exiting, ACK: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		sem_wait(&jinniSockets_sem);
		if (jinniSockets[index].uniqueSockID != uniqueSockID) {
			PRINT_DEBUG("connect_tcp_thread: Exiting, socket closed: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
			sem_post(&jinniSockets_sem);
			freeFinsFrame(ff);
			pthread_exit(NULL);
		}
		jinniSockets[index].connection_status = 2;
		PRINT_DEBUG("");
		sem_post(&jinniSockets_sem);

		ack_send(uniqueSockID, connect_call);
	}

	PRINT_DEBUG("freeing ff=%d", (int)ff);
	freeFinsFrame(ff);
	pthread_exit(NULL);
}

void connect_tcp(int index, unsigned long long uniqueSockID, struct sockaddr_in *addr) {
	uint32_t host_ip_netw;
	uint32_t host_ip;
	uint16_t host_port;
	uint32_t rem_ip_netw;
	uint32_t rem_ip;
	uint16_t rem_port;

	if (addr->sin_family != AF_INET) {
		PRINT_DEBUG("Wrong address family");
		nack_send(uniqueSockID, connect_call);
		return;
	}

	/** TODO fix host port below, it is not initialized with any variable !!! */
	/** the check below is to make sure that the port is not previously allocated */
	rem_ip_netw = addr->sin_addr.s_addr;
	rem_ip = ntohl((addr->sin_addr).s_addr);
	rem_port = ntohs(addr->sin_port);

	/** check if the same port and address have been both used earlier or not
	 * it returns (-1) in case they already exist, so that we should not reuse them
	 * according to the RFC document and man pages: Application can call connect more than
	 * once over the same UDP socket changing the address from one to another. SO the assigning
	 * will take place even if the check functions returns (-1) !!!
	 * */

	/** TODO connect for UDP means that this address will be the default address to send
	 * to. BUT IT WILL BE ALSO THE ONLY ADDRESS TO RECEIVER FROM
	 * */

	/** Reverse again because it was reversed by the application itself */
	//hostport = ntohs(addr->sin_port);
	/** TODO lock and unlock the protecting semaphores before making
	 * any modifications to the contents of the jinniSockets database
	 */
	PRINT_DEBUG("%d,%d,%d", (addr->sin_addr).s_addr, ntohs(addr->sin_port), addr->sin_family);
	PRINT_DEBUG("connect_tcp address: rem=%d (%s)/%d rem_IP_netformat=%d", rem_ip, inet_ntoa(addr->sin_addr), rem_port, rem_ip_netw);

	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("socket removed/changed");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, connect_call);
		return;
	}

	/**
	 * NOTICE THAT the relation between the host and the destined address is many to one.
	 * more than one local socket maybe connected to the same destined address
	 */
	if (jinniSockets[index].connection_status > 0) {
		PRINT_DEBUG("old destined address %d, %d", jinniSockets[index].dst_IP, jinniSockets[index].dstport);
		PRINT_DEBUG("new destined address %d, %d", rem_ip, rem_port);

	}

	/**TODO check if the port is free for binding or previously allocated
	 * Current code assume that the port is authorized to be accessed
	 * and also available
	 * */

	jinniSockets[index].listening = 0;
	jinniSockets[index].dst_IP = rem_ip;
	jinniSockets[index].dstport = rem_port;
	jinniSockets[index].connection_status = 1;

	/**
	 * the current value of host_IP is zero but to be filled later with
	 * the current IP using the IPv4 modules unless a binding has occured earlier
	 */
	host_ip = jinniSockets[index].host_IP;

	/**
	 * Default current host port to be assigned is 58088
	 * It is supposed to be randomly selected from the range found in
	 * /proc/sys/net/ipv4/ip_local_port_range
	 * default range in Ubuntu is 32768 - 61000
	 * The value has been chosen randomly when the socket firstly inserted into the jinnisockets
	 * check insertjinniSocket(processid, sockfd, fakeID, type, protocol);
	 */
	host_port = jinniSockets[index].hostport;
	if (host_port == 0) {
		PRINT_DEBUG("");
		while (1) {
			host_port = (uint16_t) randoming(MIN_port, MAX_port);
			if (checkjinniports(host_port, host_ip_netw)) {
				break;
			}
		}PRINT_DEBUG("");
		jinniSockets[index].hostport = host_port;
	}PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	/** Reverse again because it was reversed by the application itself
	 * In our example it is not reversed */
	//jinniSockets[index].host_ip.s_addr = ntohl(jinniSockets[index].host_ip.s_addr);
	/** TODO convert back to the network endian form before
	 * sending to the fins core
	 */

	metadata *params = (metadata *) malloc(sizeof(metadata));
	if (params == NULL) {
		PRINT_DEBUG("metadata creation failed");
		nack_send(uniqueSockID, connect_call);
		return;
	}
	metadata_create(params);

	int status = 1;
	metadata_writeToElement(params, "status", &status, META_TYPE_INT);

	uint32_t exec_call = EXEC_TCP_CONNECT;
	metadata_writeToElement(params, "exec_call", &exec_call, META_TYPE_INT);
	metadata_writeToElement(params, "host_ip", &host_ip, META_TYPE_INT);
	metadata_writeToElement(params, "host_port", &host_port, META_TYPE_INT);
	metadata_writeToElement(params, "rem_ip", &rem_ip, META_TYPE_INT);
	metadata_writeToElement(params, "rem_port", &rem_port, META_TYPE_INT);

	if (jinni_TCP_to_fins_cntrl(CTRL_EXEC, params)) {
		pthread_t thread;
		struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) malloc(sizeof(struct jinni_tcp_thread_data));
		thread_data->id = thread_count++;
		thread_data->index = index;
		thread_data->uniqueSockID = uniqueSockID;

		//spin off thread to handle
		if (pthread_create(&thread, NULL, connect_tcp_thread, (void *) thread_data)) {
			PRINT_ERROR("ERROR: unable to create connect_tcp_thread thread.");
			nack_send(uniqueSockID, connect_call);
			free(thread_data);
			metadata_destroy(params);
		}
	} else {
		PRINT_DEBUG("socketjinni failed to accomplish connect");
		nack_send(uniqueSockID, connect_call);
		metadata_destroy(params);
	}

	free(addr);
}

void *accept_tcp_thread(void *local) {
	struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) local;
	int id = thread_data->id;
	int index = thread_data->index;
	unsigned long long uniqueSockID = thread_data->uniqueSockID;
	int flags = thread_data->flags;
	unsigned long long uniqueSockID_new = thread_data->uniqueSockID_new;
	free(thread_data);

	int block_flag = 1; //TODO get from flags

	uint32_t exec_call = 0;
	uint32_t ret_val = 0;
	uint32_t rem_ip = 0;
	uint16_t rem_port = 0;

	PRINT_DEBUG("accept_tcp_thread: Entered: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
	struct finsFrame *ff = get_fcf(index, uniqueSockID, block_flag);
	PRINT_DEBUG("accept_tcp_thread: after get_fdf: id=%d index=%d uniqueSockID=%llu", id, index, uniqueSockID);
	if (ff == NULL || ff->ctrlFrame.opcode != CTRL_EXEC_REPLY || ff->ctrlFrame.metaData == NULL) {
		PRINT_DEBUG("accept_tcp_thread: Exiting, No fdf/opcode/metadata: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		nack_send(uniqueSockID, accept_call);
		PRINT_DEBUG("freeing ff=%d", (int)ff);
		freeFinsFrame(ff);
		pthread_exit(NULL);
	}

	int ret = 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "exec_call", &exec_call) == 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "ret_val", &ret_val) == 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "rem_ip", &rem_ip) == 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "rem_port", &rem_port) == 0;

	if (ret || exec_call != EXEC_TCP_ACCEPT || ret_val == 0) {
		PRINT_DEBUG("accept_tcp_thread: Exiting, NACK: id=%d, index=%d, uniqueSockID=%llu, ret=%d, exec_call=%d, ret_val=%d",
				id, index, uniqueSockID, ret, exec_call, ret_val);
		nack_send(uniqueSockID, accept_call);
	} else {
		PRINT_DEBUG("");
		sem_wait(&jinniSockets_sem);
		if (jinniSockets[index].uniqueSockID != uniqueSockID) {
			PRINT_DEBUG("");
			sem_post(&jinniSockets_sem);

			PRINT_DEBUG("accept_tcp_thread: Exiting, socket closed: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
			nack_send(uniqueSockID, accept_call);
		} else {
			int index_new = insertjinniSocket(uniqueSockID_new, jinniSockets[index].type, jinniSockets[index].protocol);
			if (index_new < 0) {
				PRINT_DEBUG("incorrect index !! Crash");
				sem_post(&jinniSockets_sem);

				PRINT_DEBUG("accept_tcp_thread: Exiting, insert faile: id=%d, index=%d, uniqueSockID=%llu, uniqueSockID_new=%llu",
						id, index, uniqueSockID, uniqueSockID_new);
				nack_send(uniqueSockID, accept_call);
			} else {
				jinniSockets[index_new].host_IP = jinniSockets[index].host_IP;
				jinniSockets[index_new].hostport = jinniSockets[index].hostport;
				jinniSockets[index_new].dst_IP = rem_ip;
				jinniSockets[index_new].dstport = rem_port;

				jinniSockets[index_new].connection_status = 2;
				jinniSockets[index].connection_status = 0;

				PRINT_DEBUG("");
				sem_post(&jinniSockets_sem);

				PRINT_DEBUG("accept_tcp_thread: Exiting, ACK: id=%d, index=%d, uniqueSockID=%llu, uniqueSockID_new=%llu",
						id, index, uniqueSockID, uniqueSockID_new);
				ack_send(uniqueSockID, accept_call);
			}
		}
	}

	/**TODO Free the finsFrame
	 * This is the final consumer
	 * call finsFrame_free(Struct finsFrame** ff)
	 */
	PRINT_DEBUG();

	PRINT_DEBUG("freeing ff=%d", (int)ff);
	freeFinsFrame(ff);
	pthread_exit(NULL);
}

void accept_tcp(int index, unsigned long long uniqueSockID, unsigned long long uniqueSockID_new, int flags) {
	uint32_t host_ip;
	uint32_t host_port;
	int blocking_flag;

	PRINT_DEBUG("accept_tcp: Entered: index=%d, uniqueSockID=%llu, uniqueSockID_new=%llu, flags=%d", index, uniqueSockID, uniqueSockID_new, flags);
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("socket removed/changed");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, accept_call);
		return;
	}

	if (!jinniSockets[index].listening) {
		PRINT_DEBUG("socket not listening");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, accept_call);
		return;

	}

	host_ip = jinniSockets[index].host_IP;
	host_port = (uint32_t) jinniSockets[index].hostport;
	blocking_flag = jinniSockets[index].blockingFlag;
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	PRINT_DEBUG("accept address: host=%u/%d", host_ip, host_port);

	//TODO process flags?

	metadata *params = (metadata *) malloc(sizeof(metadata));
	if (params == NULL) {
		PRINT_DEBUG("metadata creation failed");

		nack_send(uniqueSockID, accept_call);
		return;
	}
	metadata_create(params);

	int status = 0;
	metadata_writeToElement(params, "status", &status, META_TYPE_INT);

	uint32_t exec_call = EXEC_TCP_ACCEPT;
	metadata_writeToElement(params, "exec_call", &exec_call, META_TYPE_INT);
	metadata_writeToElement(params, "host_ip", &host_ip, META_TYPE_INT);
	metadata_writeToElement(params, "host_port", &host_port, META_TYPE_INT);
	metadata_writeToElement(params, "flags", &flags, META_TYPE_INT);

	if (jinni_TCP_to_fins_cntrl(CTRL_EXEC, params)) {
		pthread_t thread;
		struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) malloc(sizeof(struct jinni_tcp_thread_data));
		thread_data->id = thread_count++;
		thread_data->index = index;
		thread_data->uniqueSockID = uniqueSockID;
		thread_data->flags = 0; //TODO implement?
		thread_data->uniqueSockID_new = uniqueSockID_new;

		//spin off thread to handle
		if (pthread_create(&thread, NULL, accept_tcp_thread, (void *) thread_data)) {
			PRINT_ERROR("ERROR: unable to create accept_tcp_thread thread.");
			nack_send(uniqueSockID, accept_call);
			free(thread_data);
			metadata_destroy(params);
		}
	} else {
		PRINT_DEBUG("socketjinni failed to accomplish accept");
		nack_send(uniqueSockID, accept_call);
		metadata_destroy(params);
	}
}

void getname_tcp(int index, unsigned long long uniqueSockID, int peer) {
	int status;
	uint32_t host_ip = 0;
	uint16_t host_port = 0;
	uint32_t rem_ip = 0;
	uint16_t rem_port = 0;

	PRINT_DEBUG("getname_tcp CALL");
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("socket descriptor not found into jinni sockets");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, getname_call);
		return;
	}

	if (peer == 1) { //TODO find right number
		host_ip = jinniSockets[index].host_IP;
		host_port = jinniSockets[index].hostport;
	} else if (peer == 2) {
		status = jinniSockets[index].connection_status;
		if (status) {
			rem_ip = jinniSockets[index].dst_IP;
			rem_port = jinniSockets[index].dstport;
		}
	}

	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	if (peer == 1) { //TODO find right number
		//getsockname
	} else if (peer == 2) {
		addr.sin_addr.s_addr = (uint32_t) htonl(rem_ip);
		addr.sin_port = (uint16_t) htons(rem_port);
	} else {
		//TODO ??
	}

	//#######
	PRINT_DEBUG("address: addr=%s/%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	//#######

	int msg_len = 4 * sizeof(int) + sizeof(unsigned long long) + sizeof(struct sockaddr_in);
	u_char *msg = (u_char *) malloc(msg_len);
	if (msg == NULL) {
		PRINT_DEBUG("getname_tcp: Exiting, msg creation fail: index=%d, uniqueSockID=%llu", index, uniqueSockID);
		nack_send(uniqueSockID, getname_call);
		return;
	}
	u_char *pt = msg;

	*(int *) pt = getname_call;
	pt += sizeof(int);

	*(unsigned long long *) pt = uniqueSockID;
	pt += sizeof(unsigned long long);

	*(int *) pt = ACK;
	pt += sizeof(int);

	*(int *) pt = peer;
	pt += sizeof(int);

	*(int *) pt = sizeof(struct sockaddr_in);
	pt += sizeof(int);

	memcpy(pt, &addr, sizeof(struct sockaddr_in));
	pt += sizeof(struct sockaddr_in);

	if (pt - msg != msg_len) {
		PRINT_DEBUG("write error: diff=%d len=%d\n", pt - msg, msg_len);
		free(msg);
		PRINT_DEBUG("getname_tcp: Exiting, No fdf: index=%d, uniqueSockID=%llu", index, uniqueSockID);
		nack_send(uniqueSockID, getname_call);
		return;
	}

	PRINT_DEBUG("msg_len=%d msg=%s", msg_len, msg);
	if (send_wedge(nl_sockfd, msg, msg_len, 0)) {
		PRINT_DEBUG("getname_tcp: Exiting, fail send_wedge: index=%d, uniqueSockID=%llu", index, uniqueSockID);
		nack_send(uniqueSockID, getname_call);
	} else {
		PRINT_DEBUG("getname_tcp: Exiting, normal: index=%d, uniqueSockID=%llu", index, uniqueSockID);
	}

	free(msg);
}

void write_tcp(int index, unsigned long long uniqueSockID, u_char *data, int data_len) {
	uint16_t hostport;
	uint16_t dstport;
	uint32_t host_IP;
	uint32_t dst_IP;
	int len = data_len;

	PRINT_DEBUG("");

	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("socket descriptor not found into jinni sockets");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, sendmsg_call);
		return;
	}

	/** copying the data passed to be able to free the old memory location
	 * the new created location is the one to be included into the newly created finsFrame*/
	PRINT_DEBUG("");
	/** check if this socket already connected to a destined address or not */

	if (jinniSockets[index].connection_status == 0) {
		/** socket is not connected to an address. Send call will fail */

		PRINT_DEBUG("socketjinni failed to accomplish send");
		nack_send(uniqueSockID, sendmsg_call);
	}

	/** Keep all ports and addresses in host order until later  action taken */
	dstport = jinniSockets[index].dstport;

	dst_IP = jinniSockets[index].dst_IP;

	//hostport = jinniSockets[index].hostport;
	//hostport = 3000;

	/** addresses are in host format given that there are by default already filled
	 * host IP and host port. Otherwise, a port and IP has to be assigned explicitly below */

	//hostport = jinniSockets[index].hostport;
	/**
	 * Default current host port to be assigned is 58088
	 * It is supposed to be randomly selected from the range found in
	 * /proc/sys/net/ipv4/ip_local_port_range
	 * default range in Ubuntu is 32768 - 61000
	 * The value has been chosen randomly when the socket firsly inserted into the jinnisockets
	 * check insertjinniSocket(processid, sockfd, fakeID, type, protocol);
	 */
	hostport = jinniSockets[index].hostport;
	/**
	 * the current value of host_IP is zero but to be filled later with
	 * the current IP using the IPv4 modules unless a binding has occured earlier
	 */
	host_IP = jinniSockets[index].host_IP;
	int block_flag = jinniSockets[index].blockingFlag;
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);
	PRINT_DEBUG("");

	PRINT_DEBUG("%d,%d,%d,%d", dst_IP, dstport, host_IP, hostport);
	//free(data);
	//free(addr);
	PRINT_DEBUG("");

	/**
	 * The meta-data paraters are all passes by copy starting from this point
	 */
	if (jinni_TCP_to_fins(data, len, dstport, dst_IP, hostport, host_IP, block_flag)) {
		PRINT_DEBUG("");
		/** TODO prevent the socket interceptor from holding this semaphore before we reach this point */
		ack_send(uniqueSockID, sendmsg_call);
		PRINT_DEBUG("");

	} else {
		PRINT_DEBUG("socketjinni failed to accomplish send");
		nack_send(uniqueSockID, sendmsg_call);
	}

	return;

}

void *sendmsg_tcp_thread(void *local) {
	struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) local;
	int id = thread_data->id;
	int index = thread_data->index;
	unsigned long long uniqueSockID = thread_data->uniqueSockID;
	//int block_flag = thread_data->blocking_flag;
	free(thread_data);

	uint32_t exec_call = 0;
	uint32_t ret_val = 0;

	PRINT_DEBUG("sendmsg_tcp_thread: Entered: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
	struct finsFrame *ff = get_fcf(index, uniqueSockID, 1);
	PRINT_DEBUG("sendmsg_tcp_thread: after get_fdf: id=%d index=%d uniqueSockID=%llu", id, index, uniqueSockID);
	if (ff == NULL || ff->ctrlFrame.opcode != CTRL_EXEC_REPLY || ff->ctrlFrame.metaData == NULL) {
		nack_send(uniqueSockID, sendmsg_call); //TODO check return of nonblocking send
		PRINT_DEBUG("freeing ff=%d", (int)ff);
		freeFinsFrame(ff);
		pthread_exit(NULL);
	}

	int ret = 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "exec_call", &exec_call) == 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "ret_val", &ret_val) == 0;

	if (ret || exec_call != EXEC_TCP_SEND || ret_val == 0) {
		nack_send(uniqueSockID, sendmsg_call);
	} else {
		ack_send(uniqueSockID, sendmsg_call);
	}

	PRINT_DEBUG("freeing ff=%d", (int)ff);
	freeFinsFrame(ff);
	pthread_exit(NULL);
}

void send_tcp(int index, unsigned long long uniqueSockID, u_char *data, int data_len, int flags) {

	//	sendto_tcp(senderid, sockfd, datalen, data, flags, NULL, 0);

	//	return;

	uint32_t host_ip;
	uint16_t host_port;
	uint32_t dst_ip;
	uint16_t dst_port;
	int len = data_len;

	if (flags == -1000) {
		return (write_tcp(index, uniqueSockID, data, data_len)); //TODO remove?
	}
	/** TODO handle flags cases */
	switch (flags) {
	case MSG_CONFIRM:
	case MSG_DONTROUTE:
	case MSG_DONTWAIT:
	case MSG_EOR:
	case MSG_MORE:
	case MSG_NOSIGNAL:
	case MSG_OOB: /** case of recieving a (write call)*/
	default:
		break;
	} // end of the switch clause

	PRINT_DEBUG("send_tcp: Entered: index=%d, uniqueSockID=%llu, data_len=%d, flags=%d", index, uniqueSockID, data_len, flags);

	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("CRASH !! socket descriptor not found into jinni sockets");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, sendmsg_call);
		return;
	}

	/** copying the data passed to be able to free the old memory location
	 * the new created location is the one to be included into the newly created finsFrame*/
	PRINT_DEBUG("");
	/** check if this socket already connected to a destined address or not */

	if (jinniSockets[index].connection_status == 0) {
		/** socket is not connected to an address. Send call will fail */

		PRINT_DEBUG("socketjinni failed to accomplish send, socket found unconnected !!!");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, sendmsg_call);
		return;
	}

	/** Keep all ports and addresses in host order until later  action taken
	 * in IPv4 module
	 *  */
	dst_port = jinniSockets[index].dstport;

	dst_ip = jinniSockets[index].dst_IP;

	/** addresses are in host format given that there are by default already filled
	 * host IP and host port. Otherwise, a port and IP has to be assigned explicitly below */

	/**
	 * Default current host port to be assigned is 58088
	 * It is supposed to be randomly selected from the range found in
	 * /proc/sys/net/ipv4/ip_local_port_range
	 * default range in Ubuntu is 32768 - 61000
	 * The value has been chosen randomly when the socket firstly inserted into the jinnisockets
	 * check insertjinniSocket(processid, sockfd, fakeID, type, protocol);
	 */
	host_port = jinniSockets[index].hostport;
	/**
	 * the current value of host_IP is zero but to be filled later with
	 * the current IP using the IPv4 modules unless a binding has occured earlier
	 */
	host_ip = jinniSockets[index].host_IP;
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	PRINT_DEBUG("host=%u/%d, dst=%u/%d", host_ip, host_port, dst_ip, dst_port);

	//free(data);
	//free(addr);
	PRINT_DEBUG("");

	int blocking_flag = 1; //TODO get from flags

	/** the meta-data paraters are all passes by copy starting from this point
	 *
	 */
	if (jinni_TCP_to_fins(data, len, dst_port, dst_ip, host_port, host_ip, blocking_flag)) {
		if (blocking_flag) {
			pthread_t thread;
			struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) malloc(sizeof(struct jinni_tcp_thread_data));
			thread_data->id = thread_count++;
			thread_data->index = index;
			thread_data->uniqueSockID = uniqueSockID;
			//thread_data->blocking_flag = blocking_flag;

			PRINT_DEBUG("");

			//spin off thread to handle
			if (pthread_create(&thread, NULL, sendmsg_tcp_thread, (void *) thread_data)) {
				PRINT_ERROR("ERROR: unable to create accept_tcp_thread thread.");
				nack_send(uniqueSockID, sendmsg_call);
			}
		} else {
			ack_send(uniqueSockID, sendmsg_call);
		}
	} else {
		PRINT_DEBUG("socketjinni failed to accomplish send");
		nack_send(uniqueSockID, sendmsg_call);
	}
}

void sendto_tcp(int index, unsigned long long uniqueSockID, u_char *data, int data_len, int flags, struct sockaddr_in *addr, socklen_t addrlen) {
	uint16_t hostport;
	uint16_t dstport;
	uint32_t host_IP;
	uint32_t dst_IP;

	int len = data_len;

	PRINT_DEBUG();

	/** TODO handle flags cases */
	switch (flags) {
	case MSG_CONFIRM:
	case MSG_DONTROUTE:
	case MSG_DONTWAIT:
	case MSG_EOR:
	case MSG_MORE:
	case MSG_NOSIGNAL:
	case MSG_OOB: /** case of recieving a (write call)*/
	default:
		break;

	}PRINT_DEBUG("");

	if (addr->sin_family != AF_INET) {
		PRINT_DEBUG("Wrong address family");
		nack_send(uniqueSockID, sendmsg_call);
		return;
	}

	/** copying the data passed to be able to free the old memory location
	 * the new created location is the one to be included into the newly created finsFrame*/
	PRINT_DEBUG("");

	dst_IP = ntohl(addr->sin_addr.s_addr);/** it is in network format since application used htonl */
	/** addresses are in host format given that there are by default already filled
	 * host IP and host port. Otherwise, a port and IP has to be assigned explicitly below */

	/** Keep all ports and addresses in host order until later  action taken */
	dstport = ntohs(addr->sin_port); /** reverse it since it is in network order after application used htons */

	PRINT_DEBUG("");
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("CRASH !! socket descriptor not found into jinni sockets");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, sendmsg_call);
		return;
	}

	/*//TODO confirm this
	 if (jinniSockets[index].connection_status == 0 || dst_IP != jinniSockets[index].dst_IP || dstport != jinniSockets[index].dstport) {
	 sem_wait(&jinniSockets_sem);

	 nack_send(uniqueSockID, sendmsg_call);
	 return;
	 }
	 */

	/**
	 * the current value of host_IP is zero but to be filled later with
	 * the current IP using the IPv4 modules unless a binding has occured earlier
	 */
	host_IP = jinniSockets[index].host_IP;

	/**
	 * Default current host port to be assigned is 58088
	 * It is supposed to be randomly selected from the range found in
	 * /proc/sys/net/ipv4/ip_local_port_range
	 * default range in Ubuntu is 32768 - 61000
	 * The value has been chosen randomly when the socket firstly inserted into the jinnisockets
	 * check insertjinniSocket(processid, sockfd, fakeID, type, protocol);
	 */
	hostport = jinniSockets[index].hostport;
	if (hostport == 0) {
		while (1) {
			hostport = randoming(MIN_port, MAX_port);
			if (checkjinniports(hostport, host_IP)) {
				break;
			}
		}
		jinniSockets[index].hostport = hostport;
	}PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	int blocking_flag = 1; //TODO get from flags

	/** the meta-data paraters are all passes by copy starting from this point
	 *
	 */
	if (jinni_TCP_to_fins(data, len, dstport, dst_IP, hostport, host_IP, blocking_flag)) {
		PRINT_DEBUG("");
		/** TODO prevent the socket interceptor from holding this semaphore before we reach this point */
		ack_send(uniqueSockID, sendmsg_call);
		PRINT_DEBUG("");

	} else {
		PRINT_DEBUG("socketjinni failed to accomplish sendto");
		nack_send(uniqueSockID, sendmsg_call);
	}
}

void *recvfrom_tcp_thread(void *local) {
	struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) local;
	int id = thread_data->id;
	int index = thread_data->index;
	unsigned long long uniqueSockID = thread_data->uniqueSockID;
	int data_len = thread_data->data_len;
	int flags = thread_data->flags;
	free(thread_data);

	PRINT_DEBUG("recvfrom_tcp_thread: Entered: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);

	int blocking_flag = 1; //TODO get from flags
	uint32_t status;
	uint32_t host_ip;
	uint16_t host_port;
	uint32_t rem_ip;
	uint16_t rem_port;

	PRINT_DEBUG("recvfrom_tcp_thread: index=%d uniqueSockID=%llu", index, uniqueSockID);
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("Socket closed, canceling release_tcp.");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, release_call);
		pthread_exit(NULL);
	}

	status = jinniSockets[index].connection_status;
	host_ip = jinniSockets[index].host_IP;
	host_port = jinniSockets[index].hostport;
	if (status) {
		rem_ip = jinniSockets[index].dst_IP;
		rem_port = jinniSockets[index].dstport;
	}

	/** TODO handle flags cases, convert flags/msg_flags to */
	//thread_flags = 0; // |= FLAGS_BLOCK | MULTI_FLAG;
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	PRINT_DEBUG();
	struct finsFrame *ff = get_fdf(index, uniqueSockID, blocking_flag);
	PRINT_DEBUG("recvfrom_tcp_thread: after get_fdf uniqID=%llu ind=%d", uniqueSockID, index);

	if (ff == NULL) { //TODO add check if nonblocking send
		PRINT_DEBUG("recvfrom_tcp_thread: Exiting, No fdf: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		nack_send(uniqueSockID, recvmsg_call);
		pthread_exit(NULL);
	}

	struct sockaddr_in addr;
	uint32_t src_port;
	if (metadata_readFromElement(ff->dataFrame.metaData, "src_port", &src_port) == 0) {
		addr.sin_port = 0;
	} else {
		addr.sin_port = (uint16_t) src_port;
	}

	uint32_t src_ip;
	if (metadata_readFromElement(ff->dataFrame.metaData, "src_ip", &src_ip) == 0) {
		addr.sin_addr.s_addr = 0;
	} else {
		addr.sin_addr.s_addr = (uint32_t) src_ip;
	}

	//#######
	PRINT_DEBUG("address: %d/%d", addr.sin_addr.s_addr, ntohs(addr.sin_port));
	//#######

	int msg_len = 4 * sizeof(int) + sizeof(unsigned long long) + sizeof(struct sockaddr_in) + ff->dataFrame.pduLength;
	u_char *msg = (u_char *) malloc(msg_len);
	u_char *pt = msg;

	*(int *) pt = recvmsg_call;
	pt += sizeof(int);

	*(unsigned long long *) pt = uniqueSockID;
	pt += sizeof(unsigned long long);

	*(int *) pt = ACK;
	pt += sizeof(int);

	*(int *) pt = sizeof(addr);
	pt += sizeof(int);

	memcpy(pt, &addr, sizeof(struct sockaddr_in));
	pt += sizeof(struct sockaddr_in);

	*(int *) pt = ff->dataFrame.pduLength;
	pt += sizeof(int);

	memcpy(pt, ff->dataFrame.pdu, ff->dataFrame.pduLength);
	pt += ff->dataFrame.pduLength;

	if (pt - msg != msg_len) {
		PRINT_DEBUG("write error: diff=%d len=%d\n", pt - msg, msg_len);
		free(msg);
		PRINT_DEBUG("recvfrom_tcp_thread: Exiting, No fdf: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		nack_send(uniqueSockID, recvmsg_call);
		freeFinsFrame(ff);
		pthread_exit(NULL);
	}

	PRINT_DEBUG("msg_len=%d msg=%s", msg_len, msg);
	if (send_wedge(nl_sockfd, msg, msg_len, 0)) {
		PRINT_DEBUG("recvfrom_tcp_thread: Exiting, fail send_wedge: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		nack_send(uniqueSockID, recvmsg_call);
	} else {
		//TODO send size back to TCP handlers
		if (status) {
			PRINT_DEBUG("recvfrom address: host=%u/%d rem=%u/%d", host_ip, host_port, rem_ip, rem_port);
		} else {
			PRINT_DEBUG("recvfrom address: host=%u/%d", host_ip, host_port);
		}

		metadata *params = (metadata *) malloc(sizeof(metadata));
		if (params == NULL) {
			PRINT_ERROR("metadata creation failed");

			nack_send(uniqueSockID, recvmsg_call);
			freeFinsFrame(ff);
			pthread_exit(NULL);
		}
		metadata_create(params);

		metadata_writeToElement(params, "status", &status, META_TYPE_INT);

		uint32_t param_id = CTRL_SET_PARAM;
		metadata_writeToElement(params, "param_id", &param_id, META_TYPE_INT);
		metadata_writeToElement(params, "host_ip", &host_ip, META_TYPE_INT);
		metadata_writeToElement(params, "host_port", &host_port, META_TYPE_INT);
		if (status) {
			metadata_writeToElement(params, "rem_ip", &rem_ip, META_TYPE_INT);
			metadata_writeToElement(params, "rem_port", &rem_port, META_TYPE_INT);
		}

		if (jinni_TCP_to_fins_cntrl(CTRL_SET_PARAM, params)) {
			PRINT_DEBUG("recvfrom_tcp_thread: Exiting, normal: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		} else {
			PRINT_DEBUG("recvfrom_tcp_thread: Exiting, fail sending flow msgs: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
			metadata_destroy(params);
		}

	}

	free(msg);
	freeFinsFrame(ff);
	pthread_exit(NULL);
}

/**
 * @function recvfrom_udp
 * @param symbol tells if an address has been passed from the application to get the sender address or not
 *	Note this method is coded to be thread safe since UDPreadFrom_fins mimics blocking and needs to be threaded.
 *
 */
void recvfrom_tcp(int index, unsigned long long uniqueSockID, int data_len, int flags, int msg_flags) {

	/** symbol parameter is the one to tell if an address has been passed from the
	 * application to get the sender address or not
	 */

	int multi_flag;
	int thread_flags;

	PRINT_DEBUG("recvfrom_tcp: Entered: index=%d uniqueSockID=%llu data_len=%d flags=%d", index, uniqueSockID, data_len, flags);

	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("Socket closed, canceling read block.");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, recvmsg_call);
		return;
	}

	multi_flag = 0; //for udp, if SOL_SOCKET/SO_REUSEADDR
	//change flags?

	/** TODO handle flags cases, convert flags/msg_flags to */
	thread_flags = 0; // |= FLAGS_BLOCK | MULTI_FLAG;

	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	if (1) { //TODO thread count check
		pthread_t thread;
		struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) malloc(sizeof(struct jinni_tcp_thread_data));
		thread_data->id = thread_count++;
		thread_data->index = index;
		thread_data->uniqueSockID = uniqueSockID;
		thread_data->data_len = data_len;
		thread_data->flags = thread_flags;

		//spin off thread to handle
		if (pthread_create(&thread, NULL, recvfrom_tcp_thread, (void *) thread_data)) {
			PRINT_ERROR("ERROR: unable to create recvfrom_udp_thread thread.");
			nack_send(uniqueSockID, recvmsg_call);

			free(thread_data);
		}
	}
}

void *release_tcp_thread(void *local) {
	struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) local;
	int id = thread_data->id;
	int index = thread_data->index;
	unsigned long long uniqueSockID = thread_data->uniqueSockID;
	int flags = thread_data->flags;
	free(thread_data);

	int block_flag = 1; //TODO get from flags

	uint32_t exec_call = 0;
	uint32_t ret_val = 0;
	uint32_t rem_ip = 0;
	uint16_t rem_port = 0;

	PRINT_DEBUG("release_tcp_thread: Entered: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
	struct finsFrame *ff = get_fcf(index, uniqueSockID, block_flag);
	PRINT_DEBUG("release_tcp_thread: after get_fdf: id=%d index=%d uniqueSockID=%llu", id, index, uniqueSockID);
	if (ff == NULL || ff->ctrlFrame.opcode != CTRL_EXEC_REPLY || ff->ctrlFrame.metaData == NULL) {
		PRINT_DEBUG("release_tcp_thread: Exiting, No fdf/opcode/metadata: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		nack_send(uniqueSockID, release_call);
		PRINT_DEBUG("freeing ff=%d", (int)ff);
		freeFinsFrame(ff);
		pthread_exit(NULL);
	}

	int ret = 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "exec_call", &exec_call) == 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "ret_val", &ret_val) == 0;

	if (ret || (exec_call != EXEC_TCP_CLOSE && exec_call != EXEC_TCP_CLOSE_STUB) || ret_val == 0) {
		PRINT_DEBUG("release_tcp_thread: Exiting, NACK: id=%d, index=%d, uniqueSockID=%llu, ret=%d, exec_call=%d, ret_val=%d",
				id, index, uniqueSockID, ret, exec_call, ret_val);
		nack_send(uniqueSockID, release_call);
	} else {
		PRINT_DEBUG("");
		sem_wait(&jinniSockets_sem);
		if (jinniSockets[index].uniqueSockID != uniqueSockID) {
			PRINT_DEBUG("");
			sem_post(&jinniSockets_sem);

			PRINT_DEBUG("release_tcp_thread: Exiting, socket closed: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
			nack_send(uniqueSockID, release_call);
		} else {
			if (removejinniSocket(uniqueSockID)) {
				PRINT_DEBUG("release_tcp_thread: Exiting, ACK: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
				sem_post(&jinniSockets_sem);

				ack_send(uniqueSockID, release_call);
			} else {
				PRINT_DEBUG("release_tcp_thread: Exiting, remove fail: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
				sem_post(&jinniSockets_sem);

				nack_send(uniqueSockID, release_call);
			}

		}
	}

	PRINT_DEBUG();

	PRINT_DEBUG("freeing ff=%d", (int)ff);
	freeFinsFrame(ff);
	pthread_exit(NULL);
}

void release_tcp(int index, unsigned long long uniqueSockID) {
	uint32_t status;
	uint32_t host_ip;
	uint16_t host_port;
	uint32_t rem_ip;
	uint16_t rem_port;

	PRINT_DEBUG("release_udp: index=%d uniqueSockID=%llu", index, uniqueSockID);
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("Socket closed, canceling release_tcp.");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, release_call);
		return;
	}

	status = jinniSockets[index].connection_status;
	host_ip = jinniSockets[index].host_IP;
	host_port = jinniSockets[index].hostport;
	if (status) {
		rem_ip = jinniSockets[index].dst_IP;
		rem_port = jinniSockets[index].dstport;
	}

	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	//TODO process flags?

	if (status) {
		PRINT_DEBUG("release address: host=%u/%d rem=%u/%d", host_ip, host_port, rem_ip, rem_port);
	} else {
		PRINT_DEBUG("release address: host=%u/%d", host_ip, host_port);
	}

	metadata *params = (metadata *) malloc(sizeof(metadata));
	if (params == NULL) {
		PRINT_DEBUG("metadata creation failed");

		nack_send(uniqueSockID, release_call);
		return;
	}
	metadata_create(params);

	metadata_writeToElement(params, "status", &status, META_TYPE_INT);

	uint32_t exec_call = status ? EXEC_TCP_CLOSE : EXEC_TCP_CLOSE_STUB;
	metadata_writeToElement(params, "exec_call", &exec_call, META_TYPE_INT);
	metadata_writeToElement(params, "host_ip", &host_ip, META_TYPE_INT);
	metadata_writeToElement(params, "host_port", &host_port, META_TYPE_INT);
	if (status) {
		metadata_writeToElement(params, "rem_ip", &rem_ip, META_TYPE_INT);
		metadata_writeToElement(params, "rem_port", &rem_port, META_TYPE_INT);
	}
	//metadata_writeToElement(params, "flags", &flags, META_TYPE_INT);

	if (jinni_TCP_to_fins_cntrl(CTRL_EXEC, params)) {
		pthread_t thread;
		struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) malloc(sizeof(struct jinni_tcp_thread_data));
		thread_data->id = thread_count++;
		thread_data->index = index;
		thread_data->uniqueSockID = uniqueSockID;
		thread_data->flags = 0; //TODO implement?

		//spin off thread to handle
		if (pthread_create(&thread, NULL, release_tcp_thread, (void *) thread_data)) {
			PRINT_ERROR("ERROR: unable to create release_tcp_thread thread.");
			nack_send(uniqueSockID, release_call);
			free(thread_data);
			metadata_destroy(params);
		}
	} else {
		PRINT_DEBUG("socketjinni failed to accomplish release");
		nack_send(uniqueSockID, release_call);
		metadata_destroy(params);
	}
}

//void recvfrom_tcp(void *threadData) {
void recvfrom_tcp_old(int index, unsigned long long uniqueSockID, int data_len, int flags, int symbol) {

	/** symbol parameter is the one to tell if an address has been passed from the
	 * application to get the sender address or not
	 */

	u_char buf[MAX_DATA_PER_TCP];

	u_char *bufptr;
	bufptr = buf;
	struct sockaddr_in *addr;
	int buflen = 0;
	int i;
	int blocking_flag;

	void *msg;
	u_char * pt;
	int msg_len;
	int ret_val;
	/*
	 struct recvfrom_data *thread_data;
	 thread_data = (struct recvfrom_data *) threadData;

	 unsigned long long uniqueSockID = thread_data->uniqueSockID;
	 int data_len = thread_data->datalen;
	 int flags = thread_data->flags;
	 int symbol = thread_data->symbol;
	 */
	if (symbol == 1)
		addr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	addr = NULL;
	/** TODO handle flags cases */
	switch (flags) {

	default:
		break;

	}

	//PRINT_DEBUG("Entered recv thread:%d", thread_data->id);

	PRINT_DEBUG("");
	sem_wait(&jinniSockets_sem);
	index = findjinniSocket(uniqueSockID);
	if (index == -1) {
		PRINT_DEBUG("socket descriptor not found into jinni sockets");
		sem_post(&jinniSockets_sem);
		//recvthread_exit(thread_data);
	}

	PRINT_DEBUG("index = %d", index);
	PRINT_DEBUG();
	blocking_flag = jinniSockets[index].blockingFlag;
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	/** the meta-data parameters are all passed by copy starting from this point
	 *
	 */

	if (TCPreadFrom_fins(uniqueSockID, bufptr, &buflen, symbol, addr, blocking_flag) == 1) {

		buf[buflen] = '\0'; //may be specific to symbol==0

		PRINT_DEBUG("%d", buflen);
		PRINT_DEBUG("%s", buf);

		msg_len = 4 * sizeof(int) + sizeof(unsigned long long) + buflen + (symbol ? sizeof(struct sockaddr_in) : 0);
		msg = malloc(msg_len);
		pt = msg;

		*(int *) pt = recvmsg_call;
		pt += sizeof(int);

		*(unsigned long long *) pt = uniqueSockID;
		pt += sizeof(unsigned long long);

		*(int *) pt = ACK;
		pt += sizeof(int);

		if (symbol) {
			*(int *) pt = sizeof(struct sockaddr_in);
			pt += sizeof(int);

			memcpy(pt, addr, sizeof(struct sockaddr_in));
			pt += sizeof(struct sockaddr_in);
		}

		*(int *) pt = buflen;
		pt += sizeof(int);

		memcpy(pt, buf, buflen);
		pt += buflen;

		if (pt - (u_char *) msg != msg_len) {
			PRINT_DEBUG("write error: diff=%d len=%d\n", pt - (u_char *) msg, msg_len);
			free(msg);
			//recvthread_exit(thread_data);
		}

		PRINT_DEBUG("msg_len=%d msg=%s", msg_len, (char *) msg);
		ret_val = send_wedge(nl_sockfd, msg, msg_len, 0);
		free(msg);

		//free(buf);
		PRINT_DEBUG();
	} else {
		PRINT_DEBUG("socketjinni failed to accomplish recvfrom");
		sem_wait(&jinniSockets_sem);
		index = findjinniSocket(uniqueSockID);
		PRINT_DEBUG("");
		sem_post(&jinniSockets_sem);

		if (index == -1) {
			PRINT_DEBUG("socket descriptor not found into jinni sockets");
			//recvthread_exit(thread_data);
		} else {
			nack_send(uniqueSockID, recvmsg_call);
		}
	}PRINT_DEBUG();

	/** TODO find a way to release these buffers later
	 * using free here causing a segmentation fault
	 */
	//free(addr);
	//free(buf);
	//recvthread_exit(thread_data);
}

void recv_tcp_old(int index, unsigned long long uniqueSockID, int data_len, int flags, int msg_flags) {
	//u_char *buf= NULL;
	u_char buf[MAX_DATA_PER_TCP];
	int buflen = 0;

	int blocking_flag;
	blocking_flag = 1;

	void *msg;
	u_char * pt;
	int msg_len;
	int ret_val;

	/** TODO handle flags cases */
	switch (flags) {

	default:
		break;

	}

	PRINT_DEBUG("");
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("Socket closed, canceling read block.");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, recvmsg_call);
		return;
	}
	//TODO something?
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	/** the meta-data parameters are all passed by copy starting from this point
	 *
	 */
	/** passing 0 as value for symbol, and NULL as an address
	 * this the difference between the call from here, and the call in case of
	 * the function recvfrom_udp
	 * */
	if (TCPreadFrom_fins(uniqueSockID, buf, &buflen, 0, NULL, blocking_flag) == 1) {

		buf[buflen] = '\0'; //may be specific to symbol==0

		PRINT_DEBUG("%d", buflen);
		PRINT_DEBUG("%s", buf);

		msg_len = sizeof(u_int) + sizeof(unsigned long long) + sizeof(int) + buflen;
		msg = malloc(msg_len);
		pt = msg;

		*(u_int *) pt = recv_call;
		pt += sizeof(u_int);

		*(unsigned long long *) pt = uniqueSockID;
		pt += sizeof(unsigned long long);

		*(int *) pt = ACK;
		pt += sizeof(int);

		*(int *) pt = buflen;
		pt += sizeof(int);

		memcpy(pt, buf, buflen);
		pt += buflen;

		if (pt - (u_char *) msg != msg_len) {
			PRINT_DEBUG("write error: diff=%d len=%d\n", pt - (u_char *) msg, msg_len);
			free(msg);
			exit(1);
		}

		PRINT_DEBUG("msg_len=%d msg=%s", msg_len, (char *) msg);
		ret_val = send_wedge(nl_sockfd, msg, msg_len, 0);
		free(msg);

		PRINT_DEBUG();

		//	free(buf);
		PRINT_DEBUG();
	} else {
		PRINT_DEBUG("socketjinni failed to accomplish recv_udp");
		nack_send(uniqueSockID, recv_call);
	}

	PRINT_DEBUG();
	/** TODO find a way to release these buffers later
	 * using free here causing a segmentation fault
	 */
	//free(addr);
	//free(buf);
}

void getpeername_tcp(int index, unsigned long long uniqueSockID, int addrlen) {

	return;

}

void shutdown_tcp(int index, unsigned long long uniqueSockID, int how) {

	/**
	 *
	 * TODO Implement the checking of the shut_RD, shut_RW flags before making any operations
	 * applied on a TCP socket
	 */

	index = findjinniSocket(uniqueSockID);
	if (index == -1) {
		PRINT_DEBUG("socket descriptor not found into jinni sockets");
		exit(1);
	}

	PRINT_DEBUG("index = %d", index);
	PRINT_DEBUG();

	ack_send(uniqueSockID, shutdown_call);
}

void *getsockopt_tcp_thread(void *local) {
	struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) local;
	int id = thread_data->id;
	int index = thread_data->index;
	unsigned long long uniqueSockID = thread_data->uniqueSockID;
	int flags = thread_data->flags;
	free(thread_data);

	int block_flag = 1; //TODO get from flags
	uint32_t param_id = 0;
	uint32_t ret_val = 0;

	PRINT_DEBUG("getsockopt_tcp_thread: Entered: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);

	//##############################
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("getsockopt_tcp_thread: Exiting, socket closed: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		sem_post(&jinniSockets_sem);
		pthread_exit(NULL);
	}

	//TODO get info? remove this block if unneeded
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);
	//##############################

	PRINT_DEBUG();
	struct finsFrame *ff = get_fcf(index, uniqueSockID, block_flag);
	PRINT_DEBUG("getsockopt_tcp_thread: after get_fdf: id=%d index=%d uniqueSockID=%llu", id, index, uniqueSockID);
	if (ff == NULL) {
		PRINT_DEBUG("getsockopt_tcp_thread: Exiting, socket closed: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		nack_send(uniqueSockID, getsockopt_call);
		pthread_exit(NULL);
	}

	if (ff->ctrlFrame.opcode != CTRL_READ_PARAM_REPLY || ff->ctrlFrame.metaData == NULL) {
		PRINT_DEBUG("getsockopt_tcp_thread: Exiting, fcf errors: id=%d, index=%d, uniqueSockID=%llu opcode=%d, metaData=%d",
				id, index, uniqueSockID, ff->ctrlFrame.opcode, ff->ctrlFrame.metaData==NULL);
		nack_send(uniqueSockID, getsockopt_call);
		PRINT_DEBUG("freeing ff=%d", (int)ff);
		freeFinsFrame(ff);
		pthread_exit(NULL);
	}

	int ret = 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "param_id", &param_id) == 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "ret_val", &ret_val) == 0;

	if (ret || /*(exec_call != EXEC_TCP_CONNECT && exec_call != EXEC_TCP_ACCEPT) ||*/ret_val == 0) {
		PRINT_DEBUG("getsockopt_tcp_thread: Exiting, meta errors: id=%d, index=%d, uniqueSockID=%llu, ret=%d, exec_call=%d, ret_val=%d",
				id, index, uniqueSockID, ret, param_id, ret_val);
		nack_send(uniqueSockID, getsockopt_call);
	} else {
		//TODO switch by param_id, convert into val/len
		int len = 0;
		uint8_t *val;
		//################

		int msg_len = 4 * sizeof(int) + sizeof(unsigned long long) + len;
		u_char *msg = (u_char *) malloc(msg_len);
		u_char *pt = msg;

		*(int *) pt = getsockopt_call;
		pt += sizeof(int);

		*(unsigned long long *) pt = uniqueSockID;
		pt += sizeof(unsigned long long);

		*(int *) pt = ACK;
		pt += sizeof(int);

		*(int *) pt = param_id;
		pt += sizeof(int);

		*(int *) pt = len;
		pt += sizeof(int);

		if (len > 0) {
			memcpy(pt, val, len);
			pt += len;
		}

		if (pt - msg != msg_len) {
			PRINT_DEBUG("write error: diff=%d len=%d\n", pt - msg, msg_len);
			free(msg);
			PRINT_DEBUG("getsockopt_tcp_thread: Exiting, No fdf: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
			nack_send(uniqueSockID, getsockopt_call);
			freeFinsFrame(ff);
			pthread_exit(NULL);
		}

		PRINT_DEBUG("msg_len=%d msg=%s", msg_len, msg);
		if (send_wedge(nl_sockfd, msg, msg_len, 0)) {
			PRINT_DEBUG("getsockopt_tcp_thread: Exiting, fail send_wedge: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
			nack_send(uniqueSockID, getsockopt_call);
		} else {
			PRINT_DEBUG("getsockopt_tcp_thread: Exiting, normal: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		}
	}

	PRINT_DEBUG("freeing ff=%d", (int)ff);
	freeFinsFrame(ff);
	pthread_exit(NULL);
}

void getsockopt_tcp(int index, unsigned long long uniqueSockID, int level, int optname, int optlen, u_char *optval) {
	uint32_t host_ip;
	uint16_t host_port;
	int status;
	uint32_t rem_ip;
	uint16_t rem_port;

	PRINT_DEBUG("getsockopt_tcp: index=%d, uniqueSockID=%llu, level=%d, optname=%d, optlen=%d", index, uniqueSockID, level, optname, optlen);
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("Socket closed, canceling getsockopt_tcp.");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, getsockopt_call);
		return;
	}

	status = jinniSockets[index].connection_status;
	host_ip = jinniSockets[index].host_IP;
	host_port = jinniSockets[index].hostport;
	if (status) {
		rem_ip = jinniSockets[index].dst_IP;
		rem_port = jinniSockets[index].dstport;
	}

	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	metadata *params = (metadata *) malloc(sizeof(metadata));
	if (params == NULL) {
		PRINT_DEBUG("metadata creation failed");

		nack_send(uniqueSockID, getsockopt_call);
		return;
	}
	metadata_create(params);

	int send_dst = -1;
	uint32_t param_id = 0;
	int len = 0;
	uint8_t *val = NULL;

	metadata_writeToElement(params, "status", &status, META_TYPE_INT);
	metadata_writeToElement(params, "host_ip", &host_ip, META_TYPE_INT);
	metadata_writeToElement(params, "host_port", &host_port, META_TYPE_INT);
	if (status) {
		metadata_writeToElement(params, "rem_ip", &rem_ip, META_TYPE_INT);
		metadata_writeToElement(params, "rem_port", &rem_port, META_TYPE_INT);
	}
	metadata_writeToElement(params, "param", &optname, META_TYPE_INT);

	//in each case add params to metadata
	//if status == 0, & tcp handle here, else move to module?
	//if status == 1, & tcp handle tcp module
	//if status == 2 & tcp handle tcp module,

	switch (optname) {
	case SO_DEBUG:
		if (optlen >= sizeof(int)) {
			len = sizeof(int);
			val = (uint8_t *) &jinniSockets[index].sockopts.FSO_DEBUG; //TODO move into sem's
			send_dst = 0;
		}
		break;
	case SO_REUSEADDR:
		if (optlen >= sizeof(int)) {
			len = sizeof(int);
			val = (uint8_t *) &jinniSockets[index].sockopts.FSO_REUSEADDR; //TODO move into sem's
			send_dst = 0;
		}
		break;
	case SO_TYPE:
	case SO_PROTOCOL:
	case SO_DOMAIN:
	case SO_ERROR:
	case SO_DONTROUTE:
	case SO_BROADCAST:
	case SO_SNDBUF:
		if (optlen >= sizeof(int)) {
			len = sizeof(int);
			val = (uint8_t *) &jinniSockets[index].sockopts.FSO_SNDBUF; //TODO move into sem's
			send_dst = 0;
		}
		break;
	case SO_SNDBUFFORCE:
	case SO_RCVBUF:
		if (optlen >= sizeof(int)) {
			len = sizeof(int);
			val = (uint8_t *) &jinniSockets[index].sockopts.FSO_RCVBUF; //TODO move into sem's
			send_dst = 0;
		}
		break;
	case SO_RCVBUFFORCE:
	case SO_KEEPALIVE:
		if (optlen >= sizeof(int)) {
			len = sizeof(int);
			val = (uint8_t *) &jinniSockets[index].sockopts.FSO_KEEPALIVE; //TODO move into sem's
			send_dst = 0;
		}
		break;
	case SO_OOBINLINE:
		if (optlen >= sizeof(int)) {
			len = sizeof(int);
			val = (uint8_t *) &jinniSockets[index].sockopts.FSO_OOBINLINE; //TODO move into sem's
			send_dst = 0;
		}
		break;
	case SO_NO_CHECK:
	case SO_PRIORITY:
		if (optlen >= sizeof(int)) {
			len = sizeof(int);
			val = (uint8_t *) &jinniSockets[index].sockopts.FSO_PRIORITY; //TODO move into sem's
			send_dst = 0;
		}
		break;
	case SO_LINGER:
	case SO_BSDCOMPAT:
	case SO_TIMESTAMP:
	case SO_TIMESTAMPNS:
	case SO_TIMESTAMPING:
	case SO_RCVTIMEO:
	case SO_SNDTIMEO:
	case SO_RCVLOWAT:
	case SO_SNDLOWAT:
	case SO_PASSCRED:
		if (optlen >= sizeof(int)) {
			len = sizeof(int);
			val = (uint8_t *) &jinniSockets[index].sockopts.FSO_PASSCRED; //TODO move into sem's
			send_dst = 0;
		}
		break;
	case SO_PEERCRED:
		//TODO trickier
		break;
	case SO_PEERNAME:
	case SO_ACCEPTCONN:
	case SO_PASSSEC:
	case SO_PEERSEC:
	case SO_MARK:
	case SO_RXQ_OVFL:
	case SO_ATTACH_FILTER:
	case SO_DETACH_FILTER:
		break;
	default:
		//nack?
		PRINT_DEBUG("default=%d", optname);
		break;
	}

	if (send_dst == -1) {
		PRINT_DEBUG("freeing meta=%d", (int)params);
		metadata_destroy(params);
		nack_send(uniqueSockID, getsockopt_call);
	} else if (send_dst == 0) {
		PRINT_DEBUG("freeing meta=%d", (int)params);
		metadata_destroy(params);

		int msg_len = 4 * sizeof(int) + sizeof(unsigned long long) + len;
		u_char *msg = (u_char *) malloc(msg_len);
		u_char *pt = msg;

		*(int *) pt = getsockopt_call;
		pt += sizeof(int);

		*(unsigned long long *) pt = uniqueSockID;
		pt += sizeof(unsigned long long);

		*(int *) pt = ACK;
		pt += sizeof(int);

		*(int *) pt = param_id;
		pt += sizeof(int);

		*(int *) pt = len;
		pt += sizeof(int);

		if (len > 0) {
			memcpy(pt, val, len);
			pt += len;
		}

		if (pt - msg != msg_len) {
			PRINT_DEBUG("write error: diff=%d len=%d\n", pt - msg, msg_len);
			free(msg);
			PRINT_DEBUG("getsockopt_tcp: Exiting, No fdf: index=%d, uniqueSockID=%llu", index, uniqueSockID);
			nack_send(uniqueSockID, getsockopt_call);
			return;
		}

		PRINT_DEBUG("msg_len=%d msg=%s", msg_len, msg);
		if (send_wedge(nl_sockfd, msg, msg_len, 0)) {
			PRINT_DEBUG("getsockopt_tcp: Exiting, fail send_wedge: index=%d, uniqueSockID=%llu", index, uniqueSockID);
			nack_send(uniqueSockID, getsockopt_call);
		} else {
			PRINT_DEBUG("getsockopt_tcp: Exiting, normal: index=%d, uniqueSockID=%llu", index, uniqueSockID);
		}
	} else {
		if (jinni_TCP_to_fins_cntrl(CTRL_READ_PARAM, params)) {
			struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) malloc(sizeof(struct jinni_tcp_thread_data));
			thread_data->id = thread_count++;
			thread_data->index = index;
			thread_data->uniqueSockID = uniqueSockID;
			thread_data->flags = 0; //TODO implement?

			pthread_t thread;
			if (pthread_create(&thread, NULL, getsockopt_tcp_thread, (void *) thread_data)) {
				PRINT_ERROR("ERROR: unable to create getsockopt_tcp_thread thread.");
				nack_send(uniqueSockID, getsockopt_call);
				free(thread_data);
				metadata_destroy(params);
			}
		} else {
			PRINT_DEBUG("socketjinni failed to accomplish accept");
			nack_send(uniqueSockID, getsockopt_call);
			metadata_destroy(params);
		}
	}
}

void *setsockopt_tcp_thread(void *local) {
	struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) local;
	int id = thread_data->id;
	int index = thread_data->index;
	unsigned long long uniqueSockID = thread_data->uniqueSockID;
	int flags = thread_data->flags;
	free(thread_data);

	int block_flag = 1; //TODO get from flags
	uint32_t param_id = 0;
	uint32_t ret_val = 0;

	PRINT_DEBUG("setsockopt_tcp_thread: Entered: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);

	//##############################
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("setsockopt_tcp_thread: Exiting, socket closed: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		sem_post(&jinniSockets_sem);
		pthread_exit(NULL);
	}

	//TODO get info? remove this block if unneeded
	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);
	//##############################

	PRINT_DEBUG();
	struct finsFrame *ff = get_fcf(index, uniqueSockID, block_flag);
	PRINT_DEBUG("setsockopt_tcp_thread: after get_fdf: id=%d index=%d uniqueSockID=%llu", id, index, uniqueSockID);
	if (ff == NULL) {
		PRINT_DEBUG("setsockopt_tcp_thread: Exiting, socket closed: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		nack_send(uniqueSockID, setsockopt_call);
		pthread_exit(NULL);
	}

	if (ff->ctrlFrame.opcode != CTRL_READ_PARAM_REPLY || ff->ctrlFrame.metaData == NULL) {
		PRINT_DEBUG("setsockopt_tcp_thread: Exiting, fcf errors: id=%d, index=%d, uniqueSockID=%llu opcode=%d, metaData=%d",
				id, index, uniqueSockID, ff->ctrlFrame.opcode, ff->ctrlFrame.metaData==NULL);
		nack_send(uniqueSockID, setsockopt_call);
		PRINT_DEBUG("freeing ff=%d", (int)ff);
		freeFinsFrame(ff);
		pthread_exit(NULL);
	}

	int ret = 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "param_id", &param_id) == 0;
	ret += metadata_readFromElement(ff->ctrlFrame.metaData, "ret_val", &ret_val) == 0;

	if (ret /*|| (param_id != EXEC_TCP_CONNECT && param_id != EXEC_TCP_ACCEPT)*/|| ret_val == 0) {
		PRINT_DEBUG("setsockopt_tcp_thread: Exiting, meta errors: id=%d, index=%d, uniqueSockID=%llu, ret=%d, param_id=%d, ret_val=%d",
				id, index, uniqueSockID, ret, param_id, ret_val);
		nack_send(uniqueSockID, setsockopt_call);
	} else {
		PRINT_DEBUG("setsockopt_tcp_thread: Exiting, normal: id=%d, index=%d, uniqueSockID=%llu", id, index, uniqueSockID);
		ack_send(uniqueSockID, setsockopt_call);
	}

	PRINT_DEBUG("freeing ff=%d", (int)ff);
	freeFinsFrame(ff);
	pthread_exit(NULL);
}

void setsockopt_tcp(int index, unsigned long long uniqueSockID, int level, int optname, int optlen, u_char *optval) {
	int status;
	uint32_t host_ip;
	uint16_t host_port;
	uint32_t rem_ip;
	uint16_t rem_port;

	PRINT_DEBUG("setsockopt_tcp: index=%d, uniqueSockID=%llu, level=%d, optname=%d, optlen=%d", index, uniqueSockID, level, optname, optlen);
	sem_wait(&jinniSockets_sem);
	if (jinniSockets[index].uniqueSockID != uniqueSockID) {
		PRINT_DEBUG("Socket closed, canceling setsockopt_tcp.");
		sem_post(&jinniSockets_sem);

		nack_send(uniqueSockID, setsockopt_call);
		return;
	}

	status = jinniSockets[index].connection_status;
	host_ip = jinniSockets[index].host_IP;
	host_port = jinniSockets[index].hostport;
	if (status) {
		rem_ip = jinniSockets[index].dst_IP;
		rem_port = jinniSockets[index].dstport;
	}

	PRINT_DEBUG("");
	sem_post(&jinniSockets_sem);

	metadata *params = (metadata *) malloc(sizeof(metadata));
	if (params == NULL) {
		PRINT_DEBUG("metadata creation failed");

		nack_send(uniqueSockID, setsockopt_call);
		return;
	}
	metadata_create(params);

	int send_dst = -1;
	uint32_t param_id = 0;
	int len = 0;
	uint8_t *val = NULL;

	//in each case add params to metadata
	//if status == 0, & tcp handle here, else move to module?
	//if status == 1, & tcp handle tcp module
	//if status == 2 & tcp handle tcp module

	metadata_writeToElement(params, "status", &status, META_TYPE_INT);
	metadata_writeToElement(params, "host_ip", &host_ip, META_TYPE_INT);
	metadata_writeToElement(params, "host_port", &host_port, META_TYPE_INT);
	if (status) {
		metadata_writeToElement(params, "rem_ip", &rem_ip, META_TYPE_INT);
		metadata_writeToElement(params, "rem_port", &rem_port, META_TYPE_INT);
	}
	metadata_writeToElement(params, "param", &optname, META_TYPE_INT);

	switch (optname) {
	case SO_DEBUG:
		if (optlen >= sizeof(int)) {
			jinniSockets[index].sockopts.FSO_DEBUG = *(int *) optval;

			metadata_writeToElement(params, "value", &jinniSockets[index].sockopts.FSO_DEBUG, META_TYPE_INT);
			send_dst = 1;
		}
		break;
	case SO_REUSEADDR:
		if (optlen >= sizeof(int)) {
			jinniSockets[index].sockopts.FSO_REUSEADDR = *(int *) optval;
			metadata_writeToElement(params, "value", &jinniSockets[index].sockopts.FSO_REUSEADDR, META_TYPE_INT);
			send_dst = 1;
		}
		break;
	case SO_TYPE:
	case SO_PROTOCOL:
	case SO_DOMAIN:
	case SO_ERROR:
	case SO_DONTROUTE:
	case SO_BROADCAST:
	case SO_SNDBUF:
		if (optlen >= sizeof(int)) {
			jinniSockets[index].sockopts.FSO_SNDBUF = *(int *) optval;
			metadata_writeToElement(params, "value", &jinniSockets[index].sockopts.FSO_SNDBUF, META_TYPE_INT);
			send_dst = 1;
		}
		break;
	case SO_SNDBUFFORCE:
	case SO_RCVBUF:
		if (optlen >= sizeof(int)) {
			jinniSockets[index].sockopts.FSO_RCVBUF = *(int *) optval;
			metadata_writeToElement(params, "value", &jinniSockets[index].sockopts.FSO_RCVBUF, META_TYPE_INT);
			send_dst = 1;
		}
		break;
	case SO_RCVBUFFORCE:
	case SO_KEEPALIVE:
		if (optlen >= sizeof(int)) {
			jinniSockets[index].sockopts.FSO_KEEPALIVE = *(int *) optval;
			metadata_writeToElement(params, "value", &jinniSockets[index].sockopts.FSO_KEEPALIVE, META_TYPE_INT);
			send_dst = 1;
		}
		break;
	case SO_OOBINLINE:
		if (optlen >= sizeof(int)) {
			jinniSockets[index].sockopts.FSO_OOBINLINE = *(int *) optval;
			metadata_writeToElement(params, "value", &jinniSockets[index].sockopts.FSO_OOBINLINE, META_TYPE_INT);
			send_dst = 1;
		}
		break;
	case SO_NO_CHECK:
	case SO_PRIORITY:
		if (optlen >= sizeof(int)) {
			jinniSockets[index].sockopts.FSO_PRIORITY = *(int *) optval;
			metadata_writeToElement(params, "value", &jinniSockets[index].sockopts.FSO_PRIORITY, META_TYPE_INT);
			send_dst = 1;
		}
		break;
	case SO_LINGER:
	case SO_BSDCOMPAT:
	case SO_TIMESTAMP:
	case SO_TIMESTAMPNS:
	case SO_TIMESTAMPING:
	case SO_RCVTIMEO:
	case SO_SNDTIMEO:
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
	case SO_MARK:
	case SO_RXQ_OVFL:
	case SO_ATTACH_FILTER:
	case SO_DETACH_FILTER:
		break;
	default:
		//nack?
		PRINT_DEBUG("default=%d", optname);
		break;
	}

	if (send_dst == -1) {
		PRINT_DEBUG("freeing meta=%d", (int)params);
		metadata_destroy(params);
		nack_send(uniqueSockID, getsockopt_call);
	} else if (send_dst == 0) {
		PRINT_DEBUG("freeing meta=%d", (int)params);
		metadata_destroy(params);

		int msg_len = 4 * sizeof(int) + sizeof(unsigned long long) + len;
		u_char *msg = (u_char *) malloc(msg_len);
		u_char *pt = msg;

		*(int *) pt = getsockopt_call;
		pt += sizeof(int);

		*(unsigned long long *) pt = uniqueSockID;
		pt += sizeof(unsigned long long);

		*(int *) pt = ACK;
		pt += sizeof(int);

		*(int *) pt = param_id;
		pt += sizeof(int);

		*(int *) pt = len;
		pt += sizeof(int);

		if (len > 0) {
			memcpy(pt, val, len);
			pt += len;
		}

		if (pt - msg != msg_len) {
			PRINT_DEBUG("write error: diff=%d len=%d\n", pt - msg, msg_len);
			free(msg);
			PRINT_DEBUG("setsockopt_tcp: Exiting, No fdf: index=%d, uniqueSockID=%llu", index, uniqueSockID);
			nack_send(uniqueSockID, getsockopt_call);
			return;
		}

		PRINT_DEBUG("msg_len=%d msg=%s", msg_len, msg);
		if (send_wedge(nl_sockfd, msg, msg_len, 0)) {
			PRINT_DEBUG("setsockopt_tcp: Exiting, fail send_wedge: index=%d, uniqueSockID=%llu", index, uniqueSockID);
			nack_send(uniqueSockID, setsockopt_call);
		} else {
			PRINT_DEBUG("setsockopt_tcp: Exiting, normal: index=%d, uniqueSockID=%llu", index, uniqueSockID);
		}
	} else {
		if (jinni_TCP_to_fins_cntrl(CTRL_READ_PARAM, params)) {
			struct jinni_tcp_thread_data *thread_data = (struct jinni_tcp_thread_data *) malloc(sizeof(struct jinni_tcp_thread_data));
			thread_data->id = thread_count++;
			thread_data->index = index;
			thread_data->uniqueSockID = uniqueSockID;
			thread_data->flags = 0; //TODO implement?

			pthread_t thread;
			if (pthread_create(&thread, NULL, setsockopt_tcp_thread, (void *) thread_data)) {
				PRINT_ERROR("ERROR: unable to create setsockopt_tcp_thread thread.");
				nack_send(uniqueSockID, setsockopt_call);
				free(thread_data);
				metadata_destroy(params);
			}
		} else {
			PRINT_DEBUG("socketjinni failed to accomplish accept");
			nack_send(uniqueSockID, setsockopt_call);
			metadata_destroy(params);
		}
	}

	/*
	 metadata *udpout_meta = (metadata *) malloc(sizeof(metadata));
	 metadata_create(udpout_meta);
	 metadata_writeToElement(udpout_meta, "dstport", &dstprt, META_TYPE_INT);
	 */

	/** check the opt_name to find which bit to access in the options variable then use
	 * the following code to handle the bits individually
	 * setting a bit   number |= 1 << x;  That will set bit x.
	 * Clearing a bit number &= ~(1 << x); That will clear bit x.
	 * The XOR operator (^) can be used to toggle a bit. number ^= 1 << x; That will toggle bit x.
	 * Checking a bit      value = number & (1 << x);
	 */
	//uint32_t socketoptions;
}