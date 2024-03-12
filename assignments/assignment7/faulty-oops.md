# Kernel Opps Analysis

```console
# echo “hello_world” > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x0000000096000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041bab000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 165 Comm: sh Tainted: G           O      5.15.109 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d3bd80
x29: ffffffc008d3bd80 x28: ffffff8001bd9980 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 0000000000000012 x21: 0000005592ba2a70
x20: 0000005592ba2a70 x19: ffffff8001b02900 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d3bdf0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xb0/0xc0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace dd91cfaffca558c5 ]---
```

## What Happened?
Virtual address `0000000000000000` gives us a hint that the reason for the kernel "opps" was because of a NULL pointer reference

```console
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
```

## Where In Code Did It Occur?
Occurred in module faulty 0x10 (16) bytes into the function `faulty_write`, which is 0x20 (32) bytes long.

```console
pc : faulty_write+0x10/0x20 [faulty]
```

## Object Dump
```console
qrogers@ubuntu:~/aesd/assignment-4-qrogers6/buildroot/output$ host/bin/aarch64-linux-objdump -S target/lib/modules/5.15.109/extra/faulty.ko
```
```console
0000000000000000 <faulty_write>:
   0: d2800001  mov x1, #0x0                    // #0
   4: d2800000  mov x0, #0x0                    // #0
   8: d503233f  paciasp
   c: d50323bf  autiasp
  10: b900003f  str wzr, [x1]
  14: d65f03c0  ret
  18: d503201f  nop
  1c: d503201f  nop

```

The faulty_write is referenced
1) The value 0 is moved into x1
2) The store of the x1 value into the x1 register that we previously set to zero is going to cause that NULL pointer exception.

```
0: d2800001  mov x1, #0x0                    // #0
.
.
10: b900003f  str wzr, [x1]
```
