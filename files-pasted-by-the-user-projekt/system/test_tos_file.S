/* TOS .PRG exercising GEMDOS file I/O (Fopen/Fread/Fclose) and Malloc. */
    .section .tosprog,"a"
    .word 0x601a
    .long text_end-text_start
    .long 0
    .long 0
    .long 0
    .long 0
    .word 0
    .long 0
text_start:
    move.w #0,-(%sp)
    pea fname(%pc)
    move.w #0x3d,%d0
    trap #1
    addq.l #6,%sp
    tst.l %d0
    bmi.s f_open_fail
    move.w %d0,%d3

    lea buf(%pc),%a0
    move.l %a0,-(%sp)
    move.l #128,-(%sp)
    move.w %d3,-(%sp)
    move.w #0x3f,%d0
    trap #1
    lea 10(%sp),%sp
    move.l %d0,%d4

    lea buf(%pc),%a0
    move.b #0,(%a0,%d4.l)

    pea ok_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    pea buf(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp

    move.w %d3,-(%sp)
    move.w #0x3e,%d0
    trap #1
    addq.l #2,%sp

    move.l #1024,-(%sp)
    move.w #0x48,%d0
    trap #1
    addq.l #4,%sp
    tst.l %d0
    beq.s m_fail
    pea mok_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    bra.s done
m_fail:
    pea mbad_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    bra.s done
f_open_fail:
    pea fbad_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
done:
    move.w #0,%d0
    trap #1
fname:  .asciz "HELLO.TXT"
ok_msg: .asciz "FILE READ: "
mok_msg: .asciz "MALLOC OK\n"
mbad_msg:.asciz "MALLOC FAIL\n"
fbad_msg:.asciz "FOPEN FAIL\n"
    .even
buf: .space 256
text_end:
