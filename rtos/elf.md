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


# ELF Internals — Lessons 21–35
*A detailed continuation of the ELF learning series for embedded software engineers*

This document preserves the lesson sequence, explanations, examples, commands, tables, and simple diagrams from Lessons 21–35. Diagrams are written in plain text so they work in Markdown viewers, VS Code, Obsidian, and terminal editors.

---

# Lesson 21 — Dynamic relocations: fixing addresses at runtime

In Lesson 16, relocation was introduced as the process of fixing address-dependent references. This lesson looks at what happens when the final address is only known at runtime.

## 1. A simple example

Suppose a shared library contains:

```c
int value = 42;

int get_value(void)
{
    return value;
}
```

The library might be loaded at different virtual addresses in different processes. When its machine code accesses `value`, the correct address must be determined using the relocation and addressing mechanisms appropriate to the architecture.

Conceptually, suppose:

- The library's `value` variable is loaded at address `0x70001000`.
- A relocation needs to place that address into a location used by the program.

The runtime linker applies the required adjustment so the reference points to the actual variable.

```text
Before relocation:
+--------------------------+
| Address-dependent field  |
| Not yet resolved         |
+--------------------------+

Runtime linker applies relocation

After relocation:
+--------------------------+
| Address-dependent field  |
| Points to 0x70001000     |
+--------------------------+
```

This is conceptual; actual relocations may update a pointer, a GOT entry, or an instruction field rather than store a full address directly.

## 2. Two common relocation ideas

- **Absolute relocation:** the value depends on the target's address.
- **PC-relative relocation:** the value depends on the distance between the reference location and target.

For a simplified PC-relative calculation:

\[
\text{displacement} = S + A - P
\]

Where:

- \(S\) = target symbol address
- \(A\) = relocation addend
- \(P\) = address of the relocation location

For example, if \(S=0x1200\), \(A=0\), and \(P=0x1000\):

\[
\text{displacement}=0x1200-0x1000=0x200
\]

The instruction may encode a displacement of `0x200`, subject to the architecture's specific relocation rules and instruction encoding.

## 3. Inspect dynamic relocations

```bash
readelf -r program
readelf -r libexample.so
```

You may see relocation types such as `R_X86_64_RELATIVE` or architecture-specific GOT and PLT relocations.

**Takeaway:** relocation is not always “write the symbol address here.” The linker follows the formula and encoding defined by the relocation type.

---

# Lesson 22 — How the dynamic linker resolves symbols

Imagine two shared libraries both provide a function named `process_data()`. Which one does a program use?

The dynamic linker follows symbol-resolution rules that depend on factors such as dependency order, symbol visibility, symbol type, and loader configuration.

## 1. The lookup process

Conceptually:

```text
Application requests process_data()
              |
              v
Dynamic symbol lookup
              |
              v
Search applicable symbol scopes
              |
              v
Find a matching visible definition
              |
              v
Apply required relocation / linkage
              |
              v
Call the selected implementation
```

A symbol name alone is not always enough to determine the result. ELF supports concepts such as symbol binding, visibility, and symbol versioning.

## 2. Symbol visibility

Common ELF symbol visibility values include:

- **DEFAULT:** generally visible to other components; preemption may be possible.
- **HIDDEN:** not available for normal external lookup from other components.
- **PROTECTED:** externally visible, but references within the defining component bind to its own definition under the relevant ELF rules.

Visibility can affect which definition is selected and whether a symbol can be interposed.

## 3. Why symbol resolution matters

It helps explain problems such as:

- A shared library unexpectedly calling a different implementation.
- An executable failing with an undefined-symbol error.
- Two library versions exporting similar APIs.
- A symbol being present in a file but unavailable to external lookup.

Useful commands:

```bash
readelf -Ws libexample.so
nm -D libexample.so
```

`readelf -Ws` shows symbol-table information; `nm -D` focuses on dynamic symbols.

**Takeaway:** the dynamic linker resolves symbols according to ELF and loader rules—not simply by picking any function with the same name.

---

# Lesson 23 — Symbol versioning and shared-library compatibility

