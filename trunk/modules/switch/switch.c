/**
 * @file switch.c
 *
 *  @date Mar 14, 2011
 *      @author Abdallah Abdallah
 */

#include "switch_internal.h"

int switch_register_module(struct fins_module *module, struct fins_module *new_mod) {
	PRINT_DEBUG("Entered: module=%p, new_mod=%p, id=%d, name='%s'", module, new_mod, new_mod->id, new_mod->name);

	if (new_mod->index >= MAX_MODULES) {
		PRINT_ERROR("todo error");
		return -1;
	}

	struct switch_data *data = (struct switch_data *) module->data;

	secure_sem_wait(module->input_sem);
	if (data->fins_modules[new_mod->index] != NULL) {
		PRINT_IMPORTANT("Replacing: mod=%p, id=%d, name='%s'",
				data->fins_modules[new_mod->index], data->fins_modules[new_mod->index]->id, data->fins_modules[new_mod->index]->name);
	}
	PRINT_IMPORTANT("Registered: new_mod=%p, id=%d, name='%s'", new_mod, new_mod->id, new_mod->name);
	data->fins_modules[new_mod->index] = new_mod;
	sem_post(module->input_sem);

	PRINT_DEBUG("Exited: module=%p, new_mod=%p, id=%d, name='%s'", module, new_mod, new_mod->id, new_mod->name);
	return 0;
}

int switch_unregister_module(struct fins_module *module, int index) {
	PRINT_DEBUG("Entered: index=%d", index);

	if (index < 0 || index > MAX_MODULES) {
		PRINT_ERROR("todo error");
		return 0;
	}

	struct switch_data *data = (struct switch_data *) module->data;

	secure_sem_wait(module->input_sem);
	if (data->fins_modules[index] != NULL) {
		PRINT_IMPORTANT("Unregistering: mod=%p, id=%d, name='%s'", data->fins_modules[index], data->fins_modules[index]->id, data->fins_modules[index]->name);
		data->fins_modules[index] = NULL;
	} else {
		PRINT_IMPORTANT("No module to unregister: index=%d", index);
	}
	sem_post(module->input_sem);

	return 1;
}

//#################################################

void *switch_loop(void *local) {
	struct fins_module *module = (struct fins_module *) local;

	PRINT_IMPORTANT("Entered: module=%p", module);

	struct switch_data *data = (struct switch_data *) module->data;

	uint32_t i;
	int ret;
	struct finsFrame *ff;
	uint8_t index;

	int counter = 0;

	while (module->state == FMS_RUNNING) {
		secure_sem_wait(module->event_sem);
		secure_sem_wait(module->input_sem);
		for (i = 0; i < MAX_MODULES; i++) {
			if (data->fins_modules[i] != NULL) {
				if (!IsEmpty(data->fins_modules[i]->output_queue)) { //added as optimization
					while ((ret = sem_wait(data->fins_modules[i]->output_sem)) && errno == EINTR)
						;
					if (ret) {
						PRINT_ERROR("sem wait prob: src module_index=%u, ret=%d", i, ret);
						exit(-1);
					}
					ff = read_queue(data->fins_modules[i]->output_queue);
					sem_post(data->fins_modules[i]->output_sem);

					//if (ff != NULL) { //shouldn't occur
					counter++;

					index = ff->destinationID;
					if (index < 0 || index > MAX_MODULES) { //TODO check/change should be MAX_ID?
						PRINT_ERROR("dropping ff: illegal destination: src module_index=%u, dst module_index=%u, ff=%p, meta=%p", i, index, ff, ff->metaData);
						//TODO if FCF set ret_val=0 & return? or free or just exit(-1)?
						freeFinsFrame(ff);
					} else { //if (i != id) //TODO add this?
						//id = LOGGER_ID; //TODO comment
						if (data->fins_modules[index] != NULL) {
							PRINT_DEBUG("Counter=%d, from='%s', to='%s', ff=%p, meta=%p",
									counter, data->fins_modules[i]->name, data->fins_modules[index]->name, ff, ff->metaData);
							//TODO decide if should drop all traffic to switch input queues, or use that as linking table requests
							if (index == SWITCH_INDEX) {
								switch_process_ff(module, ff);
							} else {
								while ((ret = sem_wait(data->fins_modules[index]->input_sem)) && errno == EINTR)
									;
								if (ret) {
									PRINT_ERROR("sem wait prob: dst index=%u, ff=%p, meta=%p, ret=%d", index, ff, ff->metaData, ret);
									exit(-1);
								}
								if (write_queue(ff, data->fins_modules[index]->input_queue)) {
									sem_post(data->fins_modules[index]->event_sem);
									sem_post(data->fins_modules[index]->input_sem);
								} else {
									sem_post(data->fins_modules[index]->input_sem);
									PRINT_ERROR("Write queue error: dst index=%u, ff=%p, meta=%p", index, ff, ff->metaData);
									freeFinsFrame(ff);
								}
							}
						} else {
							PRINT_ERROR("dropping ff: destination not registered: src index=%u, dst index=%u, ff=%p, meta=%p", i, index, ff, ff->metaData);
							print_finsFrame(ff);
							//TODO if FCF set ret_val=0 & return? or free or just exit(-1)?
							freeFinsFrame(ff);
						}
					}
					//}
				}
			}
		}
		sem_post(module->input_sem);
	}

	PRINT_IMPORTANT("Exited");
	//pthread_exit(NULL);
	return NULL;
} // end of switch_init Function

