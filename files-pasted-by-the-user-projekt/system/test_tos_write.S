/* TOS .PRG: Fcreate/Fwrite/Fclose then Fopen/Fread to verify a written file. */
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
    /* Fcreate("OUT.TXT", 0) */
    move.w #0,-(%sp)
    pea out(%pc)
    move.w #0x3c,%d0
    trap #1
    addq.l #6,%sp
    tst.l %d0
    bmi create_fail
    move.w %d0,%d3

    /* Fwrite(h, len, payload) */
    pea payload(%pc)
    move.l #26,-(%sp)
    move.w %d3,-(%sp)
    move.w #0x40,%d0
    trap #1
    lea 10(%sp),%sp

    /* Fclose(h) */
    move.w %d3,-(%sp)
    move.w #0x3e,%d0
    trap #1
    addq.l #2,%sp

    /* Fopen("OUT.TXT", 0) */
    move.w #0,-(%sp)
    pea out(%pc)
    move.w #0x3d,%d0
    trap #1
    addq.l #6,%sp
    tst.l %d0
    bmi reopen_fail
    move.w %d0,%d3

    /* Fread(h, 128, buf) */
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

    pea read_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    pea buf(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    pea nl(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    bra.s done
create_fail:
    pea fail1(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    bra.s done
reopen_fail:
    pea fail2(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
done:
    move.w #0,%d0
    trap #1
out:      .asciz "OUT.TXT"
payload:  .asciz "WRITTEN BY TOS GEMDOS!\n"
read_msg: .asciz "READBACK: "
fail1:    .asciz "FCREATE FAILED\n"
fail2:    .asciz "FOPEN FAILED\n"
nl:       .asciz "\n"
    .even
buf: .space 256
text_end:
