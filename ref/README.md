# MiROS (Quantum Leaps RTOS) on QEMU (`lm3s6965evb`)

Complete preemptive priority-based MiROS RTOS setup (from Miro Samek's *Modern Embedded Systems Programming Course*, Lesson 27/28) configured to build with `arm-none-eabi-gcc` and run on QEMU without modifying `main.c` or `miros.c`.

## Prerequisites
- `arm-none-eabi-gcc`
- `qemu-system-arm`
- `make`

## Build & Run
```bash
make
make debug
```

In a second terminal, monitor the GPIO states with GDB:
```bash
make gdb
```

- **Kill background processes:** Run `make kill` to terminate any running QEMU or GDB instances.
- **Exit QEMU (`-nographic`):** Press `Ctrl + A`, release, then press `X`.
