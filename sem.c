#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"

struct semaphore {
	size_t sem_count; //number of threads able to share sem
	queue_t sem_waitthreads; //threads waiting for resource
};

sem_t sem_create(size_t count)
{
	//begin sensitive instructions
	enter_critical_section();
	//dynamically allocate a sem_t object
	sem_t new_sem = malloc(sizeof(sem_t));

	//error checking
	if (new_sem == NULL)
		return NULL;

	//initialize
	new_sem->sem_count = count;
	new_sem->sem_waitthreads = queue_create();

	//done with sensitive instructions
	exit_critical_section();

	return new_sem;
}

int sem_destroy(sem_t sem)
{
	//begin sensitive instructions
	enter_critical_section();

	//error checking
	if (sem==NULL || queue_length(sem->sem_waitthreads) != 0) {
		exit_critical_section();
		return -1;
	}
	//destroy the semaphores queue
	queue_destroy(sem->sem_waitthreads);
	free(sem);
	//done with sensitive instructions
	exit_critical_section();
	
	return 0;
}

int sem_down(sem_t sem)
{
	//begin sensitive instructions
	enter_critical_section();
	//error checking
	if (sem==NULL) {
		exit_critical_section();
		return -1;
	}
	//if sem_count is greater than 0 decrement it 
	if (sem->sem_count > 0) {
		sem->sem_count--;
	} else {
		//add to waiting threads and block it
		queue_enqueue(sem->sem_waitthreads,(void*)pthread_self());
		thread_block();

	}
	//done with sensitive instructions
	exit_critical_section();
	return 0;
}

int sem_up(sem_t sem)
{
	//begin sensitive instructions
	enter_critical_section();

	if (sem == NULL) {
		exit_critical_section();
		return -1;
	}

	//if there are waiting threads
	if (queue_length(sem->sem_waitthreads) == 0) {
		sem->sem_count++;
	} else {
		//used to store tid poped from queue
		pthread_t *tid2;
		void *tid;

		//resources available, dequeue waiting thread
		queue_dequeue(sem->sem_waitthreads, &tid);
		tid2 = (pthread_t*) tid;

		thread_unblock(*tid2);
	}
	//done with sensitive instructions
	exit_critical_section();
	return 0;
}
