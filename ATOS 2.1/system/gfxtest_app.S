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
    moveq #40,%d0
    trap #0
    tst.l %d0
    bmi failed
    move.l %d0,%d4
    /* Discard bounded type-ahead left by the command that launched us. */
    moveq #31,%d5
drain_input:
    moveq #9,%d0
    trap #0
    dbra %d5,drain_input
    lea info_message(%pc),%a0
    bsr send_command
    tst.l %d0
    bne failed
    lea reply(%pc),%a0
    move.l 8(%a0),%d2
    move.l %d2,%d3
    swap %d3
    tst.w %d3
    beq failed
    tst.w %d2
    beq failed
    sub.l #0x00100010,%d2
    lea frame_message(%pc),%a0
    move.l %d2,8(%a0)
    lea clear_message(%pc),%a0
    bsr send_command
    tst.l %d0
    bne failed
    lea frame_message(%pc),%a0
    bsr send_command
    tst.l %d0
    bne restore_failed
    lea fill_one(%pc),%a0
    bsr send_command
    tst.l %d0
    bne restore_failed
    lea fill_two(%pc),%a0
    bsr send_command
    tst.l %d0
    bne restore_failed
    lea line_one(%pc),%a0
    bsr send_command
    tst.l %d0
    bne restore_failed
    lea line_two(%pc),%a0
    bsr send_command
    tst.l %d0
    bne restore_failed
    /* Do not let the Enter that launched RUN dismiss the card. */
    move.l #750,%d1
    moveq #11,%d0
    trap #0
    /* Keep the deterministic test card visible until a later key press. */
    moveq #35,%d0
    trap #0
    lea restore_message(%pc),%a0
    bsr send_command
    tst.l %d0
    bne failed
    moveq #0,%d1
    bra exit
restore_failed:
    lea restore_message(%pc),%a0
    bsr send_command
failed:
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

/* A0 = four-long command, D4 = broker PID. */
send_command:
    move.l %a0,%a2
    move.l %d4,%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne command_failed
    lea reply(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne command_failed
    lea reply(%pc),%a0
    cmp.l (%a0),%d4
    bne command_failed
    move.l (%a2),%d2
    ori.l #0x80000000,%d2
    cmp.l 4(%a0),%d2
    bne command_failed
    moveq #0,%d0
    rts
command_failed:
    moveq #-1,%d0
    rts

    .section .data
    .align 2
info_message:    .long 1,0,0,0
clear_message:   .long 2,0,0,0
frame_message:   .long 4,0x00080008,0,1
fill_one:        .long 3,0x00200020,0x00500028,1
fill_two:        .long 3,0x00800050,0x0078003c,1
line_one:        .long 7,0x00000000,0x027f018f,1
line_two:        .long 7,0x027f0000,0x0000018f,1
restore_message: .long 6,0,0,0
    .section .rodata
service_name: .asciz "SYSTEM.GFX"
    .section .bss
    .align 2
reply: .space 20
