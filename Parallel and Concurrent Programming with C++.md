# Conceptual basis
## 1. Parallel Computing
- Serial or sequential programming is when a program executes instructions one after another without overlap. The time it takes for a sequential program to run is limited by the processor speed and the sum of the time for each step.  
      
    
- Parallel computing allows multiple instructions to run simultaneously, increasing overall program throughput compared to sequential execution.  
      
    
- Parallel execution increases the overall throughput of a program, enabling us to break down large tasks or to accomplish more tasks in a given amount of time.  
      
    
- While parallel programming can speed up complex tasks and improve efficiency, it introduces challenges like communication overhead.  
      
    
- Synchronization and coordination between parallel tasks require careful handling to avoid issues such as waiting or conflicts.

## 2. Threads
## Concurrency
Concurrency is the ability of a program to be divided into multiple parts that can run independently and possibly out of order without changing the final result. It focuses on how a program is structured to handle multiple tasks that overlap in time.  
  
Concurrency time refers to the period during which these independent tasks overlap in execution. Even if tasks are not running exactly at the same moment (due to hardware limits like a single processor), they can still be considered concurrent if their execution times overlap by switching between tasks quickly.  

## Parallelism
Parallelism works by executing multiple tasks simultaneously using multiple processors or cores, which increases the overall throughput of a program and allows large tasks to be broken down and completed faster. True parallel execution requires parallel hardware, such as multi-core CPUs or GPUs.  
  
It's best to use parallelism for computationally intensive tasks that can be divided into independent parts, like large mathematical calculations or data processing, where running parts in parallel significantly speeds up completion.  
  
However, parallelism is less beneficial for I/O-bound tasks (like handling mouse, keyboard, or disk operations) because these operations happen infrequently relative to processor speed, so the overhead of managing parallel execution may outweigh any gains. In such cases, concurrency without parallelism is often sufficient to keep the program responsive.

## Threads states
Here is a summary of the main thread states in C++ concurrent programming:  
  
- **New:** The thread is created but has not started running yet.
- **Runnable:** The thread is ready to run and can be scheduled by the operating system to use the CPU.
- **Blocked:** The thread is waiting for an event (like I/O or a resource) and does not consume CPU while waiting.
- **Terminated:** The thread has finished executing or was aborted.
  
These states represent the typical lifecycle of a thread, helping manage how tasks run concurrently and efficiently in your programs.

# 02_14 Thread detached
A detached thread (or daemon thread) runs independently from the main thread and allows the program to exit even if the detached thread is still running. This is useful for background tasks like garbage collection that can be stopped abruptly without issues.  
  
However, detaching a thread is risky when the thread performs critical operations such as I/O (e.g., writing to files), because if the program exits while the thread is still running, it may cause data corruption or other side effects.  
  
In summary, use detached threads for non-critical background tasks that can safely terminate at any time, and avoid detaching threads that need to complete important work gracefully before the program exits.

# 03_03 Data race
- A data race happens when two or more threads access the same memory location at the same time, and at least one thread is writing to it. This causes unpredictable and incorrect results.  
      
    
- It occurs because threads run concurrently, and the operating system schedules their execution unpredictably. Without coordination, threads can interfere with each other's operations.  
      
    
- The responsibility to prevent data races is on the programmer, who must ensure safe access to shared data.  
      
    
- To avoid data races, synchronization techniques like mutexes or locks are used. These ensure that only one thread modifies the shared data at a time, preventing conflicts and keeping the program behavior consistent.

# 03_06 Mutual exclusion
 Mutual exclusion is a technique used to protect critical sections of code that access shared resources, ensuring that only one thread or process can execute that section at a time. 
 
Only one thread or process can have possession of a lock   at a time so it can be used to prevent multiple threads   from simultaneously accessing a shared resource.

The operation to acquire the lock is an atomic operation,   which means it's always executed   as a single indivisible action.   To the rest of the system,   an atomic operation appears to happen instantaneously,   even if under the hood, it really takes multiple steps.  
```cpp
#include <thread>
#include <mutex>
#include <chrono>

unsigned int garlic_count = 0;
std::mutex pencil;

void shopper() {
    pencil.lock();
    for (int i=0; i<5; i++) {
        printf("Shopper %d is thinking... \n",std::this_thread::get_id());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        garlic_count++;
    }
    pencil.unlock();
}

int main() {
    std::thread barron(shopper);
    std::thread olivia(shopper);
    barron.join();
    olivia.join();
    printf("We should buy %u garlic.\n", garlic_count);
}
```
## Atomic objects
Atomic objects in C++ are special types that encapsulate a value and provide synchronized access to it, ensuring that operations on the value are atomic—meaning they happen completely or not at all without interference from other threads. This prevents data races when multiple threads access and modify the same variable.  
  
