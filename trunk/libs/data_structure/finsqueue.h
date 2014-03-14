/**
 *
 * @file queueModule.h FOR COPYRIGHTS This code is a modified version from a code which
 * has been copied from an unknown code exists online. We dont claim the ownership of
 * the original code. But we claim the ownership of the modifications.
 *
 * @author Abdallah Abdallah
 * @date Nov 2, 2010
 *
 */
#ifndef FINSQUEUE_H_
#define FINSQUEUE_H_

#include <semaphore.h>

#include <finstypes.h>
#include "queue.h"

//ADDED mrd015 !!!!!
#ifdef BUILD_FOR_ANDROID
#include <linux/sem.h>
#else
#include  <sys/sem.h>
#endif

//#include <pthread.h>    /* POSIX Threads */

typedef Queue finsQueue;

finsQueue init_queue(const char* name, int size);
int checkEmpty(finsQueue Q);
int TerminateFinsQueue(finsQueue Q);
int DisposeFinsQueue(finsQueue Q);
int term_queue(finsQueue q);

int write_queue(struct finsFrame *ff, finsQueue q);
int write_queue_front(struct finsFrame *ff, finsQueue q);

struct finsFrame *read_queue(finsQueue q);

#endif /* FINSQUEUE_H_ */
