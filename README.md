# Android Red Team Kernel Toolkit

`art-kernel-toolkit` is a kernel module that can be used to perform actions
requiring kernel privileges from userspace. It supports x86_64 and arm64 Android
Common Kernels and Linux kernels.

Example use cases:

- Setting up the right conditions to trigger kernel bugs. You may see a bug that
  requires specific conditions to be triggered, such as a particular heap
  layout. You can use the `kmalloc` plugin to do this from userspace, so you can
  get to reproducing the actual bug faster before investing more time in a full
  exploit.
- As a placeholder step in an exploit chain. If you're working on an exploit for
  a bug but don't have an information leak yet, use the `vmem` plugin as
  placeholder for this step in your exploit chain until you find a real
  information leak.
- Testing functionality at higher privilege levels. For example, on ARM systems
  only EL1 has the privilege to make HVC/SMC calls that are handled by EL2 and
  EL3. You can use the `hvc` and `smc` plugins to make these privileged calls
  from userspace.

Contributions are welcome, see [CONTRIBUTING.md](CONTRIBUTING.md).

This is not an officially supported Google product.

---

- [Clone and Build](#clone-and-build)
- [Installing](#installing)
- [Usage](#usage)
  - [Plugins](#plugins)
    - [vmem: Read/Write Virtual Memory](#vmem)
    - [pmem: Read/Write Physical Memory](#pmem)
    - [addr: Virt to Phys and Phys to Virt Translation](#addr)
    - [kaslr: Find KASLR Offset](#kaslr)
    - [kallsyms: Lookup Symbol Addresses](#kallsyms)
    - [kmalloc: Allocate/Free Memory](#kmalloc)
    - [asm: Execute Arbitrary Assembly](#asm)
    - [msr: Read/Write MSRs](#msr)
    - [smc: Execute SMCs](#smc)
    - [hvc: Execute HVCs](#hvc)
- [Security](#security)

## Clone and Build

Clone this repository with

```bash
git clone https://github.com/androidoffsec/art-kernel-toolkit
```

Next, clone and build the Linux kernel or the Android Common Kernel that you
want to use `art-kernel-toolkit` with. Then run `make` with the `KERNEL_SRC`
environment variable set to your build output directory:

```bash
KERNEL_SRC=/path/to/linux/build make
```

To cross compile for arm64:

```bash
KERNEL_SRC=/path/to/linux/build make \
    CC=clang \
    ARCH=arm64 \
    CROSS_COMPILE=aarch64-linux-gnu-
```

You can also generate a `compile_commands.json` for use with `clangd` (make sure
to set the same variables used when building):

```bash
KERNEL_SRC=/path/to/linux/build make compile_commands.json \
    CC=clang \
    ARCH=arm64 \
    CROSS_COMPILE=aarch64-linux-gnu-
```

## Installing

The easiest way to load the module is with `insmod`:

```bash
insmod /path/to/art-kernel-toolkit.ko
```

You can also set the `INSTALL_MOD_PATH` variable and run the `modules_install`
target to install it into the `/lib/modules` folder that will be packaged into
your rootfs:

```bash
INSTALL_MOD_PATH=/path/to/rootfs KERNEL_SRC=/path/to/linux/build make
```

Then the module can be loaded from this rootfs with:

```bash
modprobe art-kernel-toolkit
```

You can remove the module with:

```bash
rmmod art-kernel-toolkit
```

Note that the module will appear as `art_kernel_toolkit` in lsmod:

```bash
$ lsmod | grep art
art_kernel_toolkit 40960 0`
```

### Installing on Android

For Android, you can push the module to your device and loaded it with the
following commands:

```bash
adb root
adb push art-kernel-toolkit.ko /data/local/tmp
adb shell insmod /data/local/tmp/art-kernel-toolkit.ko
```

## Usage

The module creates files in `debugfs`, which is mounted under
`/sys/kernel/debug`. If this is not mounted by default, you can mount it
manually with:

```bash
mount -t debugfs none /sys/kernel/debug
```

On many physical Android devices, `/d/` is a symlink to /sys/kernel/debug. If
that path does not exist, you can optionally create it with:

```bash
ln -s /sys/kernel/debugfs /d
```

You should now see the kernel driver files in `/sys/kernel/debug/art/` and
`/d/art/`. Each folder in this directory is created by a "plugin", documented
below. Reading from or writing to these files will trigger various plugin
actions.

### Plugins

Each plugin defines a set of files in its own directory under `/d/art`. `/d/art`
will usually be omitted in the documentation when referring to plugin files.
Most plugins contain a `help` file in their plugin folder, which contains usage
instructions for the plugin.

Writeable files take a list of space separated arguments. For example, if a file
is documented as "`foo/file <val1> <val2>` (RW)", you can write two arguments to
this file with `echo my_val1 my_val2 > /d/art/foo/file`. Unless otherwise
specified, you can assume that integer arguments can be written in decimal, hex
(with a '0x' prefix), or octal (with a leading '0').

Readable files will be documented as having a "return" value which can be read
from the file, i.e. by running `cat /d/art/foo/file`. Most return values will be
in hex.

#### vmem

This plugin allows reading/writing from arbitrary kernel virtual memory.

Files:

- `vmem/addr <addr>` (RW)
  - `addr`: the virtual address to read from or write to
  - Returns: the last address written when read
- `vmem/val <val>` (RW)
  - `val`: the 64-bit value to write to the address specified in `vmem/addr`
  - Returns: the 64-bit value at the address specified in`vmem/addr`

##### Example

```bash
# Write address to write to or read from to `addr`
$ echo 0xffffffc009fa2378 > /d/art/vmem/addr

# Read from `val` to read 64-bit value at address
$ cat /d/art/vmem/val
0xffffff80038db270

# Write 64-bit value to `val` to write to address
$ echo 0xdeadbeef > /d/art/vmem/val

# Confirm write succeeded
$ cat /d/art/vmem/val
0xdeadbeef
```

#### pmem

This plugin allows reading/writing from arbitrary physical memory.

Files:

- `pmem/addr <addr>` (RW)
  - `addr`: the physical address to read from or write to
  - Returns: the last address written when read
- `pmem/val <val>` (RW)
  - `val`: the 64-bit value to write to the address specified in `pmem/addr`
  - Returns: the 64-bit value at the address specified in `pmem/addr`
- `pmem/bytes <byte_str>` (RW)
  - `byte_str`: a raw string of bytes to write to the address specified in
    `pmem/addr`
  - Returns: the raw bytes at the address specified in `pmem/addr`
- `pmem/bytes-read-size <max_length>` (RW)
  - `max_length`: the maximum length to allow for reads from `pmem/bytes`
    (defaults to 8). Setting this is optional if you will be manually reading
    the exact number of bytes you want from `pmem/bytes`. However, when using
    tools like `cat` or even `xxd` with the `-l` argument, this value should be
    specified to avoid reading out of bounds.
  - Returns: the last value written to `pmem/bytes-read-size`

##### Example

```bash
# Write address to write to or read from to `addr`
$ echo 0xB62CE0DC > /d/art/pmem/addr

# Write a value in base 10 to physical address:
$ echo 12345678 > /d/art/pmem/val

# Write a string to physical address:
$ echo -n 'helloworld' > /d/art/pmem/bytes

# Write hex bytes to a physical address
$ echo -n '56 67 89 ab cd ef' | xxd -r -p > /d/art/pmem/bytes

# Read five bytes from a physical address and output as hex:
$ echo 5 > /d/art/pmem/bytes-read-size
$ xxd -p /d/art/pmem/bytes 566789abcd
```

#### addr

Allows converting virtual address to and from physical addresses.

Files:

- `addr/va <va>` (RW)
  - `va`: the virtual address you want to convert to a physical address
  - Returns: the virtual address of the last address written to either `addr/va`
    or `addr/pa`
- `addr/pa <pa>` (RW)
  - `pa`: the physical address you want to convert to a virtual address
  - Returns: the physical address of the last address written to `addr/va` or
    `addr/pa`

##### Example

```bash
$ echo 0xffffff800468b400 > /d/art/addr/va
$ cat /d/art/addr/pa
0x4468b400

$ echo 0x4468b400 > /d/art/addr/pa
$ cat /d/art/addr/va
0xffffff800468b400
```

#### kaslr

Allows finding the KASLR offset. Currently only implemented for arm64.

Files:

- `kaslr/offset` (R)
  - Returns: the KASLR offset

##### Example

```bash
$ cat /d/art/kaslr/offset
0x1be5600000
```

#### kallsyms

This plugin allows looking up addresses for kernel symbols, allowing you to
determine these addresses even if `kptr_restrict` is set to 2 (which prevents
addresses from being seen in `/proc/kallsyms`).

Files:

- `kallsyms/lookup_name <sym_name>` (RW)
  - `sym_name`: name of the symbol you want to lookup
  - Returns: the last symbol name written to this file
- `kallsyms/addr` (R)
  - Returns: the address of the last symbol written to `kallsyms/lookup_name`

##### Example

```bash
# Lookup address of __sys_setuid
$ echo __sys_setuid > /d/art/kallsyms/lookup_name
$ cat /d/art/kallsyms/addr
0xffffffedb417da48
```

#### kmalloc

This plugin allows calling `kmalloc` and `kfree`.

Files:

- `kmalloc/alloc <size>` (W)
  - `size`: size of memory in bytes to allocate
- `kmalloc/free <addr>` (W)
  - `addr`: address to call `kfree` on
- `kmalloc/va` (R)
  - Returns: the virtual address of the last allocated chunk
- `kmalloc/pa` (R)
  - Returns: the physical address of the last allocated chunk
- `kmalloc/pfn` (R)
  - Returns: the page frame number of the last allocated chunk
- `kmalloc/size` (R)
  - Returns: the size of the last allocated chunk

##### Example

```bash
# Allocate 1024 bytes
$ echo 0x400 > /d/art/kmalloc/alloc

$ cat /d/art/kmalloc/size
0x400

$ cat /d/art/kmalloc/va
0xffffff8004048000

$ cat /d/art/kmalloc/pa
0x44048000

$ cat /d/art/kmalloc/pfn
0x44048

# Free allocated memory
$ echo $(cat /d/art/kmalloc/va) > /d/art/kmalloc/va
```

#### asm

Allows executing arbitrary assembly instructions. Only available on arm64.

Files:

- `asm/asm <asm_byte_str>` (W)
  - `asm_byte_str`: the raw byte string of compiled assembly code to execute.
    You do not need to add a `ret` instruction to your code as it is added for
    you, and you do not need to worry about preserving the value of any
    registers except for the stack pointer. You should make sure your code does
    not corrupt any stack frames in the call stack. The assembly will
    immediately be executed after writing to this file.
- `asm/cpumask <mask>` (RW)
  - `mask`: a bitmask choosing which CPU to run the code on (defaults to 1,
    meaning CPU 0). Exactly one bit of this bitmask must be set, to run the same
    code on multiple CPUs, you will need to write to `asm/asm` once per CPU,
    changing the mask in between writes.
  - Returns: the current CPU mask.
- `asm/x0` to `asm/x28` (R)
  - Returns: the value of the corresponding register when the assembly finished
    executing.

##### Example

```bash
# mov x0, 042; mov x9, 42; mov x28, 0x42
$ echo "400480d2490580d25c0880d2" | xxd -r -p > /d/art/asm/asm

$ cat /d/art/asm/x0
0x0000000000000022

$ cat /d/art/asm/x9
0x000000000000002a

$ cat /d/art/asm/x28
0x0000000000000042
```

#### msr

Allows reading/writing model-specific registers (MSRs). Only available on arm64.

Files:

- `msr/msr <value>` (RW)
  - `value`: the value to write to the MSR specified in `msr/regname`.
  - Returns: the current value of the MSR specified in `msr/regname` on the CPU
    specified in `msr/cpumask` when read.
- `msr/regname <regname>` (RW)
  - `regname`: the name of the MSR to read or write. Some common MSR names such
    as `sctlr_el1` are defined. For MSRs where the common name is not defined,
    use the encoded register form `s<op0>_<op1>_<CRn>_<CRm>_<op2>`. Writing to
    this file will change the values of `msr/op0`, `msr/op1`, `msr/CRn`,
    `msr/CRm`, `msr/op2`. You can also write to those files as an alternative to
    writing to this file.
  - Returns: the encoded MSR name in the form `s<op0>_<op1>_<CRn>_<CRm>_<op2>`.
- `msr/cpumask <mask>` (RW)
  - `mask`: a bitmask choosing which CPU to run the code on (defaults to 1,
    meaning CPU 0). Exactly one bit of this bitmask must be set when reading MSR
    values, although multiple bits may be set when writing MSR values.
  - Returns: the current CPU mask.
- `msr/op0 <op0>`, `msr/op1 <op1>`, `msr/CRn <CRn>`, `msr/CRm <CRm>`,
  `msr/op2 <op2>` (RW)
  - `op0`, `op1`, `CRn`, `CRm`, `op2`: Sets the corresponding component of the
    MSR encoding. Writing to these files will change the output of
    `msr/regname`.
  - Returns: the corresponding component of the encoding of the currently
    selected MSR

Note: The value written to `msr/regname` can be a common MSR name (if defined,
check the source for the full list) or an encoded register name. However, the
parsing for the encoded register name is intentionally not strict. It is case
insensitive, and any non-numeric characters are replaced with spaces. As long as
five space-separate numeric values remain, it will successfully be parsed. See
the examples section for more details.

##### Example

```bash
# Read SCTLR_EL1
$ echo sctlr_el1 > /d/art/msr/regname
$ cat /d/art/msr/regname
s3_0_c1_c0_0
$ cat /d/art/msr/msr
0x200000034f4d91d

# Set cpumask to CPU 0 and CPU 1
$ echo 0x3 > /d/art/msr/cpumask

# Disable EPAN and SPAN on CPU 0 and CPU 1
$ echo 0x3474d91d > /d/art/msr/msr

# Set CPU mask back to individual CPUs when reading
$ echo 0x1 > /d/art/msr/cpumask

# EPAN and SPAN are now unset on CPU 0 and CPU 1
$ cat /d/art/msr/msr
0x3474d91d

$ echo 0x2 > /d/art/msr/cpumask
$ cat /d/art/msr/msr
0x3474d91d

# SCTLR_EL1 is unchanged on CPU 2
$ echo 0x4 > /d/art/msr/cpumask
$ cat /d/art/msr/msr
0x200000034f4d91d
```

You can set individual components of the register encoding instead of setting
`msr/regname`:

```bash
$ echo 3 > /d/art/msr/op0
$ echo 1 > /d/art/msr/op1
$ echo 0 > /d/art/msr/CRn
$ echo 0 > /d/art/msr/CRm
$ echo 4 > /d/art/msr/op2
$ cat /d/art/msr/regname
s3_1_c0_c0_4
```

You can write an encoded MSR name to `msr/regname`, taking advantage of the
loose parsing. All of the following commands are equivalent:

```bash
$ echo s3_1_c0_c0_4 > /d/art/msr/regname
$ echo 3_1_0_0_4 > /d/art/msr/regname
$ echo 3 1 0 0 4 > /d/art/msr/regname
```

#### smc

Allows making SMC (supervisor) calls from userspace. Only available on arm64.

Files:

- `smc/cmd [x0] [x1] [x2] [x3] [x4] [x5] [x6] [x7]` (RW)
  - `x0` to `x7`: the values to set for the registers before executing an `smc`
    instruction. If a register is not specified, it is assumed to be zero
  - Returns: the last command string written to `smc/cmd`
- `smc/result` (R)
  - Returns: four space-separated hex integers, representing the values of `x0`
    to `x4` after the SMC has completed

##### Example

```bash
# Execute SMCCC_VERSION with some unused arguments in different numeric formats
# Supports up to 8 arguments including SMC ID
$ echo 0x80000000 0xdeadbeef 0777 42 > /d/art/smc/cmd

# Result is SMC Version 1.2, unused arguments are returned as is (in hex)
$ cat /d/art/smc/result
0x10002 0xdeadbeef 0x1ff 0x2a
```

#### hvc

Similar to the `smc` plugin, but for HVC (hypervisor) calls. Only available on
arm64.

Files:

- `hvc/cmd [x0] [x1] [x2] [x3] [x4] [x5] [x6] [x7]` (RW)
  - `x0` to `x7`: the values to set for the registers before executing an `hvc`
    instruction. If a register is not specified, it is assumed to be zero
  - Returns: the last command string written to `hvc/cmd`
- `hvc/result` (R)
  - Returns: four space-separated hex integers, representing the values of `x0`
    to `x4` after the SMC has completed

## Security

Installing this kernel module gives userspace applications kernel privileges. It
is intended to only be used in virtual machines or test devices. Because of
this, there is no additional security risk added by a buggy implementation of a
plugin. For example, if a plugin contains a buffer overflow feel free to submit
a PR fixing it, but do not open an issue, request a CVE, or submit the issue to
any bug bounty programs.