You should use atomic objects when you need to share simple data, like counters or flags, between concurrent threads and want to avoid the overhead of locking mechanisms like mutexes. They are ideal for simple operations such as incrementing a variable safely in a multi-threaded environment.

# 04_03 A reentrant mutex
A reentrant (or recursive) mutex allows the same thread to lock it multiple times safely, which is useful in cases like nested function calls or recursive functions that lock the same mutex repeatedly. However, there are some important gotchas to be aware of:  
  

- You must unlock the mutex the exact same number of times it was locked; otherwise, the thread can become stuck waiting forever because the mutex remains locked.
- While reentrant mutexes prevent deadlocks caused by nested locking, overusing them can hide design problems. Some developers prefer refactoring code to avoid nested locks instead of relying on recursive mutexes.
- Improper use can still lead to deadlocks or resource contention if locks are not managed carefully.

  
Understanding these pitfalls helps you use reentrant mutexes effectively and safely in your concurrent C++ programs.

- example
```cpp
/**
 * Two shoppers adding garlic and potatoes to a shared notepad
 */
#include <thread>
#include <mutex>

unsigned int garlic_count = 0;
unsigned int potato_count = 0;
std::mutex pencil;

void add_garlic() {
    pencil.lock();
    garlic_count++;
    pencil.unlock();
}

void add_potato() {
    pencil.lock();
    potato_count++;
    pencil.unlock();
}
```

# 04_06 Try lock
Try lock is necessary for threads because it provides a non-blocking way to attempt to acquire a mutex (lock). Instead of waiting (blocking) if the mutex is already held by another thread, try lock immediately returns false, letting the thread know it couldn't get the lock. This allows the thread to continue doing other useful work instead of idling.  
  
In the video, this is illustrated with two threads sharing a pencil (mutex) to write on a shared notepad. If one thread tries to lock the pencil while the other has it, try lock lets it know right away so it can keep doing other tasks rather than waiting. This improves efficiency and responsiveness in concurrent programs by reducing unnecessary waiting and better utilizing CPU time.

- example
```cpp
unsigned int items_on_notepad = 0;
std::mutex pencil;

void shopper(const char* name) {
    int items_to_add = 0;
    while (items_on_notepad <= 20) {
        if (items_to_add && pencil.try_lock()) { // add item(s) to shared items_on_notepad
            items_on_notepad += items_to_add;
            printf("%s added %u item(s) to notepad.\n", name, items_to_add);
            items_to_add = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(300)); // time spent writing
            pencil.unlock();
        } else { // look for other things to buy
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // time spent searching
            items_to_add++;
            printf("%s found something else to buy.\n", name);
        }
    }
}

int main() {
    auto start_time = std::chrono::steady_clock::now();
    std::thread barron(shopper, "Barron");
    std::thread olivia(shopper, "Olivia");
    barron.join();
    olivia.join();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
    printf("Elapsed Time: %.2f seconds\n", elapsed_time/1000.0);
}
```

# 04_09 Shared mutex
A reader-writer lock (or shared mutex) is a synchronization mechanism that allows multiple threads to read a shared resource simultaneously (shared read mode) but restricts write access to only one thread at a time (exclusive write mode). This means many threads can safely read the data concurrently, but when a thread needs to modify the data, it must have exclusive access.  
  
You should use a reader-writer lock when your program has many more threads reading shared data than writing to it—this improves performance by allowing concurrent reads without blocking. 

The general rule of thumb is that if the majority of your threads are readers and writes are infrequent, a shared mutex can boost efficiency, like in database applications. However, if most threads perform writes, a standard mutex might be just as effective since read-write locks add complexity and overhead.

```cpp
#include <thread>
#include <mutex>
#include <chrono>
#include <shared_mutex>

char WEEKDAYS[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int today = 0;
std::shared_mutex marker;

void calendar_reader(const int id) {
    for (int i=0; i<7; i++) {
        marker.lock_shared();
        printf("Reader-%d sees today is %s\n", id, WEEKDAYS[today]);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        marker.unlock_shared();
    }
}

void calendar_writer(const int id) {
    for (int i=0; i<7; i++) {
        marker.lock();
        today = (today + 1) % 7;
        printf("Writer-%d updated date to %s\n", id, WEEKDAYS[today]);
```


# 05_03 Deadlock
A deadlock occurs when two or more threads are each waiting for a resource (like a mutex) that the other thread holds.
  
It happens, for example, when a thread tries to lock a mutex it already owns and ends up waiting forever because it can't unlock it while waiting. This creates a cycle where no thread can proceed.  

