Design Document for Project 1: Threads
======================================

## Group 82 Members

* William Zhuk <williamzhuk@berkeley.edu>
* Link Arneson <arneson@berkeley.edu>
* Heesoo Jang <hejang@berkeley.edu>
* Amir Dargulov <amir_dargulov@berkeley.edu>

## Task 1: Efficient Alarm Clock

### 1. Data structures and functions

#### New Global Variables
##### `struct list * sleep_queue` (in devices / timer.c)
* new global variable inside of timer.c
* `list` of threads sorted by `wake_tick`, a variable we add to `thread` to keep track of the tick it will wake 
* always dequeue the thread with the lowest `wake_tick` value
* Will need a semaphore to make sure we don't have any race conditions between `timer_interrupt` and `timer_sleep`

##### `struct semaphore * sleep_queue_sema`
* new global variable inside of timer.c
* mutual exclusion semaphore to protect from race conditions while adding to the sleep queue 

#### Edited Structs 
##### `int64_t wake_tick`
- add variable to thread struct 
- when added to the sleep_queue, set to the tick it is supposed to wake up on
- initialize to -1

#### Edited Functions

##### `thread_create`
- initialize `wake_tick` to -1

##### `void timer_sleep (int64_t ticks):`
* take out busy-wait.
* fast-exit if `ticks` is <= 0
* Calculate which tick the thread should wake up on.
* Add it to its correct place in (already instantiated) sleep queue, using pre-existing thread element field. 
* `sema_down` the queue when doing this to prevent
timer interrupt from accidentally messing with a queue in the middle of adding something onto it in the middle.

##### `void timer_init (void)`
* instantiate the sleep queue (stored as global variable)

##### `static void timer_interrupt (struct intr_frame * args UNUSED)`
* check sema for sleep queue. continue if 1.
* while first element in queue should be woken up,
remove it from sleep queue and add it to ready queue.

#### New Functions 

##### comparator function
* linked list has a function for sorting itself, we just need to tell it what to compare based on
* to be used for `sleep_queue` and passed in as an argument to `list_insert_ordered`
* the lesser element is the list element with the lowest `wake_tick` 

### 2. Algorithms

#### Overview
When a thread goes to sleep, we take it off the ready queue and place it on a sleeping queue ordered by wake time. Every timer interrupt (tick) we check if we need to wake any threads up and add them back on the ready queue.

#### Adding a thread to the sleep queue 
`sema_down` the sleep queue semaphore 
call the `list_insert_ordered` method to add thread to `sleep_queue`
`sema_up` the sleep queue semaphore 

#### Waking thread from sleep queue 
On every tick:
- check first element in sleep queue 
- if the `wake_tick <= current tick`, dequeue the first item 
- add dequeued item to ready queue 
- repeat? until first element is not ready 

### 3. Synchronization
The semaphore we add to the sleep queue should take care of the case in which we are interrupted when adding an item to the `sleep_queue`, which is likely to happen since it is a costly operation.
We will always access `ticks` using `timer_ticks()` function to make sure it is an atomic operation.

### 4. Rationale 
We decided to implement this by adding another queue similar to the linked list used for `ready_queue` because the infrastructure is already there for having queues of threads. The queue itself takes up static space because of the way lists are implemented in Pintos. 
One downside is that because of the linked list, adding a new thread to the sleep queue will take linear time relative to the number of sleeping threads, since we are maintaining it in sorted order. 
We decided on keeping them in sorted order because then on every tick you only need to do a constant time check, and making a thread sleep is a little more costly.
Another downside is that we had to add another variable to the thread struct in order to accomplish this, which is not as elegant as we'd like, but the alternative was making a completely different linked list implementation so we would keep track of the threads' `wake_tick` values.
We considered using a different method for operations on the sleep queue, such as a lock or just disabling interrupts while accessing the queue. We didn't want to disable interrupts because inserting into the list is linear time and we didn't want to disable the queue for that long.



## Task 2: Priority Scheduler

**Idea:** We will have a variable called `donated_priority` in threads, which we will take the max of donated and regular priority when using the priority scheduler.

### 1. Data structures and functions

#### Edited Methods:

##### `void sema_up (struct semaphore * sema)`
* loop through entire list and keep track of maximum priority
element in the list. Unblock that one and remove it from list

