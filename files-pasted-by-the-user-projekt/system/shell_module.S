    .section .module_header,"a"
    .ascii "ASYSMOD0"
    .word 3
    .word 32
    .long __image_file_end
    .long 0
    .long shell_module_entry
    .long __image_memory_end
    .long 0

    .section .text.startup,"ax"
    .globl shell_module_entry
shell_module_entry:
    move.l 4(%sp),%a0
    cmpi.l #0x4b4d4150,(%a0)
    bne shell_fail
    cmpi.w #3,4(%a0)
    bne shell_fail
    cmpi.w #164,6(%a0)
    blo shell_fail
    tst.l 160(%a0)
    beq shell_fail
    tst.l 40(%a0)
    beq shell_fail
    tst.l 44(%a0)
    beq shell_fail
    tst.l 48(%a0)
    beq shell_fail
    tst.l 52(%a0)
    beq shell_fail
    tst.l 56(%a0)
    beq shell_fail
    tst.l 60(%a0)
    beq shell_fail
    tst.l 80(%a0)
    beq shell_fail
    tst.l 84(%a0)
    beq shell_fail
    tst.l 88(%a0)
    beq shell_fail
    tst.l 92(%a0)
    beq shell_fail
    lea shell_api(%pc),%a1
    move.l %a0,(%a1)
    move.l #0x45564e54,%d0
    moveq #1,%d1
    bsr shell_find_service
    tst.l %d0
    beq shell_fail
    lea shell_event(%pc),%a0
    move.l %d0,(%a0)
    move.l #0x494e5054,%d0
    moveq #1,%d1
    bsr shell_find_service
    tst.l %d0
    beq shell_fail
    move.l %d0,%a0
    cmpi.w #20,6(%a0)
    blo shell_fail
    tst.l 16(%a0)
    beq shell_fail
    lea shell_input(%pc),%a1
    move.l %a0,(%a1)
    move.l #0x50524f43,%d0
    moveq #4,%d1
    bsr shell_find_service
    tst.l %d0
    beq shell_fail
    lea shell_proc(%pc),%a0
    move.l %d0,(%a0)
    move.l #0x54494d52,%d0
    moveq #1,%d1
    bsr shell_find_service
    tst.l %d0
    beq shell_fail
    lea shell_timer(%pc),%a0
    move.l %d0,(%a0)
    move.l #0x56465320,%d0
    moveq #3,%d1
    bsr shell_find_service
    tst.l %d0
    beq shell_fail
    lea shell_vfs(%pc),%a0
    move.l %d0,(%a0)

    lea shell_service(%pc),%a1
    move.l #0x53484c4c,(%a1)
    move.w #2,4(%a1)
    move.w #28,6(%a1)
    lea shell_start(%pc),%a0
    move.l %a0,8(%a1)
    lea shell_pump(%pc),%a0
    move.l %a0,12(%a1)
    lea shell_key_count(%pc),%a0
    move.l %a0,16(%a1)
    lea shell_redraw_prompt(%pc),%a0
    move.l %a0,20(%a1)
    lea shell_line_active(%pc),%a0
    move.l %a0,24(%a1)
    move.l %a1,-(%sp)
    move.l #2,-(%sp)
    move.l #0x53484c4c,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%a1
    jsr (%a1)
    lea 12(%sp),%sp
    rts

