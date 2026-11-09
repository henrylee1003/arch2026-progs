# rubik_solve_asm: optimal 2x2x2 cube solver, hand-written RV32I.
#
# int rubik_solve_asm(const char *state, char *out, size_t cap)
#                                   a0 = state, a1 = out, a2 = cap; result in a0
#
# Same search as rubik.c: iterative deepening bounded by the two pattern
# databases, going d - 5 moves deep and looking the state up in the table of
# all states within 5 moves (tables.S). Coordinates are kept doubled, so a
# table lookup is add + load. The node being expanded lives in registers; a
# frame is written only when the search goes one level deeper.
#
# Registers during the search:
#   s0-s4  tables: move (perm, twist), distance (perm, twist), ball
#   s5     bound d          s6  budget: largest h a child may have (d - g - 1)
#   s7     frame pointer    s8  &path[g]
#   s9/s10 node (perm, twist)   s11 face of the move into the node (-1 at root)
#   a3     face  a2 3 * face    a4/a5 child    a6/a7 move tables of the face
#   t5/t6  face strides of the two move tables (bytes)
#
# Stack frame (400 bytes): 0..239 six search frames of 40 bytes, 280..315 text,
# 320..331 path, 340 cap, 344 out, 348..392 s11..s0, 396 ra.

    .section .rodata
fact:
    .half   720, 120, 24, 6, 2, 1, 0
mv_names:
    .ascii  "R\0R2R'B\0B2B'D\0D2D'"

.equ PSTRIDE, 10080             # 5040 halfwords
.equ TSTRIDE, 1458              # 729 halfwords
.equ FRAME, 40

    .text

# One child: quarter turn \k of the current face applied to a4/a5. A child
# whose bound exceeds the budget falls through to \next.
.macro TURN k, next
turn\k:
    add     t0, a6, a4
    lhu     a4, 0(t0)
    add     t0, a7, a5
    lhu     a5, 0(t0)
    add     t0, s2, a4
    lbu     t0, 0(t0)
    add     t1, s3, a5
    lbu     t1, 0(t1)
    blt     s6, t0, \next
    blt     s6, t1, \next
    addi    t2, a2, \k - 1      # move = 3 * face + turns - 1
    sb      t2, 0(s8)
    li      t4, 5
    bne     s6, t4, 1f
    mv      a0, a4              # leaf: is it within 5 moves?
    mv      a1, a5
    jal     ra, ball_find
    bgez    a0, found
    j       \next
1:  li      t3, \k              # resume here after the pop
    j       push
.endm

    .globl  rubik_solve_asm
    .type   rubik_solve_asm, @function
    .align  2
rubik_solve_asm:
    addi    sp, sp, -400
    sw      ra, 396(sp)
    sw      s0, 392(sp)
    sw      s1, 388(sp)
    sw      s2, 384(sp)
    sw      s3, 380(sp)
    sw      s4, 376(sp)
    sw      s5, 372(sp)
    sw      s6, 368(sp)
    sw      s7, 364(sp)
    sw      s8, 360(sp)
    sw      s9, 356(sp)
    sw      s10, 352(sp)
    sw      s11, 348(sp)
    sw      a1, 344(sp)
    sw      a2, 340(sp)

    # Parse: seven distinct cubies '1'..'7', copied to 280(sp).
    li      t0, 0
    li      t1, 0               # cubies seen
    li      t3, 7
1:  lbu     t4, 0(a0)
    addi    t4, t4, -49
    bgeu    t4, t3, invalid     # also catches a short string
    li      t6, 1
    sll     t6, t6, t4
    and     t2, t1, t6
    bnez    t2, invalid
    or      t1, t1, t6
    add     t2, sp, t0
    sb      t4, 280(t2)
    addi    a0, a0, 1
    addi    t0, t0, 1
    blt     t0, t3, 1b
    # Seven twists '1'..'3', the first six in base 3; the sum must be 0 mod 3.
    li      t0, 0
    li      t1, 0               # sum
    li      a4, 0               # twist rank
    li      t3, 3
    li      t5, 6
2:  lbu     t4, 0(a0)
    addi    t4, t4, -49
    bgeu    t4, t3, invalid
    add     t1, t1, t4
    bge     t0, t5, 3f
    slli    t6, a4, 1
    add     a4, t6, a4
    add     a4, a4, t4