##### `void lock_acquire (struct lock * lock)`
* add a line of code that checks effective priority of lock holder and if lower priority than current thread effective priority set donated to our effective priority.
* no sema needed for `donated_priority`

##### `void next_thread_to_run (struct thread * t)`
* loop through all of ready_queue and grab highest priority one, based on our `thread_get_priority` method 

##### `void lock_release (struct lock * lock)`
* reset current thread's `donated_priority` to minimum priority
* check if our current effective priority is still highest in ready queue and yield if not

##### `thread_create`
* check variable `thread_mlfqs` and if it is false continue
* call `thread_set_priority(priority)`

##### `thread_get_priority`
* return effective priority: max of `priority` and `donated_priority` (a field we add to thread)

#### Edited Structs

##### `int donated_priority`
* add to thread struct 
* a priority donated by another thread waiting on this one
* initialized to 0 (minimum priority)

#### `struct lock *wanted_lock`
* add to thread struct
* the lock that this thread is blocked on. Useful for recrusive donation.


#### `struct list *held_locks`
* add to thread struct
* holds a list of all locks held by thread, useful for priority calculations on lock release


#### `struct list_elem elem`
* add to lock struct
* given to threads during lock_acquire so they have a list of locks they hold

### 2. Algorithms
To implement priority donation, we add another attribute to the thread struct.
Also we must check if priority donation is enabled (by checking the bool for mlfqs)
When asking for a lock, if we fail we check who is holding the lock and then set their donated_priority to the max of their donated priority and the thread asking for a lock's donated_priority or priority, whichever is a higher priority.

#### Choosing the next thread to run
In Pintos this is decided by the scheduler in the function `next_thread_to_run`. We edit this function so instead of choosing the first element in the ready queue we check all of them and keep track of which one has the lowest priority. We call our method `thread_get_priority` to get the effective priority, because this method takes priority donation into consideration.

#### Acquiring Locks 
When a thread attempts to acquire a lock, if it has to wait, it should look at the current lock holder. If the current lock holder has a lower priority, update lock holder's `donated_priority` to match the waiting thread's, and keep doing this up chain of locks while the wanted_lock pointer is not NULL. (recursive donation)
We also, on successful acquire, add the lock to our `held_locks` list (edge case on release)

#### Releasing a Lock
When a thread releases a lock, we take the lock off of `held_locks`. We then loop through the remaining `held_locks` and get the maximum priority element from their `wait_list` and set that to `donated_priority`. If no locks held, set to min.
Check if our current effective priority is still highest in ready queue and yield if not.

#### Computing the effective priority
This will be done by calling the function `thread_get_priority` which returns the max of the thread's `priority` and `donated_priority`. We should make sure to always use this method when checking priority.

#### Priority scheduling for semaphores and locks
Sema_up now looks at its entire queue and unblocks the highest priority element. 
Threads waiting on locks now check to see if they need to donate priority to the lock holder. If the holder's effective priority is lower than the waiter's, the waiter sets the holder's `donated_priority` to its effective priority.

#### Changing thread’s priority
This will be done by calling `thread_set_priority(priority)`. We should check the ready queue and immediately yield if there is a higher priority thread.

### 3. Synchronization
We will disable interrupts while reading and writing to `donated_priority`. This will protect against the possibility of two or more waiting threads reading from this value, getting the wrong priority, and setting it. 
Interrupts won't be a problem when iterating through the ready_queue because only the scheduler does that and interrupts are disabled then anyway.

### 4. Rationale 
Our logic behind adding a separate variable to keep track of donated priority is that we need to be able to restore a thread to its initial priority.
We disabled interrupts on `donated_priority` because it's a short operation so it won't affect timing too much, and we thought it would be too much trouble to add a lock variable that we constantly have to update.




## Task 3: Advanced Scheduler

### 1. Data structures and functions 
We use a linked list of linked lists. Top-level linked list's nodes are "thread queues". Each queue represents a priority, so there will be 64 total queues. Second-level linked-list contains the threads themselves (or rather `list_elem`'s of those threads). Only active threads are present.

#### Global Variables 
##### `fixed_point_t load_avg`
Moving average that is recalculated based on the formula every fourth clock tick

##### `struct list queue_of_queues[64]`
Linked list of linked lists, each corresponding to a priority.