### The Dining Philosophers Problem

Barron and Olivia illustrate synchronization issues in concurrent programming using the Dining Philosophers problem:

- Two philosophers (representing threads) alternate between thinking and eating sushi from a shared plate
- Chopsticks are used as mutexes to protect the critical section (taking sushi from the plate)
- Each philosopher attempts to acquire two locks (chopsticks) before entering the critical section
- A deadlock occurs when both philosophers acquire one chopstick each and wait indefinitely for the other
- This scenario demonstrates how competing for multiple locks can lead to a lack of progress in concurrent programs

### Deadlock and Liveness

- Deadlock is a situation where each member of a group is waiting for another member to take action, resulting in no progress
- Liveness is the property of a program that ensures concurrent programs make progress, even if threads must take turns in critical sections

### Solution to Deadlock

Barron and Olivia demonstrate one possible solution to prevent deadlock with the Dining Philosophers problem:

- The philosophers implement a lock prioritization strategy to avoid deadlock
- By agreeing to acquire the same chopstick first, they prevent the circular wait condition that causes deadlock
- This solution ensures that one philosopher can always complete the critical section, allowing the other to proceed afterward

- deadlock
```cpp
/**
 * Two philosophers, thinking and eating sushi
 */
#include <thread>
#include <mutex>

int sushi_count = 5000;

void philosopher(std::mutex &first_chopstick, std::mutex &second_chopstick) {
    while (sushi_count > 0) {
        first_chopstick.lock();
        second_chopstick.lock();
        if (sushi_count) {
            sushi_count--;
        }
        second_chopstick.unlock();
        first_chopstick.unlock();
    }
}

int main() {
    std::mutex chopstick_a, chopstick_b;
    std::thread barron(philosopher, std::ref(chopstick_a), std::ref(chopstick_b));
    std::thread olivia(philosopher, std::ref(chopstick_b), std::ref(chopstick_a));
    barron.join();
    olivia.join();
    printf("The philosophers are done eating.\n");
}
```

- fixed
```cpp
/**
 * Two philosophers, thinking and eating sushi
 */
#include <thread>
#include <mutex>

int sushi_count = 5000;

void philosopher(std::mutex &first_chopstick, std::mutex &second_chopstick) {
    while (sushi_count > 0) {
        std::scoped_lock lock(first_chopstick, second_chopstick);
        if (sushi_count) {
            sushi_count--;
        }
    }
}

int main() {
    std::mutex chopstick_a, chopstick_b;
    std::thread barron(philosopher, std::ref(chopstick_a), std::ref(chopstick_b));
    std::thread olivia(philosopher, std::ref(chopstick_b), std::ref(chopstick_a));
    barron.join();
    olivia.join();
    printf("The philosophers are done eating.\n");
}
```

### Liveness
Liveness is a property of concurrent programs that ensures all threads eventually make progress and don't get stuck waiting indefinitely. Avoiding deadlock is key to maintaining liveness. Strategies like acquiring locks in a consistent order help prevent deadlocks and keep the program running smoothly.  
  
To avoid deadlocks, you can:  
  
- Use reentrant (recursive) mutexes that allow the same thread to lock the mutex multiple times safely.
- Design your code to prevent circular waiting by acquiring locks in a consistent order.
- Keep critical sections short and avoid nested locks when possible.

Managing locks carefully is essential to prevent deadlocks and ensure your concurrent C++ programs run smoothly.
# 05_06 Abandoned Lock: A New Form of Deadlock

- An abandoned lock occurs when a thread acquires a lock on a shared resource and exits before releasing its lock on that resource, leaving other threads trying to acquire the lock to wait indefinitely
- When a thread or process acquires a lock and then terminates unexpectedly, it may not release the lock automatically
- Barron and Olivia demonstrate an abandoned lock as two dining philosophers sharing a chopstick when one philosopher (Barron) abruptly leaves with one chopstick, resulting in a deadlock scenario; other tasks (represented by Olivia) become stuck, waiting indefinitely for a lock that will never be released
- Deadlocks can occur not only through resource competition but also through unexpected thread termination

- abandoned lock
```cpp
/**
 * Two philosophers, thinking and eating sushi
 */
#include <thread>
#include <mutex>

int sushi_count = 5000;

void philosopher(std::mutex &chopsticks) {
    while (sushi_count > 0) {
        chopsticks.lock();
        if (sushi_count) {
            sushi_count--;
        }
        chopsticks.unlock();
    }
}

int main() {
    std::mutex chopsticks;
    std::thread barron(philosopher, std::ref(chopsticks));
    std::thread olivia(philosopher, std::ref(chopsticks));
    barron.join();
    olivia.join();
    printf("The philosophers are done eating.\n");
}
```

