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
    lea service_name(%pc),%a0
    moveq #39,%d0
    trap #0
    tst.l %d0
    bne service_failed
service_loop:
    lea message(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne service_failed
    lea message(%pc),%a0
    move.l (%a0),%d3
    move.l 4(%a0),%d2
    tst.l %d3
    bne 9f
    cmpi.l #0x4b455849,%d2
    beq command_kernel_exit
9:
    cmpi.l #0x53544f50,%d2
    beq service_stop
    cmpi.l #1,%d2
    beq command_register
    cmpi.l #2,%d2
    beq command_unregister
    cmpi.l #3,%d2
    beq command_foreground
    cmpi.l #4,%d2
    beq command_query
    cmpi.l #5,%d2
    beq command_wait_key
    cmpi.l #6,%d2
    beq command_begin_keys
    cmpi.l #7,%d2
    beq command_end_keys
    move.l #-0x410,%d0
    bra reply

command_register:
    lea applications(%pc),%a1
    suba.l %a2,%a2
    moveq #3,%d1
1:  cmp.l (%a1),%d3
    beq command_ok
    tst.l (%a1)
    bne 2f
    cmpa.l #0,%a2
    bne 2f
    move.l %a1,%a2
2:  addq.l #4,%a1
    dbra %d1,1b
    cmpa.l #0,%a2
    beq command_full
    move.l %d3,(%a2)
    lea application_count(%pc),%a1
    addq.l #1,(%a1)
    lea foreground_pid(%pc),%a1
    tst.l (%a1)
    bne command_ok
    move.l %d3,(%a1)
command_ok:
    moveq #0,%d0
    bra reply
command_full:
    move.l #-0x411,%d0
    bra reply

command_unregister:
    move.l %d3,%d1
    moveq #48,%d0
    trap #0
    lea applications(%pc),%a1
    moveq #3,%d1
3:  cmp.l (%a1),%d3
    beq 4f
    addq.l #4,%a1
    dbra %d1,3b
    move.l #-0x412,%d0
    bra reply
4:  clr.l (%a1)
    lea application_count(%pc),%a2
    subq.l #1,(%a2)
    lea foreground_pid(%pc),%a2
    cmp.l (%a2),%d3
    bne command_ok
    clr.l (%a2)
    lea applications(%pc),%a1
    moveq #3,%d1
5:  tst.l (%a1)
    bne 6f
    addq.l #4,%a1
    dbra %d1,5b
    bra command_ok
6:  lea foreground_pid(%pc),%a2
    move.l (%a1),(%a2)
    bra command_ok

command_foreground:
    moveq #0,%d1
    moveq #48,%d0
    trap #0
    lea applications(%pc),%a1
    moveq #3,%d1
7:  cmp.l (%a1),%d3
    beq 8f
    addq.l #4,%a1
    dbra %d1,7b
    move.l #-0x412,%d0
    bra reply
8:  lea foreground_pid(%pc),%a1
    move.l %d3,(%a1)
    bra command_ok

command_query:
    moveq #0,%d0
    bra reply

command_wait_key:
    lea foreground_pid(%pc),%a1
    cmp.l (%a1),%d3
    bne command_not_foreground
    move.l %d3,%d1
    moveq #45,%d0
    trap #0
    bra reply
command_not_foreground:
    move.l #-0x413,%d0
    bra reply

command_begin_keys:
    /* The caller becomes the foreground: the kernel keeps the keyboard owner
       per session, so an app in another session can take it too. */
    lea foreground_pid(%pc),%a1
    move.l %d3,(%a1)
    move.l %d3,%d1
    moveq #47,%d0
    trap #0
    bra reply

command_end_keys:
    move.l %d3,%d1
    moveq #48,%d0
    trap #0
    bra reply

/* Kernel-only KEXI event: value[1] is the exiting PID. No reply is sent. */
command_kernel_exit:
    move.l 8(%a0),%d3
    lea applications(%pc),%a1
    moveq #3,%d1
10: cmp.l (%a1),%d3
    beq 11f
    addq.l #4,%a1
    dbra %d1,10b
    bra service_loop
11: clr.l (%a1)
    lea application_count(%pc),%a2
    subq.l #1,(%a2)
    lea foreground_pid(%pc),%a2
    cmp.l (%a2),%d3
    bne service_loop
    clr.l (%a2)
    lea applications(%pc),%a1
    moveq #3,%d1
12: tst.l (%a1)
    bne 13f
    addq.l #4,%a1
    dbra %d1,12b
    bra service_loop
13: move.l (%a1),(%a2)
    bra service_loop
reply:
    lea response(%pc),%a1
    move.l %d2,(%a1)
    ori.l #0x80000000,(%a1)
    move.l %d0,4(%a1)
    lea foreground_pid(%pc),%a2
    move.l (%a2),8(%a1)
    lea application_count(%pc),%a2
    move.l (%a2),12(%a1)
reply_send:
    move.l %d3,%d1
    lea response(%pc),%a0
    moveq #36,%d0
    trap #0
    tst.l %d0
    beq service_loop
    cmpi.l #-0x3c2,%d0          /* input burst filled receiver mailbox */
    bne service_failed
    moveq #10,%d0
    trap #0
    bra reply_send

service_stop:
    moveq #0,%d1
    moveq #0,%d0
    trap #0
service_failed:
    moveq #1,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
service_name: .asciz "SYSTEM.GUI"
    .section .bss
    .align 2
message: .space 20
response: .space 16
applications: .space 16
foreground_pid: .space 4
application_count: .space 4