shell_find_service:
    move.l %d1,-(%sp)
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 28(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    rts
shell_fail:
    moveq #0,%d0
    rts

shell_puts:
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 44(%a0),%a0
    jmp (%a0)
shell_put_u32:
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 48(%a0),%a0
    jmp (%a0)

shell_put_hex32:
    movem.l %d2/%a1-%a2,-(%sp)
    move.l 16(%sp),%d0
    lea shell_hex_buffer(%pc),%a0
    move.b #'0',(%a0)
    move.b #'x',1(%a0)
    clr.b 10(%a0)
    lea 2(%a0),%a2
    lea shell_hex_digits(%pc),%a1
    moveq #7,%d1
1:  move.l %d0,%d2
    lsr.l #8,%d2
    lsr.l #8,%d2
    lsr.l #8,%d2
    lsr.l #4,%d2
    andi.w #15,%d2
    move.b (%a1,%d2.w),(%a2)+
    lsl.l #4,%d0
    dbra %d1,1b
    pea shell_hex_buffer(%pc)
    bsr shell_puts
    addq.l #4,%sp
    movem.l (%sp)+,%d2/%a1-%a2
    rts

/* label, inclusive start and exclusive end */
shell_put_map_range:
    move.l 4(%sp),-(%sp)
    bsr shell_puts
    addq.l #4,%sp
    move.l 8(%sp),-(%sp)
    bsr shell_put_hex32
    addq.l #4,%sp
    pea shell_map_separator(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l 12(%sp),-(%sp)
    bsr shell_put_hex32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts

shell_start:
    lea shell_current_path(%pc),%a0
    move.b #92,(%a0)
    clr.b 1(%a0)
    pea shell_exec_path(%pc)
    move.l #0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 160(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    bsr shell_autoexec
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bsr shell_print_prompt
    rts

/* Run startup commands from C:\AUTOEXEC.BAT in bounded 512-byte windows.
   Each CR/LF-terminated line is placed in shell_buffer and dispatched by
   shell_execute, so startup stays on disk instead of in the kernel. */
shell_autoexec:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea shell_autoexec_name(%pc),%a0
    bsr shell_make_path
    tst.l %d0
    beq shell_autoexec_ret
    pea shell_attributes(%pc)
    pea shell_file_size(%pc)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 16(%a0),%a0
    jsr (%a0)
    lea 12(%sp),%sp
    tst.l %d0
    beq shell_autoexec_ret
    lea shell_attributes(%pc),%a0
    btst #4,(%a0)
    bne shell_autoexec_ret
    lea shell_autoexec_offset(%pc),%a0
    clr.l (%a0)
    lea shell_autoexec_n(%pc),%a0
    clr.l (%a0)
shell_autoexec_block:
    pea shell_loaded_size(%pc)
    move.l #512,-(%sp)
    pea shell_file_buffer(%pc)
    lea shell_autoexec_offset(%pc),%a0
    move.l (%a0),-(%sp)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 28(%a0),%a0
    jsr (%a0)
    lea 20(%sp),%sp
    tst.l %d0
    beq shell_autoexec_ret
    lea shell_loaded_size(%pc),%a0
    move.l (%a0),%d1
    beq shell_autoexec_ret
    lea shell_file_buffer(%pc),%a0
shell_autoexec_chars:
    tst.l %d1
    beq shell_autoexec_block_done
    move.b (%a0)+,%d0
    cmpi.b #10,%d0
    beq.s shell_autoexec_endl
    cmpi.b #13,%d0
    beq.s shell_autoexec_endl
    lea shell_autoexec_n(%pc),%a1
    move.l (%a1),%d2
    cmpi.l #62,%d2
    bhi.s shell_autoexec_skip
    lea shell_buffer(%pc),%a2
    lea (%a2,%d2.l),%a3
    move.b %d0,(%a3)
    addq.l #1,%d2
    move.l %d2,(%a1)
shell_autoexec_skip:
    subq.l #1,%d1
    bra.s shell_autoexec_chars
shell_autoexec_endl:
    lea shell_autoexec_n(%pc),%a1
    move.l (%a1),%d2
    lea shell_buffer(%pc),%a2
    lea (%a2,%d2.l),%a3
    clr.b (%a3)
    tst.l %d2
    beq.s shell_autoexec_endl_done
    lea shell_foreground_attached(%pc),%a1
    clr.b (%a1)
    movem.l %a0/%d1,-(%sp)
    bsr shell_execute
    movem.l (%sp)+,%a0/%d1
    lea shell_length(%pc),%a1
    clr.b (%a1)
    lea shell_buffer(%pc),%a1
    clr.b (%a1)
shell_autoexec_endl_done:
    lea shell_autoexec_n(%pc),%a1
    clr.l (%a1)
    subq.l #1,%d1
    bra.s shell_autoexec_chars
shell_autoexec_block_done:
    lea shell_autoexec_offset(%pc),%a0
    lea shell_loaded_size(%pc),%a1
    move.l (%a1),%d0
    add.l %d0,(%a0)
    lea shell_file_size(%pc),%a1
    move.l (%a0),%d0
    cmp.l (%a1),%d0
    blo shell_autoexec_block
shell_autoexec_ret:
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rts

shell_redraw_prompt:
    lea shell_cursor_visible(%pc),%a0
    clr.b (%a0)
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bsr shell_print_prompt
    bsr shell_cursor_hide
    pea shell_buffer(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bsr shell_cursor_show
    rts

shell_print_prompt:
    move.l %a2,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 60(%a0),%a0
    jsr (%a0)
    lea shell_prompt_buffer(%pc),%a0
    move.b %d0,(%a0)
    move.b #':',1(%a0)
    lea shell_current_path(%pc),%a1
    lea 2(%a0),%a2
1:  move.b (%a1)+,(%a2)+
    bne.s 1b
    subq.l #1,%a2
    move.b #'>',(%a2)+
    move.b #' ',(%a2)+
    clr.b (%a2)
    move.l %a0,-(%sp)
    bsr shell_puts
    addq.l #4,%sp
    move.l (%sp)+,%a2
    bsr shell_cursor_show
    rts

shell_pump:
    pea shell_value(%pc)
    pea shell_type(%pc)
    move.l 12(%sp),-(%sp)
    lea shell_event(%pc),%a0
    move.l (%a0),%a0
    move.l 8(%a0),%a0
    jsr (%a0)
    lea 12(%sp),%sp
    tst.l %d0
    beq shell_cursor_update
    lea shell_type(%pc),%a0
    cmpi.l #2,(%a0)
    beq shell_key_release
    cmpi.l #1,(%a0)
    bne shell_consumed
    lea shell_value(%pc),%a0
    move.l (%a0),%d0
    andi.l #0x7f,%d0
    cmpi.b #0x2a,%d0
    beq shell_left_shift_down
    cmpi.b #0x36,%d0
    beq shell_right_shift_down
    move.l %d0,-(%sp)
    bsr shell_cursor_hide
    move.l (%sp)+,%d0
    cmpi.b #0x1c,%d0
    beq shell_enter
    cmpi.b #0x0e,%d0
    beq shell_backspace
    cmpi.b #0x0f,%d0
    beq shell_complete
    cmpi.b #0x48,%d0
    beq shell_history_recall
    cmpi.b #0x50,%d0
    beq shell_history_forward
    cmpi.l #64,%d0
    bhs shell_consumed
    lea shell_shift_state(%pc),%a0
    tst.b (%a0)
    beq.s shell_use_plain_keymap
    lea shell_shift_keymap(%pc),%a0
    bra.s shell_keymap_ready
shell_use_plain_keymap:
    lea shell_keymap(%pc),%a0
shell_keymap_ready:
    move.b 0(%a0,%d0.w),%d0
    beq shell_consumed
    lea shell_length(%pc),%a0
    moveq #0,%d1
    move.b (%a0),%d1
    cmpi.b #63,%d1
    bhs shell_consumed
    lea shell_buffer(%pc),%a1
    move.b %d0,0(%a1,%d1.w)
    addq.b #1,%d1
    move.b %d1,(%a0)
    clr.b 0(%a1,%d1.w)
    lea shell_char(%pc),%a0
    move.b %d0,(%a0)
    clr.b 1(%a0)
    move.l %a0,-(%sp)
    bsr shell_puts
    addq.l #4,%sp
    bra shell_count

shell_left_shift_down:
    lea shell_shift_state(%pc),%a0
    bset #0,(%a0)
    bra shell_consumed
shell_right_shift_down:
    lea shell_shift_state(%pc),%a0
    bset #1,(%a0)
    bra shell_consumed
shell_key_release:
    lea shell_value(%pc),%a0
    move.l (%a0),%d0
    andi.l #0x7f,%d0
    cmpi.b #0x2a,%d0
    beq.s shell_left_shift_up
    cmpi.b #0x36,%d0
    bne shell_consumed
    lea shell_shift_state(%pc),%a0
    bclr #1,(%a0)
    bra shell_consumed
shell_left_shift_up:
    lea shell_shift_state(%pc),%a0
    bclr #0,(%a0)
    bra shell_consumed

shell_backspace:
    lea shell_length(%pc),%a0
    tst.b (%a0)
    beq shell_consumed
    subq.b #1,(%a0)
    moveq #0,%d0
    move.b (%a0),%d0
    lea shell_buffer(%pc),%a1
    clr.b 0(%a1,%d0.w)
    pea shell_backspace_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bra shell_count

shell_complete:
    movem.l %d2-%d4/%a2-%a3,-(%sp)
    lea shell_length(%pc),%a0
    moveq #0,%d3
    move.b (%a0),%d3
    beq shell_complete_done
    move.l %d3,%d2
    lea shell_buffer(%pc),%a2
1:  tst.l %d2
    beq.s 2f
    move.l %d2,%d0
    subq.l #1,%d0
    cmpi.b #' ',0(%a2,%d0.w)
    beq.s 2f
    subq.l #1,%d2
    bra.s 1b
2:  move.l %d3,%d4
    sub.l %d2,%d4
    beq shell_complete_done
    pea shell_completion_leaf(%pc)
    move.l #64,-(%sp)
    pea shell_work_path(%pc)
    lea 0(%a2,%d2.w),%a0
    move.l %a0,-(%sp)
    pea shell_current_path(%pc)
    bsr shell_prepare_completion
    lea 20(%sp),%sp
    tst.l %d0
    beq shell_complete_done
    lea shell_completion_leaf(%pc),%a0
    add.l (%a0),%d2
    move.l %d3,%d4
    sub.l %d2,%d4
    beq shell_complete_done
    lea shell_completion(%pc),%a3
    lea 0(%a2,%d2.w),%a0
    move.l %a0,(%a3)
    move.l %d4,4(%a3)
    clr.l 8(%a3)
    clr.b 12(%a3)
    clr.l 76(%a3)
    move.l %a3,-(%sp)
    pea shell_completion_entry(%pc)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%a0
    jsr (%a0)
    lea 12(%sp),%sp
    tst.l %d0
    beq shell_complete_done
    tst.l 8(%a3)
    beq shell_complete_done
    move.l %d4,%d2
3:  cmpi.l #63,%d3
    bhs shell_complete_done
    move.b 12(%a3,%d2.w),%d0
    beq 4f
    move.b %d0,0(%a2,%d3.w)
    addq.l #1,%d2
    addq.l #1,%d3
    clr.b 0(%a2,%d3.w)
    lea shell_length(%pc),%a0
    move.b %d3,(%a0)
    lea shell_char(%pc),%a0
    move.b %d0,(%a0)
    clr.b 1(%a0)
    move.l %a0,-(%sp)
    bsr shell_puts
    addq.l #4,%sp
    bra.s 3b
4:  cmpi.l #1,8(%a3)
    bne shell_complete_done
    btst #4,79(%a3)
    beq shell_complete_done
    cmpi.l #63,%d3
    bhs shell_complete_done
    move.b #92,0(%a2,%d3.w)
    addq.l #1,%d3
    clr.b 0(%a2,%d3.w)
    lea shell_length(%pc),%a0
    move.b %d3,(%a0)
    pea shell_slash_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
shell_complete_done:
    movem.l (%sp)+,%d2-%d4/%a2-%a3
    bra shell_count

shell_enter:
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bsr shell_history_save
    lea shell_foreground_attached(%pc),%a0
    clr.b (%a0)
    bsr shell_execute
    lea shell_foreground_attached(%pc),%a0
    tst.b (%a0)
    bne.s shell_enter_no_prompt
    bsr shell_print_prompt
shell_enter_no_prompt:
    lea shell_length(%pc),%a0
    clr.b (%a0)
    lea shell_buffer(%pc),%a0
    clr.b (%a0)
    bra shell_count

shell_history_save:
    lea shell_buffer(%pc),%a0
    tst.b (%a0)
    beq.s 4f
    lea shell_history_next(%pc),%a1
    moveq #0,%d0
    move.b (%a1),%d0
    lsl.l #6,%d0
    lea shell_history(%pc),%a1
    adda.l %d0,%a1
1:  move.b (%a0)+,(%a1)+
    bne.s 1b
    lea shell_history_next(%pc),%a0
    addq.b #1,(%a0)
    andi.b #7,(%a0)
    lea shell_history_count(%pc),%a0
    cmpi.b #8,(%a0)
    bhs.s 3f
    addq.b #1,(%a0)
3:  lea shell_history_view(%pc),%a0
    clr.b (%a0)
4:  rts

shell_history_recall:
    lea shell_history_count(%pc),%a0
    moveq #0,%d0
    move.b (%a0),%d0
    beq shell_count
    lea shell_history_view(%pc),%a1
    moveq #0,%d1
    move.b (%a1),%d1
    cmp.l %d0,%d1
    bhs.s 1f
    addq.b #1,(%a1)
    addq.l #1,%d1
1:  lea shell_history_next(%pc),%a0
    moveq #0,%d0
    move.b (%a0),%d0
    sub.l %d1,%d0
    andi.l #7,%d0
    lsl.l #6,%d0
    lea shell_history(%pc),%a0
    adda.l %d0,%a0
    bsr shell_history_replace
    bra shell_count

shell_history_forward:
    lea shell_history_view(%pc),%a0
    tst.b (%a0)
    beq shell_count
    subq.b #1,(%a0)
    moveq #0,%d1
    move.b (%a0),%d1
    beq.s 1f
    lea shell_history_next(%pc),%a0
    moveq #0,%d0
    move.b (%a0),%d0
    sub.l %d1,%d0
    andi.l #7,%d0
    lsl.l #6,%d0
    lea shell_history(%pc),%a0
    adda.l %d0,%a0
    bra.s 2f
1:  lea shell_empty_text(%pc),%a0
2:  bsr shell_history_replace
    bra shell_count

shell_history_replace:
    movem.l %d2/%a2,-(%sp)
    move.l %a0,%a2
    lea shell_length(%pc),%a0
    moveq #0,%d2
    move.b (%a0),%d2
1:  tst.l %d2
    beq.s 2f
    pea shell_backspace_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    subq.l #1,%d2
    bra.s 1b
2:  move.l %a2,%a0
    lea shell_buffer(%pc),%a1
    moveq #0,%d2
3:  move.b (%a0)+,(%a1)+
    beq.s 4f
    addq.l #1,%d2
    bra.s 3b
4:  lea shell_length(%pc),%a0
    move.b %d2,(%a0)
    move.l %a2,-(%sp)
    bsr shell_puts
    addq.l #4,%sp
    movem.l (%sp)+,%d2/%a2
    rts

shell_count:
    lea shell_keys(%pc),%a0
    addq.l #1,(%a0)
shell_consumed:
    bsr shell_cursor_show
    moveq #1,%d0
shell_pump_done:
    rts

shell_cursor_update:
    lea shell_timer(%pc),%a0
    move.l (%a0),%a0
    move.l 12(%a0),%a0
    jsr (%a0)
    lea shell_cursor_tick(%pc),%a0
    move.l %d0,%d1
    sub.l (%a0),%d1
    cmpi.l #25,%d1
    blo.s shell_cursor_idle
    move.l %d0,(%a0)
    lea shell_cursor_visible(%pc),%a0
    tst.b (%a0)
    beq.s shell_cursor_show
    bra.s shell_cursor_hide
shell_cursor_idle:
    moveq #0,%d0
    rts

shell_cursor_show:
    lea shell_cursor_visible(%pc),%a0
    tst.b (%a0)
    bne.s shell_cursor_done
    move.b #1,(%a0)
    pea shell_cursor_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bsr shell_cursor_stamp
shell_cursor_done:
    rts

shell_cursor_hide:
    lea shell_cursor_visible(%pc),%a0
    tst.b (%a0)
    beq.s shell_cursor_done
    clr.b (%a0)
    pea shell_backspace_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_space_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_backspace_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bsr shell_cursor_stamp
    rts

shell_cursor_stamp:
    lea shell_timer(%pc),%a0
    move.l (%a0),%a0
    move.l 12(%a0),%a0
    jsr (%a0)
    lea shell_cursor_tick(%pc),%a0
    move.l %d0,(%a0)
    rts

shell_execute:
    lea shell_buffer(%pc),%a0
    tst.b (%a0)
    beq shell_execute_done
    lea shell_cmd_help(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_help
    lea shell_buffer(%pc),%a0
    lea shell_cmd_uptime(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_uptime
    lea shell_buffer(%pc),%a0
    lea shell_cmd_cpu(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_cpu
    lea shell_buffer(%pc),%a0
    lea shell_cmd_tasks(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_tasks
    lea shell_buffer(%pc),%a0
    lea shell_cmd_jobs(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_jobs
    lea shell_buffer(%pc),%a0
    lea shell_cmd_memory(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_memory
    lea shell_buffer(%pc),%a0
    lea shell_cmd_inputstat(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_inputstat
    lea shell_buffer(%pc),%a0
    lea shell_cmd_dir(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_dir
    lea shell_buffer(%pc),%a0
    lea shell_cmd_ls(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_ls
    lea shell_buffer(%pc),%a0
    lea shell_cmd_cd(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_cd
    lea shell_buffer(%pc),%a0
    lea shell_cmd_type(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_type
    lea shell_buffer(%pc),%a0
    lea shell_cmd_cat(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_type
    lea shell_buffer(%pc),%a0
    lea shell_cmd_pwd(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_pwd
    lea shell_buffer(%pc),%a0
    lea shell_cmd_clear(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_clear
    lea shell_buffer(%pc),%a0
    lea shell_cmd_cls(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_clear
    lea shell_buffer(%pc),%a0
    lea shell_cmd_start(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_start_background
    lea shell_buffer(%pc),%a0
    lea shell_cmd_startfg(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_start_foreground
    lea shell_buffer(%pc),%a0
    lea shell_cmd_run(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_run
    lea shell_buffer(%pc),%a0
    lea shell_cmd_path(%pc),%a1
    bsr shell_same
    tst.l %d0
    bne shell_do_path_display
    lea shell_buffer(%pc),%a0
    lea shell_cmd_kill(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_kill
    lea shell_buffer(%pc),%a0
    lea shell_cmd_maps(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_maps
    lea shell_buffer(%pc),%a0
    lea shell_cmd_watchdog(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_watchdog
    lea shell_buffer(%pc),%a0
    lea shell_cmd_fg(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_fg
    lea shell_buffer(%pc),%a0
    lea shell_cmd_bg(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_bg
    lea shell_buffer(%pc),%a0
    lea shell_cmd_stop(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_stop
    lea shell_buffer(%pc),%a0
    lea shell_cmd_wait(%pc),%a1
    bsr shell_command_arg
    tst.l %d0
    bne shell_do_wait
    lea shell_buffer(%pc),%a0
    cmpi.b #':',1(%a0)
    bne.s shell_unknown_command
    tst.b 2(%a0)
    bne.s shell_unknown_command
    moveq #0,%d0
    move.b (%a0),%d0
    cmpi.b #'a',%d0
    blo.s shell_drive_upper_ready
    cmpi.b #'z',%d0
    bhi.s shell_unknown_command
    subi.b #32,%d0
shell_drive_upper_ready:
    cmpi.b #'C',%d0
    blo.s shell_unknown_command
    cmpi.b #'P',%d0
    bhi.s shell_unknown_command
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 84(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    tst.l %d0
    beq shell_drive_unavailable
    lea shell_current_path(%pc),%a0
    move.b #92,(%a0)
    clr.b 1(%a0)
    pea shell_drive_ready_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts
shell_drive_unavailable:
    pea shell_drive_unavailable_text(%pc)
    bra shell_error_text
shell_unknown_command:
    pea shell_unknown(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts

shell_do_path_display:
    moveq #0,%d2
1:  pea shell_exec_path(%pc)
    move.l %d2,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 160(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    tst.l %d0
    beq.s 3f
    tst.l %d2
    beq.s 2f
    pea shell_path_separator(%pc)
    bsr shell_puts
    addq.l #4,%sp
2:  pea shell_exec_path(%pc)
    bsr shell_puts
    addq.l #4,%sp
    addq.l #1,%d2
    cmpi.l #8,%d2
    blo.s 1b
3:  pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts
shell_do_help:
    pea shell_help(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts
shell_do_uptime:
    pea shell_uptime_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_timer(%pc),%a0
    move.l (%a0),%a0
    move.l 12(%a0),%a0
    jsr (%a0)
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_ticks_suffix(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts
shell_do_tasks:
    bra shell_do_tasks_body
shell_do_cpu:
    pea shell_cpu_info(%pc)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 140(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    tst.l %d0
    beq shell_cpu_unavailable
    pea shell_cpu_family_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_cpu_info(%pc),%a0
    moveq #0,%d0
    move.b (%a0),%d0
    cmpi.b #1,%d0
    beq.s 1f
    cmpi.b #2,%d0
    beq.s 2f
    cmpi.b #3,%d0
    beq.s 3f
    cmpi.b #4,%d0
    beq.s 4f
    cmpi.b #5,%d0
    beq.s 5f
    pea shell_cpu_68000(%pc)
    bra.s 6f
1:  pea shell_cpu_68010(%pc)
    bra.s 6f
2:  pea shell_cpu_68020(%pc)
    bra.s 6f
3:  pea shell_cpu_68030(%pc)
    bra.s 6f
4:  pea shell_cpu_68040(%pc)
    bra.s 6f
5:  pea shell_cpu_68060(%pc)
6:
    bsr shell_puts
    addq.l #4,%sp
    pea shell_cpu_vendor_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_cpu_machine_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_cpu_info(%pc),%a0
    move.b 2(%a0),%d0
    cmpi.b #1,%d0
    beq.s 11f
    cmpi.b #2,%d0
    beq.s 12f
    cmpi.b #3,%d0
    beq.s 13f
    cmpi.b #4,%d0
    beq.s 14f
    cmpi.b #5,%d0
    beq.s 15f
    pea shell_machine_st(%pc)
    bra.s 16f
11: pea shell_machine_megast(%pc)
    bra.s 16f
12: pea shell_machine_ste(%pc)
    bra.s 16f
13: pea shell_machine_megaste(%pc)
    bra.s 16f
14: pea shell_machine_tt(%pc)
    bra.s 16f
15: pea shell_machine_falcon(%pc)
16: bsr shell_puts
    addq.l #4,%sp
    pea shell_cpu_clock_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_cpu_info(%pc),%a0
    tst.b 7(%a0)
    beq.s 17f
    moveq #0,%d0
    move.b 8(%a0),%d0
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_cpu_mhz_text(%pc)
    bra.s 18f
17: pea shell_cpu_unknown_text(%pc)
18: bsr shell_puts
    addq.l #4,%sp
    pea shell_cpu_address_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    moveq #0,%d0
    lea shell_cpu_info(%pc),%a0
    move.b 3(%a0),%d0
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_cpu_bits_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_cpu_mmu_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_cpu_info(%pc),%a0
    tst.b 4(%a0)
    beq.s 2f
    pea shell_cpu_present(%pc)
    bra.s 3f
2:  pea shell_cpu_absent(%pc)
3:  bsr shell_puts
    addq.l #4,%sp
    pea shell_cpu_cache_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_cpu_info(%pc),%a0
    tst.b 5(%a0)
    beq.s 4f
    tst.b 6(%a0)
    beq.s 5f
    pea shell_cpu_cache_id(%pc)
    bra.s 6f
5:  pea shell_cpu_cache_i(%pc)
    bra.s 6f
4:  pea shell_cpu_absent(%pc)
6:  bsr shell_puts
    addq.l #4,%sp
    pea shell_cpu_fpu_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_cpu_info(%pc),%a0
    move.b 1(%a0),%d0
    beq.s 7f
    cmpi.b #2,%d0
    beq.s 8f
    pea shell_cpu_fpu_external(%pc)
    bra.s 9f
8:  pea shell_cpu_fpu_integrated(%pc)
    bra.s 9f
7:  pea shell_cpu_absent(%pc)
9:  bsr shell_puts
    addq.l #4,%sp
    rts
shell_cpu_unavailable:
    pea shell_cpu_unavailable_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts
shell_do_tasks_body:
    pea shell_tasks_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_proc(%pc),%a0
    move.l (%a0),%a0
    move.l 8(%a0),%a0
    jsr (%a0)
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts
shell_do_jobs:
    movem.l %d2/%a2,-(%sp)
    pea shell_jobs_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    moveq #0,%d2
    lea shell_jobs_found(%pc),%a0
    clr.b (%a0)
shell_jobs_loop:
    pea shell_job_info(%pc)
    move.l %d2,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 100(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    tst.l %d0
    beq shell_jobs_next
    lea shell_jobs_found(%pc),%a0
    move.b #1,(%a0)
    pea shell_job_name(%pc)
    move.l %d2,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 104(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    pea shell_name_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_job_name(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_job_separator(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_pid_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_job_info(%pc),%a2
    move.l (%a2),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_ppid_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l 4(%a2),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_state_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l 8(%a2),%d0
    lea shell_state_unknown(%pc),%a0
    cmpi.l #1,%d0
    bne.s 1f
    lea shell_state_ready(%pc),%a0
    bra.s 5f
1:  cmpi.l #2,%d0
    bne.s 2f
    lea shell_state_running(%pc),%a0
    bra.s 5f
2:  cmpi.l #3,%d0
    bne.s 3f
    lea shell_state_sleeping(%pc),%a0
    bra.s 5f
3:  cmpi.l #4,%d0
    bne.s 4f
    lea shell_state_waiting(%pc),%a0
    bra.s 5f
4:  cmpi.l #5,%d0
    bne.s 6f
    lea shell_state_terminated(%pc),%a0
    bra.s 5f
6:  cmpi.l #6,%d0
    bne.s 7f
    lea shell_state_input(%pc),%a0
    bra.s 5f
7:  cmpi.l #7,%d0
    bne.s 8f
    lea shell_state_suspended(%pc),%a0
    bra.s 5f
8:  cmpi.l #8,%d0
    bne.s 5f
    lea shell_state_message(%pc),%a0
5:  move.l %a0,-(%sp)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_job_resources(%pc)
    move.l %d2,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 112(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    tst.l %d0
    beq shell_jobs_next
    pea shell_memory_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_job_resources(%pc),%a2
    move.l (%a2),%d0
    add.l 4(%a2),%d0
    add.l 8(%a2),%d0
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_files_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l 12(%a2),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_stack_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l %d2,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 144(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_stack_separator(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_job_resources(%pc),%a2
    move.l 4(%a2),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_cpu_ticks_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l %d2,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 132(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
shell_jobs_next:
    addq.l #1,%d2
    cmpi.l #4,%d2
    blo shell_jobs_loop
    lea shell_jobs_found(%pc),%a0
    tst.b (%a0)
    bne.s shell_jobs_done
    pea shell_no_jobs(%pc)
    bsr shell_puts
    addq.l #4,%sp
shell_jobs_done:
    movem.l (%sp)+,%d2/%a2
    rts

shell_do_kill:
    move.l %d0,%a0
    bsr shell_parse_u32
    tst.l %d1
    beq shell_kill_usage
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 108(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    tst.l %d0
    bmi shell_kill_busy
    beq shell_kill_not_found
    pea shell_kill_ok_text(%pc)
    bra shell_error_text
shell_kill_usage:
    pea shell_kill_usage_text(%pc)
    bra shell_error_text
shell_kill_busy:
    pea shell_kill_busy_text(%pc)
    bra shell_error_text
shell_kill_not_found:
    pea shell_kill_not_found_text(%pc)
    bra shell_error_text

shell_do_watchdog:
    move.l %d0,%a0
    move.l %a0,%a1
1:  tst.b (%a1)
    beq shell_watchdog_usage
    cmpi.b #' ',(%a1)
    beq.s 2f
    addq.l #1,%a1
    bra.s 1b
2:  clr.b (%a1)+
3:  cmpi.b #' ',(%a1)
    bne.s 4f
    addq.l #1,%a1
    bra.s 3b
4:  move.l %a1,%a2
    bsr shell_parse_u32
    tst.l %d1
    beq shell_watchdog_usage
    lea shell_watchdog_pid(%pc),%a0
    move.l %d0,(%a0)
    move.l %a2,%a0
    bsr shell_parse_u32
    tst.l %d1
    beq shell_watchdog_usage
    move.l %d0,-(%sp)
    lea shell_watchdog_pid(%pc),%a0
    move.l (%a0),-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 136(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    tst.l %d0
    bmi shell_watchdog_invalid
    beq shell_watchdog_not_found
    pea shell_watchdog_ok_text(%pc)
    bra shell_error_text
shell_watchdog_usage:
    pea shell_watchdog_usage_text(%pc)
    bra shell_error_text
shell_watchdog_invalid:
    pea shell_watchdog_invalid_text(%pc)
    bra shell_error_text
shell_watchdog_not_found:
    pea shell_kill_not_found_text(%pc)
    bra shell_error_text

shell_do_fg:
    move.l %d0,%a0
    bsr shell_parse_u32
    tst.l %d1
    beq shell_fg_usage
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 116(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    tst.l %d0
    beq shell_fg_failed
    lea shell_foreground_attached(%pc),%a0
    move.b #1,(%a0)
    pea shell_fg_ok_text(%pc)
    bra shell_error_text
shell_fg_usage:
    pea shell_fg_usage_text(%pc)
    bra shell_error_text
shell_fg_failed:
    pea shell_fg_failed_text(%pc)
    bra shell_error_text
shell_do_bg:
    move.l %d0,%a0
    bsr shell_parse_u32
    tst.l %d1
    beq shell_bg_usage
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 120(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    tst.l %d0
    bmi shell_bg_not_suspended
    beq shell_bg_not_found
    pea shell_bg_ok_text(%pc)
    bra shell_error_text
shell_bg_usage:
    pea shell_bg_usage_text(%pc)
    bra shell_error_text
shell_bg_not_suspended:
    pea shell_bg_not_suspended_text(%pc)
    bra shell_error_text
shell_bg_not_found:
    pea shell_bg_not_found_text(%pc)
    bra shell_error_text
shell_do_stop:
    move.l %d0,%a0
    bsr shell_parse_u32
    tst.l %d1
    beq shell_stop_usage
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 124(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    tst.l %d0
    bmi shell_stop_invalid_state
    beq shell_stop_not_found
    pea shell_stop_ok_text(%pc)
    bra shell_error_text
shell_stop_usage:
    pea shell_stop_usage_text(%pc)
    bra shell_error_text
shell_stop_invalid_state:
    pea shell_stop_invalid_state_text(%pc)
    bra shell_error_text
shell_stop_not_found:
    pea shell_stop_not_found_text(%pc)
    bra shell_error_text
shell_do_maps:
    move.l %a2,-(%sp)
    move.l %d0,%a0
    bsr shell_parse_u32
    tst.l %d1
    beq shell_maps_usage
    pea shell_map_info(%pc)
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 156(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    tst.l %d0
    beq shell_maps_not_found
    pea shell_maps_pid_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_map_info(%pc),%a2
    move.l (%a2),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_maps_domain_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l 4(%a2),-(%sp)
    bsr shell_put_hex32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l 12(%a2),-(%sp)
    move.l 8(%a2),-(%sp)
    pea shell_maps_text_label(%pc)
    bsr shell_put_map_range
    lea 12(%sp),%sp
    move.l 20(%a2),-(%sp)
    move.l 16(%a2),-(%sp)
    pea shell_maps_rodata_label(%pc)
    bsr shell_put_map_range
    lea 12(%sp),%sp
    move.l 28(%a2),-(%sp)
    move.l 24(%a2),-(%sp)
    pea shell_maps_data_label(%pc)
    bsr shell_put_map_range
    lea 12(%sp),%sp
    move.l 36(%a2),-(%sp)
    move.l 32(%a2),-(%sp)
    pea shell_maps_policy_label(%pc)
    bsr shell_put_map_range
    lea 12(%sp),%sp
    move.l 44(%a2),-(%sp)
    move.l 40(%a2),-(%sp)
    pea shell_maps_bss_label(%pc)
    bsr shell_put_map_range
    lea 12(%sp),%sp
    move.l 52(%a2),-(%sp)
    move.l 48(%a2),-(%sp)
    pea shell_maps_stack_label(%pc)
    bsr shell_put_map_range
    lea 12(%sp),%sp
    pea shell_maps_image_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    tst.l 56(%a2)
    beq.s 1f
    pea shell_maps_shared_text(%pc)
    bra.s 2f
1:  pea shell_maps_private_text(%pc)
2:  bsr shell_puts
    addq.l #4,%sp
    pea shell_maps_stack_used_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l 60(%a2),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    move.l (%sp)+,%a2
    pea shell_maps_bytes_text(%pc)
    bra shell_error_text
shell_maps_usage:
    move.l (%sp)+,%a2
    pea shell_maps_usage_text(%pc)
    bra shell_error_text
shell_maps_not_found:
    move.l (%sp)+,%a2
    pea shell_kill_not_found_text(%pc)
    bra shell_error_text
shell_do_wait:
    move.l %d0,%a0
    bsr shell_parse_u32
    tst.l %d1
    beq shell_wait_usage
    move.l %d0,%d2
    pea shell_wait_status(%pc)
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 128(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    tst.l %d0
    bmi shell_wait_running
    beq shell_wait_not_found
    pea shell_wait_pid_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l %d2,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    lea shell_wait_status(%pc),%a0
    move.l (%a0),%d0
    cmpi.l #-3,%d0
    beq.s shell_wait_killed
    cmpi.l #-4,%d0
    beq.s shell_wait_watchdog
    bmi.s shell_wait_fault
    pea shell_wait_exit_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_wait_status(%pc),%a0
    move.l (%a0),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bra shell_error_text
shell_wait_killed:
    pea shell_wait_killed_text(%pc)
    bra shell_error_text
shell_wait_watchdog:
    pea shell_wait_watchdog_text(%pc)
    bra shell_error_text
shell_wait_fault:
    neg.l %d0
    move.l %d0,%d2
    pea shell_wait_fault_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l %d2,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bra shell_error_text
shell_wait_usage:
    pea shell_wait_usage_text(%pc)
    bra shell_error_text
shell_wait_running:
    pea shell_wait_running_text(%pc)
    bra shell_error_text
shell_wait_not_found:
    pea shell_wait_not_found_text(%pc)
    bra shell_error_text
shell_do_inputstat:
    pea shell_input_dropped_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_input(%pc),%a0
    move.l (%a0),%a0
    move.l 16(%a0),%a0
    jsr (%a0)
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_input_pending_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l #0,-(%sp)
    lea shell_event(%pc),%a0
    move.l (%a0),%a0
    move.l 16(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts
shell_do_memory:
    pea shell_st_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_st_stats(%pc)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 52(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    lea shell_st_stats(%pc),%a0
    move.l 4(%a0),%d0
    lsr.l #8,%d0
    lsr.l #2,%d0
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_tt_label(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_tt_stats(%pc)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 56(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    lea shell_tt_stats(%pc),%a0
    move.l 4(%a0),%d0
    lsr.l #8,%d0
    lsr.l #2,%d0
    move.l %d0,-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_kb_suffix(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts

shell_do_dir:
    move.l %d0,%a0
    bsr shell_make_path
    tst.l %d0
    beq shell_bad_path
    move.l #0,-(%sp)
    pea shell_dir_entry(%pc)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%a0
    jsr (%a0)
    lea 12(%sp),%sp
    tst.l %d0
    beq shell_not_found
    rts

/* Accept Linux-style "ls -l" while keeping the compact FAT listing format. */
shell_do_ls:
    move.l %d0,%a0
    cmpi.b #'-',(%a0)
    bne shell_do_dir
    cmpi.b #'l',1(%a0)
    bne shell_do_dir
    tst.b 2(%a0)
    beq.s shell_ls_current
    cmpi.b #' ',2(%a0)
    bne shell_do_dir
    addq.l #3,%a0
shell_ls_skip_spaces:
    cmpi.b #' ',(%a0)
    bne.s shell_ls_ready
    addq.l #1,%a0
    bra.s shell_ls_skip_spaces
shell_ls_ready:
    move.l %a0,%d0
    bra shell_do_dir
shell_ls_current:
    addq.l #2,%a0
    move.l %a0,%d0
    bra shell_do_dir

shell_do_pwd:
    lea shell_current_path(%pc),%a0
    move.l %a0,-(%sp)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts

shell_do_clear:
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 40(%a0),%a0
    jmp (%a0)

shell_do_cd:
    move.l %d0,%a0
    tst.b (%a0)
    beq shell_bad_path
    cmpi.b #'.',(%a0)
    bne.s shell_cd_regular
    cmpi.b #'.',1(%a0)
    bne.s shell_cd_regular
    tst.b 2(%a0)
    bne.s shell_cd_regular
    bsr shell_parent_path
    rts
shell_cd_regular:
    lea shell_cd_argument(%pc),%a1
    move.l %a0,(%a1)
    bsr shell_make_path
    tst.l %d0
    beq shell_bad_path
    pea shell_attributes(%pc)
    pea shell_file_size(%pc)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 16(%a0),%a0
    jsr (%a0)
    lea 12(%sp),%sp
    tst.l %d0
    bne.s shell_cd_stat_ok
    /* Root-only retry with the unmodified relative component. */
    lea shell_current_path(%pc),%a0
    cmpi.b #92,(%a0)
    bne shell_not_found
    tst.b 1(%a0)
    bne shell_not_found
    lea shell_cd_argument(%pc),%a0
    move.l (%a0),%a0
    cmpi.b #92,(%a0)
    beq shell_not_found
    cmpi.b #'/',(%a0)
    beq shell_not_found
    pea shell_attributes(%pc)
    pea shell_file_size(%pc)
    move.l %a0,-(%sp)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 16(%a0),%a0
    jsr (%a0)
    lea 12(%sp),%sp
    tst.l %d0
    beq shell_not_found
shell_cd_stat_ok:
    lea shell_attributes(%pc),%a0
    moveq #0,%d0
    move.b (%a0),%d0
    andi.b #0x10,%d0
    beq shell_not_directory
    lea shell_work_path(%pc),%a0
    lea shell_current_path(%pc),%a1
    bsr shell_copy_string
    rts

shell_do_type:
    move.l %d0,%a0
    tst.b (%a0)
    beq shell_type_usage
    bsr shell_make_path
    tst.l %d0
    beq shell_bad_path
    pea shell_attributes(%pc)
    pea shell_file_size(%pc)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 16(%a0),%a0
    jsr (%a0)
    lea 12(%sp),%sp
    tst.l %d0
    beq shell_not_found
    lea shell_attributes(%pc),%a0
    btst #4,(%a0)
    bne shell_is_directory
    lea shell_stream_offset(%pc),%a0
    clr.l (%a0)
shell_type_read_block:
    pea shell_loaded_size(%pc)
    move.l #512,-(%sp)
    pea shell_file_buffer(%pc)
    lea shell_stream_offset(%pc),%a0
    move.l (%a0),-(%sp)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 28(%a0),%a0
    jsr (%a0)
    lea 20(%sp),%sp
    tst.l %d0
    beq shell_read_error
    lea shell_file_buffer(%pc),%a0
    lea shell_loaded_size(%pc),%a1
    move.l (%a1),%d1
    beq shell_type_done
shell_type_loop:
    tst.l %d1
    beq shell_type_block_done
    move.b (%a0)+,%d0
    cmpi.b #10,%d0
    beq.s shell_type_emit
    cmpi.b #13,%d0
    beq.s shell_type_skip
    cmpi.b #9,%d0
    beq.s shell_type_emit
    cmpi.b #32,%d0
    blo.s shell_type_dot
    cmpi.b #126,%d0
    bls.s shell_type_emit
shell_type_dot:
    moveq #'.',%d0
shell_type_emit:
    lea shell_char(%pc),%a1
    move.b %d0,(%a1)
    clr.b 1(%a1)
    move.l %a0,-(%sp)
    move.l %d1,-(%sp)
    move.l %a1,-(%sp)
    bsr shell_puts
    addq.l #4,%sp
    move.l (%sp)+,%d1
    move.l (%sp)+,%a0
shell_type_skip:
    subq.l #1,%d1
    bra.s shell_type_loop
shell_type_block_done:
    lea shell_stream_offset(%pc),%a0
    lea shell_loaded_size(%pc),%a1
    move.l (%a1),%d0
    add.l %d0,(%a0)
    lea shell_file_size(%pc),%a1
    move.l (%a0),%d0
    cmp.l (%a1),%d0
    blo shell_type_read_block
shell_type_done:
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts

shell_do_start_background:
    lea shell_start_foreground(%pc),%a1
    clr.b (%a1)
    bra.s shell_do_start
shell_do_start_foreground:
    lea shell_start_foreground(%pc),%a1
    move.b #1,(%a1)
shell_do_start:
    move.l %d0,%a0
    tst.b (%a0)
    beq shell_start_usage
    lea shell_start_filename(%pc),%a1
    move.l %a0,(%a1)
    move.l %a0,%a1
1:  tst.b (%a1)
    beq.s 3f
    cmpi.b #' ',(%a1)
    beq.s 2f
    addq.l #1,%a1
    bra.s 1b
2:  clr.b (%a1)+
    cmpi.b #' ',(%a1)
    beq.s 2b
3:  lea shell_start_arguments(%pc),%a0
    move.l %a1,(%a0)
    lea shell_start_filename(%pc),%a0
    move.l (%a0),%a0
    lea shell_path_retry(%pc),%a1
    clr.b (%a1)
    bsr shell_make_path
    tst.l %d0
    beq shell_bad_path
shell_start_launch:
    pea shell_start_pid(%pc)
    pea shell_current_path(%pc)
    lea shell_start_arguments(%pc),%a0
    move.l (%a0),-(%sp)
    pea shell_work_path(%pc)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 96(%a0),%a0
    jsr (%a0)
    lea 16(%sp),%sp
    tst.l %d0
    bpl.s shell_start_launched
    lea shell_path_retry(%pc),%a1
    tst.b (%a1)
    bne.s shell_start_next_path
    lea shell_start_filename(%pc),%a0
    move.l (%a0),%a0
    bsr shell_simple_program
    tst.l %d0
    beq shell_start_failed
shell_start_next_path:
    lea shell_path_retry(%pc),%a1
    addq.b #1,(%a1)
    lea shell_start_filename(%pc),%a0
    move.l (%a0),%a0
    bsr shell_make_exec_path
    tst.l %d0
    beq shell_start_failed
    bra shell_start_launch
shell_start_launched:
    lea shell_start_foreground(%pc),%a0
    tst.b (%a0)
    beq.s shell_start_report
    lea shell_start_pid(%pc),%a0
    move.l (%a0),-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 116(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    tst.l %d0
    beq shell_start_failed
    lea shell_foreground_attached(%pc),%a0
    move.b #1,(%a0)
shell_start_report:
    pea shell_start_ok_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_start_pid(%pc),%a0
    move.l (%a0),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts

/* TOS .PRG (0x601A): the kernel loads/relocates it, builds the basepage and
   runs it with the TOS trap vectors. */
shell_run_tos:
    pea shell_work_path(%pc)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 164(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    move.l %d0,-(%sp)
    pea shell_tos_exit_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts

shell_do_run:
    move.l %d0,%a0
    tst.b (%a0)
    beq shell_run_usage
    lea shell_run_filename(%pc),%a1
    move.l %a0,(%a1)
    move.l %a0,%a1
1:  tst.b (%a1)
    beq.s 3f
    cmpi.b #' ',(%a1)
    beq.s 2f
    addq.l #1,%a1
    bra.s 1b
2:  clr.b (%a1)+
    cmpi.b #' ',(%a1)
    beq.s 2b
3:  move.l %a1,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 92(%a0),%a0
    jsr (%a0)
    addq.l #4,%sp
    lea shell_run_filename(%pc),%a0
    move.l (%a0),%a0
    lea shell_path_retry(%pc),%a1
    clr.b (%a1)
    bsr shell_make_path
    tst.l %d0
    beq shell_bad_path
shell_run_stat:
    pea shell_attributes(%pc)
    pea shell_file_size(%pc)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 16(%a0),%a0
    jsr (%a0)
    lea 12(%sp),%sp
    tst.l %d0
    bne.s shell_run_stat_ok
    lea shell_path_retry(%pc),%a1
    tst.b (%a1)
    bne.s shell_run_next_path
    lea shell_run_filename(%pc),%a0
    move.l (%a0),%a0
    bsr shell_simple_program
    tst.l %d0
    beq shell_not_found
shell_run_next_path:
    lea shell_path_retry(%pc),%a1
    addq.b #1,(%a1)
    lea shell_run_filename(%pc),%a0
    move.l (%a0),%a0
    bsr shell_make_exec_path
    tst.l %d0
    beq shell_not_found
    bra shell_run_stat
shell_run_stat_ok:
    lea shell_attributes(%pc),%a0
    btst #4,(%a0)
    bne shell_is_directory
    lea shell_file_size(%pc),%a0
    move.l (%a0),%d0
    cmpi.l #32,%d0
    blo shell_run_invalid
    cmpi.l #262144,%d0
    bhi shell_run_invalid
    pea shell_loaded_size(%pc)
    move.l #32,-(%sp)
    pea shell_app_header(%pc)
    move.l #0,-(%sp)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 28(%a0),%a0
    jsr (%a0)
    lea 20(%sp),%sp
    tst.l %d0
    beq shell_run_read_error
    lea shell_loaded_size(%pc),%a0
    cmpi.l #32,(%a0)
    bne shell_run_invalid
    lea shell_app_header(%pc),%a0
    cmpi.w #0x601a,(%a0)
    beq shell_run_tos
    cmpi.l #0x41535953,(%a0)
    bne shell_run_invalid
    cmpi.l #0x41505030,4(%a0)
    bne shell_run_invalid
    cmpi.w #1,8(%a0)
    bne shell_run_invalid
    cmpi.w #32,10(%a0)
    bne shell_run_invalid
    lea shell_file_size(%pc),%a1
    move.l 12(%a0),%d0
    cmp.l (%a1),%d0
    bne shell_run_invalid
    move.l 16(%a0),%d0
    cmp.l (%a1),%d0
    blo shell_run_invalid
    cmpi.l #262144,%d0
    bhi shell_run_invalid
    move.l 20(%a0),%d1
    cmpi.l #32,%d1
    blo shell_run_invalid
    cmp.l (%a1),%d1
    bhs shell_run_invalid
    move.l 24(%a0),%d1
    cmpi.l #1024,%d1
    blo shell_run_invalid
    cmpi.l #16384,%d1
    bhi shell_run_invalid
    btst #0,%d1
    bne shell_run_invalid

    /* Executable image: read/write/execute/supervisor, TT-RAM preferred. */
    move.l #1,-(%sp)
    move.l #15,-(%sp)
    move.l #0x0200,-(%sp)
    move.l #16,-(%sp)
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 8(%a0),%a0
    jsr (%a0)
    lea 20(%sp),%sp
    tst.l %d0
    beq shell_run_no_memory
    lea shell_app_image(%pc),%a0
    move.l %d0,(%a0)

    pea shell_loaded_size(%pc)
    lea shell_file_size(%pc),%a0
    move.l (%a0),-(%sp)
    lea shell_app_image(%pc),%a0
    move.l (%a0),-(%sp)
    move.l #0,-(%sp)
    pea shell_work_path(%pc)
    lea shell_vfs(%pc),%a0
    move.l (%a0),%a0
    move.l 28(%a0),%a0
    jsr (%a0)
    lea 20(%sp),%sp
    tst.l %d0
    beq shell_run_loaded_read_error
    lea shell_loaded_size(%pc),%a0
    move.l (%a0),%d0
    lea shell_file_size(%pc),%a1
    cmp.l (%a1),%d0
    bne shell_run_loaded_read_error
    bsr shell_verify_app_checksum
    tst.l %d0
    beq shell_run_bad_checksum
    bsr shell_zero_app_bss

    lea shell_app_image(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%d0
    addq.l #8,%d0              /* lower + upper kernel stack guards */
    move.l #0,-(%sp)
    move.l #11,-(%sp)
    move.l #0x0201,-(%sp)
    move.l #16,-(%sp)
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 8(%a0),%a0
    jsr (%a0)
    lea 20(%sp),%sp
    tst.l %d0
    beq shell_run_stack_failed
    lea shell_app_stack(%pc),%a0
    move.l %d0,(%a0)

    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 80(%a0),%a0
    jsr (%a0)
    /* Enter user mode through the kernel-owned RTE/TRAP boundary. */
    lea shell_app_image(%pc),%a1
    move.l (%a1),%a1
    pea shell_current_path(%pc)
    move.l 16(%a1),-(%sp)
    move.l %a1,-(%sp)
    move.l 24(%a1),%d0
    lea shell_app_stack(%pc),%a0
    move.l (%a0),%a0
    addq.l #4,%a0              /* skip lower guard */
    add.l %d0,%a0
    move.l %a0,-(%sp)
    move.l 20(%a1),%d0
    add.l %d0,%a1
    move.l %a1,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 88(%a0),%a0
    jsr (%a0)
    lea 20(%sp),%sp
    lea shell_app_result(%pc),%a0
    move.l %d0,(%a0)
    bsr shell_free_app_stack
    bsr shell_free_app_image
    lea shell_app_result(%pc),%a0
    move.l (%a0),%d0
    bmi.s shell_app_fault_result
    pea shell_app_exit_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    lea shell_app_result(%pc),%a0
    move.l (%a0),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts
shell_app_fault_result:
    neg.l %d0
    move.l %d0,-(%sp)
    pea shell_app_fault_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts

shell_verify_app_checksum:
    move.l %d2,-(%sp)
    lea shell_app_image(%pc),%a0
    move.l (%a0),%a0
    move.l 28(%a0),%a1
    moveq #0,%d1
    moveq #27,%d0
1:  moveq #0,%d2
    move.b (%a0)+,%d2
    add.l %d2,%d1
    dbra %d0,1b
    addq.l #4,%a0
    lea shell_file_size(%pc),%a1
    move.l (%a1),%d0
    subi.l #32,%d0
    beq.s 3f
    subq.l #1,%d0
2:  moveq #0,%d2
    move.b (%a0)+,%d2
    add.l %d2,%d1
    subq.l #1,%d0
    bpl.s 2b
3:  lea shell_app_image(%pc),%a0
    move.l (%a0),%a0
    cmp.l 28(%a0),%d1
    seq %d0
    andi.l #1,%d0
    move.l (%sp)+,%d2
    rts

shell_zero_app_bss:
    lea shell_app_image(%pc),%a0
    move.l (%a0),%a0
    move.l 12(%a0),%d0
    move.l 16(%a0),%d1
    sub.l %d0,%d1
    beq.s 2f
    add.l %d0,%a0
    subq.l #1,%d1
1:  clr.b (%a0)+
    subq.l #1,%d1
    bpl.s 1b
2:  rts

shell_free_app_stack:
    lea shell_app_stack(%pc),%a0
    move.l (%a0),%d0
    beq.s 1f
    clr.l (%a0)
    move.l #0x0201,-(%sp)
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 12(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
1:  rts

shell_free_app_image:
    lea shell_app_image(%pc),%a0
    move.l (%a0),%d0
    beq.s 1f
    clr.l (%a0)
    move.l #0x0200,-(%sp)
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a0
    move.l (%a0),%a0
    move.l 12(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
1:  rts

shell_run_stack_failed:
    bsr shell_free_app_image
shell_run_no_memory:
    pea shell_run_no_memory_text(%pc)
    bra shell_error_text
shell_run_loaded_read_error:
    bsr shell_free_app_image
shell_run_read_error:
    pea shell_run_read_error_text(%pc)
    bra shell_error_text
shell_run_bad_checksum:
    bsr shell_free_app_image
    pea shell_run_checksum_text(%pc)
    bra shell_error_text
shell_run_invalid:
    pea shell_run_invalid_text(%pc)
    bra shell_error_text
shell_run_usage:
    pea shell_run_usage_text(%pc)
    bra shell_error_text
shell_start_failed:
    pea shell_start_failed_text(%pc)
    bra shell_error_text
shell_start_usage:
    pea shell_start_usage_text(%pc)
    bra shell_error_text

shell_bad_path:
    pea shell_bad_path_text(%pc)
    bra.s shell_error_text
shell_not_found:
    pea shell_not_found_text(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_work_path(%pc)
    bsr shell_puts
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
    rts
shell_not_directory:
    pea shell_not_directory_text(%pc)
    bra.s shell_error_text
shell_is_directory:
    pea shell_is_directory_text(%pc)
    bra.s shell_error_text
shell_read_error:
    pea shell_read_error_text(%pc)
shell_error_text:
    bsr shell_puts
    addq.l #4,%sp
shell_execute_done:
    rts

shell_type_usage:
    pea shell_type_usage_text(%pc)
    bra shell_error_text

/* Return argument pointer in d0 for an exact command optionally followed by spaces. */
shell_command_arg:
1:  move.b (%a1)+,%d1
    beq.s 2f
    cmp.b (%a0)+,%d1
    bne.s 4f
    bra.s 1b
2:  tst.b (%a0)
    beq.s 3f
    cmpi.b #' ',(%a0)
    bne.s 4f
2:  addq.l #1,%a0
    cmpi.b #' ',(%a0)
    beq.s 2b
3:  move.l %a0,%d0
    rts
4:  moveq #0,%d0
    rts

/* Parse one complete unsigned decimal argument. d1 is nonzero on success. */
shell_parse_u32:
    moveq #0,%d0
    moveq #0,%d1
    tst.b (%a0)
    beq.s 4f
1:  moveq #0,%d2
    move.b (%a0)+,%d2
    beq.s 3f
    cmpi.b #'0',%d2
    blo.s 4f
    cmpi.b #'9',%d2
    bhi.s 4f
    subi.b #'0',%d2
    cmpi.l #429496729,%d0
    bhi.s 4f
    bne.s 2f
    cmpi.b #5,%d2
    bhi.s 4f
2:  move.l %d0,%d1
    lsl.l #2,%d0
    add.l %d1,%d0
    lsl.l #1,%d0
    add.l %d2,%d0
    bra.s 1b
3:  moveq #1,%d1
4:  rts

/* Build an absolute FAT path in shell_work_path. Empty argument means cwd. */
shell_make_path:
    move.l #64,-(%sp)
    pea shell_work_path(%pc)
    move.l %a0,-(%sp)
    pea shell_current_path(%pc)
    bsr shell_build_path
    lea 16(%sp),%sp
    rts

/* Resolve a simple executable name relative to PATH instead of cwd. */
shell_make_exec_path:
    move.l %a0,-(%sp)
    pea shell_exec_path(%pc)
    moveq #0,%d0
    move.b shell_path_retry(%pc),%d0
    subq.l #1,%d0
    move.l %d0,-(%sp)
    lea shell_api(%pc),%a1
    move.l (%a1),%a1
    move.l 160(%a1),%a1
    jsr (%a1)
    addq.l #8,%sp
    tst.l %d0
    beq.s shell_make_exec_drop
    move.l (%sp)+,%a0
    move.l #64,-(%sp)
    pea shell_work_path(%pc)
    move.l %a0,-(%sp)
    pea shell_exec_path(%pc)
    bsr shell_build_path
    lea 16(%sp),%sp
    rts
shell_make_exec_drop:
    addq.l #4,%sp
shell_make_exec_none:
    moveq #0,%d0
    rts

/* D0=1 only when A0 contains a bare filename. Explicit paths and drive
   specifications must never be silently redirected through PATH. */
shell_simple_program:
    moveq #0,%d0
    tst.b (%a0)
    beq.s 2f
1:  move.b (%a0)+,%d1
    beq.s 3f
    cmpi.b #92,%d1
    beq.s 2f
    cmpi.b #'/',%d1
    beq.s 2f
    cmpi.b #':',%d1
    beq.s 2f
    bra.s 1b
2:  rts
3:  moveq #1,%d0
    rts

shell_copy_string:
1:  move.b (%a0)+,(%a1)+
    bne.s 1b
    rts

shell_parent_path:
    lea shell_current_path(%pc),%a0
    move.l %a0,%a1
1:  tst.b (%a1)+
    bne.s 1b
    subq.l #2,%a1
    cmp.l %a0,%a1
    beq.s 3f
2:  cmpi.b #92,(%a1)
    beq.s 3f
    subq.l #1,%a1
    cmp.l %a0,%a1
    bhi.s 2b
3:  cmp.l %a0,%a1
    bne.s 4f
    move.b #92,(%a0)
    clr.b 1(%a0)
    rts
4:  clr.b (%a1)
    rts

/* VFS directory callback: name, size, attributes, context. */
shell_dir_entry:
    move.l 4(%sp),-(%sp)
    bsr shell_puts
    addq.l #4,%sp
    move.l 12(%sp),%d0
    btst #4,%d0
    beq.s 1f
    pea shell_dir_suffix(%pc)
    bsr shell_puts
    addq.l #4,%sp
    bra.s 2f
1:  pea shell_size_separator(%pc)
    bsr shell_puts
    addq.l #4,%sp
    move.l 8(%sp),-(%sp)
    bsr shell_put_u32
    addq.l #4,%sp
    pea shell_newline(%pc)
    bsr shell_puts
    addq.l #4,%sp
2:  moveq #1,%d0
    rts

shell_same:
    moveq #0,%d0
1:  move.b (%a0)+,%d1
    cmp.b (%a1)+,%d1
    bne.s 2f
    tst.b %d1
    bne.s 1b
    moveq #1,%d0
2:  rts

shell_key_count:
    lea shell_keys(%pc),%a0
    move.l (%a0),%d0
    rts

shell_line_active:
    moveq #0,%d0
    lea shell_length(%pc),%a0
    move.b (%a0),%d0
    rts

    .section .rodata
shell_backspace_text: .asciz "\b"
shell_space_text: .asciz " "
shell_cursor_text: .asciz "_"
shell_slash_text: .asciz "\\"
shell_newline: .asciz "\n"
shell_empty_text: .asciz ""
shell_autoexec_name: .asciz "AUTOEXEC.BAT"
shell_cmd_help: .asciz "help"
shell_cmd_uptime: .asciz "uptime"
shell_cmd_cpu: .asciz "cpu"
shell_cmd_tasks: .asciz "tasks"
shell_cmd_jobs: .asciz "jobs"
shell_cmd_memory: .asciz "memory"
shell_cmd_inputstat: .asciz "inputstat"
shell_cmd_dir: .asciz "dir"
shell_cmd_ls: .asciz "ls"
shell_cmd_cd: .asciz "cd"
shell_cmd_type: .asciz "type"
shell_cmd_cat: .asciz "cat"
shell_cmd_pwd: .asciz "pwd"
shell_cmd_clear: .asciz "clear"
shell_cmd_cls: .asciz "cls"
shell_cmd_start: .asciz "start"
shell_cmd_startfg: .asciz "startfg"
shell_cmd_run: .asciz "run"
shell_cmd_path: .asciz "path"
shell_path_separator: .asciz ";"
shell_cmd_kill: .asciz "kill"
shell_cmd_maps: .asciz "maps"
shell_cmd_watchdog: .asciz "watchdog"
shell_cmd_fg: .asciz "fg"
shell_cmd_bg: .asciz "bg"
shell_cmd_stop: .asciz "stop"
shell_cmd_wait: .asciz "wait"
shell_help: .asciz "COMMANDS: help cpu uptime tasks jobs maps kill watchdog stop fg bg wait memory inputstat dir ls cd type cat pwd clear cls run start startfg\n"
shell_cpu_68000: .asciz "68000-COMPATIBLE\n"
shell_cpu_68010: .asciz "68010-COMPATIBLE\n"
shell_cpu_68020: .asciz "68020-COMPATIBLE\n"
shell_cpu_68030: .asciz "68030-COMPATIBLE\n"
shell_cpu_68040: .asciz "68040-COMPATIBLE\n"
shell_cpu_68060: .asciz "68060-COMPATIBLE\n"
shell_cpu_family_label: .asciz "CPU FAMILY: "
shell_cpu_vendor_text: .asciz "VENDOR: NOT IDENTIFIABLE BY 68K ISA\n"
shell_cpu_machine_label: .asciz "MACHINE: "
shell_machine_st: .asciz "ATARI ST\n"
shell_machine_megast: .asciz "ATARI MEGA ST\n"
shell_machine_ste: .asciz "ATARI STE\n"
shell_machine_megaste: .asciz "ATARI MEGA STE\n"
shell_machine_tt: .asciz "ATARI TT\n"
shell_machine_falcon: .asciz "ATARI FALCON\n"
shell_cpu_clock_label: .asciz "SYSTEM CLOCK: "
shell_cpu_mhz_text: .asciz " MHZ\n"
shell_cpu_unknown_text: .asciz "UNKNOWN\n"
shell_cpu_address_label: .asciz "ADDRESS BUS: "
shell_cpu_bits_text: .asciz " BIT\n"
shell_cpu_mmu_label: .asciz "ON-CPU MMU: "
shell_cpu_cache_label: .asciz "ON-CPU CACHE: "
shell_cpu_fpu_label: .asciz "FPU: "
shell_cpu_present: .asciz "PRESENT\n"
shell_cpu_absent: .asciz "NOT PRESENT\n"
shell_cpu_cache_i: .asciz "INSTRUCTION\n"
shell_cpu_cache_id: .asciz "INSTRUCTION + DATA\n"
shell_cpu_fpu_external: .asciz "EXTERNAL 68881/68882-COMPATIBLE (TYPE UNKNOWN)\n"
shell_cpu_fpu_integrated: .asciz "INTEGRATED\n"
shell_cpu_unavailable_text: .asciz "CPU INFORMATION UNAVAILABLE\n"
shell_unknown: .asciz "UNKNOWN COMMAND\n"
shell_bad_path_text: .asciz "INVALID OR TOO LONG PATH\n"
shell_not_found_text: .asciz "PATH NOT FOUND: "
shell_not_directory_text: .asciz "NOT A DIRECTORY\n"
shell_is_directory_text: .asciz "FILE IS A DIRECTORY\n"
shell_read_error_text: .asciz "FILE READ ERROR\n"
shell_type_usage_text: .asciz "USAGE: TYPE FILE OR CAT FILE\n"
shell_run_usage_text: .asciz "USAGE: RUN PROGRAM.APP\n"
shell_start_usage_text: .asciz "USAGE: START PROGRAM.APP [ARGS]\n"
shell_start_ok_text: .asciz "PROCESS READY, PID: "
shell_start_failed_text: .asciz "PROCESS START FAILED\n"
shell_run_invalid_text: .asciz "INVALID OR UNSUPPORTED APP IMAGE\n"
shell_run_no_memory_text: .asciz "NOT ENOUGH MEMORY FOR PROGRAM\n"
shell_run_read_error_text: .asciz "PROGRAM READ ERROR\n"
shell_run_checksum_text: .asciz "PROGRAM CHECKSUM FAILED\n"
shell_app_exit_text: .asciz "PROGRAM EXIT: "
shell_tos_exit_text: .asciz "TOS PROGRAM EXIT: "
shell_tos_ret_text: .asciz "TOS-CALL-RETURNED\n"
shell_app_fault_text: .asciz "PROGRAM ABORTED, FAULT CODE: "
shell_drive_ready_text: .asciz "DRIVE MOUNTED\n"
shell_drive_unavailable_text: .asciz "DRIVE NOT AVAILABLE OR NOT FAT16\n"
shell_dir_suffix: .asciz "  <DIR>\n"
shell_size_separator: .asciz "  "
shell_uptime_label: .asciz "UPTIME: "
shell_ticks_suffix: .asciz " TICKS AT 50 HZ\n"
shell_tasks_label: .asciz "TASKS: "
shell_jobs_label: .asciz "USER PROCESSES:\n"
shell_name_label: .asciz "NAME "
shell_job_separator: .asciz "  "
shell_pid_label: .asciz "PID "
shell_ppid_label: .asciz "  PPID "
shell_state_label: .asciz "  STATE "
shell_state_ready: .asciz "READY\n"
shell_state_running: .asciz "RUNNING\n"
shell_state_sleeping: .asciz "SLEEPING\n"
shell_state_waiting: .asciz "WAITING\n"
shell_state_terminated: .asciz "TERMINATED\n"
shell_state_input: .asciz "INPUT\n"
shell_state_suspended: .asciz "SUSPENDED\n"
shell_state_message: .asciz "MESSAGE WAIT\n"
shell_state_unknown: .asciz "UNKNOWN\n"
shell_memory_label: .asciz "  MEMORY "
shell_files_label: .asciz " BYTES  FILES "
shell_stack_label: .asciz "  STACK "
shell_stack_separator: .asciz "/"
shell_cpu_ticks_label: .asciz "  CPU TICKS "
shell_no_jobs: .asciz "NONE\n"
shell_kill_usage_text: .asciz "USAGE: KILL PID\n"
shell_kill_ok_text: .asciz "PROCESS TERMINATED\n"
shell_kill_busy_text: .asciz "PROCESS CANNOT BE TERMINATED NOW\n"
shell_kill_not_found_text: .asciz "PROCESS NOT FOUND\n"
shell_maps_usage_text: .asciz "USAGE: MAPS PID\n"
shell_maps_pid_label: .asciz "PROCESS MAP PID "
shell_maps_domain_label: .asciz "  DOMAIN "
shell_maps_text_label: .asciz "TEXT    R-X  "
shell_maps_rodata_label: .asciz "RODATA  R--  "
shell_maps_data_label: .asciz "DATA    RW-  "
shell_maps_policy_label: .asciz "POLICY  R--  "
shell_maps_bss_label: .asciz "BSS     RW-  "
shell_maps_stack_label: .asciz "STACK   RW-  "
shell_maps_image_label: .asciz "IMAGE: "
shell_maps_private_text: .asciz "PRIVATE\n"
shell_maps_shared_text: .asciz "SHARED\n"
shell_maps_stack_used_label: .asciz "STACK USED: "
shell_maps_bytes_text: .asciz " BYTES\n"
shell_map_separator: .asciz " - "
shell_hex_digits: .ascii "0123456789ABCDEF"
shell_watchdog_usage_text: .asciz "USAGE: WATCHDOG PID CPU_TICKS (0 DISABLES)\n"
shell_watchdog_ok_text: .asciz "PROCESS WATCHDOG UPDATED\n"
shell_watchdog_invalid_text: .asciz "INVALID PROCESS OR WATCHDOG LIMIT\n"
shell_fg_usage_text: .asciz "USAGE: FG PID\n"
shell_fg_ok_text: .asciz "TERMINAL ATTACHED - PRESS A KEY\n"
shell_fg_failed_text: .asciz "PROCESS NOT WAITING FOR INPUT\n"
shell_bg_usage_text: .asciz "USAGE: BG PID\n"
shell_bg_ok_text: .asciz "PROCESS RESUMED IN BACKGROUND\n"
shell_bg_not_suspended_text: .asciz "PROCESS NOT SUSPENDED\n"
shell_bg_not_found_text: .asciz "PROCESS NOT FOUND\n"
shell_stop_usage_text: .asciz "USAGE: STOP PID\n"
shell_stop_ok_text: .asciz "PROCESS SUSPENDED\n"
shell_stop_invalid_state_text: .asciz "PROCESS CANNOT BE SUSPENDED NOW\n"
shell_stop_not_found_text: .asciz "PROCESS NOT FOUND\n"
shell_wait_usage_text: .asciz "USAGE: WAIT PID\n"
shell_wait_pid_text: .asciz "PID "
shell_wait_exit_text: .asciz " EXIT "
shell_wait_fault_text: .asciz " FAULT "
shell_wait_killed_text: .asciz " KILLED\n"
shell_wait_watchdog_text: .asciz " WATCHDOG\n"
shell_wait_running_text: .asciz "PROCESS STILL ACTIVE\n"
shell_wait_not_found_text: .asciz "NO COMPLETION RECORD\n"
shell_input_dropped_label: .asciz "IKBD DROPPED: "
shell_input_pending_label: .asciz "  EVENTS PENDING: "
shell_st_label: .asciz "ST-RAM FREE: "
shell_tt_label: .asciz " KB\nTT-RAM FREE: "
shell_kb_suffix: .asciz " KB\n"
shell_keymap:
    .byte 0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0
    .byte 'q','w','e','r','t','y','u','i','o','p','[',']',0,0,'a','s'
    .byte 'd','f','g','h','j','k','l',';',39,'`',0,92,'z','x','c','v'
    .byte 'b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0
shell_shift_keymap:
    .byte 0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0
    .byte 'Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,'A','S'
    .byte 'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V'
    .byte 'B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0

    .section .bss
    .align 2
shell_api: .space 4
shell_event: .space 4
shell_input: .space 4
shell_proc: .space 4
shell_timer: .space 4
shell_vfs: .space 4
shell_type: .space 4
shell_value: .space 4
shell_keys: .space 4
shell_cursor_tick: .space 4
shell_cursor_visible: .space 2
shell_shift_state: .space 2
shell_length: .space 2
shell_char: .space 2
shell_prompt_buffer: .space 68
shell_buffer: .space 64
shell_history: .space 512
shell_history_count: .space 1
shell_history_next: .space 1
shell_history_view: .space 1
shell_foreground_attached: .space 1
shell_start_foreground: .space 1
shell_path_retry: .space 1
    .balign 4
shell_current_path: .space 64
shell_exec_path: .space 64
shell_work_path: .space 64
shell_file_size: .space 4
shell_loaded_size: .space 4
shell_stream_offset: .space 4
shell_attributes: .space 4
shell_autoexec_offset: .space 4
shell_autoexec_n: .space 4
shell_cd_argument: .space 4
shell_app_image: .space 4
shell_app_stack: .space 4
shell_app_result: .space 4
shell_run_filename: .space 4
shell_start_filename: .space 4
shell_start_arguments: .space 4
shell_start_pid: .space 4
shell_wait_status: .space 4
shell_watchdog_pid: .space 4
shell_job_info: .space 16
shell_job_resources: .space 16
shell_job_name: .space 13
shell_jobs_found: .space 2
shell_cpu_info: .space 12
    .balign 4
shell_map_info: .space 64
shell_hex_buffer: .space 12
    .balign 4
shell_app_header: .space 32
shell_st_stats: .space 16
shell_tt_stats: .space 16
shell_service: .space 28
shell_file_buffer: .space 512
    .balign 4
shell_completion: .space 80
shell_completion_leaf: .space 4
