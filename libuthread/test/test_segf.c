#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

/*
 * This tests whether the TPS protection signal handler prints on
 * regular seg faults (It shouldn't).
 */

void* thread1(void* arg)
{
	//Cause regular seg fault
	printf("This should simply segfault, without a TPS message.\n");
	*(int*) 0 = 0;

	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t tid;

	/* Init TPS API */
	tps_init(1);

	/* Create thread 1 and wait */
	pthread_create(&tid, NULL, thread1, NULL);
	pthread_join(tid, NULL);

	return 0;
}
