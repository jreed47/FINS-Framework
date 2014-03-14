/*
 * logger.c
 *
 *  Created on: Feb 3, 2013
 *      Author: Jonathan Reed
 */
#include "logger_internal.h"

void *switch_to_logger(void *local) {
	struct fins_module *module = (struct fins_module *) local;
	PRINT_DEBUG("Entered: module=%p", module);
	PRINT_IMPORTANT("Thread started: module=%p, index=%u, id=%u, name='%s'", module, module->index, module->id, module->name);

	while (module->state == FMS_RUNNING) {
		logger_get_ff(module);
		PRINT_DEBUG("");
	}

	PRINT_IMPORTANT("Thread exited: module=%p, index=%u, id=%u, name='%s'", module, module->index, module->id, module->name);
	PRINT_DEBUG("Exited: module=%p", module);
	return NULL;
}

void logger_get_ff(struct fins_module *module) {
	struct logger_data *md = (struct logger_data *) module->data;

	struct finsFrame *ff;
	do {
		secure_sem_wait(module->event_sem);
		secure_sem_wait(module->input_sem);
		ff = read_queue(module->input_queue);
		sem_post(module->input_sem);
	} while (module->state == FMS_RUNNING && ff == NULL && !md->interrupt_flag); //TODO change logic here, combine with switch_to_logger?

	if (module->state != FMS_RUNNING) {
		if (ff != NULL) {
			freeFinsFrame(ff);
		}
		return;
	}

	if (ff != NULL) {
		if (ff->metaData == NULL) {
			PRINT_ERROR("Error fcf.metadata==NULL");
			exit(-1);
		}

		if (ff->dataOrCtrl == FF_CONTROL) {
			logger_fcf(module, ff);
			PRINT_DEBUG("");
		} else if (ff->dataOrCtrl == FF_DATA) {
			if (md->started) {
				gettimeofday(&md->end, 0);
				md->packets++;
				md->bytes += ff->dataFrame.pduLength;
				//md->bytes += ff->dataFrame.pduLength - 28; //for end-end throughput of exp 2, to remove IP/UDP hdrs
			} else {
				md->started = 1;
				gettimeofday(&md->start, 0);
				md->packets = 1;
				md->bytes = ff->dataFrame.pduLength;
				//md->bytes = ff->dataFrame.pduLength - 28; //for end-end throughput of exp 2, to remove IP/UDP hdrs

				md->saved_packets = 0;
				md->saved_bytes = 0;
				md->saved_curr = 0;

				timer_once_start(md->to_data->tid, md->interval);
				PRINT_IMPORTANT("Logger starting");
			}
			freeFinsFrame(ff);
		} else {
			PRINT_ERROR("todo error: dataOrCtrl=%u", ff->dataOrCtrl);
			exit(-1);
		}
	} else if (md->interrupt_flag) {
		md->interrupt_flag = 0;

		logger_interrupt(module);
	} else {
		PRINT_WARN("todo error");
	}
}

