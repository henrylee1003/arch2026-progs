# Quiz 3 Problem C in RV32I: op1, op2, op3 and the Q11.20 multiply.
# All four are leaf routines and use only caller-saved registers.

    .text

# uint32_t op1_asm(uint32_t a, uint32_t b, int *n)
# Two passes per loop trip, so the pass count costs one addi per two passes.
    .globl  op1_asm
    .type   op1_asm, @function
    .align  2
op1_asm:
    li      t1, 0
    beqz    a1, 2f
1:  and     t0, a0, a1
    xor     a0, a0, a1
    slli    a1, t0, 1
    beqz    a1, 3f
    and     t0, a0, a1
    xor     a0, a0, a1
    slli    a1, t0, 1
    addi    t1, t1, 2
    bnez    a1, 1b
2:  sw      t1, 0(a2)
    ret
3:  addi    t1, t1, 1
    sw      t1, 0(a2)
    ret
    .size   op1_asm, .-op1_asm

# uint32_t op2_asm(uint32_t a, uint32_t b)
    .globl  op2_asm
    .type   op2_asm, @function
    .align  2
op2_asm:
    sub     a0, a0, a1
    ret
    .size   op2_asm, .-op2_asm

# uint32_t op3_asm(uint32_t a, uint32_t b)
# a + b = 2 (a & b) + (a ^ b), so floor((a + b) / 2) = (a & b) + ((a ^ b) >> 1)
# with no carry out of bit 31.
    .globl  op3_asm
    .type   op3_asm, @function
    .align  2
op3_asm:
    and     t0, a0, a1
    xor     a0, a0, a1
    srli    a0, a0, 1
    add     a0, a0, t0
    ret
    .size   op3_asm, .-op3_asm

# int32_t q_mul_asm(int32_t a, int32_t b)
# Multiply the magnitudes by shift-and-add, taking the smaller one as the
# multiplier so the loop stops after its highest set bit. The 64-bit product
# is kept in a3:a2 and the shifted multiplicand in a4:a0. Then negate if the
# signs differ, add 2^19 and return bits 20..51.
    .globl  q_mul_asm
    .type   q_mul_asm, @function
    .align  2
q_mul_asm:
    xor     t2, a0, a1          # bit 31: sign of the product
    srai    t0, a0, 31
    xor     a0, a0, t0
    sub     a0, a0, t0          # |a|
    srai    t0, a1, 31
    xor     a1, a1, t0
    sub     a1, a1, t0          # |b|
    bgeu    a0, a1, 1f          # multiplier a1 = the smaller magnitude
    xor     a0, a0, a1
    xor     a1, a0, a1
    xor     a0, a0, a1
1:  li      a2, 0
    li      a3, 0
    li      a4, 0
    beqz    a1, 4f
2:  andi    t0, a1, 1
    beqz    t0, 3f
    add     a2, a2, a0
    sltu    t1, a2, a0
    add     a3, a3, a4
    add     a3, a3, t1
3:  srli    t1, a0, 31
    slli    a4, a4, 1
    or      a4, a4, t1
    slli    a0, a0, 1
    srli    a1, a1, 1
    bnez    a1, 2b
4:  bgez    t2, 5f
    seqz    t1, a2              # 64-bit negate
    not     a3, a3
    add     a3, a3, t1
    neg     a2, a2
5:  lui     t0, 0x80            # 2^19
    add     a2, a2, t0
    sltu    t1, a2, t0
    add     a3, a3, t1
    srli    a2, a2, 20
    slli    a3, a3, 12
    or      a0, a2, a3
    ret
    .size   q_mul_asm, .-q_mul_asm
