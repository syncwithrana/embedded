# Disable pagination so output streams continuously without "--Type <RET>--" prompts
set pagination off

# Connect to QEMU's GDB server
target remote :1234

# Set a write watchpoint on GPIOF->DATA (0x400253FC)
watch *(unsigned int *)0x400253FC

# Automatically print the new register value and resume on every write
commands
    silent
    printf "GPIOF->DATA = 0x%02x  (R:%d B:%d G:%d)\n", \
        *(unsigned int *)0x400253FC, \
        (*(unsigned int *)0x400253FC >> 1) & 1, \
        (*(unsigned int *)0x400253FC >> 2) & 1, \
        (*(unsigned int *)0x400253FC >> 3) & 1
    continue
end

# Start execution
continue