- fixed
```cpp
/**
 * Two philosophers, thinking and eating sushi
 */
#include <thread>
#include <mutex>

int sushi_count = 5000;

void philosopher(std::mutex &chopsticks) {
    while (sushi_count > 0) {
        std::scoped_lock lock(chopsticks);
        if (sushi_count) {
            sushi_count--;
        }
        if (sushi_count == 10) {
            printf("This philosopher has had enough!\n");
            break;
        }
    }
}

int main() {
    std::mutex chopsticks;
    std::thread barron(philosopher, std::ref(chopsticks));
    std::thread olivia(philosopher, std::ref(chopsticks));
    barron.join();
    olivia.join();
    printf("The philosophers are done eating.\n");
}
```

# 05_09 Starvation

- Starvation occurs when a process or thread is perpetually denied access to the resources it needs, preventing it from progressing
- Ideally, threads will take turns accessing shared resources, but this is not guaranteed due to how operating systems schedule thread execution
- A "greedy" thread frequently holding a lock on a shared resource can lead to other threads being starved out of being able to make progress
- In simple scenarios with a few equally prioritized threads, starvation is less likely to be a concern; however, thread priorities can significantly impact the likelihood of starvation: higher-priority threads are generally scheduled to execute more often, and lower-priority threads may struggle to gain access to resources

Veja pastas begin

## Implications

- Starvation can significantly impact system performance and fairness in resource allocation
- Designers of concurrent systems need to consider thread priorities and the number of concurrent threads to prevent starvation
- While occasional delays in resource access may be tolerable, persistent starvation can severely hamper the functionality of affected threads

# 05_09 Livelock

- Livelock is a situation in concurrent computing where two or more threads block each other from making progress
- Barron and Olivia illustrate a livelock scenario as two overly polite threads that continue to offer the last piece of sushi to the other thread out of politeness, creating a situation where neither can progress 
- Unlike deadlock, threads in livelock are actively trying to resolve the problem but fail to make progress

## Characteristics of Livelock

- Threads respond to each other's actions, creating a cycle of mutual interference
- All threads are busy, but their combined efforts prevent any actual accomplishment
- The program never reaches its end state or terminates

## Causes and Prevention

- Livelocks often result from algorithms designed to detect and recover from deadlocks
- They occur when multiple processes simultaneously attempt to resolve a deadlock
- To prevent livelock, ensure only one process takes action to resolve conflicts
- Use mechanisms like priority systems or random selection to choose which process acts

# 06_03 Limitations of Locks and Mutexes

- A lock or mutex restricts multiple threads from accessing a shared resource simultaneously; however, it doesn't provide a way for threads to signal each other or synchronize actions efficiently

## Condition Variables

- Condition variables serve as a queue for threads waiting for a specific condition to occur
- They work in conjunction with mutexes to implement a higher-level construct called a monitor

## Monitors

- Monitors protect critical sections of code with mutual exclusion and provide mechanisms for threads to wait and be notified
- Barron and Olivia describe the concept of monitors using a room analogy: the monitor is like a room containing protected procedures and shared data, the mutex acts as a lock on the door, and condition variables are like waiting rooms outside the monitor

## Condition Variable Operations

There are three main operations associated with condition variables:

- Wait: Releases the mutex lock and puts the thread to sleep in a queue
- Signal (or notify/wake): Wakes up a single thread from the waiting queue
- Broadcast (or notifyAll/wakeAll): Wakes up all threads in the waiting queue

## Practical Application

One common use case for condition variables is implementing a shared queue or buffer, which typically involves:

- A mutex to ensure exclusive access to the queue
- Two condition variables: one for signaling when the buffer is not full (for adding items) and another for signaling when the buffer is not empty (for removing items)

## Summary

- Condition variables enable efficient thread synchronization by allowing threads to wait for specific conditions and signal each other when those conditions are met
- This mechanism improves upon simple mutex-based solutions by reducing “busy-waiting” and enabling more sophisticated coordination between threads

# 06_06 Producer-Consumer Pattern

The producer-consumer pattern is a common design in concurrent programming. It involves producers adding elements to a shared data structure and consumers removing and processing them.

### Queue structure

- The shared data structure is typically a queue
- Queues operate on a first-in, first-out (FIFO) principle

### Synchronization challenges

- Mutual exclusion is needed to ensure only one thread accesses the queue at a time
- Producers must not add data to a full queue
- Consumers must not remove data from an empty queue
- Some languages offer thread-safe queue implementations

### Buffer management

- Buffer overflow can occur if consumers can't keep up with producers
- Unbounded queues exist but are still limited by physical memory

### Processing rates

