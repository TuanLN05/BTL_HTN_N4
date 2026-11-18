# FreeRTOS Kernel

Vendored from https://github.com/FreeRTOS/FreeRTOS-Kernel at tag V11.2.0
(commit 0adc196d4bd52a2d91102b525b0aafc1e14a2386).

This project includes the kernel headers, tasks.c, queue.c, list.c, the
GCC Cortex-M3 port, and heap_4.c. Upstream source files are unmodified;
see LICENSE.md for the MIT license. library.json selects the sources
for PlatformIO. Application configuration is in src/FreeRTOSConfig.h
at the project root.

Additional kernel modules must be copied from the same upstream version
and added to library.json before using their APIs.
