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
    move.l (%a0),%d3            /* authenticated original client PID */
    cmpi.l #0x53544f50,4(%a0)
    beq service_stop
    move.l 4(%a0),%d2
    cmpi.l #1,%d2
    beq command_info
    cmpi.l #6,%d2
    beq command_restore
    cmpi.l #2,%d2
    blo command_invalid
    cmpi.l #19,%d2
    bhi command_invalid
    lea request(%pc),%a1
    move.l 4(%a0),(%a1)
    move.l 8(%a0),4(%a1)
    move.l 12(%a0),8(%a1)
    move.l 16(%a0),12(%a1)
    cmpi.l #9,%d2
    blo 1f
    cmpi.l #11,%d2
    bls 2f
    cmpi.l #13,%d2
    bne 1f
2:
    move.l (%a0),12(%a1)
1:
    move.l %a1,%a0
    /* Pass the authenticated IPC sender separately to the kernel. The
       request payload remains devoted to drawing coordinates and colours. */
    move.l %d3,%d1
    moveq #42,%d0
    trap #0
    bra command_reply_status
command_restore:
    move.l %d3,%d1
    moveq #44,%d0
    trap #0
    bra command_reply_status
command_info:
    lea gfx_info(%pc),%a0
    moveq #43,%d0
    trap #0
    tst.l %d0
    bne command_reply_status
    lea response(%pc),%a1
    lea gfx_info(%pc),%a0
    move.l (%a0),%d3
    lsl.l #8,%d3
    lsl.l #8,%d3
    move.w 6(%a0),%d3
    move.l %d3,4(%a1)
    move.l 8(%a0),%d3
    lsl.l #8,%d3
    lsl.l #8,%d3
    move.w 14(%a0),%d3
    move.l %d3,8(%a1)
    move.l 20(%a0),%d3
    swap %d3
    move.w 18(%a0),%d3
    move.l %d3,12(%a1)
    bra command_reply
command_invalid:
    move.l #-0x3e2,%d0
command_reply_status:
    lea response(%pc),%a1
    move.l %d0,4(%a1)
    clr.l 8(%a1)
    clr.l 12(%a1)
command_reply:
    lea message(%pc),%a0
    move.l 4(%a0),%d2
    ori.l #0x80000000,%d2
    lea response(%pc),%a1
    move.l %d2,(%a1)
reply_send:
    move.l (%a0),%d1
    lea response(%pc),%a0
    moveq #36,%d0
    trap #0
    tst.l %d0
    beq service_loop
    cmpi.l #-0x3c1,%d0          /* client exited before receiving its reply */
    beq service_loop
    cmpi.l #-0x3c2,%d0          /* receiver mailbox temporarily full */
    bne service_failed
    moveq #10,%d0               /* yield, preserving the pending response */
    trap #0
    lea message(%pc),%a0
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
service_name: .asciz "SYSTEM.GFX"
    .section .bss
    .align 2
message:  .space 20
request:  .space 16
response: .space 16
gfx_info: .space 24