3:  addi    a0, a0, 1
    addi    t0, t0, 1
    bge     t5, t0, 2b
    lbu     t4, 0(a0)
    bnez    t4, invalid
4:  blt     t1, t3, 5f
    sub     t1, t1, t3
    j       4b
5:  bnez    t1, invalid
    # Lehmer rank: add (6 - i)! for every smaller cubie right of position i.
    li      a3, 0
    la      a6, fact
    li      t0, 0
    li      t3, 7
6:  add     t2, sp, t0
    lbu     t4, 280(t2)
    lhu     t5, 0(a6)
    addi    t1, t0, 1
7:  bge     t1, t3, 9f
    add     t2, sp, t1
    lbu     t6, 280(t2)
    bgeu    t6, t4, 8f
    add     a3, a3, t5
8:  addi    t1, t1, 1
    j       7b
9:  addi    a6, a6, 2
    addi    t0, t0, 1
    blt     t0, t3, 6b
    slli    s9, a3, 1           # doubled coordinates
    slli    s10, a4, 1

    la      s0, rubik_asm_pmove
    la      s1, rubik_asm_tmove
    la      s2, rubik_asm_pdist
    la      s3, rubik_asm_tdist
    la      s4, rubik_asm_ball
    li      t5, PSTRIDE
    li      t6, TSTRIDE
    li      s6, 0               # moves found so far
    mv      a0, s9
    mv      a1, s10
    jal     ra, ball_find
    bgez    a0, follow          # within 5 moves: the table does it all

    add     t0, s2, s9          # first bound: max(h, 6)
    lbu     s5, 0(t0)
    add     t0, s3, s10
    lbu     t0, 0(t0)
    bgeu    s5, t0, 1f
    mv      s5, t0
1:  li      t0, 6
    bgeu    s5, t0, new_bound
    mv      s5, t0

new_bound:
    addi    s6, s5, -1
    mv      s7, sp
    addi    s8, sp, 320
    li      s11, -1
    li      a3, 0
    li      a2, 0
    mv      a6, s0
    mv      a7, s1
face_start:
    beq     a3, s11, next_face  # never the same face twice in a row
    mv      a4, s9
    mv      a5, s10
    TURN    1, turn2
    TURN    2, turn3
    TURN    3, next_face

next_face:
    addi    a3, a3, 1
    addi    a2, a2, 3
    add     a6, a6, t5
    add     a7, a7, t6
    li      t0, 3
    blt     a3, t0, face_start
    beq     s7, sp, bound_up    # the root is exhausted
    addi    s7, s7, -FRAME      # pop
    lw      s9, 0(s7)
    lw      s10, 4(s7)
    lw      s11, 8(s7)
    lw      a3, 12(s7)
    lw      t3, 16(s7)
    lw      a4, 20(s7)
    lw      a5, 24(s7)
    lw      a6, 28(s7)
    lw      a7, 32(s7)
    lw      a2, 36(s7)
    addi    s8, s8, -1
    addi    s6, s6, 1
    li      t0, 2
    blt     t3, t0, turn2
    beq     t3, t0, turn3
    j       next_face

push:
    sw      s9, 0(s7)
    sw      s10, 4(s7)
    sw      s11, 8(s7)
    sw      a3, 12(s7)
    sw      t3, 16(s7)
    sw      a4, 20(s7)
    sw      a5, 24(s7)
    sw      a6, 28(s7)
    sw      a7, 32(s7)
    sw      a2, 36(s7)
    addi    s7, s7, FRAME
    addi    s8, s8, 1
    addi    s6, s6, -1
    mv      s11, a3
    mv      s9, a4
    mv      s10, a5
    li      a3, 0
    li      a2, 0
    mv      a6, s0
    mv      a7, s1
    j       face_start

bound_up:
    addi    s5, s5, 1
    li      t0, 12
    blt     s5, t0, new_bound
    j       invalid             # unreachable: the diameter is 11

found:
    addi    t0, sp, 320
    sub     s6, s8, t0
    addi    s6, s6, 1           # moves found by the search
    mv      s9, a4
    mv      s10, a5

    # Finish from (s9, s10) with the table, one lookup per move.
follow:
    or      t0, s9, s10
    beqz    t0, text
    mv      a0, s9
    mv      a1, s10
    jal     ra, ball_find
    add     t0, sp, s6
    sb      a0, 320(t0)
    addi    s6, s6, 1
    mv      a6, s0
    mv      a7, s1
    li      t0, 3
