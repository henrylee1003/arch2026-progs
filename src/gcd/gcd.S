# gcd_u32_asm: binary GCD without a divider, hand-written RV32I.
#
# uint32_t gcd_u32_asm(uint32_t a, uint32_t b)
#                                         a0 = a, a1 = b; result in a0
#
# Stein's algorithm: the common power of two k = ctz(a | b) is set aside, both
# operands are made odd, and gcd(a, b) = gcd(|a - b|, min(a, b)) runs until the
# difference is zero. Three choices make it cheaper than the compiled C:
#   * ctz by table: ctz_tab[x & 255] strips up to seven factors of two with
#     andi/add/lbu/srl; an all-zero low byte (entry 8) repeats the step;
#   * no swap: the loop exists twice, loop_b with the difference in b and
#     loop_a with it in a, so "swap" is a jump to the other copy instead of
#     three moves;
#   * the difference of two odd numbers is even and nonzero until the end, so
#     each subtraction goes straight into the strip of the operand it wrote.
# Leaf routine: only caller-saved registers, no stack.

    .section .rodata
    .align  2
# ctz_tab[i]: trailing zeros of i for i = 1..255, and 8 for i = 0.
ctz_tab:
    .byte 8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
    .byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0

# STRIP reg: reg >>= ctz(reg) for a nonzero reg. t2 = ctz_tab, t3 = 8.
.macro STRIP reg
1:  andi    t1, \reg, 255
    add     t1, t2, t1
    lbu     t1, 0(t1)
    srl     \reg, \reg, t1
    beq     t1, t3, 1b
.endm

    .text

    .globl  gcd_u32_asm
    .type   gcd_u32_asm, @function
    .align  2
gcd_u32_asm:
    beqz    a0, ret_b           # gcd(0, b) = b
    beqz    a1, ret_a           # gcd(a, 0) = a
    la      t2, ctz_tab
    li      t3, 8
    or      t4, a0, a1          # k = ctz(a | b), counted in t5
    li      t5, 0
1:  andi    t1, t4, 255
    add     t1, t2, t1
    lbu     t1, 0(t1)
    add     t5, t5, t1
    srl     t4, t4, t1
    beq     t1, t3, 1b
    STRIP   a0                  # a odd from here on

loop_b:                         # b holds the newest value: make it odd
    STRIP   a1
    bltu    a1, a0, a_bigger
    sub     a1, a1, a0          # b >= a: b = b - a (even, or 0 when equal)
    bnez    a1, loop_b
    sll     a0, a0, t5          # gcd = a << k
ret_a:
    ret
a_bigger:
    sub     a0, a0, a1          # a > b: a = a - b, even and nonzero
loop_a:                         # the same loop with a and b exchanged
    STRIP   a0
    bltu    a0, a1, b_bigger
    sub     a0, a0, a1
    bnez    a0, loop_a
    sll     a0, a1, t5          # gcd = b << k
    ret
b_bigger:
    sub     a1, a1, a0
    j       loop_b
ret_b:
    mv      a0, a1
    ret
    .size   gcd_u32_asm, .-gcd_u32_asm
