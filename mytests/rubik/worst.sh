#!/bin/bash
# Usage: mytests/rubik/worst.sh [O1|O2|O3|Os]
# Sweep every state on rv32emu with mytests/rubik/worst.c, linked against the
# objects of the current build. Takes about 15 minutes.
set -e
cd "$(dirname "$0")/../.."
o=${1:-O2}
make -s elfs >/dev/null
B=build/mytests
mkdir -p $B
CC=${CROSS_COMPILE:-riscv-none-elf-}gcc
LD=${CROSS_COMPILE:-riscv-none-elf-}ld
$CC -march=rv32i_zicsr -mabi=ilp32 -ffreestanding -fno-pic -O2 -Wall -Wextra \
    -iquote . -Icommon -c mytests/rubik/worst.c -o $B/worst.o
$LD -T common/linker.ld --no-warn-rwx-segments -o $B/worst.elf \
    build/common/start.o build/common/perf.o build/common/rt.o \
    build/common/print.o build/rubik/$o/impl.o build/rubik/impl_asm.o \
    $B/worst.o
third_party/rv32emu/build/rv32emu $B/worst.elf
