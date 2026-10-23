# ELF — Lessons 1–20
*A practical guide for embedded software engineers*

> These notes collect the 20 ELF lessons into one Markdown document. Diagrams are kept in simple ASCII form so they render in plain-text editors and Markdown viewers.

---

# Lesson 1 — What is ELF?

**ELF (Executable and Linkable Format)** is a file format used for executables, object files, shared libraries, and core dumps on many Unix-like systems.

A typical build flow:

```text
source.c  ──compiler──>  object.o  ──linker──>  executable
```

- `source.c`: human-readable source.
- `object.o`: compiled machine code plus symbols and relocation information.
- `executable`: linked program, with information needed to run it.

An object file is not simply raw machine code. It also carries metadata that helps the linker and development tools.

Common ELF file types include:
- Relocatable object files (`.o`)
- Executables
- Shared libraries (`.so`)
- Core dumps

Useful first commands:

```bash
file program
readelf -h program
```

**Embedded connection:** Cross-compilers often produce ELF files for the target CPU. A debugger may use that ELF even when the firmware flashed to the device is a raw `.bin`.

---

# Lesson 2 — The structure of an ELF file

An ELF file has a header and may contain tables describing its contents.

```text
+--------------------------------+
| ELF header                     |
+--------------------------------+
| Program header table           |  <- describes loadable segments
+--------------------------------+
| Sections / file contents       |
| .text, .data, .symtab, etc.    |
+--------------------------------+
| Section header table           |  <- describes sections
+--------------------------------+
```

The exact ordering and presence of tables can vary. For example, a stripped executable can lack a section header table and still be loadable.

Two views are especially important:

- **Sections** organize content for linkers, debuggers, and inspection tools.
- **Segments** describe how portions of a file are mapped into memory by a loader.

Inspect the header and section table:

```bash
readelf -h program
readelf -S program
```

**Remember:** Sections and segments are related, but they are not the same thing.

---

# Lesson 3 — Important ELF sections

Common sections include:

| Section | Purpose |
|---|---|
| `.text` | Machine instructions |
| `.rodata` | Read-only constants and string literals |
| `.data` | Initialized writable static-storage data |
| `.bss` | Zero-initialized or uninitialized static-storage data |
| `.symtab` | Symbol table, when retained |
| `.strtab` | Strings used by the symbol table |
| `.debug_info` | DWARF debugging information, when built with debug info |

Example:

```c
int count = 7;       // commonly in .data
int total;           // commonly in .bss

const char *message = "hello";
```

The string literal `"hello"` is commonly placed in `.rodata`; the pointer variable `message` itself is writable initialized data and commonly resides in `.data`.

For a bare-metal target:
- Initialized `.data` values are stored in the load image, commonly in flash.
- Startup code copies those values into RAM.
- Startup code clears `.bss` to zero.

Inspect sections:

```bash
readelf -S firmware.elf
size -A firmware.elf
```

Exact placement depends on compiler options, linker script, and target.

---

# Lesson 4 — Linker scripts, MEMORY, VMA, and LMA

On bare-metal systems, the linker script defines where sections should live.

A simplified example:

```ld
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
    .text : {
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data : {
        *(.data*)
    } > RAM AT > FLASH

    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    } > RAM
}
```

This is illustrative; real scripts need target-specific sections, alignment, vector tables, stack/heap definitions, and symbols.

- **VMA (Virtual Memory Address):** address where a section is intended to reside while running.
- **LMA (Load Memory Address):** address where the section's initial contents are stored in the load image.

For `.data`, the VMA is often RAM while the LMA is flash.

```text
FLASH:
+---------------------------+
| .text                     |
| .rodata                   |
| initial bytes for .data   |  <- LMA
+---------------------------+

RAM:
+---------------------------+
| .data                     |  <- VMA; copied from flash at startup
| .bss                      |  <- cleared to zero at startup
+---------------------------+
```

The startup code performs the copy and zeroing; the linker script usually provides symbols that tell it what ranges to use.

---

# Lesson 5 — Symbols and relocation

A **symbol** represents something such as a function or variable. A symbol table can tell the linker the symbol's name, value, type, and other attributes.

Consider two files:

```c
/* main.c */
int add(int, int);

int main(void)
{
    return add(2, 3);
}
```

```c
/* add.c */
int add(int a, int b)
{
    return a + b;
}
```

When `main.c` is compiled alone, the compiler cannot yet know the final address of `add`. The object file can contain:
- An undefined symbol reference to `add`
- A relocation entry identifying a location that needs adjustment

The linker finds the definition of `add` and applies the required fix-up.

```text
main.o                    add.o
+------------------+      +------------------+
| call add         |      | definition: add  |
| undefined add    |      | machine code     |
| relocation entry |      +------------------+
+--------+---------+
         |
         v
       LINKER
         |
         v
+---------------------------+
| executable with resolved |
| references where possible|
+---------------------------+
```

Useful commands:

```bash
nm main.o
readelf -s main.o
readelf -r main.o
```

---

# Lesson 6 — Strong and weak symbols

A **strong symbol** is the ordinary definition. A **weak symbol** is a definition that can generally be overridden by a strong one.

GCC example:

```c
__attribute__((weak))
void board_init(void)
{
    /* Default implementation */
}
```

Another object can provide a normal, strong definition of `board_init()`; the linker will generally select the strong definition over the weak default.

Common uses:
- Default hooks
- Optional platform-specific implementations
- Library defaults that applications may override

Be careful: duplicate strong definitions usually cause a linker error, and exact rules depend on symbol type, object format, and toolchain.

Inspect symbols with:

```bash
nm object.o
readelf -s object.o
```

---

# Lesson 7 — Reading `nm` symbol letters

`nm` displays symbols and uses letters to indicate their general type.

Common letters:

| Letter | Typical meaning |
|---|---|
| `T` | Global function or symbol in text/code |
| `t` | Local text symbol |
| `D` | Initialized data |
| `B` | BSS / zero-initialized storage |
| `U` | Undefined in this object; referenced but not defined here |
| `W` | Weak symbol |

Example:

```text
00000000 T main
00000000 D count
00000000 B buffer
         U printf
```

An undefined (`U`) symbol is not automatically an error. It may be resolved by another object or a library during linking.

Commands:

```bash
nm file.o
nm -n firmware.elf
```

`-n` sorts symbols by address, which is handy when examining memory layout.

---

# Lesson 8 — Program headers and loadable segments

Program headers describe segments used when loading an ELF file.

Inspect them with:

```bash
readelf -lW program
```

A common segment type is `PT_LOAD`, displayed as `LOAD`.

Important fields include:

- `Offset`: where the segment begins in the file.
- `VirtAddr`: intended virtual address.
- `FileSiz`: number of bytes supplied by the file.
- `MemSiz`: number of bytes required in memory.
- `Flags`: permissions such as read, write, and execute.

Example concept:

```text
File:
[ bytes that exist on disk ........ ]

Memory after loading:
[ file-backed bytes ][ zero-filled extra space ]
                    ^ MemSiz can exceed FileSiz
```

When `MemSiz > FileSiz`, the extra memory in a conventional loadable segment is zero-filled. This is a common way `.bss` storage is represented.

Do not assume one segment equals one section. A segment can contain several sections.

---

# Lesson 9 — ELF versus raw binary (`.bin`)

An ELF file carries metadata; a raw binary generally contains only the selected load-image bytes.

| ELF | Raw `.bin` |
|---|---|
| Has an ELF header | No ELF header |
| Can include symbols and relocations | No standard symbol table |
| Can include debug information | No standard debug information |
| Describes program headers/segments | Does not inherently state its load address |
| Useful for linking and debugging | Commonly used for flashing firmware |

Conversion example:

```bash
arm-none-eabi-objcopy -O binary firmware.elf firmware.bin
```

The raw image's destination address must come from the flashing procedure, memory map, or accompanying metadata—not from a built-in ELF header.

A common embedded workflow is:

```text
source -> object files -> firmware.elf
                         |        |
                         |        +--> debugger uses symbols/debug info
                         +-----------> objcopy produces firmware.bin
```

Keep the ELF that matches the exact firmware image flashed to the target. It is invaluable for debugging.

---

# Lesson 10 — Linker map files and memory usage

A **linker map file** records useful information about the link: section placement, symbol addresses, object-file contributions, and memory usage.

Generate one with GCC-style linking:

```bash
-Wl,-Map=firmware.map
```

Then inspect:
- Where `.text`, `.data`, and `.bss` are placed
- Which object files contribute most to code size
- The addresses of functions and variables
- Whether a memory region is overflowing

Useful commands:

```bash
arm-none-eabi-size firmware.elf
arm-none-eabi-size -A firmware.elf
arm-none-eabi-nm -n firmware.elf
```

Conceptual memory map:

```text
FLASH
0x08000000  +------------------+
            | vectors / .text  |
            | .rodata          |
            | .data load bytes |
            +------------------+

RAM
0x20000000  +------------------+
            | .data runtime    |
            | .bss             |
            | heap / buffers   |
            | stack            |
            +------------------+
```

The actual order and addresses come from the linker script and target memory map.

---

# Lesson 11 — Entry point and startup code

An ELF executable has an entry-point address recorded in its ELF header. For a hosted Linux executable, the entry point is normally runtime startup code, which eventually invokes `main()`.

Bare-metal Cortex-M startup is different: reset hardware typically reads the initial stack pointer and reset-handler address from the vector table. The reset handler performs initialization before calling `main()`.

A simplified bare-metal sequence:

```text
Reset
  |
  v
Read vector table / enter reset handler
  |
  v
Initialize stack and runtime
  |
  +--> Copy .data initial values from flash to RAM
  |
  +--> Clear .bss in RAM
  |
  +--> Initialize platform / C runtime as required
  |
  v
main()
```

This explains why a global variable with an initializer can have its initial value in flash but be accessed from RAM during execution.

---

# Lesson 12 — Debug symbols and DWARF

Compile with debug information using `-g`:

```bash
gcc -g main.c -o program
```

Debug information commonly uses DWARF and can describe:
- Source files and line mappings
- Variables and types
- Function and scope information
- Relationships between machine instructions and source code

Common sections include `.debug_info`, `.debug_line`, and `.debug_abbrev`.

A stripped executable may have much of its symbol/debug information removed, reducing file size. Debuggers then have less information available.

Important embedded rule: **the ELF used for debugging must match the firmware actually running on the target.** A mismatched ELF can make breakpoints, stack traces, and source-line mappings misleading.

Optimization can also change how source code maps to instructions, inline functions, or remove variables that are no longer needed.

---

# Lesson 13 — Static linking and dynamic linking

**Static linking** incorporates required library code into the executable at link time, where supported and selected.

**Dynamic linking** uses shared libraries that are loaded and linked at runtime.

```text
Static:
object files + static libraries
              |
              v
       self-contained executable
       (for the linked dependencies)

Dynamic:
executable + references to shared libraries
              |
              v
       runtime dynamic loader
              |
              v
       executable + mapped shared libraries
```

A shared library on Linux commonly has a `.so` suffix, such as `libc.so.6`.

Inspect dynamic dependencies and metadata:

```bash
readelf -d program
```

`ldd program` can also display dependencies for trusted local executables. Avoid using `ldd` on untrusted files; use static inspection such as `readelf` when appropriate.

Bare-metal firmware is typically linked into a fixed image and does not use a Linux-style dynamic loader.

---

# Lesson 14 — Architecture, ELF class, and endianness

The ELF header identifies properties such as:
- ELF class: 32-bit (`ELF32`) or 64-bit (`ELF64`)
- Byte order
- Target machine architecture
- ELF file type

Inspect it:

```bash
readelf -h firmware.elf
```

A file's architecture must be compatible with the intended CPU and toolchain. Examples include ARM, AArch64, x86-64, and RISC-V.

Endianness controls byte order. For the value `0x12345678`:

```text
Big-endian bytes:     12 34 56 78
Little-endian bytes:  78 56 34 12
```

The ELF header's encoding field describes the byte order used for the ELF file's encoded structures. The target's data conventions are another important part of interpreting machine data correctly.

This matters when inspecting dumps, exchanging data with peripherals, or debugging a cross-compiled target.

---

# Lesson 15 — ELF file types: `ET_REL`, `ET_EXEC`, `ET_DYN`

The ELF header's `Type` field identifies the broad kind of ELF file.

| Type | Meaning |
|---|---|
| `ET_REL` | Relocatable object file, commonly `.o` |
| `ET_EXEC` | Executable file |
| `ET_DYN` | Shared object; also commonly used for PIE executables |

Examples:

```bash
readelf -h main.o
readelf -h program
readelf -h libexample.so
```

A typical object file reports `REL`. A traditional executable reports `EXEC`. A shared library reports `DYN`; a PIE executable commonly reports `DYN` too.

Therefore, **`ET_DYN` by itself does not prove that a file is a shared library**. More context, such as dynamic metadata and how the file is built/used, is needed.

---

# Lesson 16 — Relocation: fixing address-dependent references

Relocation is the process of adjusting address-dependent values when code and data are combined or loaded.

At static link time, the linker may know the final address of a function and patch a call instruction or related data.

At runtime, the dynamic loader may need to apply dynamic relocations because shared-library addresses are only known after mapping.

```text
Before final address is known:
    call target  ---> unresolved / placeholder reference

After relocation:
    call target  ---> correct target address or displacement
```

Relocation records generally identify:
- The location to adjust
- The kind of relocation
- The symbol involved, if any
- An addend or other information used in the calculation

Relocation names depend on architecture. Examples include `R_ARM_CALL`, `R_AARCH64_CALL26`, and `R_X86_64_PC32`.

Potential link-time problems include:
- Undefined symbols
- Relocation overflow
- Unsupported relocation types
- Incompatible object files or architectures

Inspect relocations:

```bash
readelf -r object.o
```

---

# Lesson 17 — The dynamic section and runtime linking metadata

The `.dynamic` section contains metadata used by a dynamic linker. It describes dependencies and other information needed for runtime linking. The loader uses the relevant dynamic information through the ELF's program headers and mapped contents; it does not rely solely on section names.

Inspect it with:

```bash
readelf -d program
```

Entries you may see include:

- `NEEDED`: a required shared library.
- `SONAME`: a shared library's identifying name.
- `STRTAB`: location of dynamic strings.
- `SYMTAB`: location of the dynamic symbol table.
- `JMPREL`: relocation information associated with procedure linkage.
- `PLTGOT`: address-related information for PLT/GOT linkage.

The exact entries depend on how the file was linked.

A simplified runtime sequence:

```text
Kernel maps executable
        |
        v
Dynamic loader maps required libraries
        |
        v
Loader processes dynamic metadata and relocations
        |
        v
Program starts / continues execution
```

On Linux, the dynamic loader is commonly responsible for mapping shared libraries, resolving symbols, and applying relocations. Some relocations can be deferred, depending on the linking mode and loader settings.

---

# Lesson 18 — Dynamic linking: GOT and PLT

Imagine this program calls `printf()`:

```c
#include <stdio.h>

int main(void)
{
    printf("Hello!\n");
    return 0;
}
```

The actual `printf()` implementation may live in a shared library, and that library can be mapped at different addresses in different processes.

Two structures commonly help with external function calls:

- **PLT (Procedure Linkage Table):** provides function-call stubs.
- **GOT (Global Offset Table):** stores addresses or address-related values used by position-independent code and dynamic linking.

Conceptual call flow:

```text
+------------------------+
| Application calls      |
| printf()               |
+-----------+------------+
            |
            v
+------------------------+
| PLT entry              |
| Function-call stub     |
+-----------+------------+
            |
            v
+------------------------+
| GOT entry              |
| Target/address data    |
+-----------+------------+
            |
            v
+------------------------+
| printf() in shared lib |
+------------------------+
```

This is conceptual; exact instructions and mechanisms depend on the architecture and linking configuration.

### Lazy binding

Some environments support lazy binding for eligible function calls:

1. The program starts without resolving every eligible external call.
2. The first call can enter a dynamic resolver.
3. The resolver finds the target and updates linkage information.
4. Later calls use the resolved path.

With eager binding, relevant relocations are resolved earlier, commonly during startup. Behavior depends on the loader, linker options, security settings, and executable type.

Inspect dynamic metadata and code:

```bash
readelf -d program
readelf -r program
objdump -d -j .plt program
```

Section names and PLT layout vary by architecture and toolchain.

**Key distinction:** the PLT provides call stubs; the GOT provides address-related storage used by code and the linker/loader.

---

# Lesson 19 — Position-independent code (PIC) and PIE

**Position-independent code (PIC)** is compiled so it can execute without requiring all references to be tied to one fixed load address.

A **position-independent executable (PIE)** is an executable built to support position-independent loading, commonly allowing Linux to map it at varying virtual addresses.

GCC examples:

```bash
# Compile code suitable for a shared library
gcc -fPIC -c library.c -o library.o
gcc -shared library.o -o libexample.so

# Build a position-independent executable
gcc -fPIE -pie main.c -o program
```

- `-fPIC`: generate position-independent code, commonly for shared libraries.
- `-fPIE`: generate code intended for position-independent executables.
- `-pie`: request a PIE executable.