Linux shared libraries can use **symbol versioning** to manage API and ABI compatibility.

For example, a library may export a function with one symbol version for older applications and a newer version for newer applications.

Conceptually:

```text
libexample.so
    |
    +-- process_data@LIB_1.0
    |
    +-- process_data@@LIB_2.0
```

This is illustrative notation:

- `@` commonly denotes a specific, non-default version.
- `@@` commonly marks the default version used for unversioned references.

The actual version labels and syntax depend on the toolchain and inspection tool.

## Why versioning exists

Suppose an API evolves over time:

```c
/* Older behavior */
int process_data(/* old interface */);

/* Newer behavior */
int process_data(/* updated interface */);
```

Changing a function's ABI can break existing binaries. Symbol versioning can allow multiple compatible implementations to coexist in a shared library, letting older binaries continue to bind to the expected version.

## Inspect symbol versions

```bash
readelf --version-info libexample.so
readelf -Ws libexample.so
```

You may also use:

```bash
objdump -T libexample.so
```

These commands can reveal version requirements and versioned exported symbols.

A missing required symbol version can prevent a program from starting, even if a library with the expected filename exists.

**Takeaway:** having the right shared-library filename does not guarantee ABI compatibility. The required symbols and their versions must also be available.

---

# Lesson 24 — What happens when you run an ELF executable?

When you type:

```bash
./program
```

Linux needs to turn the ELF file into a running process. One important system call involved is `execve()`.

## 1. The execution flow

```text
+-------------------------+
| Shell                   |
| You execute ./program   |
+------------+------------+
             |
             v
+-------------------------+
| execve()                |
| Requests execution      |
+------------+------------+
             |
             v
+-------------------------+
| Kernel ELF loader       |
| Validates ELF and maps  |
| loadable segments       |
+------------+------------+
             |
             v
+-------------------------+
| Runtime entry point     |
| Dynamic loader, if      |
| required, then startup  |
+------------+------------+
             |
             v
+-------------------------+
| main()                  |
+-------------------------+
```

One subtle point: `execve()` replaces the calling process's program image; it does not normally create a new process by itself. A shell commonly calls `fork()` or a related mechanism first when launching an external command.

## 2. What does the kernel inspect?

The kernel reads the ELF header and program headers. Among other things, it needs to establish:

- Whether the file is a supported executable.
- Which loadable segments to map.
- The entry point.
- Whether a program interpreter is specified.

For a dynamically linked executable, the ELF may contain a `PT_INTERP` program header identifying the dynamic loader.

Inspect it:

```bash
readelf -lW ./program
```

Look for an entry named `INTERP`.

## 3. Static versus dynamic execution

```text
Static executable:
Kernel -> executable entry point -> startup -> main()

Dynamic executable:
Kernel -> dynamic loader -> relocations / libraries
       -> executable startup -> main()
```

This is simplified: exact behavior depends on the executable type, architecture, and runtime environment.

**Key takeaway:** the kernel maps the executable, while the dynamic loader handles runtime linking when needed.

---

# Lesson 25 — How Linux finds shared libraries

Suppose your executable depends on:

```text
libexample.so
```

The dynamic loader must locate a compatible library.

## 1. Where does it look?

On Linux, shared-library lookup can involve several mechanisms:

- A dependency containing a path with `/` can be interpreted as a pathname.
- `DT_RPATH` or `DT_RUNPATH` metadata may specify search directories.
- `LD_LIBRARY_PATH` can add directories, subject to secure-execution restrictions.
- The system's loader cache may provide candidate locations.
- Default library directories are searched according to loader rules.

The exact order depends on details such as `DT_RPATH` versus `DT_RUNPATH`, whether a dependency is direct or indirect, and security mode.

## 2. Inspect dependencies

```bash
readelf -d ./program
```

Look for entries such as:

```text
NEEDED   Shared-library dependency
RPATH    Legacy runtime search path
RUNPATH  Runtime search path
```

Not every executable contains all these entries.

For a trusted local program, you can also run:

```bash
ldd ./program
```

Avoid using `ldd` on untrusted executables; static inspection with `readelf` is safer.

