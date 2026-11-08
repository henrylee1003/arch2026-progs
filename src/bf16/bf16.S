# BFloat16 narrowing, multiply and square root in RV32I (Quiz 1 Problem B).
# Same results as bf16.c for every input. Leaf routines, caller-saved
# registers only.

    .text

# uint16_t bf16_from_f32_asm(uint32_t f32bits)
    .globl  bf16_from_f32_asm
    .type   bf16_from_f32_asm, @function
    .align  2
bf16_from_f32_asm:
    lui     t1, 0x7f800         # exponent field of binary32
    and     t0, a0, t1
    beq     t0, t1, 1f          # Inf or NaN: no rounding
    srli    t0, a0, 16
    andi    t0, t0, 1
    add     a0, a0, t0
    lui     t1, 8
    addi    t1, t1, -1          # 0x7fff
    add     a0, a0, t1
1:  srli    a0, a0, 16
    ret
    .size   bf16_from_f32_asm, .-bf16_from_f32_asm

# MUL_BIT k: if bit k of t3 is set, add t2 << k to a3.
.macro MUL_BIT k
    andi    t5, t3, 1 << \k
    beqz    t5, 1f
.if \k
    slli    t5, t2, \k
    add     a3, a3, t5
.else
    add     a3, a3, t2
.endif
1:
.endm

# uint16_t bf16_mul_asm(uint16_t a, uint16_t b)
# t0/t1 exponents, t2/t3 significands, a2 exponent adjustment for denormals,
# a3 product, t6 result sign.
    .globl  bf16_mul_asm
    .type   bf16_mul_asm, @function
    .align  2
bf16_mul_asm:
    xor     t6, a0, a1
    srli    t6, t6, 15
    slli    t6, t6, 15
    srli    t0, a0, 7
    andi    t0, t0, 0xff
    srli    t1, a1, 7
    andi    t1, t1, 0xff
    andi    t2, a0, 0x7f
    andi    t3, a1, 0x7f
    li      t4, 0xff
    bne     t0, t4, 1f          # a is Inf or NaN
    bnez    t2, m_ret
    or      t5, t1, t3
    beqz    t5, m_nan
    j       m_inf
1:  bne     t1, t4, 2f          # b is Inf or NaN
    bnez    t3, m_ret_b
    or      t5, t0, t2
    beqz    t5, m_nan
    j       m_inf
2:  or      t5, t0, t2
    beqz    t5, m_zero
    or      t5, t1, t3
    beqz    t5, m_zero
    li      a2, 0
    bnez    t0, 4f
3:  slli    t2, t2, 1           # normalize a denormal a
    addi    a2, a2, -1
    andi    t5, t2, 0x80
    beqz    t5, 3b
    li      t0, 1
    j       5f
4:  ori     t2, t2, 0x80
5:  bnez    t1, 7f
6:  slli    t3, t3, 1           # normalize a denormal b
    addi    a2, a2, -1
    andi    t5, t3, 0x80
    beqz    t5, 6b
    li      t1, 1
    j       8f
7:  ori     t3, t3, 0x80
8:  li      a3, 0               # 8 x 8 product, bit 7 of t3 is always set
    MUL_BIT 0
    MUL_BIT 1
    MUL_BIT 2
    MUL_BIT 3
    MUL_BIT 4
    MUL_BIT 5
    MUL_BIT 6
    slli    t5, t2, 7
    add     a3, a3, t5
    add     a4, t0, t1
    addi    a4, a4, -127
    add     a4, a4, a2          # result exponent
    srli    t5, a3, 15
    beqz    t5, 9f
    srli    a3, a3, 8           # product reached bit 15
    addi    a4, a4, 1
    j       10f
9:  srli    a3, a3, 7
10: andi    a3, a3, 0x7f        # truncated fraction
    bge     a4, t4, m_inf
    blez    a4, 11f
    slli    a4, a4, 7
    or      a0, t6, a4
    or      a0, a0, a3
    ret
11: li      t5, -6
    blt     a4, t5, m_zero
    li      t5, 1
    sub     t5, t5, a4
    srl     a3, a3, t5          # denormal result
    or      a0, t6, a3
    ret
m_inf:
    li      t5, 0x7f80
    or      a0, t6, t5
    ret
m_zero:
    mv      a0, t6
    ret
m_nan:
    li      a0, 0x7fc0
    ret
m_ret_b:
    mv      a0, a1
m_ret:
    ret
    .size   bf16_mul_asm, .-bf16_mul_asm

# SQRT_STEP: one digit of the bit-by-bit integer square root of t4.
# t5 = root so far, t3 = current power of four.
.macro SQRT_STEP
    add     t6, t5, t3
    srli    t5, t5, 1
    bltu    t4, t6, 1f
    sub     t4, t4, t6
    or      t5, t5, t3
1:  srli    t3, t3, 2
.endm

# uint16_t bf16_sqrt_asm(uint16_t a)
# The C version searches for the largest r with (r * r) / 128 <= m. That r is
# the integer square root of 128 * m + 127, found here digit by digit with no
# multiply. For a normal input m is in [128, 511], so r is in [128, 255] and
# the new exponent in [64, 190]: no renormalization, overflow or underflow.
    .globl  bf16_sqrt_asm
    .type   bf16_sqrt_asm, @function
    .align  2
bf16_sqrt_asm:
    srli    t0, a0, 7
    andi    t0, t0, 0xff        # exponent
    andi    t1, a0, 0x7f        # fraction
    srli    t2, a0, 15          # sign
    li      t3, 0xff
    bne     t0, t3, 1f
    bnez    t1, 3f              # NaN: unchanged
    bnez    t2, 2f              # -Inf: NaN
    ret                         # +Inf: unchanged
1:  or      t4, t0, t1
    beqz    t4, 4f              # +0 or -0: +0
    bnez    t2, 2f              # negative: NaN
    beqz    t0, 4f              # denormal: +0
    addi    t0, t0, -127
    ori     t1, t1, 0x80
    andi    t4, t0, 1
    beqz    t4, 5f
    slli    t1, t1, 1           # odd exponent: double m
    addi    t0, t0, -1
5:  srai    t0, t0, 1
    addi    t0, t0, 127         # new exponent
    slli    t4, t1, 7
    addi    t4, t4, 127         # 128 * m + 127
    li      t5, 0
    lui     t3, 4               # 1 << 14
    SQRT_STEP
    SQRT_STEP
    SQRT_STEP
    SQRT_STEP
    SQRT_STEP
    SQRT_STEP
    SQRT_STEP
    SQRT_STEP
    andi    t5, t5, 0x7f
    slli    t0, t0, 7
    or      a0, t0, t5
3:  ret
2:  li      a0, 0x7fc0
    ret
4:  li      a0, 0
    ret
    .size   bf16_sqrt_asm, .-bf16_sqrt_asm
