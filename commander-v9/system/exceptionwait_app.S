    .section .app_header,"a"
    .ascii "ASYSAPP0"
    .word 1
    .word 32
    .long __app_file_end
    .long __app_memory_end
    .long app_entry
    .long 2048
    .long 0

    .section .text.startup,"ax"
    .globl app_entry
app_entry:
    lea before_info(%pc),%a0
    moveq #12,%d0
    trap #0
    tst.l %d0
    bne app_failed
    moveq #'B',%d6
    lea bus_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-258,%d0
    bne app_failed
    moveq #'A',%d6
    lea address_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-259,%d0
    bne app_failed
    moveq #'D',%d6
    lea divide_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-261,%d0
    bne app_failed
    moveq #'C',%d6
    lea chk_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-262,%d0
    bne app_failed
    moveq #'V',%d6
    lea trapv_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-263,%d0
    bne app_failed
    moveq #'P',%d6
    lea privilege_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-264,%d0
    bne app_failed
    moveq #'L',%d6
    lea linea_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-266,%d0
    bne app_failed
    moveq #'F',%d6
    lea linef_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-267,%d0
    bne app_failed
    moveq #'1',%d6
    lea trap1_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-289,%d0
    bne app_failed
    moveq #'T',%d6
    lea trap15_request(%pc),%a0
    bsr spawn_wait
    cmpi.l #-303,%d0
    bne app_failed
    moveq #'M',%d6
    lea after_info(%pc),%a0
    moveq #12,%d0
    trap #0
    tst.l %d0
    bne app_failed
    lea before_info(%pc),%a0
    lea after_info(%pc),%a1
    move.l 16(%a0),%d0
    cmp.l 16(%a1),%d0
    bne app_failed
    move.l 20(%a0),%d0
    cmp.l 20(%a1),%d0
    bne app_failed
    lea app_success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s app_exit

spawn_wait:
    move.l %a0,%a2
    moveq #34,%d0
    trap #0
    tst.l %d0
    bne.s spawn_wait_failed
    move.l 128(%a2),%d1
    lea 132(%a2),%a0
    moveq #33,%d0
    trap #0
    tst.l %d0
    bne.s spawn_wait_failed
    move.l 132(%a2),%d0
    rts
spawn_wait_failed:
    moveq #'S',%d6
    move.l #0x7fffffff,%d0
    rts

app_failed:
    lea app_error(%pc),%a0
    moveq #1,%d0
    trap #0
    move.l %d6,%d1
    moveq #8,%d0
    trap #0
    moveq #'\n',%d1
    moveq #8,%d0
    trap #0
    moveq #1,%d1
app_exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .data
    .align 2
bus_request:
    .asciz "\\SYSTEM\\BUSFLT.APP"
    .space 45
    .space 64
    .long 0,0
address_request:
    .asciz "\\SYSTEM\\ADRFLT.APP"
    .space 45
    .space 64
    .long 0,0
divide_request:
    .asciz "\\SYSTEM\\DIVFLT.APP"
    .space 45
    .space 64
    .long 0,0
chk_request:
    .asciz "\\SYSTEM\\CHKFLT.APP"
    .space 45
    .space 64
    .long 0,0
trapv_request:
    .asciz "\\SYSTEM\\TRAPVFLT.APP"
    .space 43
    .space 64
    .long 0,0
privilege_request:
    .asciz "\\SYSTEM\\PRIVFLT.APP"
    .space 44
    .space 64
    .long 0,0
linea_request:
    .asciz "\\SYSTEM\\LINEA.APP"
    .space 46
    .space 64
    .long 0,0
linef_request:
    .asciz "\\SYSTEM\\LINEF.APP"
    .space 46
    .space 64
    .long 0,0
trap1_request:
    .asciz "\\SYSTEM\\TRAP1.APP"
    .space 46
    .space 64
    .long 0,0
trap15_request:
    .asciz "\\SYSTEM\\TRAP15.APP"
    .space 45
    .space 64
    .long 0,0

    .section .rodata
app_success: .asciz "EXWAIT: 10 CHILD FAULTS / WAKE / REAP / MEMORY PASS\n"
app_error:   .asciz "EXWAIT: CHILD EXCEPTION WAIT FAILED AT "

    .section .bss
    .align 2
before_info: .space 24
after_info:  .space 24
