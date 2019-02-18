Final Report for Project 1: Threads
===================================

#### Changes since Design Doc
* added held_locks to thread struct and adjusted lock release to correctly evaluate priority
* added wanted_lock to thread struct to allow for recursive priority donation, in acquire lock
* Changes mlfqs from design doc implementation of 64 separate queues to just getting max from ready queue
* Added some methods for quality of life improvements, like checking if yield was necessary and doing so as a thread func
* Took out sema for Part 1 and implemented timer_interrupt by disabling interrupts instead of checking sema and skipping

#### Reflection
In general, our group had a fairly good pace with the project until the end. We had some struggle meeting up, and the design doc was hurt a bit because of that, especially since part 3 was finished close to deadline, which was fine but worried many members. Implementation was done progressively and not in a rush: part 1 was finished very early and passed tests, part 2 was finished around a week before deadline, but part 3 was not implemented or worked on until right before deadline. One member (Will) did a final push for part 3 as there were computer issues and a lack of communication, and the code quality took a small hit because of the rush.

##### Group Members
* William was an all-rounder, who helped with many aspects: He worked on part 1 of design doc and implementation, and helped with design doc for part 2 and helped debug part 2. He also implemented part 3 and debugged it when Taubstummen had computer problems.
* Heesoo helped with design doc extra questions, helped debug part 1, and implemented part 2 with Link.
* Link did a large portion of the design doc's part 1 and 2, as well as implementing part 2 with Heesoo.
* Taubstummen did all of part 3 of design doc.

