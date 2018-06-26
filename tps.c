#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

//Struct which holds references to a TPS location
struct tps_page {
	//Actual location in memory
	void *tps_location;
	//Number of pointers to this page
	int ref_counter;
};

//Struct for each thread; each thread which calls create or clone will have one.
struct p_tps {
	//Ptr to page to allow cloning by reference
	struct tps_page *tps_info;
	//TID of owning thread
	pthread_t tps_tid;
};

//Node for Linked List of TPSes, used for internal storage
struct node {
	struct p_tps *tps;
	struct node *next;
};

//Globals:
static struct node *head = NULL; //Global for TPS Storage

//Private functions:

//Stack-like push 
static void ll_insert(struct p_tps* tps1)
{
	//create node
	struct node *n = (struct node*) malloc(sizeof(struct node));
	
	n->tps=tps1;
	n->next = head;
	head = n;
	return;
}

//Find a node with given tps TID
static struct p_tps* ll_find(pthread_t tid)
{

	//start at first node
	struct node* current = head;

	//list is empty
	if (head == NULL)
		return NULL;

	//iterate through list
	while (current->tps->tps_tid != tid) {
		//didn't find node
		if (current->next == NULL)
			return NULL;
		else
			current = current->next;
	}

	//found node of appropriate tps tid
	return current->tps;
}

//Find tps memory page matching pageaddr.
static int ll_find_memloc(void *pageaddr)
{
	struct node *current = head;

	if (head == NULL)
		return 0;

	while (current->tps->tps_info->tps_location != pageaddr) {

		if (current->next == NULL)
			return 0;
		else
			current = current->next;
	}

	return 1;
}

//Delete a node with given tps TID
static struct p_tps* ll_delete(pthread_t tid)
{
	struct node* current = head;
	struct node* previous = NULL;
	
	if (head == NULL)
		return NULL;

	//iterate through list
	while (current->tps->tps_tid != tid) {
		//at end of list, didn't find node
		if (current->next == NULL) {
			return NULL;
		} else {
			previous = current;
			current = current->next;
		}
	}

	//found node to delete 
	if (current == head)
		head = head->next;
	else
		previous->next = current->next;

	return current->tps;
}

/*
 * Initializes tps structs, guarantees info == NULL when initialized and
 * that each tps has a corresponding thread tid.
 * Should always be placed in a critical section.
 */
static struct p_tps *tps_init_whole(pthread_t tid)
{
	struct p_tps *targetinit = malloc(sizeof(struct p_tps));

	//error checking
	if (targetinit == NULL)
		return NULL;

	//Initialize
	targetinit->tps_tid = tid;
	targetinit->tps_info = NULL;

	return targetinit;
}

/*
 * Initializes tps pages. Guarantees that a page cannot exist without a 
 * pageholder.
 * Should always be placed in a critical section.
 */
static int tps_init_page(struct p_tps *pageholder)
{
	struct tps_page *newpage = malloc(sizeof(struct tps_page));

	//error checking
	if (newpage == NULL)
		return -1;

	//Allocate memory for the tps
	newpage->tps_location = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);

	//Protect tps and check for errors
	if (mprotect(newpage->tps_location, TPS_SIZE, PROT_NONE))
		return -1;

	//pages must always have ref counter greater than 0
	newpage->ref_counter = 1;
	pageholder->tps_info = newpage;

	return 0;
}

//For differentiating segfaults and tps protection faults
static void segv_handler(int sig, siginfo_t *si, void *context)
{
	/*
	* Get the address corresponding to the beginning of the page where the
	* fault occurred
	*/
	void *p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));

	if (ll_find_memloc(p_fault))
		fprintf(stderr, "TPS protection error!\n");

	/* In any case, restore the default signal handlers */
	signal(SIGSEGV, SIG_DFL);
	signal(SIGBUS, SIG_DFL);
	/* And transmit the signal again in order to cause the program to crash */
	raise(sig);
}

//API functions:
int tps_init(int segv)
{
	//Initialize seg fault handler
	if (segv) {
		struct sigaction sa;

		//Create and assign segv_handler
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;

		sa.sa_sigaction = segv_handler;

		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGSEGV, &sa, NULL);
	}

	return 0;
}

int tps_create(void)
{
	pthread_t cur_tid = pthread_self();
	struct p_tps *newtps;

	enter_critical_section();

	//Check for calling thread in list.
	newtps = ll_find(cur_tid);

	//If the calling thread already has a tps
	if (newtps != NULL)
		goto err_exit_crit;

	//If no errors, create a new tps.
	newtps = tps_init_whole(cur_tid);
	if (newtps == NULL)
		goto err_exit_crit;

	//Create page for new tps
	if (tps_init_page(newtps))
		goto err_exit_crit;

	//And insert into the list of TPSes
	ll_insert(newtps);

	exit_critical_section();

	return 0;

	//Exit, but must exit critical section first
err_exit_crit:
	exit_critical_section();
	return -1;
}