## 3. A common failure

Imagine the executable requires `libexample.so`, but the loader cannot find it.

You might see an error like:

```text
error while loading shared libraries:
libexample.so: cannot open shared object file
```

The file may exist on your machine but be outside the loader's search paths. Alternatively, the library may have an incompatible architecture or be missing a required symbol version.

**Embedded Linux connection:** this is a common deployment problem when an application works in a development environment but fails on the target device.

---

# Lesson 26 — From `_start()` to `main()`

One of the most useful distinctions in ELF is that **`main()` is not usually the ELF entry point** for a normal Linux C application.

Consider:

```c
#include <stdio.h>

int main(int argc, char **argv)
{
    printf("Hello\n");
    return 0;
}
```

Who calls `main()`? The C runtime startup code.

## 1. Typical startup sequence

```text
+-------------------------+
| Kernel / dynamic loader |
+------------+------------+
             |
             v
+-------------------------+
| _start                  |
| Low-level process entry |
+------------+------------+
             |
             v
+-------------------------+
| C runtime startup       |
| Sets up runtime state   |
| and arguments           |
+------------+------------+
             |
             v
+-------------------------+
| main(argc, argv)        |
+------------+------------+
             |
             v
+-------------------------+
| Program termination     |
| Runtime handles exit    |
+-------------------------+
```

The exact startup symbols and functions depend on the C runtime, libc, architecture, and toolchain.

## 2. Inspect the entry point

```bash
readelf -h ./program
```

Find the `Entry point address` field.

Then inspect symbols:

```bash
nm -n ./program
```

If symbols are retained, you may see `_start` and `main`. A stripped executable may not expose their names in the ordinary symbol table.

To disassemble the startup code:

```bash
objdump -d ./program
```

## 3. Compare with bare-metal startup

For a typical Linux application:

```text
Kernel -> _start -> C runtime -> main()
```

For a typical bare-metal Cortex-M application:

```text
Reset -> vector table / reset handler
      -> copy .data
      -> clear .bss
      -> platform initialization
      -> main()
```

They share a fundamental idea: the runtime environment must be initialized before normal C application code executes.

The mechanisms differ because Linux provides a process environment, while bare-metal firmware must establish much of its runtime state itself.

---

# Lesson 27 — Disassembly: connecting ELF to machine code

You already know that `.text` contains machine instructions. Now inspect those instructions and connect them to C functions.

## 1. A simple C program

```c
int add(int a, int b)
{
    return a + b;
}

int main(void)
{
    return add(10, 20);
}
```

Compile with debug information and without optimization:

```bash
gcc -g -O0 -no-pie main.c -o program
```

Inspect the disassembly:

```bash
objdump -d program
```

To focus on one function:

```bash
objdump -d --disassemble=add program
```

You might see something conceptually like this on x86-64:

```text
0000000000401126 <add>:
  401126: push   %rbp
  401127: mov    %rsp,%rbp
  40112a: mov    %edi,-0x4(%rbp)
  40112d: mov    %esi,-0x8(%rbp)
  ...
  40113a: add    %edx,%eax
  40113c: pop    %rbp
  40113d: ret
```

The exact instructions depend on the compiler and target. This example is illustrative, not guaranteed output.

## 2. What are you looking at?

Each disassembly line typically contains:

- **Address:** where the instruction is located.
- **Machine-code bytes:** the encoded instruction.
- **Mnemonic:** the assembly instruction.
- **Operands:** registers, memory locations, or immediate values.

For example:

```text
40113a: add %edx,%eax
```

This adds the value in one register to another, following x86-64 operand conventions.

## 3. Connect addresses to source code

With debug information:

```bash
objdump -S -d program
```

This attempts to interleave source code and disassembly.

You can also use GDB:

```bash
gdb ./program
```

Inside GDB:

```gdb
break main
run
disassemble /m main
```

**Embedded connection:** the same technique works with a cross-compiled ELF. Use your target's `objdump`—for example, `arm-none-eabi-objdump`—to inspect ARM firmware, and the appropriate toolchain for other architectures.

---