- The average rate of production should be less than the average rate of consumption
- Data may arrive in bursts, requiring the consumer to catch up between bursts

## Pipeline Architecture

- A pipeline is a chain of processing elements where each element's output is the input to the next element, creating a sequence of producer-consumer pairs connected by buffers
- An advantage of a pipeline is that it allows for parallel processing of multiple items at different stages
- An important consideration is that each element in the pipeline must process data faster than upstream elements produce it
- Barron and Olivia demonstrate the producer-consumer concept using a soup-serving analogy, showing how multiple consumers can help balance the workload

# 06_09 Semaphores

- Semaphores are a synchronization mechanism used to control access to shared resources
- Unlike locks or mutexes, semaphores can allow multiple threads to access a resource simultaneously
- Semaphores include a counter to track how many times they've been acquired or released

## Semaphore Functionality

- Threads can acquire a semaphore when its count is positive, decrementing the counter
- If the counter reaches zero, threads attempting to acquire the semaphore are blocked and queued
- Threads release the semaphore when done, incrementing the counter and potentially signaling waiting threads

## Types of Semaphores

### Counting semaphore

- Counting semaphores can have values of 0, 1, 2, 3, and so on, representing the number of available resources
- They can be used to manage access to a limited pool of resources, such as database connections or items in a queue

Barron, Olivia, and Steve demonstrate a counting semaphore using a two-port phone charger:

- The number of available ports represents a semaphore with an initial value of two
- As devices are plugged in, the semaphore value is decremented; when unplugged, the semaphore count is incremented
- When all ports are in use (for example, the semaphore value is 0), additional threads must wait to acquire the semaphore

### Binary semaphore

- Binary semaphores are restricted to one of two values: 0 (locked) or 1 (unlocked)
- They are similar to mutexes, but with a key difference that any thread can release a semaphore, not just the one that acquired it

## Key Advantages of Semaphores

- Flexibility in allowing multiple threads to access resources simultaneously
- Ability to function as a signaling mechanism between threads
- Versatility in managing various types of resource pools and synchronization scenarios
# 07_03 Data Races vs. Race Conditions

### Data races

- Data races occur when two or more threads concurrently access the same memory location, with at least one thread writing to or changing that memory value
- This can lead to threads overwriting each other or reading incorrect values
- Data races can be detected using automated tools and prevented by ensuring mutual exclusion for shared resources

### Race conditions

- Race conditions are flaws in the timing or order of a program's execution that cause incorrect behavior
- They are often more difficult to detect and prevent than data races

### Important distinctions

- Data races and race conditions are different problems that are often confused due to their similar names
- It's possible to have data races without race conditions and vice versa
- Many race conditions are caused by data races, and many data races lead to race conditions, but they are not dependent on each other
- Barron and Olivia demonstrate these concepts with an analogy of creating a shopping list for a party: A pencil serves as a mutex to protect the shared resource (shopping list) from data races; however, even with mutex protection, a race condition can still occur due to the nondeterministic order of thread execution

### Challenges in detecting race conditions

- Race conditions can be difficult to discover during testing, as they may only manifest under specific timing conditions
- They are often classified as "heisenbugs": bugs that seem to disappear or change behavior when studied

### Potential detection methods

- Inserting sleep statements at different points in the code can sometimes help uncover race conditions by altering thread execution order
- However, attempts to study race conditions may inadvertently prevent them from occurring, making them challenging to reproduce and debug

# 07_06 Preventing Race Conditions with Barriers

### Barriers

- A barrier is a synchronization mechanism for thread coordination
- It serves as a stopping point for a group of threads, preventing them from proceeding until all (or a sufficient number of) threads have reached the barrier

Barron relates the concept of a barrier to a sports team huddle:

- Players finish individual activities before joining the huddle
- Team members wait in the huddle until everyone arrives
- Once all are present, they break the huddle and resume their activities

### Applying barriers to solve race conditions

Barron and Olivia demonstrate how barriers can be used to solve a race condition using their shopping list scenario:

- Olivia adds three bags of chips to the list before reaching the barrier
- Barron waits at the barrier until Olivia completes her task
- After both reach the barrier, they "break" (analogous to leaving the huddle)
- Barron then doubles the number of chips on the list

There are two possible execution scenarios:

1. Olivia executes first, adds her chips, and then both meet at the barrier so Barron can continue to double the chips
2. Barron executes first but waits at the barrier for Olivia to complete her task before continuing to double the chips

In both cases, the final result is correct: eight bags of chips on the shopping list.

### Benefits of using barriers

Barriers ensure the correct order of operations regardless of thread scheduling:

- The order in which threads are scheduled to execute becomes less critical
- Synchronization is guaranteed by the barrier mechanism
- Consistent results are achieved even with varying execution orders