void logger_fcf(struct fins_module *module, struct finsFrame *ff) {
	PRINT_DEBUG("Entered: module=%p, ff=%p, meta=%p", module, ff, ff->metaData);

	//TODO fill out
	switch (ff->ctrlFrame.opcode) {
	case CTRL_ALERT:
		PRINT_DEBUG("opcode=CTRL_ALERT (%d)", CTRL_ALERT);
		PRINT_WARN("todo");
		module_reply_fcf(module, ff, FCF_FALSE, 0);
		break;
	case CTRL_ALERT_REPLY:
		PRINT_DEBUG("opcode=CTRL_ALERT_REPLY (%d)", CTRL_ALERT_REPLY);
		PRINT_WARN("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_READ_PARAM:
		PRINT_DEBUG("opcode=CTRL_READ_PARAM (%d)", CTRL_READ_PARAM);
		PRINT_WARN("todo");
		module_reply_fcf(module, ff, FCF_FALSE, 0);
		break;
	case CTRL_READ_PARAM_REPLY:
		PRINT_DEBUG("opcode=CTRL_READ_PARAM_REPLY (%d)", CTRL_READ_PARAM_REPLY);
		PRINT_WARN("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_SET_PARAM:
		PRINT_DEBUG("opcode=CTRL_SET_PARAM (%d)", CTRL_SET_PARAM);
		logger_set_param(module, ff);
		break;
	case CTRL_SET_PARAM_REPLY:
		PRINT_DEBUG("opcode=CTRL_SET_PARAM_REPLY (%d)", CTRL_SET_PARAM_REPLY);
		PRINT_WARN("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_EXEC:
		PRINT_DEBUG("opcode=CTRL_EXEC (%d)", CTRL_EXEC);
		PRINT_WARN("todo");
		module_reply_fcf(module, ff, FCF_FALSE, 0);
		break;
	case CTRL_EXEC_REPLY:
		PRINT_DEBUG("opcode=CTRL_EXEC_REPLY (%d)", CTRL_EXEC_REPLY);
		PRINT_WARN("todo");
		freeFinsFrame(ff);
		break;
	case CTRL_ERROR:
		PRINT_DEBUG("opcode=CTRL_ERROR (%d)", CTRL_ERROR);
		PRINT_WARN("todo");
		freeFinsFrame(ff);
		break;
	default:
		PRINT_ERROR("opcode=default (%d)", ff->ctrlFrame.opcode);
		exit(-1);
		break;
	}
}

void logger_set_param(struct fins_module *module, struct finsFrame *ff) {
	PRINT_DEBUG("Entered: module=%p, ff=%p, meta=%p", module, ff, ff->metaData);

	switch (ff->ctrlFrame.param_id) {
	case LOGGER_SET_PARAM_FLOWS:
		PRINT_DEBUG("param_id=LOGGER_SET_PARAM_FLOWS (%d)", ff->ctrlFrame.param_id);
		module_set_param_flows(module, ff);
		break;
	case LOGGER_SET_PARAM_LINKS:
		PRINT_DEBUG("param_id=LOGGER_SET_PARAM_LINKS (%d)", ff->ctrlFrame.param_id);
		module_set_param_links(module, ff);
		break;
	case LOGGER_SET_PARAM_DUAL:
		PRINT_DEBUG("param_id=LOGGER_SET_PARAM_DUAL (%d)", ff->ctrlFrame.param_id);
		module_set_param_dual(module, ff);
		break;
	case LOGGER_SET_INTERVAL__id:
		PRINT_DEBUG("LOGGER_SET_INTERVAL");
		PRINT_WARN("todo");
		module_reply_fcf(module, ff, FCF_FALSE, 0);
		break;
	case LOGGER_SET_REPEATS__id:
		PRINT_DEBUG("LOGGER_SET_REPEATS");
		PRINT_WARN("todo");
		module_reply_fcf(module, ff, FCF_FALSE, 0);
		break;
	default:
		PRINT_DEBUG("param_id=default (%d)", ff->ctrlFrame.param_id);
		PRINT_WARN("todo");
		module_reply_fcf(module, ff, FCF_FALSE, 0);
		break;
	}
}

void logger_interrupt(struct fins_module *module) {
	PRINT_DEBUG("Entered: module=%p", module);

	struct logger_data *md = (struct logger_data *) module->data;

	if (md->started) {
		struct timeval current;
		gettimeofday(&current, 0);

		double diff_curr = time_diff(&md->start, &current) / 1000.0;
		double diff_period = diff_curr - md->saved_curr;
		int diff_packets = md->packets - md->saved_packets;
		int diff_bytes = md->bytes - md->saved_bytes;
		double diff_through = 8.0 * diff_bytes / diff_period;
		PRINT_IMPORTANT("period=%f-%f,\t packets=%d,\t bytes=%d,\t through=%f", md->saved_curr, diff_curr, diff_packets, diff_bytes, diff_through);

		if (0) {
			//TODO remove this test code for module pushing to RTM
			uint32_t serial_num = gen_control_serial_num();
			metadata *meta = (metadata *) secure_malloc(sizeof(metadata));
			metadata_create(meta);

			struct finsFrame *ff = (struct finsFrame *) secure_malloc(sizeof(struct finsFrame));
			ff->dataOrCtrl = FF_CONTROL;
			ff->metaData = meta;

			ff->ctrlFrame.sender_id = module->index;
			ff->ctrlFrame.serial_num = serial_num;
			ff->ctrlFrame.opcode = CTRL_ALERT;
			ff->ctrlFrame.param_id = LOGGER_ALERT_UPDATE__id;

			ff->ctrlFrame.data = (uint8_t *) secure_malloc(500);
			sprintf((char *) ff->ctrlFrame.data, "period=%f-%f,\t packets=%d,\t bytes=%d,\t through=%f", md->saved_curr, diff_curr, diff_packets, diff_bytes,
					diff_through);
			PRINT_DEBUG("value='%s'", ff->ctrlFrame.data);
			ff->ctrlFrame.data_len = strlen((char *) ff->ctrlFrame.data);

			int sent = module_send_flow(module, ff, 0/*LOGGER_FLOW_RTM*/);
			if (sent == 0) {
				freeFinsFrame(ff);
			}
		}

		md->saved_packets = md->packets;
		md->saved_bytes = md->bytes;
		md->saved_curr = diff_curr;

		//if (diff_curr > 2 * logger_repeats * logger_interval / 1000.0) {
		if (diff_curr >= 1 * md->repeats * md->interval / 1000.0) {
			md->started = 0;

			double test = time_diff(&md->start, &md->end) / 1000.0;
			//double through = 8.0 * (logger_bytes - 10 * 1470) / test;
			double through = 8.0 * md->bytes / test;
			PRINT_IMPORTANT("Logger stopping: total=%f,\t packets=%d,\t bytes=%d,\t through=%f", test, md->packets, md->bytes, through);
		} else {
			timer_once_start(md->to_data->tid, md->interval);
		}
	} else {
		PRINT_ERROR("run over?");
	}
}

void logger_init_knobs(struct fins_module *module) {
	metadata_element *root = config_root_setting(module->knobs);

	//metadata_element *exec_elem = secure_config_setting_add(root, OP_EXEC_STR, META_TYPE_GROUP);

	metadata_element *get_elem = secure_config_setting_add(root, OP_GET_STR, META_TYPE_GROUP);
	elem_add_param(get_elem, LOGGER_GET_INTERVAL__str, LOGGER_GET_INTERVAL__id, LOGGER_GET_INTERVAL__type);
	elem_add_param(get_elem, LOGGER_GET_REPEATS__str, LOGGER_GET_REPEATS__id, LOGGER_GET_REPEATS__type);

	metadata_element *set_elem = secure_config_setting_add(root, OP_SET_STR, META_TYPE_GROUP);
	elem_add_param(set_elem, LOGGER_SET_INTERVAL__str, LOGGER_SET_INTERVAL__id, LOGGER_SET_INTERVAL__type);
	elem_add_param(set_elem, LOGGER_SET_REPEATS__str, LOGGER_SET_REPEATS__id, LOGGER_SET_REPEATS__type);

	metadata_element *alert_elem = secure_config_setting_add(root, OP_LISTEN_STR, META_TYPE_GROUP);
	elem_add_param(alert_elem, LOGGER_ALERT_UPDATE__str, LOGGER_ALERT_UPDATE__id, LOGGER_ALERT_UPDATE__type); //test
}

int logger_init(struct fins_module *module, metadata_element *params, struct envi_record *envi) {
	PRINT_DEBUG("Entered: module=%p, params=%p, envi=%p", module, params, envi);
	module->state = FMS_INIT;
	module_create_structs(module);

	logger_init_knobs(module);

	module->data = secure_malloc(sizeof(struct logger_data));
	struct logger_data *md = (struct logger_data *) module->data;

	//TODO extract this from meta?
	md->started = 0;
	md->interval = LOGGER_INTERVAL_DEFAULT;
	md->repeats = LOGGER_REPEATS_DEFAULT;

	md->to_data = secure_malloc(sizeof(struct intsem_to_timer_data));
	md->to_data->handler = intsem_to_handler;
	md->to_data->flag = &md->flag;
	md->to_data->interrupt = &md->interrupt_flag;
	md->to_data->sem = module->event_sem;
	timer_create_to((struct to_timer_data *) md->to_data);

	return 1;
}

int logger_run(struct fins_module *module, pthread_attr_t *attr) {
	PRINT_DEBUG("Entered: module=%p, attr=%p", module, attr);
	module->state = FMS_RUNNING;

	logger_get_ff(module);

	struct logger_data *md = (struct logger_data *) module->data;
	secure_pthread_create(&md->switch_to_logger_thread, attr, switch_to_logger, module);

	return 1;
}

int logger_pause(struct fins_module *module) {
	PRINT_DEBUG("Entered: module=%p", module);
	module->state = FMS_PAUSED;

	//TODO
	return 1;
}

int logger_unpause(struct fins_module *module) {
	PRINT_DEBUG("Entered: module=%p", module);
	module->state = FMS_RUNNING;

	//TODO
	return 1;
}

int logger_shutdown(struct fins_module *module) {
	PRINT_DEBUG("Entered: module=%p", module);
	module->state = FMS_SHUTDOWN;
	sem_post(module->event_sem);

	struct logger_data *md = (struct logger_data *) module->data;
	timer_stop(md->to_data->tid);

	//TODO expand this

	PRINT_IMPORTANT("Joining switch_to_logger_thread");
	pthread_join(md->switch_to_logger_thread, NULL);

	return 1;
}

int logger_release(struct fins_module *module) {
	PRINT_DEBUG("Entered: module=%p", module);

	struct logger_data *md = (struct logger_data *) module->data;
	//TODO free all module related mem

	//delete timer
	timer_delete(md->to_data->tid);
	free(md->to_data);

	//free common module data
	if (md->link_list != NULL) {
		list_free(md->link_list, free);
	}
	free(md);
	module_destroy_structs(module);
	free(module);
	return 1;
}

void logger_dummy(void) {

}

static struct fins_module_ops logger_ops = { .init = logger_init, .run = logger_run, .pause = logger_pause, .unpause = logger_unpause, .shutdown =
		logger_shutdown, .release = logger_release, };

struct fins_module *logger_create(uint32_t index, uint32_t id, uint8_t *name) {
	PRINT_DEBUG("Entered: index=%u, id=%u, name='%s'", index, id, name);

	struct fins_module *module = (struct fins_module *) secure_malloc(sizeof(struct fins_module));

	strcpy((char *) module->lib, LOGGER_LIB);
	module->flows_max = LOGGER_MAX_FLOWS;
	module->ops = &logger_ops;
	module->state = FMS_FREE;

	module->index = index;
	module->id = id;
	strcpy((char *) module->name, (char *) name);

	PRINT_DEBUG("Exited: index=%u, id=%u, name='%s', module=%p", index, id, name, module);
	return module;
}