# Lesson 28 — Core dumps and stack traces

A core dump captures information about a process at the time of a crash. It can preserve memory, registers, and other state useful for debugging, depending on system configuration.

Imagine this program:

```c
int main(void)
{
    int *p = 0;
    *p = 42;
    return 0;
}
```

On a typical Linux system, this invalid memory access causes a fault, usually resulting in `SIGSEGV`.

## 1. Enable core dumps

In a shell, check the current limit:

```bash
ulimit -c
```

A value of `0` means core dumps are disabled by that shell limit. For a test session, try:

```bash
ulimit -c unlimited
```

Core dump handling also depends on system settings. Some Linux systems send dumps to a service such as `systemd-coredump` instead of creating a simple `core` file in the working directory.

## 2. Inspect a core dump with GDB

If you have a core file and the matching executable:

```bash
gdb ./program core
```

Useful GDB commands:

```gdb
bt
info registers
frame 0
list
```

- `bt`: displays the stack backtrace.
- `info registers`: displays register values.
- `frame 0`: selects the top stack frame.
- `list`: displays source around the current location when debug information is available.

A conceptual stack trace might look like this:

```text
#0  crash_function() at crash.c:12
#1  process_packet() at packet.c:48
#2  main() at main.c:20
```

This suggests that `process_packet()` called `crash_function()`, which faulted. It does not automatically prove that the bug originated in the top frame; earlier memory corruption can cause a later crash.

## 3. Why the matching ELF matters

For meaningful source-level debugging, use the executable and debug information corresponding to the crashed build. If the binary or libraries differ from those used during the crash, symbols and addresses may be misleading.

**Embedded connection:** on Linux-based devices, core dumps can help diagnose crashes in user-space services. On bare-metal systems, you typically use fault handlers, register snapshots, crash logs, or a debugger instead of a conventional Linux core dump.

---

# Lesson 29 — Debugging stripped ELF files

A stripped ELF has some symbol or debugging information removed to reduce its size or limit the exposed metadata. Stripping does **not** necessarily remove all information needed to execute the program.

## 1. Compare the two files

Build a debug executable:

```bash
gcc -g main.c -o program
cp program program.debug
```

Create a stripped copy:

```bash
strip program
```

Inspect both:

```bash
file program
file program.debug

readelf -S program
readelf -S program.debug
```

The stripped file may no longer contain the full symbol table or DWARF debugging sections.

## 2. What changes for debugging?

| With debug information | Stripped executable |
|---|---|
| Source-line mappings may be available | Usually no source-line mappings |
| Local variable and type details may be available | Often unavailable |
| Function names may be available | Some names may be missing |
| Easier source-level debugging | More reliance on addresses and disassembly |

Stripping does not guarantee that every function name disappears. Dynamic symbols needed for runtime linking may remain, and other metadata can also survive.

## 3. Keep symbols separate from the deployed binary

A common workflow is to retain an unstripped ELF for debugging while deploying a smaller stripped executable.

```text
                 Build
                   |
                   v
             program ELF
               /      \
              /        \
      Keep symbols    Strip symbols
           |                |
           v                v
    Debug artifact     Deployment file
```

The debug artifact must correspond to the deployed binary. Build IDs or separate debug files can help tools associate the correct symbols with a binary.

Useful commands:

```bash
nm program.debug
nm program
readelf -n program
```

If ordinary symbols are missing, disassembly is still useful:

```bash
objdump -d program
```

**Key takeaway:** stripping makes debugging harder, not necessarily impossible. Preserve the matching debug ELF or separate debug information for production builds.

---

# Lesson 30 — Weak symbols, vector tables, and startup code

Let's connect ELF symbols to actual embedded firmware.

## 1. Weak symbols as default handlers

In embedded C, weak symbols are often used to provide default implementations that a board or application can override.

```c
__attribute__((weak))
void UART_IRQHandler(void)
{
    /* Default handler */
}
```

Another source file can define a strong version:

```c
void UART_IRQHandler(void)
{
    /* Actual UART interrupt handling */
}
```

When these definitions participate in the same link, the strong definition generally takes precedence over the weak default.