Barriers provide a reliable method for synchronizing threads in concurrent programming, ensuring correct execution order and preventing race conditions.

# 07_09 Latch
A latch is a synchronization tool used in concurrent programming to coordinate multiple threads. It starts with a count value, and threads can either wait until this count reaches zero or decrease the count by calling a countdown function. Unlike a barrier, which releases threads when a certain number of them have arrived, a latch releases threads only after the countdown has been called enough times to bring the count to zero. This helps ensure that some threads wait for others to complete specific tasks before proceeding, improving control over execution order in your C++ programs.

# 08 Asynchronous tasks
## Computational Graphs

- Computational graphs model relationships between program steps
- Graphs visualize which steps can be executed in parallel to help coordinate parallel execution and identify dependencies

### Directed acyclic graphs (DAGs)

- Nodes represent tasks or units of work
- Directed edges indicate progression and dependencies between tasks

Barron illustrates the concept of a computational graph using a salad-making process, which involves chopping lettuce, chopping tomatoes, mixing the ingredients, and adding salad dressing. 

- Chopping lettuce and tomatoes can occur asynchronously
- "Spawn" node represents the start of parallel execution
- "Sync" node ensures both chopping tasks complete before mixing

### Key Terminology

- Spawn/fork: Initiates parallel execution paths
- Sync/join: Synchronizes completion of parallel tasks
- Asynchronous: Tasks that can occur in any order relative to each other
- Critical path: Longest sequence of dependent operations in the graph

### Analyzing Parallel Potential

- Work: Total execution time on a single processor (sum of all task times)
- Span: Shortest possible execution time with maximum parallelization (sum of critical path times)
- Ideal parallelism: Ratio of work to span, indicating maximum speed improvement

# 08_05 Thread Pools
## Challenges with Creating Many Threads

Barron and Olivia introduce the concept of running asynchronous tasks in parallel using a cooking analogy where they each represent independent threads (or processes) chopping different vegetables representing parallel tasks.

- As more tasks (vegetables) are added, they can spawn a new thread for each one
- Creating a new thread for each task can lead to inefficiencies because thread creation incurs overhead in terms of processor time and memory usage

## Thread Pools

- Thread pools can be a more efficient alternative to creating individual threads for each task
- Thread pools maintain a small collection of worker threads that can be reused to execute multiple tasks
- Submitting tasks to a thread pool is analogous to adding items to a to-do list for worker threads

### Benefits of thread pools

- Reusing threads from a pool overcomes the overhead of creating new threads for each task
- Thread pools are especially advantageous when the task execution time is less than thread creation time
- Using preexisting threads in a pool can make programs more responsive by eliminating the delay of thread creation

# 08_08 Futures

- Asynchronous tasks allow multiple operations to be accomplished simultaneously
- Futures are a mechanism for handling the results of asynchronous operations
- It acts as a placeholder for a result that will be available at a later time
- A future is like an "I owe you" note for the result of an asynchronous task

Barron and Olivia demonstrate a future in action using an analogy in the kitchen:

- Barron asks Olivia to count vegetables in the pantry, representing an asynchronous task
- Olivia provides an "I owe you" note (future) before leaving to complete the task
- Barron continues with other work while holding onto the future
- Olivia eventually returns with the result (zero vegetables), fulfilling the promise
- The resolved future allows Barron to make a decision (to go to the store)

### Working with Futures

- Futures are read-only and may not have an immediate result
- A thread might need to wait for the future to be resolved
- "Resolving" or "fulfilling" a future means writing the result value to it
# 08_11 Divide-and-Conquer Algorithms

- Divide-and-conquer algorithms break down complex problems into smaller, manageable subproblems
- They are well suited for parallel execution across multiple processors

A divide-and-conquer algorithm is structured in three parts:

- Divide: The main problem is split into smaller subproblems of roughly equal size
- Conquer: Each subproblem is solved recursively
- Combine: Solutions to subproblems are merged to form the final solution

Barron and Olivia demonstrate the divide-and-conquer approach using an array of shopping receipts to calculate total spending.

- Sequential approach: Iterating through receipts and accumulating values
- Parallel approach: Dividing receipts among multiple processors for simultaneous calculation, then combining results

### Recursive Division Process

- Problems can be continuously subdivided until reaching a defined base case
- The base case may be individual elements or a threshold amount, depending on the algorithm
- After reaching the base case, subgroup results are combined as the recursion unwinds

In code, this is often implemented using an if-else structure in code.

- Base case: When the problem is small enough to solve directly
- Recursive case: Divides the problem into "left" and "right" subproblems, solves them recursively, then combines the results

