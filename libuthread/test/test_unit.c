#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

char msg1[TPS_SIZE] = "Hello world!\n";
char msg2[TPS_SIZE] = "hello world!\n";
char msg3[TPS_SIZE] = "oello world!\n";

sem_t sem1, sem2;

int testcount=0;

void* thread2(void* arg)
{
	char *buffer = malloc(TPS_SIZE);

	tps_create();

	tps_write(0, TPS_SIZE, msg3);

	//16: test write value
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg3, buffer, TPS_SIZE));
	printf("%d: + thread2 read msg3\n", ++testcount);

	//swap
	sem_up(sem1);
	sem_down(sem2);

	tps_write(0, TPS_SIZE, msg2);

	//20: copy on write
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg2, buffer, TPS_SIZE));
	printf("%d: + thread2 read msg2\n", ++testcount);

	//swap
	sem_up(sem1);
	sem_down(sem2);

	free(buffer);
	tps_destroy();

	return NULL;
}

void* thread1(void* arg)
{
	pthread_t t1tid;
	char *buffer_write = malloc(TPS_SIZE);
	char *buffer_read = malloc(TPS_SIZE);
	char *buffer_null = NULL;

	//1: fail on destroy with no tps
	assert(tps_destroy() == -1);
	printf("%d:  + errcheck: no tps created\n", ++testcount);

	//2: read nothing
	assert(tps_read(0, TPS_SIZE, buffer_read) == -1);
	printf("%d:  + errcheck: no tps to read from\n", ++testcount);

	//3: write nothing
	assert(tps_read(0, TPS_SIZE, buffer_read) == -1);
	printf("%d:  + errcheck: no tps to write to\n", ++testcount);
	
	//4: first creation
	assert(tps_create() == 0);
	printf("%d:  + thread1 creation\n", ++testcount);

	//5: fail on create with tps already existing
	assert(tps_create() == -1);
	printf("%d:  + errcheck: tps already created\n", ++testcount);

	//6: destroy
	assert(tps_destroy() == 0);
	printf("%d:  + thread1 destroy\n", ++testcount);

	//7: recreation
	assert(tps_create() == 0);
	printf("%d:  + thread1 recreation\n", ++testcount);

	//8: write
	assert(tps_write(0, TPS_SIZE, msg1) == 0);
	printf("%d:  + thread1 first write\n", ++testcount);

	//9: write offset + length out of bounds
	//msg2 should not be written.
	assert(tps_write(1, TPS_SIZE, msg2) == -1);
	printf("%d:  + errcheck: out of bounds\n", ++testcount);

	//10: read NULL buffer
	assert(tps_write(0, TPS_SIZE, buffer_null) == -1);
	printf("%d: + errcheck: buffer is null\n", ++testcount);

	//11: read
	memset(buffer_read, 0, TPS_SIZE);
	assert(tps_read(0, TPS_SIZE, buffer_read) == 0);
	printf("%d: + thread1 first read\n", ++testcount);

	//12: read correct value
	assert(!memcmp(msg1, buffer_read, TPS_SIZE));
	printf("%d: + thread1 read msg1\n", ++testcount);

	//13: read offset + length out of bounds
	assert(tps_read(1, TPS_SIZE, buffer_read) == -1);
	printf("%d: + errcheck: out of bounds\n", ++testcount);

	//14: read NULL buffer
	assert(tps_read(0, TPS_SIZE, buffer_null) == -1);
	printf("%d: + errcheck: buffer is null\n", ++testcount);

	//15: create thread 2 and get blocked
	pthread_create(&t1tid, NULL, thread2, NULL);
	assert(tps_clone(t1tid) == -1);
	printf("%d: + errcheck: clone target has no tps\n", ++testcount);

	sem_down(sem1);

	//17: clone with tps already existing
	assert(tps_clone(t1tid) == -1);
	printf("%d: + errcheck: clone tps already existing\n", ++testcount);

	//destroy for clone
	tps_destroy();

	//18: clone
	assert(tps_clone(t1tid) == 0);
	printf("%d: + thread1 clone thread2\n", ++testcount);

	//19: read
	memset(buffer_read, 0, TPS_SIZE);
	assert(tps_read(0, TPS_SIZE, buffer_read) == 0);
	printf("%d: + thread1 read msg3\n", ++testcount);

	//swap to thread2
	sem_up(sem2);
	sem_down(sem1);

	//21: read after copy on write
	memset(buffer_read, 0, TPS_SIZE);
	assert(tps_read(0, TPS_SIZE, buffer_read) == 0);
	printf("%d: + thread1 read msg3\n", ++testcount);

	sem_up(sem2);
	pthread_join(t1tid,NULL);

	//cleanup
	free(buffer_write);
	free(buffer_read);
	tps_destroy();

	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t tid;

	/* Create two semaphores for thread synchro */
	sem1 = sem_create(0);
	sem2 = sem_create(0);

	/* Init TPS API */
	tps_init(1);

	/* Create thread 1 and wait */
	pthread_create(&tid, NULL, thread1, NULL);
	pthread_join(tid, NULL);

	/* Destroy resources and quit */
	sem_destroy(sem1);
	sem_destroy(sem2);
	return 0;
}
