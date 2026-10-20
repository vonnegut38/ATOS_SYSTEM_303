/* TOS window VDI test: draws ONLY through the GEM VDI (trap #2, d0=0x73).
 *
 * Why this program exists: the VDI in this tree is the classic mono/planar one
 * and used to draw into the kernel framebuffer, i.e. straight over the desktop,
 * while the window showed the program's own screen.  With the VDI redirected to
 * the program's screen (system/tos_prg.c: tos_vdi_follow_screen) every VDI
 * primitive must appear INSIDE the TOS window and leave the desktop alone.
 *
 * Screen: rez 2 = 640x400 mono, one bitplane, 80 bytes/row.
 * The drawing is deliberately VDI-only - no direct screen writes - so a missing
 * redirect is immediately visible as "nothing inside the window" or as paint
 * spilled over the desktop.
 */
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;

/* VDIPB = {contrl,intin,ptsin,intout,ptsout}; the trap takes d0=0x73 (VDI),
   d1=&VDIPB (see system/tos_entry.S:asf_tos_trap2). */
static int16_t v_contrl[16];
static int16_t v_intin[128];
static int16_t v_ptsin[128];
static int16_t v_intout[128];
static int16_t v_ptsout[128];
static void *v_pb[5] = { v_contrl, v_intin, v_ptsin, v_intout, v_ptsout };

/* d1 = &VDIPB, d0 = 0x73, trap #2.  The address is moved from a register
   operand on purpose: an address-form operand made this gcc's m68k backend
   crash (print_operand_address ICE).  The trap stub saves and restores every
   register, so only d0/d1 and memory need to be declared clobbered. */
static void vdi_trap(void *pb)
{
    __asm__ __volatile__("move.l %0,%%d1\n\t"
                         "move.w #0x73,%%d0\n\t"
                         "trap #2"
                         : : "r"(pb) : "d0", "d1", "memory");
}
static int16_t vdi_call(void)
{
    vdi_trap((void *)v_pb);
    return v_contrl[0];
}

/* One VDI call with the TOS argument counts:
     contrl[0] = opcode          contrl[1] = points in ptsin
     contrl[2] = points out      contrl[3] = words in intin
     contrl[4] = words out       contrl[5] = GDP sub-opcode (v_bar = 1)
   The caller fills [2]/[4] with zero; the VDI writes the result counts. */
static int16_t vdi_call_args(int16_t op, int16_t n_ptsin, int16_t n_intin, int16_t sub)
{
    v_contrl[0] = op;
    v_contrl[1] = n_ptsin;
    v_contrl[2] = 0;
    v_contrl[3] = n_intin;
    v_contrl[4] = 0;
    v_contrl[5] = sub;
    vdi_call();
    return v_contrl[2];
}
static int16_t vdi(int16_t op, int16_t n_ptsin, int16_t n_intin)
{
    return vdi_call_args(op, n_ptsin, n_intin, 0);
}

static int32_t setscreen(int32_t log, int32_t phys, int16_t rez)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.w %1,-(%%sp)\n\t"
                         "move.l %2,-(%%sp)\n\t"
                         "move.l %3,-(%%sp)\n\t"
                         "move.w %4,-(%%sp)\n\t"
                         "trap #14\n\t"
                         "add.l #12,%%sp"
                         : "=r"(r)
                         : "d"((int32_t)rez), "r"(phys), "r"(log), "r"((int32_t)5)
                         : "d1", "d2", "a0", "a1", "a2", "memory");
    return r;
}
static int32_t xbios1(int16_t fn, int32_t a)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #14\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r) : "r"(a), "r"((int32_t)fn)
                         : "d1", "d2", "a0", "a1", "a2", "memory");
    return r;
}
static int32_t gdos1(int16_t fn, int32_t a)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #1\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r)
                         : "r"(a), "r"((int32_t)fn)
                         : "d1", "d2", "a0", "a1", "a2", "memory");
    return r;
}
static void mark(unsigned at, uint8_t value)
{
    uint32_t address = 0x0840u + (uint32_t)at;
    __asm__ __volatile__("move.b %1,%0@" : : "a"(address), "d"((uint32_t)value) : "memory");
}