This is useful for interrupt handlers, platform hooks, and optional initialization functions.

## 2. Vector tables

On Cortex-M microcontrollers, the vector table contains the initial stack pointer and exception/interrupt handler addresses.

```text
Vector table in flash
+----------------------------+
| Initial stack pointer      |
+----------------------------+
| Reset_Handler address      |
+----------------------------+
| NMI_Handler address        |
+----------------------------+
| HardFault_Handler address  |
+----------------------------+
| ...                        |
+----------------------------+
| UART_IRQHandler address    |
+----------------------------+
```

The exact layout depends on the processor and device.

A startup file may use weak aliases so that an unimplemented interrupt handler points to a default handler. If your application supplies the corresponding strong symbol, the linker can select that implementation.

## 3. Startup symbols and linker scripts

A linker script can define symbols for startup code:

```ld
.data :
{
    _sdata = .;
    *(.data*)
    _edata = .;
} > RAM AT > FLASH

_bss_start = ADDR(.bss);
_bss_end   = ADDR(.bss) + SIZEOF(.bss);
```

These are linker-defined symbols, not ordinary C variables storing values. Startup code commonly takes their addresses to determine which memory ranges to copy or clear.

**Takeaway:** ELF symbols are not just function names for debugging. They help connect startup code, interrupt handlers, and the linker-defined memory layout.

---

# Lesson 31 — Section garbage collection

A firmware image can contain functions and data that are never used. Linker garbage collection can remove eligible unused sections and reduce image size.

## 1. Enable function/data sections

With GCC, compile functions and data into separate sections:

```bash
arm-none-eabi-gcc \
    -ffunction-sections \
    -fdata-sections \
    -c main.c -o main.o
```

Then link with:

```bash
arm-none-eabi-gcc \
    main.o \
    -Wl,--gc-sections \
    -T linker.ld \
    -o firmware.elf
```

- `-ffunction-sections`: places functions in separate sections, commonly named like `.text.function_name`.
- `-fdata-sections`: places data objects in separate sections.
- `--gc-sections`: allows the linker to discard eligible sections that are unreachable from its roots.

Exact section names and behavior depend on the toolchain.

## 2. How it works

Imagine the call graph:

```text
main()
  |
  +--> init_uart()
  |
  +--> process_data()

unused_debug_function()
unused_test_function()
```

If the unused functions are isolated in separate sections and aren't otherwise required, the linker may discard their sections.

Without fine-grained sections, multiple functions may share one section, making it harder to remove just one unused function.

## 3. A critical embedded warning

Some code or data is accessed indirectly and may not appear as a normal direct reference. Examples include:

- Interrupt vector tables
- Registration tables
- Functions referenced only by name or address through external mechanisms
- Hardware-specific startup structures

Such sections may need to be retained explicitly using linker-script directives such as `KEEP()`.

For example:

```ld
.isr_vector :
{
    KEEP(*(.isr_vector))
} > FLASH
```

The section name must match the one used by your startup code or toolchain.

Inspect what remains:

```bash
arm-none-eabi-size -A firmware.elf
arm-none-eabi-nm -n firmware.elf
```

**Takeaway:** garbage collection can shrink firmware significantly, but the linker must know which sections are genuinely required.

---

# Lesson 32 — Optimization and Link-Time Optimization (LTO)

The final ELF does not necessarily contain a direct, one-to-one representation of your C source. Compiler optimization can transform or eliminate code before and during linking.

## 1. Ordinary optimization

Compile this function:

```c
int square(int x)
{
    return x * x;
}
```

At `-O0`, the compiler typically prioritizes straightforward debugging over aggressive optimization.

At `-O2`, it may produce more efficient machine code, use fewer instructions, or keep values in registers. The exact result depends on the compiler and target.

```bash
gcc -O0 -g main.c -o program_O0
gcc -O2 -g main.c -o program_O2

size program_O0 program_O2
objdump -d program_O2
```

Optimized code can be harder to trace line by line because variables may be eliminated and functions may be inlined.

## 2. What LTO adds

**Link-Time Optimization (LTO)** allows the compiler to optimize across translation units during the link.

