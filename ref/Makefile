CC      = arm-none-eabi-gcc
GDB     = arm-none-eabi-gdb
QEMU    = qemu-system-arm

CFLAGS  = -mcpu=cortex-m3 -mthumb -ffreestanding -O0 -g -Wall -I.
LDFLAGS = -T linker.ld -nostartfiles -nostdlib

SRCS    = startup.c main.c bsp.c miros.c stubs.c
TARGET  = rtos.elf

all: $(TARGET)

$(TARGET): $(SRCS) linker.ld TM4C123GH6PM.h miros.h bsp.h qassert.h
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	$(QEMU) -M lm3s6965evb -nographic -kernel $(TARGET)

debug: $(TARGET)
	$(QEMU) -M lm3s6965evb -nographic -s -S -kernel $(TARGET)

gdb: $(TARGET)
	$(GDB) $(TARGET) -x watch_gpio.gdb

clean:
	rm -f $(TARGET)

kill:
	-killall -9 qemu-system-arm 2>/dev/null || true
	-killall -9 arm-none-eabi-gdb 2>/dev/null || true
	-killall -9 gdb 2>/dev/null || true

.PHONY: all run debug gdb clean kill
