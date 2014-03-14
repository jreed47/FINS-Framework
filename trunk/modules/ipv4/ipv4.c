/**
 * @file IP4.c
 * @brief FINS module - Internet Protocol version 4
 *
 * Main function of the module.
 */

#include "ipv4_internal.h"

void *switch_to_ipv4(void *local) {
	struct fins_module *module = (struct fins_module *) local;
	PRINT_DEBUG("Entered: module=%p", module);
	PRINT_IMPORTANT("Thread started: module=%p, index=%u, id=%u, name='%s'", module, module->index, module->id, module->name);

	while (module->state == FMS_RUNNING) {
		ipv4_get_ff(module);
		PRINT_DEBUG("");
	}

	PRINT_IMPORTANT("Thread exited: module=%p, index=%u, id=%u, name='%s'", module, module->index, module->id, module->name);
	PRINT_DEBUG("Exited: module=%p", module);
	return NULL;
}

//####################################### autoconfig
void ipv4_init_knobs(struct fins_module *module) {
	//metadata_element *root = config_root_setting(module->knobs);

	//metadata_element *exec_elem = secure_config_setting_add(root, OP_EXEC_STR, META_TYPE_GROUP);

	//metadata_element *get_elem = secure_config_setting_add(root, OP_GET_STR, META_TYPE_GROUP);

	//metadata_element *set_elem = secure_config_setting_add(root, OP_SET_STR, META_TYPE_GROUP);
	//elem_add_param(set_elem, LOGGER_SET_REPEATS__str, LOGGER_SET_REPEATS__id, LOGGER_SET_REPEATS__type);
}

void ipv4_ifr_get_addr_func(struct if_record *ifr, struct linked_list *ret_list) {
	if (ifr_running_test(ifr)) { //ifr->status ?
		//struct linked_list *temp_list = list_find_all(ifr->addr_list, addr_is_v4);
		struct linked_list *temp_list = list_filter(ifr->addr_list, addr_is_v4, addr_clone);
		if (list_join(ret_list, temp_list)) {
			free(temp_list);
		} else {
			PRINT_WARN("todo error");
			//list_free(temp_list, nop_func);
			list_free(temp_list, free);
		}
	}
}

int ipv4_init(struct fins_module *module, metadata_element *params, struct envi_record *envi) {
	PRINT_DEBUG("Entered: module=%p, params=%p, envi=%p", module, params, envi);
	module->state = FMS_INIT;
	module_create_structs(module);

	ipv4_init_knobs(module);

	module->data = secure_malloc(sizeof(struct ipv4_data));
	struct ipv4_data *md = (struct ipv4_data *) module->data;

	md->addr_list = list_create(IPV4_ADDRESS_LIST_MAX);
	list_for_each1(envi->if_list, ipv4_ifr_get_addr_func, md->addr_list);
	if (envi->if_loopback) {
		md->addr_loopback = (struct addr_record *) list_find(envi->if_loopback->addr_list, addr_is_v4);
	}
	if (envi->if_main) {
		md->addr_main = (struct addr_record *) list_find(envi->if_main->addr_list, addr_is_v4);
	}

	md->route_list = list_filter(envi->route_list, route_is_addr4, route_clone);
	if (md->route_list->len > IPV4_ROUTE_LIST_MAX) {
		PRINT_WARN("todo");
		struct linked_list *leftover = list_split(md->route_list, IPV4_ROUTE_LIST_MAX - 1);
		list_free(leftover, free);
	}
	md->route_list->max = IPV4_ROUTE_LIST_MAX;

	md->unique_id = 0;
	//when recv pkt would need to check addresses
	//when send pkt would need to check routing table & addresses (for ip address)
	//both of these would need to be updated by switch etc

	//routing_table = IP4_get_routing_table();

	PRINT_DEBUG("after ip4 sort route table");
	memset(&md->stats, 0, sizeof(struct ipv4_statistics));

	return 1;
}

int ipv4_run(struct fins_module *module, pthread_attr_t *attr) {
	PRINT_DEBUG("Entered: module=%p, attr=%p", module, attr);
	module->state = FMS_RUNNING;

	ipv4_get_ff(module);

	struct ipv4_data *md = (struct ipv4_data *) module->data;
	secure_pthread_create(&md->switch_to_ipv4_thread, attr, switch_to_ipv4, module);

	return 1;
}

int ipv4_pause(struct fins_module *module) {
	PRINT_DEBUG("Entered: module=%p", module);
	module->state = FMS_PAUSED;

	//TODO
	return 1;
}

int ipv4_unpause(struct fins_module *module) {
	PRINT_DEBUG("Entered: module=%p", module);
	module->state = FMS_RUNNING;

	//TODO
	return 1;
}

int ipv4_shutdown(struct fins_module *module) {
	PRINT_DEBUG("Entered: module=%p", module);
	module->state = FMS_SHUTDOWN;
	sem_post(module->event_sem);

	struct ipv4_data *md = (struct ipv4_data *) module->data;
	//TODO expand this

	PRINT_IMPORTANT("Joining switch_to_ipv4_thread");
	pthread_join(md->switch_to_ipv4_thread, NULL);

	return 1;
}

int ipv4_release(struct fins_module *module) {
	PRINT_DEBUG("Entered: module=%p", module);
	struct ipv4_data *md = (struct ipv4_data *) module->data;

	//TODO free all module related mem
	list_free(md->addr_list, free);
	list_free(md->route_list, free);

	//free common module data
	if (md->link_list != NULL) {
		list_free(md->link_list, free);
	}
	free(md);
	module_destroy_structs(module);
	free(module);
	return 1;
}

void ipv4_dummy(void) {

}

static struct fins_module_ops ipv4_ops = { .init = ipv4_init, .run = ipv4_run, .pause = ipv4_pause, .unpause = ipv4_unpause, .shutdown = ipv4_shutdown,
		.release = ipv4_release, };

struct fins_module *ipv4_create(uint32_t index, uint32_t id, uint8_t *name) {
	PRINT_DEBUG("Entered: index=%u, id=%u, name='%s'", index, id, name);

	struct fins_module *module = (struct fins_module *) secure_malloc(sizeof(struct fins_module));

	strcpy((char *) module->lib, IPV4_LIB);
	module->flows_max = IPV4_MAX_FLOWS;
	module->ops = &ipv4_ops;
	module->state = FMS_FREE;

	module->index = index;
	module->id = id;
	strcpy((char *) module->name, (char *) name);

	PRINT_DEBUG("Exited: index=%u, id=%u, name='%s', module=%p", index, id, name, module);
	return module;
}