1:  blt     a0, t0, 2f          # face = move / 3, turns = move % 3 + 1
    addi    a0, a0, -3
    add     a6, a6, t5
    add     a7, a7, t6
    j       1b
2:  addi    a0, a0, 1
3:  add     t1, a6, s9
    lhu     s9, 0(t1)
    add     t1, a7, s10
    lhu     s10, 0(t1)
    addi    a0, a0, -1
    bnez    a0, 3b
    j       follow

    # Text of path[0..s6) into 280(sp), then copy it if it fits.
text:
    addi    t1, sp, 280
    la      t3, mv_names
    li      t2, 0
    beqz    s6, 3f
1:  beqz    t2, 2f
    li      t4, ' '
    sb      t4, 0(t1)
    addi    t1, t1, 1
2:  add     t4, sp, t2
    lbu     t4, 320(t4)
    slli    t4, t4, 1
    add     t4, t3, t4
    lbu     t0, 0(t4)
    sb      t0, 0(t1)
    lbu     t0, 1(t4)
    addi    t1, t1, 1
    beqz    t0, 4f
    sb      t0, 0(t1)
    addi    t1, t1, 1
4:  addi    t2, t2, 1
    blt     t2, s6, 1b
3:  sb      zero, 0(t1)
    addi    t0, sp, 280
    sub     t1, t1, t0          # text length
    lw      a1, 344(sp)
    lw      a2, 340(sp)
    bltu    t1, a2, 5f          # length + 1 <= cap
    li      a0, -2
    beqz    a2, epilogue
    sb      zero, 0(a1)
    j       epilogue
5:  lbu     t2, 0(t0)
    sb      t2, 0(a1)
    addi    t0, t0, 1
    addi    a1, a1, 1
    bnez    t2, 5b
    mv      a0, s6
    j       epilogue

invalid:
    li      a0, -1
epilogue:
    lw      ra, 396(sp)
    lw      s0, 392(sp)
    lw      s1, 388(sp)
    lw      s2, 384(sp)
    lw      s3, 380(sp)
    lw      s4, 376(sp)
    lw      s5, 372(sp)
    lw      s6, 368(sp)
    lw      s7, 364(sp)
    lw      s8, 360(sp)
    lw      s9, 356(sp)
    lw      s10, 352(sp)
    lw      s11, 348(sp)
    addi    sp, sp, 400
    ret
    .size   rubik_solve_asm, .-rubik_solve_asm

# ball_find: a0 = doubled perm, a1 = doubled twist. Returns the stored move in
# a0, or -1 if the state is more than 5 moves away. Branchless-style lower
# bound over the 12,224 sorted entries in 14 fixed steps; the sentinel after
# the table stops a search that runs past the last entry. Uses t0-t3 only.
.macro STEP off
    lw      t3, \off(t0)
    bgeu    t3, a0, 1f
    addi    t0, t0, \off
1:
.endm

    .type   ball_find, @function
    .align  2
ball_find:
    slli    a0, a0, 11
    or      a0, a0, a1
    slli    a0, a0, 4           # smallest entry of this state
    mv      t0, s4
    li      t1, 24448           # 6112 entries
    add     t2, t0, t1
    lw      t3, 0(t2)
    bgeu    t3, a0, 1f
    mv      t0, t2
1:  srli    t1, t1, 1           # 3056
    add     t2, t0, t1
    lw      t3, 0(t2)
    bgeu    t3, a0, 2f
    mv      t0, t2
2:  srli    t1, t1, 1           # 1528
    add     t2, t0, t1
    lw      t3, 0(t2)
    bgeu    t3, a0, 3f
    mv      t0, t2
3:  srli    t1, t1, 1           # 764
    add     t2, t0, t1
    lw      t3, 0(t2)
    bgeu    t3, a0, 4f
    mv      t0, t2
4:  STEP    1528
    STEP    764
    STEP    380
    STEP    192
    STEP    96
    STEP    48
    STEP    24
    STEP    12
    STEP    4
    STEP    4
    lw      t3, 0(t0)
    bgeu    t3, a0, 5f
    lw      t3, 4(t0)
5:  srli    t1, t3, 4
    srli    t2, a0, 4
    bne     t1, t2, 6f
    andi    a0, t3, 15
    ret
6:  li      a0, -1
    ret
    .size   ball_find, .-ball_find
