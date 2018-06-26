# Project 1 Report
## Phase 1
* A semaphore is a counter that, when passed to __sem_up()__
either has it's counter incremented or releases a threads TID from it's
queue, in order to unblock the thread. Likewise, __sem_down()__, either
decrements a semaphore's counter or stores the calling threads TID in
its wait queue. In order to meet the aforementioned functionality,
our semaphore struct contains a __queue_t__ field (__q__) as the queue
and a __size_t__ field (__count__) as the counter.


* Our implementation of __sem_down()__ guarantees the required
functionality because it is written so that it always decrements
the counter, if the counter is positive. And it always blocks
the calling thread and stores it's TID in the semaphore's queue,
if the semaphore's counter is 0. To do this, __sem_down()__ must
check to see if the counter of the inputted __sem__ struct,
(__count__), is greater than 0. If it is, then its counter
is decremented and sem_down returns. If not, the current thread's
ID is stored in the queue of the inputted __sem__ struct, it and
it is blocked. We mark the entire operation as a critical section
because the conditions of each of the __if__ statements did not always
evaluate correctly when threads accessing them were not given mutual
exclusion.


* Our implementation of __sem_up()__ guarantees the required
functionality because it is written in a way that always increments
the __sem__ counter if the __sem__ queue is empty, or it always removes
and unblocks a thread from the semaphore's queue, if the semaphore's
queue is not empty. To do this, __sem_up()__ must check to see if the
queue of the __sem__ struct is empty. If it's empty, then its counter is
incremented and the function returns. If its queue is not empty, then
a blocked thread's ID is dequeued from the queue of the __sem__
struct, the thread is unblocked, and then the function returns. We
mark the entire operation as a critical section because the conditions
of each of the __if__ statements did not always evaluate correctly when
threads accessing them were not given mutual exclusion.


* __sem_create()__ returns a semaphore struct. It does this by first
dynamically allocating a __sem__ struct. After verifying that the
allocation was successful, the __sem->count__ field is set to __count__
and then a queue struct is obtained by calling __queue_create()__.The
queue is then stored in the __sem->q__ field. Lastly, the semaphore
is returned.


* __sem_destroy()__ takes a __sem__ and deallocates it's memory. It
first checks to make sure the inputted __sem__ struct is NULL. It then
checks to make sure the queue of the __sem__ struct is empty. If it's
not, the function returns -1. If it is, the function calls __free()__
on the __sem__ struct and then returns 0. We mark from the call to
queue_length(), that checks to see if threads are being blocked in the
__sem__'s queue, to after the call to free(__sem__) as a critical
section. We do this because function calls and accesses to __sem__ did
not always receive the expected result when the threads accessing it
were not given mutual exclusion.




## Phase 2.1 

* We use separate structs for a TPS and for the page it points to,
__p_tps__ and __tps_page__  respectively. The phase 2.3 specifications
instruct us to "dissociate the TPS object from the memory page".
__p_tps__ is the TPS for a given thread and __tps_page__ is its memory
page. So we clearly meet the dissociation requirement. This
dissociation leads logically to implementing __COW__. It allows us to
allocate and assign a TPS to a thread without also allocating a page
for it (at least right away). We can then set the page address of a
__p_tps__ struct to that of any page belonging to a thread by calling
__clone()__. This allows us to have several threads pointing to and
reading from 1 __tps_page__. Which aligns  with how read should
function for __COW__. A __p_tps__ is created in __tps_create__ by using
tps_init_whole to first initiate a __p_tps__ struct and then using
__tps_init_page()__ to allocate and then initialize its __tps_page__().


* Our implementation of __tps_read()__ is consistent with the API
specifications because it will read the information stored in the
current threads page, store this information in its buffer, and then
return. We first call __enter_critical_section()__ because we’re about
to make a page of TPS memory readable but we do not want any other
threads to be able to read it or for a thread outside of the API to
be able to successful access it. With this, the read function does
its part to meet the requirement that each page is protected. We then
use __ll_find()__ to search for the current threads page of memory.
We must then make this page readable, by using mprotect() to change its
access flags from __PROT_NONE__ to __PROT_READ__. The read is performed
by using memcpy to copy the specified section of the page to the buffer
inputted by the calling thread. The access flag is restored to
__PROT_NONE__ and then we call __exit_critical_section()__  because the
memory page is no longer accessible, so we no longer need mutual
exclusion.


* Our implementation of __tps_write()__ is consistent with the API
specifications because it will write the information stored in the
calling threads buffer into the threads assigned, private page. We
first call __enter_critical_section()__ because we’re about to make
the page of memory readable but we do not want any other threads to
be able to read it or for a thread outside of the API to be able to
successfully access it. With this, the write function does its part to
meet the requirement that each page is protected. We then use
__ll_find()__ to search for the current thread's page of memory. We
must then make this page writable, by using mprotect() to change its
access flags from  __PROT_NONE__ to __PROT_WRITE__. The write is
performed by using __memcpy()__ to copy the buffer inputted by the
calling thread to the specified section of the TPS page. Then, the
access flag is restored to __PROT_NONE__ and then we call
__exist_critical_section()__ because the memory page is no longer
accessible, so we no longer need mutual exclusion.


* Our implementation of __tps_destroy()__ uses
__enter_critical_section()__ to atomoically call __ll_find()__
on the calling threads ID. This retrieves the __p_tps__ struct of the
calling thread and then use __munmap()__ to deallocate it's memory
page (if the calling thread is the only thread with a reference to
this page). We then use __free()__ to deallocate the thread's p_tps
struct. The sensitive instructions are done so we call
__exit_critical_section()__.