### Advantages

- Subproblems are independent, allowing for parallel execution on different processors
- Can significantly reduce computation time for large datasets

### Considerations

- Not all divide-and-conquer algorithms benefit from parallelization
- The overhead of parallelization must be weighed against potential performance gains
- Factors to consider include problem size and operational complexity

# 9 Evaluating Parallel Performance
## Weak Scaling vs. Strong Scaling

### Weak scaling: Increase the problem size

- Weak scaling allows us to tackle larger problems in the same amount of time by adding more processors while keeping the workload per processor constant
- For example, one person can decorate 10 cupcakes in an hour, while two people working in parallel can decorate 20 cupcakes in the same time

### Strong scaling: Accomplish tasks faster

- Strong scaling involves breaking down a fixed-size problem across multiple processors to execute it faster
- For example, decorating 10 cupcakes takes one person an hour, but two people working together can complete the task in about 30 minutes

## Key Metrics in Parallel Computing

### Throughput

- Throughput measures the number of tasks completed in a given time period
- Adding more processors increases overall throughput, for example, from 10 cupcakes/hour with one worker to 30 cupcakes/hour with three workers

### Latency

- Latency is the time taken to execute a single task from start to finish
- It remains constant per task even when multiple processors are used (for example, six minutes to decorate one cupcake)

### Speedup

- Speedup is calculated as the ratio of sequential execution time to parallel execution time
- For example, if one worker takes 60 minutes and two workers take 30 minutes, the speedup is 2

## Limitations of Parallelization

- Real-world programs often have both parallelizable and sequential components
- Sequential parts of a program (for example, packing cupcakes into a shared container) limit the maximum achievable speedup, which leads to diminishing returns as more processors are added

## Amdahl's Law

- Amdahl's Law is an equation used to estimate the potential speedup of a parallel program
- It helps determine the effectiveness of parallelizing a program based on its parallelizable portion

### The Equation

$$\text{Overall Speedup} = \frac{1}{(1-P) + \frac{P}{S}}$$

- 'P' represents the portion of the program that can be parallelized
- 'S' represents the speedup for the parallelized part running on multiple processors

Barron uses a program with 95% parallelizable code as an example:

- With two processors, the overall speedup is about 1.9
- Increasing to three processors yields a speedup of 2.7, and four processors result in 3.5
- However, even with 1000 processors, the maximum speedup is only around 19.6
- Increasing to a million processors barely improves the speedup to just under 20
- The non-parallelizable 5% of the program creates an upper limit on achievable speedup

### Limitations on the Effectiveness of Parallelization

- The degree of parallelization significantly impacts potential speedup 
- Programs with 90% parallelizable code have a maximum speedup of 10
- 75% parallelizable programs are limited to a speedup of 4
- 50% parallelizable programs can achieve a maximum speedup of only 2

### Decision-Making in Parallel Programming

- Amdahl's Law helps determine whether parallelizing a program is worthwhile
- It illustrates why parallel computing is most beneficial for highly parallelizable programs
- The costs and overhead of parallelization should be weighed against potential benefits
- Parallelizing everything is not always the best approach, despite the availability of multicore processors

## 09_07 Measuring Speedup and Efficiency

- Amdahl's Law estimates potential speedup from parallelization
- Empirical measurement provides actual performance data

### Calculating Speedup

Speedup = Time for sequential execution / Time for parallel execution

- Speedup measures the benefit of parallel problem-solving
- Any speedup greater than 1 indicates improvement from parallelization
- Speedup less than 1 suggests that a sequential algorithm is preferable

Barron and Olivia demonstrate this by measuring how long it takes them to add up shopping receipts

- Sequential task: Adding receipts alone took 25 seconds
- Parallel task: Adding receipts together took 17 seconds
- Calculated speedup: 25/17 = 1.47 (almost 1.5 times faster)

### Calculating Efficiency

Efficiency = Speedup / Number of processors

- Efficiency describes how well processors are utilized
- For example, with two processors: if the speedup is 1.47, then the efficiency is 73.5%
- With eight processors: if the speedup is 2.2, then the efficiency is 27.5%

### Benchmarking Best Practices

- Limit other running programs to avoid resource competition
- Average multiple independent runs to account for execution variability
- Allow for "warm-up" in environments with just-in-time compilation
- Run the algorithm once before measurement to ensure a consistent cache state

# 10 Designing Parallel Programs
## Partitioning: Breaking Down the Problem

- Partitioning involves dividing the problem into discrete chunks of work for distribution among multiple tasks
- At this stage of the design process, the focus is on maximum decomposition without considering practical limitations like processor count
- Different decomposition methods may have varying advantages depending on the problem and hardware
- Domain decomposition and functional decomposition offer valuable perspectives on problem-solving in parallel computing