Support and defaults vary by target and toolchain.

PIC/PIE are useful for shared libraries, flexible loading, and ASLR. Bare-metal microcontrollers more commonly use fixed memory layouts, although position-independent firmware and relocatable designs are possible.

---

# Lesson 20 — ELF loading, memory permissions, and ASLR

When you run a Linux executable, the kernel examines its program headers—especially `PT_LOAD` segments—to determine what to map into the process's virtual address space.

```text
+-----------------------------+
| ELF executable on disk      |
| Header, program headers,     |
| file contents               |
+--------------+--------------+
               |
               v
+-----------------------------+
| Kernel ELF loader            |
| Maps loadable segments       |
+--------------+--------------+
               |
               v
+-----------------------------+
| Process virtual memory       |
| Code, data, BSS, stack,      |
| libraries, other mappings    |
+-----------------------------+
```

The dynamic loader may then map shared libraries and perform relocations before execution reaches the application's entry point.

## Virtual memory regions

A process commonly has regions corresponding to:

| Region | Typical contents |
|---|---|
| Text | Executable instructions |
| Read-only data | Constants and string literals |
| Data | Initialized writable globals |
| BSS | Zero-initialized static-storage variables |
| Heap | Dynamically allocated memory |
| Stack | Function calls and local variables |
| Shared libraries | Library code and data |

These are conceptual regions; exact locations and organization vary by operating system, architecture, and executable.

Unlike a typical bare-metal linker script, Linux processes use virtual addresses that need not directly correspond to physical RAM addresses.

## Memory permissions

Common segment permissions include:

- `R`: readable
- `W`: writable
- `E`: executable

A typical arrangement is:

| Segment | Typical permissions |
|---|---|
| Instructions | Read + Execute |
| Constants | Read |
| Global variables | Read + Write |

Separating permissions reduces the damage certain memory-corruption bugs can cause. This is conceptually related to configuring an MPU in embedded systems, though the mechanisms differ.

## ASLR — Address Space Layout Randomization

ASLR randomizes selected memory locations so that addresses are less predictable between process runs. Shared libraries and other regions can be mapped at different virtual addresses; PIE can also allow the executable's own mapping to vary.

This connects back to GOT, PLT, and relocations: references must still work when load addresses vary.

## Inspect the loadable segments

```bash
readelf -l ./program
readelf -lW ./program
```

Look for:
- `LOAD`: a loadable segment.
- `R E`: commonly an executable code segment.
- `RW`: commonly a writable data segment.
- `FileSiz` versus `MemSiz`: file-backed size versus memory size.

To inspect the ELF type and dynamic metadata:

```bash
readelf -h ./program
readelf -d ./program
```

A PIE executable commonly has type `DYN`, but the type alone does not distinguish every PIE executable from every shared object.

---

# Quick reference — commands worth remembering

```bash
# Identify the file and inspect its ELF header
file program
readelf -h program

# Sections, segments, symbols, and relocations
readelf -S program
readelf -lW program
readelf -s program
readelf -r program

# Dynamic-linking metadata
readelf -d program

# Symbols and sizes
nm program
nm -n firmware.elf
size firmware.elf
size -A firmware.elf

# Disassemble code
objdump -d program

# Produce a raw firmware image
arm-none-eabi-objcopy -O binary firmware.elf firmware.bin

# Generate a linker map (GCC-style linker invocation)
-Wl,-Map=firmware.map
```

# Big-picture recap

```text
ELF header
  ├── File type, architecture, entry point
  │
  ├── Sections
  │     ├── .text / .rodata / .data / .bss
  │     ├── symbols and strings
  │     └── debug information
  │
  ├── Relocations
  │     └── Address-dependent references to fix
  │
  ├── Program headers / segments
  │     └── Describe loadable memory mappings
  │
  └── Dynamic metadata (when applicable)
        ├── Shared-library dependencies
        ├── Runtime relocations
        └── GOT / PLT-related information
```

The most useful mental model:

- **Compiler:** turns source into object files.
- **Linker:** combines objects, resolves symbols, and applies static relocations.
- **ELF metadata:** describes the file and its contents.
- **Loader:** maps loadable segments and, when needed, coordinates dynamic linking.
- **Debugger:** uses the ELF's symbols and debug information to relate machine execution to source code.
- **Bare-metal startup:** uses the linked image and startup code to initialize RAM and enter the application.
