    .section .app_header,"a"
    .ascii "ASYSAPP0"
    .word 1
    .word 32
    .long __app_file_end
    .long __app_memory_end
    .long app_entry
    .long 4096
    .long 0

    .section .text.startup,"ax"
    .globl app_entry
app_entry:
    lea gfx_name(%pc),%a0
    moveq #40,%d0
    trap #0
    tst.l %d0
    bmi failed
    move.l %d0,%d4
    lea gui_name(%pc),%a0
    moveq #40,%d0
    trap #0
    tst.l %d0
    bmi failed
    move.l %d0,%d6
    moveq #1,%d2
    bsr gui_command
    tst.l %d0
    bne restore_failed
    moveq #6,%d2
    bsr gui_command
    tst.l %d0
    bne unregister_failed
    /* Let the shell finish any launch-result redraw selected immediately
       before GUI ownership became visible, then paint one authoritative
       desktop frame. Ten 50 Hz ticks are bounded and occur only at startup. */
    move.l #10,%d1
    moveq #11,%d0
    trap #0
    lea desktop_command(%pc),%a0
    bsr send_gfx
    tst.l %d0
    bne session_failed
    lea info_command(%pc),%a0
    bsr send_gfx
    tst.l %d0
    bmi session_failed
    lea screen_size(%pc),%a0
    move.l %d0,(%a0)
    /* Register the desktop's logical full-screen surface. SYSTEM.GFX derives
       ownership from this process' PID; the client never supplies an owner. */
    lea surface_create_command(%pc),%a0
    move.l %d0,4(%a0)
    bsr send_gfx
    tst.l %d0
    ble session_failed
    lea surface_handle(%pc),%a0
    move.l %d0,(%a0)
    lea surface_query_command(%pc),%a0
    move.l %d0,4(%a0)
    bsr send_gfx
    lea screen_size(%pc),%a0
    cmp.l (%a0),%d0
    bne session_failed
    bsr draw_desktop
    tst.l %d0
    bne session_failed
    /* Keep client drawing outside the nested window/decorations call chain. */
    bsr draw_client_button
    tst.l %d0
    bne session_failed
    bsr draw_info_button
    tst.l %d0
    bne session_failed