void switch_process_ff(struct fins_module *module, struct finsFrame *ff) {
	PRINT_IMPORTANT("Entered: module=%p, ff=%p", module, ff);

	if (ff->metaData == NULL) {
		PRINT_ERROR("Error fcf.metadata==NULL");
		exit(-1);
	}

	PRINT_ERROR("TODO: switch process received frames: ff=%p, meta=%p", ff, ff->metaData);
	print_finsFrame(ff);

	if (ff->dataOrCtrl == CONTROL) {
		switch_fcf(module, ff);
		PRINT_DEBUG("");
	} else if (ff->dataOrCtrl == DATA) {
		if (ff->dataFrame.directionFlag == DIR_UP) {
			//switch_in_fdf(module, ff);
			PRINT_DEBUG("todo");
		} else { //directionFlag==DIR_DOWN
			//switch_out_fdf(ff);
			PRINT_ERROR("todo");
		}
	} else {
		PRINT_ERROR("todo error");
		exit(-1);
	}
}

void switch_fcf(struct fins_module *module, struct finsFrame *ff) {
	PRINT_DEBUG("Entered: ff=%p, meta=%p", ff, ff->metaData);

	//TODO fill out
	switch (ff->ctrlFrame.opcode) {
	case CTRL_ALERT:
		PRINT_DEBUG("opcode=CTRL_ALERT (%d)", CTRL_ALERT);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_ALERT_REPLY:
		PRINT_DEBUG("opcode=CTRL_ALERT_REPLY (%d)", CTRL_ALERT_REPLY);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_READ_PARAM:
		PRINT_DEBUG("opcode=CTRL_READ_PARAM (%d)", CTRL_READ_PARAM);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_READ_PARAM_REPLY:
		PRINT_DEBUG("opcode=CTRL_READ_PARAM_REPLY (%d)", CTRL_READ_PARAM_REPLY);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_SET_PARAM:
		PRINT_DEBUG("opcode=CTRL_SET_PARAM (%d)", CTRL_SET_PARAM);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_SET_PARAM_REPLY:
		PRINT_DEBUG("opcode=CTRL_SET_PARAM_REPLY (%d)", CTRL_SET_PARAM_REPLY);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_EXEC:
		PRINT_DEBUG("opcode=CTRL_EXEC (%d)", CTRL_EXEC);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_EXEC_REPLY:
		PRINT_DEBUG("opcode=CTRL_EXEC_REPLY (%d)", CTRL_EXEC_REPLY);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_ERROR:
		PRINT_DEBUG("opcode=CTRL_ERROR (%d)", CTRL_ERROR);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	default:
		PRINT_DEBUG("opcode=default (%d)", ff->ctrlFrame.opcode);
		PRINT_ERROR("todo");
		freeFinsFrame(ff);
		break;
	}
}

