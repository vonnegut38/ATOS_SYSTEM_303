    .section .module_header,"a"
    .ascii  "ASYSMOD0"
    .word   3
    .word   32
    .long   __image_file_end
    .long   0
    .long   vfs_module_entry
    .long   __image_memory_end
    .long   0

    .section .text.startup,"ax"
    .globl vfs_module_entry
vfs_module_entry:
    move.l 4(%sp),%a0
    cmpi.l #0x4b4d4150,(%a0)
    bne.s vfs_fail
    cmpi.w #3,4(%a0)
    bne.s vfs_fail
    cmpi.w #80,6(%a0)
    blo.s vfs_fail
    lea vfs_api(%pc),%a1
    move.l %a0,(%a1)
    lea vfs_service(%pc),%a1
    move.l #0x56465320,(%a1)
    move.w #3,4(%a1)
    move.w #32,6(%a1)
    lea vfs_stat_system(%pc),%a0
    move.l %a0,8(%a1)
    lea vfs_read_system(%pc),%a0
    move.l %a0,12(%a1)
    lea vfs_stat_path(%pc),%a0
    move.l %a0,16(%a1)
    lea vfs_read_path(%pc),%a0
    move.l %a0,20(%a1)
    lea vfs_list_path(%pc),%a0
    move.l %a0,24(%a1)
    lea vfs_read_path_at(%pc),%a0
    move.l %a0,28(%a1)
    move.l %a1,-(%sp)
    move.l #3,-(%sp)
    move.l #0x56465320,-(%sp)
    lea vfs_api(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%a1
    jsr (%a1)
    lea 12(%sp),%sp
    rts
vfs_fail:
    moveq #0,%d0
    rts

vfs_stat_system:
    lea vfs_api(%pc),%a0
    move.l (%a0),%a0
    move.l 32(%a0),%a0
    jmp (%a0)

vfs_read_system:
    lea vfs_api(%pc),%a0
    move.l (%a0),%a0
    move.l 36(%a0),%a0
    jmp (%a0)

vfs_stat_path:
    lea vfs_api(%pc),%a0
    move.l (%a0),%a0
    move.l 64(%a0),%a0
    jmp (%a0)

vfs_read_path:
    lea vfs_api(%pc),%a0
    move.l (%a0),%a0
    move.l 68(%a0),%a0
    jmp (%a0)

vfs_list_path:
    lea vfs_api(%pc),%a0
    move.l (%a0),%a0
    move.l 72(%a0),%a0
    jmp (%a0)

vfs_read_path_at:
    lea vfs_api(%pc),%a0
    move.l (%a0),%a0
    move.l 76(%a0),%a0
    jmp (%a0)

    .section .bss
    .align 2
vfs_api:
    .space 4
vfs_service:
    .space 32