### Domain (data) decomposition

- Focuses on dividing the data associated with the problem into small, ideally equal-sized partitions
- Computations are then associated with the partitioned data
- Example from the video: Decorating a tray of cupcakes can be divided into blocks or cyclically

### Functional decomposition

- Begins by considering all computational work required by the program
- Divides the overall work into separate tasks performing different portions
- Data requirements for these tasks are a secondary consideration
- Example from the video: Breaking down the process of making cupcakes into individual tasks

### Complementary approaches

- Domain and functional decomposition are complementary and often used in combination
- Developers typically start with domain decomposition as it forms the foundation for many parallel algorithms
- Exploring both approaches can reveal optimization opportunities and potential issues that might be missed by considering data alone

## Communication between Parallel Tasks

- After decomposing a problem into separate tasks, the next step is to establish communication between tasks
- Communication involves coordinating execution and sharing data

## Types of Task Communication

### 1. Independent tasks

- Some problems can be decomposed into tasks that don't require data sharing
- Example from the video: Frosting cupcakes independently, with no need for communication between tasks

### 2. Interdependent tasks

- Tasks that require information from other tasks to complete their work
- Example from the video: Decorating cupcakes in a rainbow pattern, where each task needs to know the colors of neighboring cupcakes

## Communication Structures

### 1. Point-to-point communication

- Direct links between neighboring tasks
- Suitable when each task communicates with a small number of other tasks
- Involves sender (producer) and receiver (consumer) roles

### 2. Collective communication

- Used when tasks need to communicate with a larger group
- Includes broadcasting (one-to-many) and scattering/gathering (distributing and collecting data)

### 3. Centralized management

- One task acts as a central coordinator for a group of distributed workers
- Can become a bottleneck as the number of workers increases
- Divide-and-conquer strategies can help distribute computation and communication load

## Communication Factors to Consider

### 1. Synchronous vs. asynchronous communication

- Synchronous (blocking): Tasks wait for communication to complete before continuing
- Asynchronous (non-blocking): Tasks can perform other work while communication is in progress

### 2. Performance considerations

- Processing overhead: Time spent on communication versus data processing
- Latency: Time for a message to travel from sender to receiver
- Bandwidth: Amount of data that can be communicated per unit of time

## Context-Specific Considerations

- For multithreaded programs on a single system, factors like latency and bandwidth may be less critical
- In distributed systems across multiple physical machines, intersystem communication factors significantly impact overall performance

## Agglomeration

- The agglomeration stage of the parallel design process follows the initial partitioning of a problem into separate tasks and establishing communication between them

## Granularity

### Fine-grained parallelism

- Breaks a program into a large number of small tasks
- Advantages: Allows for better load balancing across processors
- Disadvantages: Increases overhead for communication and synchronization, resulting in a low computation-to-communication ratio

### Coarse-grained parallelism

- Splits the program into a small number of large tasks
- Advantages: Has lower communication overhead, allowing more time for computation
- Disadvantages: May produce load imbalance, with some tasks processing more data while others remain idle

### Medium-grained parallelism

- Balances the trade-offs between fine and coarse-grained approaches
- Often the most efficient solution for general purpose computers

## Demonstration

Barron and Olivia illustrate the concept of agglomeration by frosting cupcakes:

- 12 cupcakes were divided into 12 separate tasks
- This approach initially required 34 communication events between tasks
- Tasks were combined through agglomeration to match the number of available processors (two)
- The program was restructured into two tasks, each responsible for six cupcakes
- This reduced communication events between tasks from 34 to two; however, each communication now involves sharing more information

## Recommendations

- Avoid hard-coded limits on the number of tasks
- Use compile-time or runtime parameters to control granularity
- Design programs to adapt to changes in the number of available processors

## Mapping in Parallel Design

- Mapping is the fourth and final stage of our parallel design process
- It involves specifying where each established task will be executed

### Applicability

- Mapping is not applicable for single-processor systems or systems with automated task scheduling
- It becomes relevant in distributed systems or specialized hardware with multiple parallel processors

### Goals and strategies

The main goal of mapping is to minimize total execution time. Two key strategies are:

1. Increasing concurrency: Place tasks capable of executing concurrently on different processors.
2. Improving locality: Place frequently communicating tasks on the same processor to keep them close together.

### Challenges and considerations

- These strategies often conflict, requiring design trade-offs
- Dynamic workloads may necessitate periodic remapping using dynamic load-balancing techniques
- Mapping algorithm design depends heavily on program structure and hardware specifications
- Various load-balancing algorithms utilize domain decomposition and agglomeration techniques