/* Draw a horizontal band of lines, a filled bar and a text label - all VDI. */
static void draw(void)
{
    int i;

    v_intin[0] = 1;                      /* work_in: device 1 = screen */
    for (i = 1; i < 10; ++i) v_intin[i] = 1;
    v_intin[10] = 2;                     /* work_in[10] = 2: raster coordinates */
    (void)vdi(100, 0, 11);               /* v_opnvwk: workstation handle in contrl[6] */
    mark(2u, 0x03u);

    (void)vdi(3, 0, 0);                  /* v_clrwk: clear the workstation */

    v_intin[0] = 1;
    (void)vdi(15, 0, 1);                 /* vsl_type(1) = solid */
    v_intin[0] = 1;
    (void)vdi(17, 0, 1);                 /* vsl_color(1) = ink */
    v_ptsin[0] = 4; v_ptsin[1] = 0;      /* vsl_width: the width is in ptsin[0] */
    (void)vdi(16, 1, 0);

    for (i = 0; i < 8; ++i) {            /* v_pline: a fan of diagonals */
        v_ptsin[0] = 20;      v_ptsin[1] = (int16_t)(20 + i * 40);
        v_ptsin[2] = 300;     v_ptsin[3] = 200;
        v_ptsin[4] = 580;     v_ptsin[5] = (int16_t)(40 + i * 40);
        (void)vdi(6, 3, 0);
    }
    v_ptsin[0] = 20; v_ptsin[1] = 20;    /* a plain rectangle outline */
    v_ptsin[2] = 580; v_ptsin[3] = 20;
    v_ptsin[4] = 580; v_ptsin[5] = 380;
    v_ptsin[6] = 20;  v_ptsin[7] = 380;
    v_ptsin[8] = 20;  v_ptsin[9] = 20;
    (void)vdi(6, 5, 0);

    v_intin[0] = 1;
    (void)vdi(23, 0, 1);                 /* vsf_interior(1) = solid */
    v_intin[0] = 1;
    (void)vdi(24, 0, 1);                 /* vsf_style(1) */
    v_intin[0] = 1;
    (void)vdi(25, 0, 1);                 /* vsf_color(1) = ink */
    for (i = 0; i < 4; ++i) {            /* v_bar: four filled bars */
        v_ptsin[0] = (int16_t)(60 + i * 130); v_ptsin[1] = 300;
        v_ptsin[2] = (int16_t)(140 + i * 130); v_ptsin[3] = 360;
        (void)vdi_call_args(11, 2, 0, 1);   /* v_bar: GDP sub-opcode 1 */
    }

    /* New attributes: a dashed line and a wide line prove that line_type and
       line_width are applied, and a hatched v_bar proves vsf_style. */
    v_intin[0] = 2;
    (void)vdi(15, 0, 1);                 /* vsl_type(2) = dashed */
    v_ptsin[0] = 20; v_ptsin[1] = 380; v_ptsin[2] = 580; v_ptsin[3] = 380;
    (void)vdi(6, 2, 0);                  /* dashed baseline */
    v_intin[0] = 8;
    (void)vdi(23, 0, 1);                 /* vsf_interior(8) = hatched */
    v_intin[0] = 4;
    (void)vdi(24, 0, 1);                 /* vsf_style(4) = up diagonal */
    v_ptsin[0] = 60; v_ptsin[1] = 250; v_ptsin[2] = 200; v_ptsin[3] = 290;
    (void)vdi_call_args(11, 2, 0, 1);    /* hatched bar */
    v_intin[0] = 1;
    (void)vdi(23, 0, 1);                 /* back to solid */
    v_intin[0] = 1;
    (void)vdi(24, 0, 1);
    v_intin[0] = 1;
    (void)vdi(15, 0, 1);                 /* vsl_type(1) = solid again */

    v_intin[0] = 1;
    (void)vdi(21, 0, 1);                 /* vst_font(1) */
    v_intin[0] = 1;
    (void)vdi(22, 0, 1);                 /* vst_color(1) = ink */
    v_ptsin[0] = 0; v_ptsin[1] = 16;     /* vst_height: height in ptsin[1] */
    (void)vdi(12, 1, 0);
    {                                    /* v_gtext: the string continues in intin[] */
        const char *s = "VDI IN WINDOW";
        int n = 0;
        while (s[n] && n < 40) { v_intin[n] = (int16_t)(uint8_t)s[n]; ++n; }
        v_ptsin[0] = 40; v_ptsin[1] = 60;
        (void)vdi(8, 1, (int16_t)n);      /* v_gtext: contrl[3] = characters */
    }
    /* Second workstation cycle: close, open again, draw through the NEW handle.
       This exercises the handle bookkeeping (the old code always drew through
       ws[0] and could not tell the slots apart).  v_clrwk is deliberately not
       called here: it would erase the figure above, and the screenshot is the
       evidence. */
    (void)vdi(2, 0, 0);                  /* v_clswk: close workstation 1 */
    {int k;for(k=0;k<11;k++)v_intin[k]=1;v_intin[10]=2;}
    (void)vdi(100, 0, 11);               /* v_opnvwk again: must get a fresh slot */
    v_intin[0] = 1;
    (void)vdi(25, 0, 1);                 /* vsf_color(1) */
    v_intin[0] = 1;
    (void)vdi(23, 0, 1);
    v_ptsin[0] = 420; v_ptsin[1] = 250; v_ptsin[2] = 540; v_ptsin[3] = 290;
    (void)vdi_call_args(11, 2, 0, 1);    /* a bar drawn by the second handle */
    mark(3u, 0x55u);
}

int32_t _tos_main(void)
{
    mark(0u, 0x01u);
    (void)setscreen(-1, -1, 2);          /* rez 2 = 640x400 mono */
    mark(1u, 0x02u);

    draw();

    { int vi; for (vi = 0; vi < 12; ++vi) (void)xbios1(37, 0); }

    /* Bounded hold: any key ends it, and Vsync keeps the compositor alive. */
    { volatile uint32_t spin;
      for (spin = 0; spin < 60000000u; ++spin) {
          if((spin & 0xFFFFu)==0u && gdos1(0x0Bu, 0) > 0) { (void)gdos1(0x07u, 0); break; }
          if((spin & 0x3FFFFu)==0u) (void)xbios1(37, 0);
      } }
    return 0;
}
