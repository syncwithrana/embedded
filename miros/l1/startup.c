#include <stdint.h>

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
extern int main(void);
extern void SysTick_Handler(void);
extern void PendSV_Handler(void);

void Reset_Handler(void) {
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }
    for (dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0;
    }
    main();
    while (1);
}

void Default_Handler(void) {
    while (1);
}

__attribute__((section(".isr_vector")))
const uint32_t vector_table[] = {
    (uint32_t)&_estack,
    (uint32_t)Reset_Handler,
    (uint32_t)Default_Handler, /* NMI */
    (uint32_t)Default_Handler, /* HardFault */
    0, 0, 0, 0, 0, 0, 0,       /* Reserved */
    (uint32_t)Default_Handler, /* SVCall */
    0, 0,
    (uint32_t)PendSV_Handler,  /* PendSV -> handled in miros.c */
    (uint32_t)SysTick_Handler  /* SysTick -> handled in bsp.c */
};