int tps_destroy(void)
{
	pthread_t cur_tid = pthread_self();
	struct p_tps *destroy_tps;

	enter_critical_section();

	//Attempt to delete the current thread's TPS from the list
	destroy_tps = ll_delete(cur_tid);

	//Checks if current thread didn't have a TPS
	if (destroy_tps == NULL)
		goto err_exit_crit;

	//If the TPS's page has no other references, destroy page
	if (destroy_tps->tps_info->ref_counter == 1) {
		//Deallocate TPS location
		if (munmap(destroy_tps->tps_info->tps_location, TPS_SIZE))
			goto err_exit_crit;

		//Free page
		free(destroy_tps->tps_info);
	}

	//Free resources
	free(destroy_tps);

	exit_critical_section();

	return 0;

	//Exit, but must exit critical section first
err_exit_crit:
	exit_critical_section();
	return -1;
}

int tps_read(size_t offset, size_t length, char *buffer)
{
	pthread_t cur_tid = pthread_self();
	struct p_tps *target_tps;
	
	//Error handling
	if (offset + length > TPS_SIZE || buffer == NULL)
		return -1;

	enter_critical_section();

	//Find TPS for current thread
	target_tps = ll_find(cur_tid);

	//Check if TPS was found
	if (target_tps == NULL)
		goto err_exit_crit;

	//Location of TPS
	void *target_memloc = target_tps->tps_info->tps_location;

	//Unprotect for read
	if (mprotect(target_memloc, TPS_SIZE, PROT_READ))
		goto err_exit_crit;

	//Copy data to buffer
	memcpy(buffer, target_memloc + offset, length);

	//Protect after read
	if (mprotect(target_memloc, TPS_SIZE, PROT_NONE))
		goto err_exit_crit;

	exit_critical_section();

	return 0;

	//Exit, but must exit critical section first
err_exit_crit:
	exit_critical_section();
	return -1;
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	pthread_t cur_tid = pthread_self();
	struct p_tps *target_tps;

	//Error checking: Requested data in bounds of TPS and buffer allocated
	if (offset + length > TPS_SIZE || buffer == NULL)
		return -1;

	enter_critical_section();

	//Find TPS of current thread
	target_tps = ll_find(cur_tid);

	//TPS wasn't found
	if (target_tps == NULL)
		goto err_exit_crit;

	//Location of TPS in memory
	void *target_memloc = target_tps->tps_info->tps_location;

	//Copy on write if the TPS has been cloned
	if (target_tps->tps_info->ref_counter > 1) {
		int *refcounter = &target_tps->tps_info->ref_counter;

		//create a copy of the page and point to it with target_tps
		if (tps_init_page(target_tps)){
			goto err_exit_crit;
		} else {
			void *new_memloc = target_tps->tps_info->tps_location;

			//Unprotect for copy: Write to new TPS, read from old TPS 
			if (mprotect(new_memloc, TPS_SIZE, PROT_WRITE) || mprotect(target_memloc, TPS_SIZE, PROT_READ))
				goto err_exit_crit;

			memcpy(new_memloc, target_memloc, TPS_SIZE);

			//Protect after copy
			if (mprotect(new_memloc, TPS_SIZE, PROT_NONE) || mprotect(target_memloc, TPS_SIZE, PROT_NONE))
				goto err_exit_crit;

			//New TPS page created, old reference is lowered
			refcounter--;
			target_memloc = target_tps->tps_info->tps_location;
		}
	}

	//Unprotect for write
	if (mprotect(target_memloc, TPS_SIZE, PROT_WRITE))
		goto err_exit_crit;

	//Write
	memcpy(target_memloc + offset, buffer, length);

	//Protect after write
	if (mprotect(target_memloc, TPS_SIZE, PROT_NONE))
		goto err_exit_crit;

	exit_critical_section();

	return 0;

	//Exit, but must exit critical section first
err_exit_crit:
	exit_critical_section();
	return -1;
}

int tps_clone(pthread_t tid)
{
	pthread_t dest_tid = pthread_self();
	pthread_t src_tid = tid;
	struct p_tps *src_tps;
	struct p_tps *dest_tps;

	enter_critical_section();

	//Search for both source and destination in TPS list
	src_tps = ll_find(src_tid);
	dest_tps = ll_find(dest_tid);

	//If source doesn't have a TPS, error
	if (src_tps == NULL)
		goto err_exit_crit;

	//If destination already has a TPS, error
	if (dest_tps != NULL)
		goto err_exit_crit;

	//Create new TPS to hold reference
	dest_tps = tps_init_whole(dest_tid);

	//Check for errors on Initialization
	if (dest_tps == NULL)
		goto err_exit_crit;

	//Set the tps page to reference the source and increase the ref counter
	dest_tps->tps_info = src_tps->tps_info;
	src_tps->tps_info->ref_counter++;

	//Add the new TPS to the list of TPSes
	ll_insert(dest_tps);

	exit_critical_section();

	return 0;

	//Exit, but must exit critical section first
err_exit_crit:
	exit_critical_section();
	return -1;
}

