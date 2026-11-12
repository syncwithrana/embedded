#ifndef BSP_H
#define BSP_H

/* system clock tick [Hz] */
#define BSP_TICKS_PER_SEC 100U

void BSP_init(void);
void BSP_delay(uint32_t ticks);
uint32_t BSP_tickCtr(void);

void BSP_ledRedOn(void);
void BSP_ledRedOff(void);

void BSP_ledBlueOn(void);
void BSP_ledBlueOff(void);

void BSP_ledGreenOn(void);
void BSP_ledGreenOff(void);

#endif /* BSP_H */