* A TPS API must keep track of how many TPS objects exist as well as the
memory pages these TPS objects point to. It is intuitive to use a linked
list to keep track of all __p_tps__ structs. This method indirectly keeps
track of the pages, and the threads that point to them, because each
__p_tps__ struct has a pointer to a page of memory and also stores the ID
of the thread that owns the __p_tps__. Each __node__ in the linked list
points to a __p_tps__ struct and is assigned to a particular thread.


* Our linked list API builds a list out of __node__ struct objects.
These __node__ objects contain a pointer of type __p_tps__ and a
pointer to the next __node__ in the list. This API also includes
the functions __ll_insert()__, __ll_find()__, __ll_find_memloc()__,
and __ll_delete()__.


* We use the global __node__ struct __head__ to store a pointer to the
beginning of the linked list. It must be global because it is shared
by every thread that uses the API. It is used as a starting point for
iterating through the TPS-holding linked list.


* __ll_insert()__ is used to insert a __p_tps__ struct into the linked
list that manages all created TPS’s. It accepts a __p_tps__ struct,
dynamically allocates a node object, stores a reference to the __p_tps__
struct in the __node__, and then adds this node object to the front of
the list by setting the node objects next field to the node pointed to
by head.


* __ll_find()__ is used to find and return the address of the __p_tps__
struct assigned to the thread with ID __tid__ (the parameter of
__ll_find__).

* __ll_delete()__ is used to remove a TPS objects from the list that
stores TPS objects.

* __ll_find_memloc()__ is used to identify whether a page of memory is
already being pointed to by a thread. It is used in the __sig_segv()__
handler for checking if an address belongs to a __p_tps__ page.
It takes a page address as a parameter, and then iterates through the
__p_tps__ struct holding linked list until a node containing __p_tps__
struct containing a __tps_page__ struct, which points to address
__pageaddr__ is found. If found, the function returns 1. If not found,
0 is returned. 


* __tps_init_whole()__ is used to allocate a p_tps struct and initialize
it's fields. This function is called by tps_create() during the creation
of a thread private space.

* __tps_init_page()__ is used to allocate a tps_page struct and
initialize it's fields. This function is called by tps_create() during
the creation of a thread private space.


## Phase 2.2
* Our TPS implementation guarantees protection because before the
functions that modify the TPS (__tps_clone()__, __tps_read ()__,
__tps_write()__, __tps_create()__) return, they set the access flags
of the TPS page to __PROT_NONE__. This will cause a segfault if a
thread tries to read or write to the page after any of the functions
that can access the page return. The functions that access the page
call __enter_critical_section()__ before calling __mprotect()__ to
change the pages flags to __PROT_READ__ or __PROT_WRITE__. The flag
of the page(s) is reset to __PROT_NONE__ before the function calls
__exit_critical_section()__. So the API functions have exclusive access
to the page while the page is readable or writable. This means that each
page can only be accessed, without causing a segfault, within the API
functions. Therefore, each TPS has a protected page of memory.

* To get the required functionality from __segv_handler()__ we filled
in the __if__ condition that determines if the address stored in
__p_fault__ is an address belonging to one of our __tps_page__ structs.
We input __p_fault__  into __ll_find__memloc()__ in order to determine
this. If it does belong to a TPS `"TPS protection error!\n"` is printed.
__segv_handler__ will be called in the event of a segfault because of
the following: `sa.sa_sigaction = segv_handler;` in __tps_init()__.



## Phase 2.3
* Our implementation of copy-on-write (__COW__) satisfies the API
because when a thread calls __tps_clone()__ it is assigned a __p_tps__
that points to the page assigned to the thread that the calling thread
requested to clone. It's buffer is filled with the contents of the
page being cloned, and then __tps_clone()__ returns. A page hasn't
been allocated for the calling thread at this point which is consistent
with the API. However, when __tps_write()__ is called by a thread to
write to the page the thread that called clone is pointing to, a
new page is made. The contents of the old page are copied to it, and then
the write is performed. With this method, the calling thread is able to
copy as well as write even though it doesn't have its own page object
until after calling write. A read is performed by calling __tps_read()__,
which will not allocate a new tps_page for a reading thread. This
satisfies the API requirements for __COW__.



## Phase 2 Testers
* We chose to write tests which check for any of the possible errors. For
instance, each error check in `tps.c` should have a corresponding test.
Also, we created tests for the functionality, aimed at the boundary
cases. We targeted ways the API would be used, such as destroying a TPS
and recreating it to ensure all of the memory was correctly allocated.
For the phase on TPS protection, we tested for the segmentation fault
handler in the file `tps_segvh.c` with the wrapper function given by
Professor Porquet. We obtained the address of the TPS in memory and
attempted writing to the address when it was protected; this should
cause a TPS protection error and segfault. Also, we tested the copy
on write by copying thread2’s TPS from thread1, then writing from
thread2. Lastly, `test_segf.c` shows that our __sig_segv()__ function
knows how to differentiate between segfaults that involve a TPS address
and those that do not. It does this by dereferencing a NULL pointer,
which does not result in `"TPS protection error!\n"` being printed out.


### Reference
http://man7.org/linux/man-pages/man2/mprotect.2.html
http://man7.org/linux/man-pages/man2/mmap.2.html