For example:

```c
/* math.c */
int square(int x)
{
    return x * x;
}
```

```c
/* main.c */
int square(int);

int main(void)
{
    return square(5);
}
```

Without cross-file optimization, `square()` may remain a separate function. With LTO, the compiler may inline it or simplify the overall program, depending on the code and options.

A typical GCC workflow is:

```bash
gcc -O2 -flto -c math.c -o math.o
gcc -O2 -flto -c main.c -o main.o
gcc -O2 -flto main.o math.o -o program
```

The linker must use a compatible LTO-capable toolchain/plugin.

## 3. Why embedded engineers care

LTO can:
- Reduce firmware size.
- Inline functions across source files.
- Remove unused code.
- Improve execution speed.

But it can also:
- Make source-level debugging less intuitive.
- Change function boundaries and symbol visibility in the final image.
- Expose build or toolchain compatibility issues.

For fair size comparisons, compare builds with equivalent optimization and linker settings.

**Takeaway:** the ELF describes the *result* of compilation and linking, not a literal translation of your source files.

---

# Lesson 33 — From ELF to HEX and BIN

In embedded development, your compiler and linker usually produce an ELF file, but your programmer or bootloader may expect an Intel HEX or raw binary image.

## 1. The three formats

### ELF — `firmware.elf`

Contains machine code, symbols, relocation/debug metadata, and information about program layout.

### BIN — `firmware.bin`

A raw sequence of image bytes. The file itself does not specify the load address.

### HEX — `firmware.hex`

A text-based Intel HEX representation containing data records and address information.

## 2. Convert using `objcopy`

For an ARM bare-metal toolchain:

```bash
# ELF -> raw binary
arm-none-eabi-objcopy -O binary firmware.elf firmware.bin

# ELF -> Intel HEX
arm-none-eabi-objcopy -O ihex firmware.elf firmware.hex
```

Inspect the output:

```bash
file firmware.elf firmware.bin firmware.hex
arm-none-eabi-size firmware.elf
```

## 3. Why the load address matters

Suppose your linker script places code at flash address `0x08000000`.

```text
Flash address: 0x08000000
+---------------------------+
| Vector table              |
| Machine instructions      |
| Constant data             |
| Initial values for .data  |
+---------------------------+
```

A raw `.bin` starts with the first byte of the selected image, but it does not independently tell a programmer where that byte belongs. The flashing tool must know the destination address.

Intel HEX records encode addresses, so a compatible programmer can use those addresses when programming.

**Important:** conversion does not automatically make an image suitable for every bootloader. The linker layout, image format, and flashing procedure must agree.

---

# Lesson 34 — Understand a real firmware memory layout

Let's put together the linker, ELF, and startup concepts using a small C example.

```c
#include <stdint.h>

const uint32_t magic = 0x12345678;

uint32_t counter = 10;
uint32_t error_count;

static uint8_t buffer[256];

int main(void)
{
    counter++;
    buffer[0] = 42;
    return (int)(counter + error_count + magic);
}
```

The compiler and linker will typically place these objects as follows:

| Object | Likely placement | Reason |
|---|---|---|
| `magic` | `.rodata` | Constant data |
| `counter` | `.data` | Initialized writable global |
| `error_count` | `.bss` | Zero-initialized global |
| `buffer` | `.bss` | Zero-initialized static-storage array |
| `main()` | `.text` | Machine instructions |

Exact placement depends on the compiler, linker script, optimization, and target.

## 1. Inspect the ELF

```bash
arm-none-eabi-readelf -S firmware.elf
arm-none-eabi-readelf -lW firmware.elf
arm-none-eabi-nm -n firmware.elf
arm-none-eabi-size -A firmware.elf
```

These answer different questions:

- `readelf -S`: what sections exist?
- `readelf -lW`: what loadable segments exist?
- `nm -n`: what symbols and addresses are available?
- `size -A`: how much space do sections occupy?

## 2. Follow `.data` from flash to RAM

Imagine your linker script gives `.data` a load address in flash and a runtime address in SRAM.

