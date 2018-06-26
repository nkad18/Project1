#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

/*
 * This tests whether the TPS protection signal handler
 * prints an error when writing to a protected area.
 */

void *latest_addr;

//For obtaining the address of the TPS
int *__real_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
int *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	latest_addr = __real_mmap(addr, len, prot, flags, fildes, off);
	return latest_addr;
}

//Unused, but was specified in the given makefile
void *__real_munmap(void *addr, size_t length);
void *__wrap_munmap(void *addr, size_t length)
{
	return __real_munmap(addr, length);
}

void* thread1(void* arg)
{
	char *tps_address;
	
	tps_create();

	//get tps address with wrapper
	tps_address = latest_addr;

	//cause tps protection error
	printf("This should print 'TPS protection error!' to stderr and then segfault\n");
	tps_address[0] = '\0';
	
	tps_destroy();

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