int switch_init(struct fins_module *module, uint32_t *flows, uint32_t flows_num, metadata_element *params, struct envi_record *envi) {
	PRINT_IMPORTANT("Entered: module=%p, params=%p, envi=%p", module, params, envi);
	module->state = FMS_INIT;
	module_create_queues(module);

	switch_event_sem = module->event_sem;

	module->data = secure_malloc(sizeof(struct switch_data));
	struct switch_data *data = (struct switch_data *) module->data;

	if (module->max_flows < flows_num) {
		PRINT_ERROR("todo error");
		return 0;
	}
	data->flows_num = flows_num;

	int i;
	for (i = 0; i < flows_num; i++) {
		data->flows[i] = flows[i];
	}

	for (i = 0; i < MAX_MODULES; i++) {
		data->fins_modules[i] = NULL;
	}

	return 1;
}

int switch_run(struct fins_module *module, pthread_attr_t *attr) {
	PRINT_IMPORTANT("Entered: module=%p, attr=%p", module, attr);
	module->state = FMS_RUNNING;

	struct switch_data *data = (struct switch_data *) module->data;
	secure_pthread_create(&data->switch_thread, attr, switch_loop, module);

	return 1;
}

int switch_pause(struct fins_module *module) {
	PRINT_IMPORTANT("Entered: module=%p", module);
	module->state = FMS_PAUSED;

//TODO
	return 1;
}

int switch_unpause(struct fins_module *module) {
	PRINT_IMPORTANT("Entered: module=%p", module);
	module->state = FMS_RUNNING;

//TODO
	return 1;
}

int switch_shutdown(struct fins_module *module) {
	PRINT_IMPORTANT("Entered: module=%p", module);
	module->state = FMS_SHUTDOWN;
	sem_post(module->event_sem);

	struct switch_data *data = (struct switch_data *) module->data;

//TODO expand this

	PRINT_IMPORTANT("Joining switch_thread");
	pthread_join(data->switch_thread, NULL);

	return 1;
}

int switch_release(struct fins_module *module) {
	PRINT_IMPORTANT("Entered: module=%p", module);

	struct switch_data *data = (struct switch_data *) module->data;
//TODO free all module related mem

	free(data);

	module_destroy_queues(module);
//sem_destroy(module->input_sem);
//free(module->input_sem);
//sem_destroy(module->event_sem);
//free(module->event_sem);

	free(module);
	return 1;
}

void switch_dummy(void) {

}

static struct fins_module_switch_ops switch_ops = { .init = switch_init, .run = switch_run, .pause = switch_pause, .unpause = switch_unpause, .shutdown =
		switch_shutdown, .release = switch_release, .register_module = switch_register_module, .unregister_module = switch_unregister_module };

struct fins_module *switch_create(uint32_t index, uint32_t id, uint8_t *name) {
	PRINT_IMPORTANT("Entered: index=%u, id=%u, name='%s'", index, id, name);

	struct fins_module *module = (struct fins_module *) secure_malloc(sizeof(struct fins_module));

	strcpy((char *) module->lib, SWITCH_LIB);
	module->max_flows = SWITCH_MAX_FLOWS; //TODO change?
	module->ops = (struct fins_module_ops *) &switch_ops;
	module->state = FMS_FREE;

	module->index = index;
	module->id = id;
	strcpy((char *) module->name, (char *) name);

	PRINT_IMPORTANT("Exited: index=%u, id=%u, name='%s', module=%p", index, id, name, module);
	return module;
}
