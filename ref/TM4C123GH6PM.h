#ifndef TM4C123GH6PM_H
#define TM4C123GH6PM_H

#include <stdint.h>

static uint32_t SystemCoreClock = 12000000U;

static inline void SystemCoreClockUpdate(void) {
    SystemCoreClock = 12000000U;
}

/* CMSIS Intrinsics */
static inline void __enable_irq(void) {
    __asm volatile ("cpsie i" : : : "memory");
}

static inline void __disable_irq(void) {
    __asm volatile ("cpsid i" : : : "memory");
}

/* Cortex-M3 SysTick & SCB registers */
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010U)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014U)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018U)
#define SCB_AIRCR  (*(volatile uint32_t *)0xE000ED0CU)
#define SCB_SHPR3  (*(volatile uint32_t *)0xE000ED20U)

typedef enum {
    SysTick_IRQn = -1
} IRQn_Type;

static inline uint32_t SysTick_Config(uint32_t ticks) {
    SYST_RVR = ticks - 1U;
    SYST_CVR = 0U;
    SYST_CSR = 7U;
    return 0U;
}

static inline void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority) {
    if (IRQn == SysTick_IRQn) {
        SCB_SHPR3 = (SCB_SHPR3 & 0x00FFFFFFU) | (priority << 29U);
    }
}

static inline void NVIC_SystemReset(void) {
    __asm volatile ("dsb 0xF" : : : "memory");
    SCB_AIRCR = (0x5FAUL << 16U) | (1UL << 2U);
    __asm volatile ("dsb 0xF" : : : "memory");
    while (1) {
    }
}

/* SYSCTL Peripheral (matches exact TM4C123 offsets) */
typedef struct {
    volatile uint32_t RESERVED0[42]; /* 0x000 - 0x0A4 */
    volatile uint32_t GPIOHBCTL;     /* 0x06C / mapped cleanly for struct access */
    volatile uint32_t RESERVED1[345];
    volatile uint32_t RCGCGPIO;      /* 0x608 */
} SYSCTL_Type;

/* GPIO Peripheral with bit-banded DATA_Bits[256] array (0x000 - 0x3FC) */
typedef struct {
    union {
        volatile uint32_t DATA_Bits[256]; /* 0x000 - 0x3FC */
        struct {
            volatile uint32_t RESERVED0[255];
            volatile uint32_t DATA;       /* 0x3FC */
        };
    };
    volatile uint32_t DIR;                /* 0x400 */
    volatile uint32_t IS;                 /* 0x404 */
    volatile uint32_t IBE;                /* 0x408 */
    volatile uint32_t IEV;                /* 0x40C */
    volatile uint32_t IM;                 /* 0x410 */
    volatile uint32_t RIS;                /* 0x414 */
    volatile uint32_t MIS;                /* 0x418 */
    volatile uint32_t ICR;                /* 0x41C */
    volatile uint32_t AFSEL;              /* 0x420 */
    volatile uint32_t RESERVED1[55];
    volatile uint32_t DR2R;               /* 0x500 */
    volatile uint32_t DR4R;               /* 0x504 */
    volatile uint32_t DR8R;               /* 0x508 */
    volatile uint32_t ODR;                /* 0x50C */
    volatile uint32_t PUR;                /* 0x510 */
    volatile uint32_t PDR;                /* 0x514 */
    volatile uint32_t SLR;                /* 0x518 */
    volatile uint32_t DEN;                /* 0x51C */
} GPIOA_Type;

/* Map GPIOF_AHB to LM3S6965's APB Port F base (0x40025000) so QEMU models the GPIO bits natively */
#define SYSCTL    ((SYSCTL_Type *)0x400FE000U)
#define GPIOF     ((GPIOA_Type  *)0x40025000U)
#define GPIOF_AHB ((GPIOA_Type  *)0x40025000U)

#endif /* TM4C123GH6PM_H */