#### Edited Structs 
``` c
struct thread { // PintOS Thread Struct
    // ...
    int niceness;
    fixed_point_t recent_cpu;
    // ...
  }
```
#### Edited Functions 
##### `void thread_set_nice (int new_nice)` 
- "If the running thread no longer has the highest priority, it should yield the CPU" 
- We recalculate the thread priority and check if there is a non-empty queue in the "queue of queues" with an attributed higher priority, in which case we call thread_yield() and the rest of the logic will be handled in schedule().

#### New Functions 
##### `void recalc_thread_priorities()`
- Will recalculate the priorities of the threads.
  
##### `uint8_t assign_thread_queue(struct thread *thread)`
- Will assign the thread to a new queue each if need be or remove the thread from the queue_of_queues if it's blocked. 

### 2. Algorithms
The main logic is handled in or revolves around schedule(). In `next_thread_to_run`, we simply iterate through queue_of_queues until first non-empty queue the top of which we pop and use as the next_thread_to_run().
In `thread_schedule_tail`, we call `recalc_thread_priorities` so we can decide where to place the previous thread. We find the appropriate `thread_queue` and append it to the end.
    
#### `void recalc_thread_priorities()`
  - If we are updating on a TIMER_FREQ step, we:
    - Calculate `load_avg`.
    - Loop through every thread and compute its `recent_cpu` . We disable interrupts for this step.
    - Finally, we calculate the priority (and format it to uint_8).
    - After the loop is over we re-enable interrupts.
    (Comment #1: Just a side comment. Not sure, but might be adding a deletion method here as well. As in, it will check if a thread is blocked and delete it from the queue_of_queues).
  - If we are updating on every fourth timer tick, we:
    - Only recalculate the priority of the previous thread. We disable interrupts for this step as well.
    - Then we insert it into the queue at the appropriate location.
    (Comment #2: That is due to the fact that 1s takes a lot longer to cycle than 4 timer ticks)

#### `uint8_t assign_thread_queue(struct thread *thread)`
  - We use the metadata in the `thread` struct to figure out where to place the thread in our queue_of_queues.
  - We use the built-in linked-list functions to place the thread accordingly.
  - We place new threads at the front of the queue, and blocked threads are removed from the list.

### 3. Synchronization
The only synchronization issue we may have occurs when computing each thread's priority value.
To solve that, we disable interrupts when we are handling that.

### 4. Rationale 
The explanation for using a non-modified linked list for queueing is based on the fact of an incredible simplicity of that data structure and the extensive availability of the built-in functions associated with it. 

## Additional Questions

### 1.
Create two locks, A and B.
When creating a thread t_i, give it a priority i and the job to print i when run.
Create t_0. As t_0, acquire a lock on A and sleep.
Create t_2. As t_2, request lock on A.
Create t_1. As t_1, acquire a lock on B, then request a lock on A.
As t_3, request a lock on B.
Wake t_0 and release its lock.

Expected behavior: print 1
Actual behavior: print 2

### 2. 
timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
0 |   0.0 |   0.0 |   0.0 |  63.0 | 61.0 |  59.0 | A
4 |   4.0 |   1.0 |   2.0 |  62.0 | 60.75 |  58.5 | A
8 |   8.0 |   1.0 |   2.0 |  61.0 | 60.75 |  58.5 | A
12 |  12.0 |   1.0 |   2.0 |  60.0 | 60.75 |  58.5 | B
16 |  12.0 |   5.0 |   2.0 |  60.0 | 59.75 |  58.5 | A
20 | 2.363 | 1.454 | 2.181 | 62.40 | 60.63 | 58.45 | A
24 | 6.363 | 1.454 | 2.181 | 61.40 | 60.63 | 58.45 | A
28 | 10.36 | 1.454 | 2.181 | 60.40 | 60.63 | 58.45 | B
32 | 10.36 | 5.454 | 2.181 | 60.40 | 59.63 | 58.45 | A
36 | 14.36 | 5.454 | 2.181 | 59.40 | 59.63 | 58.45 | B

### 3. 
It depends on exact implementation details like:
* starting load_avg (set to 0)
* ready_threads at any time-step (set to 3)
* TIMER_FREQ (how often to update load_avg & recent_cpu, currently 19)
* what order to calculate load_avg, recent_cpu, & priority (currently recent_cpu -> load -> priority)
