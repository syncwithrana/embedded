# RTOS Study Notes: Semaphores

## 1. What is a semaphore?

A semaphore is a synchronization object managed by an RTOS. A task can
**take** a token from it, and another task or an interrupt handler may
**give** a token to it.

The RTOS manages tasks that must wait because no token is available.
Instead of repeatedly checking a variable, a task can block inside the
semaphore-take operation and become ready again when a token becomes
available.

The exact API and some details depend on the RTOS.

## 2. The two main types

### Binary semaphore

A binary semaphore can hold at most one pending token. Its count is
effectively `0` or `1`.

-   `0`: no token is available.
-   `1`: a token is available.

A binary semaphore is often used to notify a task that an event has
occurred.

### Counting semaphore

A counting semaphore can hold multiple tokens, up to a configured
maximum.

For example, a counting semaphore with maximum count `3` can hold counts
`0`, `1`, `2`, or `3`.

A counting semaphore is often used to track a pool of available
resources. It can also count pending notifications, as long as each
event is represented by a `give` and the count does not exceed its
limit.

## 3. `take()` and `give()`

### `take()`

-   If the count is greater than zero, a task can take a token and the
    count decreases.
-   If the count is zero, the task can block until a token becomes
    available (depending on the API and timeout used).

### `give()`

-   Makes a token available, increasing the count when permitted.
-   If a task is blocked waiting for a token, the RTOS can make it
    Ready.
-   Becoming Ready does not necessarily mean the task runs immediately;
    the scheduler decides when it runs.

**Important:** Standard RTOS semaphore `give()` operations generally do
not block waiting for the count to fall below the maximum. If the
semaphore is already full, the operation may fail or leave the count
unchanged, depending on the RTOS API.

## 4. A binary semaphore initialized to zero

Consider:

``` c
semaphore_init(&sem, 0);
```

The semaphore starts empty.

Task A:

``` c
void task_A(void)
{
    semaphore_take(&sem);  // Blocks if no token is available
    do_the_work();         // Continues after it obtains a token
}
```

Task B:

``` c
void task_B(void)
{
    wait_for_event();
    semaphore_give(&sem); // Signal that the event occurred
}
```

The sequence is:

1.  Task A calls `semaphore_take()` while the count is `0`.
2.  The RTOS blocks Task A inside the take operation.
3.  Task B eventually detects the event and calls `semaphore_give()`.
4.  Task A becomes Ready and resumes when scheduled.

If `give()` happens before Task A calls `take()`, a binary semaphore can
retain one pending token. Task A can then take that token immediately.
Multiple gives before the token is consumed may coalesce into one
pending token; exact behavior depends on the RTOS API.

These function names are illustrative pseudocode, not standard C library
functions.

## 5. Counting semaphore initialized to zero

A counting semaphore can also start at zero:

``` c
semaphore_init(&sem, 0);
```

A task calling `take()` blocks while the count is zero. Each permitted
`give()` adds a token. Unlike a binary semaphore, a counting semaphore
can retain multiple pending tokens.

For example, if three gives happen before any take, a counting semaphore
may retain a count of `3`, provided its maximum is at least `3`.

This is why a counting semaphore can be useful when several events may
occur while a task is busy. It counts the pending tokens; it does not
identify which task or event produced them.

## 6. Resource counting: the parking-lot example

Suppose a parking lot has three spaces. Initialize the semaphore to `3`,
where each token represents one **available space**.

``` c
semaphore_init(&parking, 3);
```

  Event                                     Available spaces / count
  --------------------------------------- --------------------------
  Initially, all spaces are free                                   3
  Car A parks and takes a token                                    2
  Car B parks and takes a token                                    1
  Car C parks and takes a token                                    0
  Another car tries to park and takes                         Blocks
  A parked car leaves and gives a token                            1

The waiting car can proceed after a space is released and a token
becomes available.

The semaphore does not know that a car has physically parked or left.
The program must call `take()` or `give()` at the appropriate time.

### Why `give()` is not the operation to wait for a space

If the count represents available spaces, a car entering takes a token.
A car leaving returns a token with `give()`.

If the count is already `3` (all spaces are free), another `give()` does
not mean "wait until a car parks." A standard semaphore's give operation
generally does not block at its maximum; it may fail or leave the count
unchanged. To wait at both ends of a bounded counter, use a different
synchronization design.

## 7. Binary vs counting semaphore

  -----------------------------------------------------------------------
  Property                Binary semaphore        Counting semaphore
  ----------------------- ----------------------- -----------------------
  Maximum pending tokens  1                       Configured maximum

  Can start at 0?         Yes                     Yes

  Can `take()` block at   Yes                     Yes
  0?                                              

  Can remember multiple   At most one pending     Yes, up to the maximum
  pending gives?          token                   

  Common uses             Event notification,     Resource pools,
                          task synchronization    counting pending events
  -----------------------------------------------------------------------

Both types can be used for synchronization. A semaphore does not have to
represent a physical resource.

## 8. FreeRTOS example

In FreeRTOS, binary and counting semaphores have different creation
APIs, while both use the same take/give functions.

``` c
#include "FreeRTOS.h"
#include "semphr.h"

SemaphoreHandle_t binary;
SemaphoreHandle_t counting;

void setup_semaphores(void)
{
    binary = xSemaphoreCreateBinary();

    // Maximum count = 3, initial count = 0
    counting = xSemaphoreCreateCounting(3, 0);
}

void example(void)
{
    // The task can wait indefinitely for a token.
    xSemaphoreTake(binary, portMAX_DELAY);

    // Give a token.
    xSemaphoreGive(binary);

    xSemaphoreTake(counting, portMAX_DELAY);
    xSemaphoreGive(counting);
}
```

This is an API illustration; production code should check whether
creation succeeded and check return values where relevant. A newly
created FreeRTOS binary semaphore starts empty. A counting semaphore's
initial and maximum counts are supplied to its creation function.

A standard FreeRTOS semaphore give does not wait for the count to drop
below its maximum. Check the return value to learn whether the operation
succeeded.

## 9. Related mechanisms (not additional semaphore types)

  -----------------------------------------------------------------------
  Mechanism                           Main purpose
  ----------------------------------- -----------------------------------
  Mutex                               Exclusive ownership of a shared
                                      resource

  Condition variable                  Wait for a condition while
                                      coordinating with a mutex

  Event flags / event groups          Represent and wait on event bits

  Task notifications                  Notify a particular task in RTOSes
                                      that support them

  Queue / message queue               Transfer data between tasks
  -----------------------------------------------------------------------

These mechanisms may overlap in what they can accomplish, but they are
not simply extra semaphore types.

## 10. Core takeaways

-   Binary semaphore: at most one pending token.
-   Counting semaphore: multiple pending tokens up to a configured
    maximum.
-   Either can be initialized to `0`.
-   `take()` can block when the count is zero.
-   `give()` makes a token available; it generally does not block when
    the semaphore is full.
-   The semaphore counts tokens, not the identity or meaning of the
    event. The program defines what each token represents.