```text
FLASH image                         SRAM at startup

.data initial bytes                 .data runtime bytes
+------------------+                +------------------+
| counter = 10     | -- copied ---> | counter = 10     |
+------------------+                +------------------+

.bss has no initial bytes           +------------------+
in the ordinary file image          | error_count = 0  |
                                    | buffer = all 0s  |
                                    +------------------+
```

Startup code copies `.data` and clears `.bss` before calling `main()`.

That is why examining only the raw binary is not enough to understand the complete runtime memory layout.

**Takeaway:** combine the linker script, ELF sections, program headers, and startup code to understand where firmware actually lives and how it initializes.

---

# Lesson 35 — Hands-on ELF investigation

Let's finish with a small experiment you can run on Linux or in your cross-compilation environment.

## 1. Create two C files

`main.c`

```c
int add(int a, int b);

int global_value = 7;
int zero_value;

int main(void)
{
    return add(global_value, zero_value);
}
```

`add.c`

```c
int add(int a, int b)
{
    return a + b;
}
```

## 2. Compile separately

For a native Linux build:

```bash
gcc -g -O0 -c main.c -o main.o
gcc -g -O0 -c add.c -o add.o
```

Now `main.o` references `add()`, but it doesn't contain its definition.

Inspect it:

```bash
nm main.o
readelf -r main.o
```

Look for:
- `U add` in the symbol table.
- A relocation associated with the call to `add()`.

The precise relocation type depends on the architecture and compiler.

## 3. Link the files

```bash
gcc -g -O0 main.o add.o -o program
```

Inspect the resulting executable:

```bash
readelf -h program
readelf -S program
readelf -lW program
nm -n program
objdump -d program
```

Now you can connect the concepts:

```text
main.c + add.c
      |
      v
main.o + add.o
      |
      | symbols + relocation entries
      v
     LINKER
      |
      v
  program (ELF)
      |
      +--> Header: file type and entry point
      |
      +--> Sections: .text, .data, .bss, ...
      |
      +--> Program headers: loadable segments
      |
      +--> Symbols: functions and variables
      |
      +--> Machine code: inspect with objdump
```

## 4. Try a linker map

Re-link with a map file:

```bash
gcc -g -O0 main.o add.o \
    -Wl,-Map=program.map \
    -o program
```

Open `program.map` and search for `main`, `add`, `.text`, and `.data`.

This helps you see how object files contribute to the final executable and where the linker places their contents.

For your bare-metal project, use the corresponding cross-compiler, linker script, and tools instead of the native `gcc` commands.

---

# Final recap — Lessons 21–35

| Lesson | Main topic |
|---|---|
| 21 | Runtime relocation calculations |
| 22 | Dynamic symbol resolution and visibility |
| 23 | Symbol versioning and ABI compatibility |
| 24 | `execve()` and the kernel's ELF loading path |
| 25 | Shared-library lookup and missing dependencies |
| 26 | `_start()`, C runtime startup, and `main()` |
| 27 | Disassembly and mapping machine code to C |
| 28 | Core dumps and stack traces |
| 29 | Debugging stripped ELF files |
| 30 | Weak symbols, vector tables, and startup symbols |
| 31 | Linker section garbage collection |
| 32 | Compiler optimization and LTO |
| 33 | ELF-to-HEX/BIN conversion |
| 34 | Firmware memory layout investigation |
| 35 | Hands-on ELF inspection from C to executable |

## The big picture

```text
C source
   |
   v
Compiler -> object files (.o)
                |
                | symbols + relocations
                v
             Linker
                |
                v
             ELF file
             /      \
            /        \
   Inspect/debug     Convert image
   readelf/nm/       ELF -> BIN/HEX
   objdump/GDB
```

- **Compiler:** turns source into object files.
- **Linker:** combines objects, resolves symbols, and applies static relocations.
- **ELF metadata:** describes the file and its contents.
- **Loader:** maps loadable segments and, when needed, coordinates dynamic linking.
- **Debugger:** uses symbols and debug information to relate machine execution to source code.
- **Bare-metal startup:** uses the linked image and startup code to initialize RAM and enter the application.
