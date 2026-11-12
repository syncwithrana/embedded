#include <stdint.h>
#include "TM4C123GH6PM.h"
#include "bsp.h"

/* Bridge Q_onAssert (used by miros.c) to assert_failed (defined in your bsp.c) */
extern void assert_failed(char const *module, int loc);

void Q_onAssert(char const *module, int loc) {
    assert_failed(module, loc);
}

/* Provide OS_onStartup / OS_onIdle if missing from your lesson's bsp.c/main.c */
__attribute__((weak))
void OS_onStartup(void) {
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);
    NVIC_SetPriority(SysTick_IRQn, 0U);
}

__attribute__((weak))
void OS_onIdle(void) {
    __asm volatile ("WFI");
}