event_loop:
    lea event(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne session_failed
    lea event(%pc),%a0
    tst.l (%a0)
    bne event_loop
    cmpi.l #0x4b4b4559,4(%a0)
    beq keyboard_event
    cmpi.l #0x4b4d4f55,4(%a0)
    bne event_loop
    bsr mouse_event
    lea quit_requested(%pc),%a0
    tst.b (%a0)
    bne desktop_exit
    bra event_loop

keyboard_event:
    move.l 8(%a0),%d0
    move.l %d0,%d1
    andi.l #0xff00007f,%d1
    cmpi.l #0x01000001,%d1       /* Escape make */
    beq desktop_exit
    cmpi.l #0x0100000f,%d1       /* Tab make */
    beq keyboard_change_focus
    cmpi.l #0x0100004b,%d1       /* Left make */
    beq keyboard_change_focus
    cmpi.l #0x0100004d,%d1       /* Right make */
    beq keyboard_change_focus
    cmpi.l #0x0100001c,%d1       /* Return make */
    bne event_loop
    lea control_focus(%pc),%a0
    tst.b (%a0)
    bne keyboard_activate_info
    lea button_active(%pc),%a0
    eori.b #1,(%a0)
    bsr draw_client_button
    bra event_loop
keyboard_activate_info:
    lea info_active(%pc),%a0
    eori.b #1,(%a0)
    bsr draw_info_button
    tst.l %d0
    bne session_failed
    bsr draw_info_panel
    tst.l %d0
    bne session_failed
    bra event_loop
keyboard_change_focus:
    lea control_focus(%pc),%a0
    eori.b #1,(%a0)
    bsr draw_client_button
    tst.l %d0
    bne session_failed
    bsr draw_info_button
    bra event_loop
desktop_exit:
    bsr destroy_surface
    moveq #7,%d2
    bsr gui_command
    moveq #2,%d2
    bsr gui_command
    lea restore_command(%pc),%a0
    bsr send_gfx
    moveq #0,%d1
    bra exit

mouse_event:
    movem.l %d0-%d3/%a0-%a2,-(%sp)
    lea event(%pc),%a0
    move.l 8(%a0),%d0
    moveq #0,%d1
    move.w %d0,%d1               /* mouse Y */
    swap %d0
    andi.l #0xffff,%d0            /* mouse X */
    move.l 12(%a0),%d2
    swap %d2
    lsr.w #8,%d2
    andi.l #7,%d2
    lea click_release(%pc),%a0
    clr.b (%a0)
    move.l %d2,%d3
    andi.l #3,%d3
    bne mouse_button_down
    btst #2,%d2
    beq mouse_button_up
    move.b #1,(%a0)              /* complete click coalesced by one poll */
    lea dragging(%pc),%a0
    clr.b (%a0)
    bra mouse_button_down
mouse_button_up:
    lea dragging(%pc),%a0
    cmpi.b #2,(%a0)
    bne 1f
    bsr resize_window_commit
1:
    clr.b (%a0)
    bra mouse_done
mouse_button_down:
    lea dragging(%pc),%a0
    tst.b (%a0)
    beq mouse_press
    cmpi.b #2,(%a0)
    beq mouse_resize_track
    cmpi.b #3,(%a0)
    beq mouse_done
    bra mouse_drag
mouse_press:
    /* FILE owns x=64..112 in the fixed menu bar.  Its first bounded popup
       never overlaps the initial window and is latched until release. */
    lea menu_open(%pc),%a0
    tst.b (%a0)
    beq.s mouse_menu_closed
    cmpi.l #64,%d0
    blo.s mouse_menu_dismiss
    cmpi.l #144,%d0
    bhs.s mouse_menu_dismiss
    cmpi.l #18,%d1
    blo.s mouse_menu_dismiss
    cmpi.l #40,%d1
    bhs.s mouse_menu_dismiss
    lea quit_requested(%pc),%a1
    move.b #1,(%a1)
    lea dragging(%pc),%a0
    move.b #3,(%a0)
    bra mouse_done
mouse_menu_dismiss:
    bsr hide_file_menu
    lea dragging(%pc),%a0
    move.b #3,(%a0)
    bra mouse_done
mouse_menu_closed:
    cmpi.l #18,%d1
    bhs.s mouse_window_press
    cmpi.l #64,%d0
    blo.s mouse_window_press
    cmpi.l #112,%d0
    bhs.s mouse_window_press
    move.b #1,(%a0)
    bsr draw_file_menu
    lea dragging(%pc),%a0
    move.b #3,(%a0)
    bra mouse_done
mouse_window_press:
    lea window_xy(%pc),%a1
    lea window_wh(%pc),%a2
    /* Resize gadget: lower-right 14 by 14 pixels, bounded on all sides. */
    moveq #0,%d2
    move.w (%a1),%d2
    moveq #0,%d3
    move.w (%a2),%d3
    add.l %d3,%d2              /* right edge */
    cmp.l %d2,%d0
    bhs 2f
    subi.l #14,%d2
    cmp.l %d2,%d0
    blo 2f
    moveq #0,%d3
    move.w 2(%a1),%d3
    lea window_wh(%pc),%a2
    moveq #0,%d2
    move.w 2(%a2),%d2
    add.l %d2,%d3              /* bottom edge */
    cmp.l %d3,%d1
    bhs 2f
    move.l %d3,%d2
    subi.l #14,%d2
    cmp.l %d2,%d1
    blo 2f
    /* Keep the pointer's offset from the exact lower-right corner. */
    lea window_xy(%pc),%a1
    lea window_wh(%pc),%a2
    moveq #0,%d2
    move.w (%a1),%d2
    add.w (%a2),%d2
    sub.w %d0,%d2
    moveq #0,%d3
    move.w 2(%a1),%d3
    add.w 2(%a2),%d3
    sub.w %d1,%d3
    lea resize_offset(%pc),%a1
    move.w %d2,(%a1)
    move.w %d3,2(%a1)
    lea window_wh(%pc),%a1
    lea pending_wh(%pc),%a2
    move.l (%a1),(%a2)
    move.b #2,(%a0)
    bra mouse_done
2:
    lea window_xy(%pc),%a1
    /* First client control: an 80 by 22 push button. */
    moveq #0,%d2
    move.w (%a1),%d2
    addi.l #16,%d2
    cmp.l %d2,%d0
    blo 3f
    addi.l #80,%d2
    cmp.l %d2,%d0
    bhs 3f
    moveq #0,%d2
    move.w 2(%a1),%d2
    addi.l #38,%d2
    cmp.l %d2,%d1
    blo 3f
    addi.l #22,%d2
    cmp.l %d2,%d1
    bhs 3f
    lea button_active(%pc),%a1
    eori.b #1,(%a1)
    lea control_focus(%pc),%a1
    clr.b (%a1)
    bsr draw_client_button
    tst.l %d0
    bne mouse_done
    bsr draw_info_button
    lea dragging(%pc),%a0
    move.b #3,(%a0)
    bra mouse_done
3:
    lea window_xy(%pc),%a1
    /* Second client control: INFO/HIDE, also 80 by 22 pixels. */
    moveq #0,%d2
    move.w (%a1),%d2
    addi.l #112,%d2
    cmp.l %d2,%d0
    blo 4f
    addi.l #80,%d2
    cmp.l %d2,%d0
    bhs 4f
    moveq #0,%d2
    move.w 2(%a1),%d2
    addi.l #38,%d2
    cmp.l %d2,%d1
    blo 4f
    addi.l #22,%d2
    cmp.l %d2,%d1
    bhs 4f
    lea info_active(%pc),%a1
    eori.b #1,(%a1)
    lea control_focus(%pc),%a1
    move.b #1,(%a1)
    bsr draw_client_button
    tst.l %d0
    bne mouse_done
    bsr draw_info_button
    tst.l %d0
    bne mouse_done
    bsr draw_info_panel
    tst.l %d0
    bne mouse_done
    lea dragging(%pc),%a0
    move.b #3,(%a0)
    bra mouse_done
4:
    lea window_xy(%pc),%a1
    moveq #0,%d2
    move.w (%a1),%d2
    cmp.l %d2,%d0
    blo mouse_done
    move.l %d2,%d3
    lea window_wh(%pc),%a2
    add.w (%a2),%d3
    cmp.l %d3,%d0
    bhs mouse_done
    moveq #0,%d2
    move.w 2(%a1),%d2
    cmp.l %d2,%d1
    blo mouse_done
    move.l %d2,%d3
    addi.l #18,%d3
    cmp.l %d3,%d1
    bhs mouse_done
    /* Close gadget: ten pixels inside the title bar. */
    moveq #0,%d3
    move.w (%a1),%d3
    addq.l #5,%d3
    cmp.l %d3,%d0
    blo 8f
    addi.l #10,%d3
    cmp.l %d3,%d0
    bhs 8f
    moveq #0,%d3
    move.w 2(%a1),%d3
    addq.l #4,%d3
    cmp.l %d3,%d1
    blo 8f
    addi.l #10,%d3
    cmp.l %d3,%d1
    bhs 8f
    lea quit_requested(%pc),%a0
    move.b #1,(%a0)
    bra mouse_done
8:
    sub.l %d2,%d1
    sub.l %d2,%d2               /* keep D2 deterministic */
    moveq #0,%d2
    move.w (%a1),%d2
    sub.l %d2,%d0
    lea drag_offset(%pc),%a1
    move.w %d0,(%a1)
    move.w %d1,2(%a1)
    move.b #1,(%a0)
    bra mouse_done
mouse_drag:
    lea drag_offset(%pc),%a1
    moveq #0,%d2
    move.w (%a1),%d2
    sub.l %d2,%d0
    moveq #0,%d2
    move.w 2(%a1),%d2
    sub.l %d2,%d1
    tst.l %d0
    bpl 1f
    moveq #0,%d0
1:  cmpi.l #20,%d1
    bhs 2f
    moveq #20,%d1
2:  lea screen_size(%pc),%a1
    moveq #0,%d2
    move.w (%a1),%d2
    lea window_wh(%pc),%a2
    moveq #0,%d3
    move.w (%a2),%d3
    sub.l %d3,%d2
    cmp.l %d2,%d0
    bls 3f
    move.l %d2,%d0
3:  moveq #0,%d2
    move.w 2(%a1),%d2
    moveq #0,%d3
    move.w 2(%a2),%d3
    sub.l %d3,%d2
    cmp.l %d2,%d1
    bls 4f
    move.l %d2,%d1
4:  bsr move_window
    bra mouse_done
mouse_resize_track:
    lea window_xy(%pc),%a1
    lea resize_offset(%pc),%a2
    sub.w (%a1),%d0
    add.w (%a2),%d0
    sub.w 2(%a1),%d1
    add.w 2(%a2),%d1
    cmpi.l #220,%d0
    bhs 5f
    move.l #220,%d0
5:  cmpi.l #130,%d1
    bhs 6f
    move.l #130,%d1
6:  lea screen_size(%pc),%a2
    moveq #0,%d2
    move.w (%a2),%d2
    moveq #0,%d3
    move.w (%a1),%d3
    sub.l %d3,%d2
    cmp.l %d2,%d0
    bls 7f
    move.l %d2,%d0
7:  moveq #0,%d2
    move.w 2(%a2),%d2
    moveq #0,%d3
    move.w 2(%a1),%d3
    sub.l %d3,%d2
    cmp.l %d2,%d1
    bls 8f
    move.l %d2,%d1
8:  lea pending_wh(%pc),%a1
    move.w %d0,(%a1)
    move.w %d1,2(%a1)
mouse_done:
    lea click_release(%pc),%a0
    tst.b (%a0)
    beq.s 1f
    lea dragging(%pc),%a0
    clr.b (%a0)
1:
    movem.l (%sp)+,%d0-%d3/%a0-%a2
    rts

draw_desktop:
    lea clear_command(%pc),%a0
    bsr send_gfx
    tst.l %d0
    bne 9f
    bsr draw_menu_bar
    tst.l %d0
    bne 9f
    bsr draw_window
9:  rts

/* Commit a tracked resize once, on button release. */
resize_window_commit:
    lea pending_wh(%pc),%a2
    lea window_wh(%pc),%a1
    move.l (%a2),%d2
    cmp.l (%a1),%d2
    beq 9f
    /* Remove the old window, then paint the new one over the same origin. */
    movem.l %d0-%d3/%a0-%a2,-(%sp)
    lea window_xy(%pc),%a0
    moveq #0,%d0
    move.w (%a0),%d0
    moveq #0,%d1
    move.w 2(%a0),%d1
    moveq #0,%d2
    lea window_wh(%pc),%a2
    move.w (%a2),%d2
    moveq #0,%d3
    move.w 2(%a2),%d3
    bsr fill_background
    lea pending_wh(%pc),%a0
    lea window_wh(%pc),%a1
    move.l (%a0),(%a1)
    bsr draw_window
    tst.l %d0
    bne 8f
    bsr draw_client_button
    tst.l %d0
    bne 8f
    bsr draw_info_button
    tst.l %d0
    bne 8f
    bsr draw_info_panel
8:
    movem.l (%sp)+,%d0-%d3/%a0-%a2
9:  rts

move_window:
    lea window_xy(%pc),%a1
    cmp.w (%a1),%d0
    bne 1f
    cmp.w 2(%a1),%d1
    beq 9f
1:  lea new_window_xy(%pc),%a0
    move.w %d0,(%a0)
    move.w %d1,2(%a0)
    lea copy_command(%pc),%a0
    lea window_xy(%pc),%a1
    move.l (%a1),4(%a0)
    lea window_wh(%pc),%a2
    move.w (%a2),8(%a0)
    move.w 2(%a2),10(%a0)
    lea new_window_xy(%pc),%a1
    move.l (%a1),12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    /* Clear the old vertical area which is not covered by the new window. */
    lea window_wh(%pc),%a2
    lea window_xy(%pc),%a0
    lea new_window_xy(%pc),%a1
    moveq #0,%d0
    move.w (%a0),%d0             /* old X */
    moveq #0,%d2
    move.w (%a1),%d2             /* new X */
    cmp.l %d0,%d2
    beq 4f
    blo 2f
    move.l %d2,%d3
    sub.l %d0,%d3
    moveq #0,%d1
    move.w (%a2),%d1
    cmp.l %d1,%d3
    bls 3f
    move.l %d1,%d3
    bra 3f
2:  move.l %d0,%d3
    sub.l %d2,%d3
    moveq #0,%d1
    move.w (%a2),%d1
    cmp.l %d1,%d3
    bls 21f
    move.l %d1,%d3
21: move.l %d0,%d2
    moveq #0,%d1
    move.w (%a2),%d1
    add.l %d1,%d2
    sub.l %d3,%d2
    move.l %d2,%d0               /* right exposed strip X */
3:  moveq #0,%d1
    move.w 2(%a0),%d1            /* old Y */
    move.l %d3,%d2               /* width */
    moveq #0,%d3
    move.w 2(%a2),%d3
    bsr fill_background
4:
    /* Clear the old horizontal area not covered by the new window. */
    lea window_wh(%pc),%a2
    lea window_xy(%pc),%a0
    lea new_window_xy(%pc),%a1
    moveq #0,%d1
    move.w 2(%a0),%d1            /* old Y */
    moveq #0,%d3
    move.w 2(%a1),%d3            /* new Y */
    cmp.l %d1,%d3
    beq 7f
    blo 5f
    sub.l %d1,%d3
    moveq #0,%d2
    move.w 2(%a2),%d2
    cmp.l %d2,%d3
    bls 6f
    move.l %d2,%d3
    bra 6f
5:  move.l %d1,%d2
    sub.l %d3,%d2
    moveq #0,%d0
    move.w 2(%a2),%d0
    cmp.l %d0,%d2
    bls 51f
    move.l %d0,%d2
51: move.l %d1,%d3
    moveq #0,%d0
    move.w 2(%a2),%d0
    add.l %d0,%d3
    sub.l %d2,%d3
    move.l %d3,%d1               /* bottom exposed strip Y */
    move.l %d2,%d3               /* height */
6:  moveq #0,%d0
    move.w (%a0),%d0             /* old X */
    moveq #0,%d2
    move.w (%a2),%d2
    bsr fill_background
7:  lea new_window_xy(%pc),%a0
    lea window_xy(%pc),%a1
    move.l (%a0),(%a1)
9:  rts

/* D0=x, D1=y, D2=width, D3=height. */
fill_background:
    lea fill_command(%pc),%a0
    move.w %d0,4(%a0)
    move.w %d1,6(%a0)
    move.w %d2,8(%a0)
    move.w %d3,10(%a0)
    clr.l 12(%a0)
    bsr send_gfx
    rts

draw_window:
    lea window_xy(%pc),%a1
    lea fill_command(%pc),%a0
    move.l (%a1),4(%a0)
    lea window_wh(%pc),%a2
    move.w (%a2),8(%a0)
    move.w 2(%a2),10(%a0)
    clr.l 12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    lea frame_command(%pc),%a0
    lea window_xy(%pc),%a1
    move.l (%a1),4(%a0)
    lea window_wh(%pc),%a2
    move.w (%a2),8(%a0)
    move.w 2(%a2),10(%a0)
    move.l #1,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    move.l (%a1),%d0
    addq.w #2,%d0
    swap %d0
    addq.w #2,%d0
    swap %d0
    lea fill_command(%pc),%a0
    move.l %d0,4(%a0)
    lea window_wh(%pc),%a2
    move.w (%a2),%d1
    subq.w #4,%d1
    move.w %d1,8(%a0)
    move.w #16,10(%a0)
    move.l #1,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    bsr draw_decorations
    tst.l %d0
    bne 9f
9:  rts

draw_menu_bar:
    /* The menu bar belongs to the desktop, not to a movable window. */
    lea fill_command(%pc),%a0
    clr.l 4(%a0)
    lea screen_size(%pc),%a1
    move.w (%a1),8(%a0)
    move.w #18,10(%a0)
    move.l #1,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    lea frame_command(%pc),%a0
    clr.l 4(%a0)
    lea screen_size(%pc),%a1
    move.w (%a1),8(%a0)
    move.w #18,10(%a0)
    clr.l 12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    lea menu_logo_text(%pc),%a3
    moveq #8,%d5
    bsr draw_menu_label
    tst.l %d0
    bne 9f
    lea menu_desk_text(%pc),%a3
    moveq #32,%d5
    bsr draw_menu_label
    tst.l %d0
    bne 9f
    lea menu_file_text(%pc),%a3
    moveq #72,%d5
    bsr draw_menu_label
    tst.l %d0
    bne 9f
    lea menu_view_text(%pc),%a3
    moveq #120,%d5
    bsr draw_menu_label
9:  rts

/* A3 zero-terminated menu text, D5 x; black glyphs on white. */
draw_menu_label:
1:  moveq #0,%d0
    move.b (%a3)+,%d0
    beq 9f
    lea glyph_command(%pc),%a0
    move.w %d5,4(%a0)
    move.w #1,6(%a0)
    move.l %d0,8(%a0)
    move.l #0x00010001,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    addq.l #8,%d5
    bra 1b
9:  rts

draw_file_menu:
    lea fill_command(%pc),%a0
    move.w #64,4(%a0)
    move.w #18,6(%a0)
    move.w #80,8(%a0)
    move.w #22,10(%a0)
    move.l #1,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne.s 9f
    lea frame_command(%pc),%a0
    move.w #64,4(%a0)
    move.w #18,6(%a0)
    move.w #80,8(%a0)
    move.w #22,10(%a0)
    clr.l 12(%a0)
    bsr send_gfx
    tst.l %d0
    bne.s 9f
    lea menu_close_text(%pc),%a3
    moveq #72,%d5
1:  moveq #0,%d0
    move.b (%a3)+,%d0
    beq.s 9f
    lea glyph_command(%pc),%a0
    move.w %d5,4(%a0)
    move.w #21,6(%a0)
    move.l %d0,8(%a0)
    move.l #0x00010001,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne.s 9f
    addq.l #8,%d5
    bra.s 1b
9:  rts

hide_file_menu:
    moveq #64,%d0
    moveq #18,%d1
    moveq #80,%d2
    moveq #22,%d3
    bsr fill_background
    tst.l %d0
    bne.s 9f
    bsr draw_menu_bar
9:  lea menu_open(%pc),%a0
    clr.b (%a0)
    rts

draw_client_button:
    /* Clear and frame only the control rectangle. */
    lea window_xy(%pc),%a1
    lea fill_command(%pc),%a0
    move.w (%a1),%d0
    addi.w #16,%d0
    move.w %d0,4(%a0)
    move.w 2(%a1),%d0
    addi.w #38,%d0
    move.w %d0,6(%a0)
    move.w #80,8(%a0)
    move.w #22,10(%a0)
    clr.l 12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    lea window_xy(%pc),%a1
    lea frame_command(%pc),%a0
    move.w (%a1),%d0
    addi.w #16,%d0
    move.w %d0,4(%a0)
    move.w 2(%a1),%d0
    addi.w #38,%d0
    move.w %d0,6(%a0)
    move.w #80,8(%a0)
    move.w #22,10(%a0)
    move.l #1,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    lea control_focus(%pc),%a0
    tst.b (%a0)
    bne 1f
    lea window_xy(%pc),%a1
    lea frame_command(%pc),%a0
    move.w (%a1),%d0
    addi.w #18,%d0
    move.w %d0,4(%a0)
    move.w 2(%a1),%d0
    addi.w #40,%d0
    move.w %d0,6(%a0)
    move.w #76,8(%a0)
    move.w #18,10(%a0)
    move.l #1,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
1:
    lea button_active(%pc),%a0
    tst.b (%a0)
    beq 2f
    lea active_text(%pc),%a3
    moveq #32,%d5
    bra 3f
2:  lea start_text(%pc),%a3
    moveq #32,%d5
3:
    lea window_xy(%pc),%a1
    add.w (%a1),%d5
4:  moveq #0,%d0
    move.b (%a3)+,%d0
    beq 9f
    lea glyph_command(%pc),%a0
    move.w %d5,4(%a0)
    move.w 2(%a1),%d1
    addi.w #41,%d1
    move.w %d1,6(%a0)
    move.l %d0,8(%a0)
    move.l #0x01000001,12(%a0) /* white on black, opaque */
    bsr send_gfx
    tst.l %d0
    bne 9f
    addq.l #8,%d5
    bra 4b
9:  rts

draw_info_button:
    lea window_xy(%pc),%a1
    lea fill_command(%pc),%a0
    move.w (%a1),%d0
    addi.w #112,%d0
    move.w %d0,4(%a0)
    move.w 2(%a1),%d0
    addi.w #38,%d0
    move.w %d0,6(%a0)
    move.w #80,8(%a0)
    move.w #22,10(%a0)
    clr.l 12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    lea window_xy(%pc),%a1
    lea frame_command(%pc),%a0
    move.w (%a1),%d0
    addi.w #112,%d0
    move.w %d0,4(%a0)
    move.w 2(%a1),%d0
    addi.w #38,%d0
    move.w %d0,6(%a0)
    move.w #80,8(%a0)
    move.w #22,10(%a0)
    move.l #1,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    lea control_focus(%pc),%a0
    cmpi.b #1,(%a0)
    bne 1f
    lea window_xy(%pc),%a1
    lea frame_command(%pc),%a0
    move.w (%a1),%d0
    addi.w #114,%d0
    move.w %d0,4(%a0)
    move.w 2(%a1),%d0
    addi.w #40,%d0
    move.w %d0,6(%a0)
    move.w #76,8(%a0)
    move.w #18,10(%a0)
    move.l #1,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
1:  lea info_active(%pc),%a0
    tst.b (%a0)
    beq 2f
    lea hide_text(%pc),%a3
    bra 3f
2:  lea info_text(%pc),%a3
3:  moveq #0,%d5
    move.w window_xy(%pc),%d5
    addi.l #136,%d5
4:  moveq #0,%d0
    move.b (%a3)+,%d0
    beq 9f
    lea glyph_command(%pc),%a0
    move.w %d5,4(%a0)
    move.w window_xy+2(%pc),%d1
    addi.w #41,%d1
    move.w %d1,6(%a0)
    move.l %d0,8(%a0)
    move.l #0x01000001,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    addq.l #8,%d5
    bra 4b
9:  rts

draw_info_panel:
    /* Clear only the information area; hiding never repaints the window. */
    lea window_xy(%pc),%a1
    lea fill_command(%pc),%a0
    move.w (%a1),%d0
    addi.w #16,%d0
    move.w %d0,4(%a0)
    move.w 2(%a1),%d0
    addi.w #70,%d0
    move.w %d0,6(%a0)
    move.w #208,8(%a0)
    move.w #52,10(%a0)
    clr.l 12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    lea info_active(%pc),%a0
    tst.b (%a0)
    beq 9f
    lea screen_size(%pc),%a0
    move.l (%a0),%d0
    cmpi.l #0x02800190,%d0
    beq 1f
    cmpi.l #0x028000c8,%d0
    beq 2f
    lea low_mode_text(%pc),%a3
    lea low_size_text(%pc),%a4
    bra 3f
1:  lea mono_mode_text(%pc),%a3
    lea mono_size_text(%pc),%a4
    bra 3f
2:  lea medium_mode_text(%pc),%a3
    lea medium_size_text(%pc),%a4
3:  lea window_xy(%pc),%a1
    moveq #0,%d5
    move.w (%a1),%d5
    addi.l #16,%d5
    moveq #0,%d6
    move.w 2(%a1),%d6
    addi.l #72,%d6
    bsr draw_info_line
    move.l %a4,%a3
    lea window_xy(%pc),%a1
    moveq #0,%d5
    move.w (%a1),%d5
    addi.l #16,%d5
    moveq #0,%d6
    move.w 2(%a1),%d6
    addi.l #88,%d6
    bsr draw_info_line
    lea surface_text(%pc),%a3
    lea window_xy(%pc),%a1
    moveq #0,%d5
    move.w (%a1),%d5
    addi.l #16,%d5
    moveq #0,%d6
    move.w 2(%a1),%d6
    addi.l #104,%d6
    bsr draw_info_line
9:  rts

/* A3 zero-terminated text, D5 x, D6 y. */
draw_info_line:
1:  moveq #0,%d0
    move.b (%a3)+,%d0
    beq 9f
    lea glyph_command(%pc),%a0
    move.w %d5,4(%a0)
    move.w %d6,6(%a0)
    move.l %d0,8(%a0)
    move.l #0x01000001,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    addq.l #8,%d5
    bra 1b
9:  rts

draw_decorations:
    /* Black close gadget on the white title bar. */
    lea window_xy(%pc),%a1
    lea fill_command(%pc),%a0
    move.w (%a1),%d0
    addq.w #5,%d0
    move.w %d0,4(%a0)
    move.w 2(%a1),%d0
    addq.w #4,%d0
    move.w %d0,6(%a0)
    move.w #10,8(%a0)
    move.w #10,10(%a0)
    clr.l 12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    /* A small framed resize handle in the lower-right corner. */
    lea window_xy(%pc),%a1
    lea frame_command(%pc),%a0
    move.w (%a1),%d0
    lea window_wh(%pc),%a2
    add.w (%a2),%d0
    subi.w #14,%d0
    move.w %d0,4(%a0)
    move.w 2(%a1),%d0
    add.w 2(%a2),%d0
    subi.w #14,%d0
    move.w %d0,6(%a0)
    move.w #12,8(%a0)
    move.w #12,10(%a0)
    move.l #1,12(%a0)
    bsr send_gfx
    tst.l %d0
    bne 9f
    /* Fixed-size IPC glyphs keep all user pointers outside the kernel. */
    lea title_text(%pc),%a3
    lea window_xy(%pc),%a1
    moveq #0,%d5
    move.w (%a1),%d5
    addi.l #24,%d5
1:  moveq #0,%d0
    move.b (%a3)+,%d0
    beq 9f
    lea glyph_command(%pc),%a0
    move.w %d5,4(%a0)
    move.w 2(%a1),%d1
    addq.w #2,%d1
    move.w %d1,6(%a0)
    move.l %d0,8(%a0)
    move.l #0x00010001,12(%a0) /* black on white, opaque */
    bsr send_gfx
    tst.l %d0
    bne 9f
    addq.l #8,%d5
    bra 1b
9:  rts

/* A0 command, D4 broker. Ignore asynchronous GUI input while awaiting the
   authenticated broker response; the kernel coalesces the next mouse state. */
send_gfx:
    move.l %a0,%a2
    move.l %d4,%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne send_failed
1:  lea reply(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne send_failed
    lea reply(%pc),%a0
    cmp.l (%a0),%d4
    bne 1b
    move.l (%a2),%d2
    ori.l #0x80000000,%d2
    cmp.l 4(%a0),%d2
    bne 1b
    move.l 8(%a0),%d0
    rts
send_failed:
    moveq #-1,%d0
    rts

gui_command:
    lea gui_request(%pc),%a0
    move.l %d2,(%a0)
    move.l %d6,%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne gui_failed
1:  lea reply(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne gui_failed
    lea reply(%pc),%a0
    cmp.l (%a0),%d6
    bne 1b
    move.l %d2,%d1
    ori.l #0x80000000,%d1
    cmp.l 4(%a0),%d1
    bne 1b
    move.l 8(%a0),%d0
    rts
gui_failed:
    moveq #-1,%d0
    rts

/* Release the logical surface once. Kernel owner cleanup remains the final
   safety net for abnormal process termination. */
destroy_surface:
    lea surface_handle(%pc),%a1
    move.l (%a1),%d1
    beq 1f
    lea surface_destroy_command(%pc),%a0
    move.l %d1,4(%a0)
    bsr send_gfx
    clr.l (%a1)
1:  rts

session_failed:
    bsr destroy_surface
    moveq #7,%d2
    bsr gui_command
unregister_failed:
    moveq #2,%d2
    bsr gui_command
restore_failed:
    lea restore_command(%pc),%a0
    bsr send_gfx
failed:
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .data
    .align 2
info_command:    .long 1,0,0,0
clear_command:   .long 2,0,0,0
fill_command:    .long 3,0,0,0
frame_command:   .long 4,0,0,0
restore_command: .long 6,0,0,0
copy_command:    .long 12,0,0,0
desktop_command: .long 13,0,0,0
glyph_command:   .long 14,0,0,0
surface_create_command:  .long 9,0,1,0
surface_destroy_command: .long 10,0,0,0
surface_query_command:   .long 11,0,0,0
gui_request:     .long 0,0,0,0
window_xy:       .word 80,50
new_window_xy:   .word 80,50
window_wh:       .word 240,140
pending_wh:      .word 240,140
drag_offset:     .word 0,0
resize_offset:   .word 0,0
button_active:   .word 0
info_active:     .word 0
control_focus:   .word 0
menu_open:       .word 0
    .section .rodata
gfx_name: .asciz "SYSTEM.GFX"
gui_name: .asciz "SYSTEM.GUI"
title_text: .asciz "ATARI DESKTOP"
start_text: .asciz "START"
active_text: .asciz "READY"
info_text: .asciz "INFO"
hide_text: .asciz "HIDE"
mono_mode_text: .asciz "SHIFTER MONO"
medium_mode_text: .asciz "SHIFTER MEDIUM"
low_mode_text: .asciz "SHIFTER LOW"
mono_size_text: .asciz "640 X 400"
medium_size_text: .asciz "640 X 200"
low_size_text: .asciz "320 X 200"
surface_text: .asciz "SURFACE READY"
menu_file_text: .asciz "FILE"
menu_view_text: .asciz "VIEW"
menu_close_text: .asciz "CLOSE"
menu_logo_text: .byte 0xf6,0xf7,0
menu_desk_text: .asciz "DESK"
    .section .bss
    .align 2
screen_size: .space 4
surface_handle: .space 4
dragging: .space 2
quit_requested: .space 2
click_release: .space 2
reply: .space 20
event: .space 20
