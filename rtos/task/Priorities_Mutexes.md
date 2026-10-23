# RTOS Study Notes: Priorities, Task States, Mutexes, and Priority Inversion

## 1. Task priorities

An RTOS scheduler decides which ready task should run. In the simple
fixed-priority, preemptive model used in these notes, the
highest-priority eligible Ready task runs.

For the examples below, assume a larger number means a higher priority:

-   Task H: priority 3 (high)
-   Task M: priority 2 (medium)
-   Task L: priority 1 (low)

Priority numbering varies between RTOSes; this convention is only for
the examples.

## 2. Task states

The basic task states discussed are:

-   **Running:** currently executing on the CPU.
-   **Ready:** able to run, waiting for CPU time.
-   **Blocked:** waiting for something, such as a semaphore token,
    mutex, delay, or event.

A blocked task is not simply spinning and repeatedly checking a
variable. The RTOS can stop scheduling it until the condition it waits
for is satisfied.

When a blocked task's wait condition is satisfied, it generally becomes
Ready. It does not necessarily run immediately; the scheduler chooses
the next task according to the RTOS scheduling rules.

## 3. Shared resources and race conditions

A shared resource is data or an object that multiple tasks can access.

If two tasks read and modify shared state without suitable coordination,
their operations can interleave and produce an incorrect result. This is
a **race condition**.

A **critical section** is a portion of code that must be protected from
unsafe concurrent access. The correct protection depends on what shares
the resource and which execution contexts can access it.

For task-to-task mutual exclusion, a mutex is a common choice. Disabling
interrupts or using a critical-section primitive may be needed in other
contexts, but these are not interchangeable with a mutex in every
situation.

## 4. Mutex basics

A mutex provides mutual exclusion: typically, only the task that owns
the mutex may unlock it.

Conceptually:

``` c
mutex_lock(&mutex);
update_shared_data();
mutex_unlock(&mutex);
```

If another task tries to lock a mutex that is already held, it may block
until the mutex becomes available.

A mutex differs from a semaphore in ownership semantics:

-   **Mutex:** has an owner; the owning task normally unlocks it.
-   **Semaphore:** represents available token(s) or a notification; it
    generally does not have mutex-style ownership.

Some RTOS mutexes support priority inheritance; this is RTOS/API
dependent. A binary semaphore is not automatically a mutex, even if both
can have a binary-like available/unavailable state.

## 5. Priority inversion

Priority inversion can occur when a low-priority task holds a mutex
needed by a high-priority task, while a medium-priority task prevents
the low-priority task from running.

Assume a single-core, fixed-priority, preemptive RTOS:

-   H has priority 3.
-   M has priority 2.
-   L has priority 1.

A typical sequence:

1.  L locks a mutex.
2.  H becomes Ready and tries to lock that mutex.
3.  Because L owns the mutex, H blocks.
4.  M becomes Ready and preempts L.
5.  L cannot run to finish its work and release the mutex, so H's wait
    is prolonged by M.

H is the highest-priority task, but it cannot proceed because it depends
on a resource held by L. The medium-priority task indirectly delays H.
This is priority inversion.

## 6. Priority inheritance

Priority inheritance is a technique some mutex implementations use to
reduce this form of priority inversion.

In the example, when H blocks on a mutex owned by L, L can temporarily
inherit H's priority:

-   L's **base (normal) priority** remains 1.
-   L's **effective priority** may temporarily become 3.
-   L can then run ahead of M, finish the protected work, and release
    the mutex.
-   H can acquire the mutex when it becomes available.
-   L's effective priority can return to its base priority when the
    inheritance is no longer needed.

The exact rules for restoring priorities can be more complex when tasks
hold nested mutexes or have multiple inheritance relationships. Those
details depend on the RTOS implementation.

Priority inheritance does not make the mutex available immediately. It
helps the owner run so it can release the mutex sooner.

## 7. Mutex vs binary semaphore

  -----------------------------------------------------------------------
  Property                Mutex                   Binary semaphore
  ----------------------- ----------------------- -----------------------
  Main purpose            Protect exclusive       Synchronization or
                          access to a resource    event notification

  Ownership               Usually owned by the    No mutex-style owner
                          task that locks it      

  Who releases it         Normally the owner      A task or supported
                                                  interrupt context can
                                                  give it

  Priority inheritance    Often supported by RTOS Not normally provided
                          mutexes; check the API  as a mutex ownership
                                                  feature

  Can a task wait?        Yes, until the mutex    Yes, until a token
                          becomes available       becomes available
  -----------------------------------------------------------------------

A binary semaphore can be initialized empty and used for notification:

``` c
semaphore_init(&sem, 0);
semaphore_take(&sem);  // Block if no token is available
do_the_work();
```

Another task can call `semaphore_give(&sem)` to make a token available.
In RTOSes that provide an ISR-safe give operation, an interrupt handler
may signal a task this way. The exact ISR-safe API and scheduling
behavior are RTOS-specific.

Do not use a binary semaphore as a direct substitute for a mutex when
exclusive ownership and priority inheritance are required.

## 8. Scheduler behavior: Ready is not the same as Running

A common source of confusion is the difference between waking a task and
running it:

1.  A task blocks while waiting for a mutex or semaphore.
2.  The resource becomes available or a token is given.
3.  The RTOS makes the waiting task Ready.
4.  The scheduler decides when it runs.

If a higher-priority task becomes Ready, a preemptive RTOS may switch to
it immediately, subject to the RTOS's rules and any scheduling
constraints. Do not assume every RTOS always switches at the exact same
point.

## 9. Practical rules of thumb

-   Use a mutex to protect shared data that requires exclusive access.
-   Keep critical sections short; do not hold a mutex longer than
    needed.
-   Use a semaphore to signal an event or represent available
    tokens/resources.
-   Check whether the RTOS mutex supports priority inheritance if
    priority inversion matters.
-   Remember that a blocked task does not continuously consume CPU by
    polling.
-   Be precise about task states: an unblocked task normally becomes
    Ready, not necessarily Running immediately.

## 10. Core takeaways

-   The scheduler chooses among eligible Ready tasks.
-   Race conditions arise when concurrent access to shared state is not
    coordinated correctly.
-   A mutex protects exclusive access and has ownership semantics.
-   Priority inversion can happen when a high-priority task waits for a
    mutex held by a low-priority task.
-   Priority inheritance can temporarily raise the mutex owner's
    effective priority so it can release the mutex sooner.
-   Binary semaphores and mutexes are different mechanisms, even when
    their states appear similar.
