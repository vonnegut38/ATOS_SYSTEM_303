/* ATOS Sound — bezpieczna sonda obecnosci NatFeats (audyt r2/r3).
 *
 * Opcode 0x7300 (NF_ID) przy WYLACZONYM NatFeats jest w Hatari nielegalnym
 * rozkazem, wiec sonda podmienia TYLKO wektor 4 na czas proby i natychmiast go
 * przywraca.  Utwardzenia z audytu r3 (sekcja 10):
 *   1. handler sprawdza PC: obsluguje WYLACZNIE wlasny opcode.  Kazdy inny
 *      wyjatek 4 jest przekazywany dalej: przywracamy wektor, zostawiamy ramke
 *      nietknieta i wracamy przez rte, wiec rozkaz wykona sie ponownie i trafi
 *      do prawdziwego handlera z poprawnym PC,
 *   2. handler zachowuje WSZYSTKIE rejestry, ktorych uzywa (D0, A0),
 *   3. na czas podmiany wektora maskujemy przerwania (SR = poziom 7), zeby zaden
 *      ISR nie zobaczyl niespojenego stanu.
 *
 * int ksnd_nf_probe(const char *name, uint32_t *id_out)
 *   -> 1 = NatFeats odpowiada (id_out = ID), 0 = brak (wyjatek 4), -1 = blad.
 */
    .text
    .globl ksnd_nf_probe
    .type  ksnd_nf_probe,@function
ksnd_nf_probe:
    movem.l %d2-%d4/%a2-%a4,-(%sp)
    move.l  4+24(%sp),%d3           /* nazwa (24 = 6 zapisanych rejestrow!) */
    move.l  8+24(%sp),%a4           /* gdzie zapisac ID */
    lea     ksnd_probe_flag,%a2
    clr.l   (%a2)                   /* 0 = nie bylo wyjatku */
    move.l  0x10,%a3                /* stary wektor 4 */

    move.w  %sr,%d2                 /* maskujemy przerwania tylko na czas proby */
    ori.w   #0x0700,%sr
    lea     ksnd_illegal_handler,%a0
    move.l  %a0,0x10

    move.l  %d3,-(%sp)              /* nazwa na stosie */
    clr.l   -(%sp)                  /* slot adresu powrotu (opcode pomija A7+4) */
ksnd_probe_opcode:
    .word   0x7300                  /* NatFeat_ID: wynik w d0 */
    addq.l  #8,%sp

    move.l  %a3,0x10                /* wektor wraca NATYCHMIAST */
    move.w  %d2,%sr                 /* ...i przerwania tez */

    tst.l   (%a2)
    bne.s   9f                      /* byl wyjatek => brak NatFeats */
    tst.l   %d0
    beq.s   9f                      /* ID == 0 => funkcji nie ma */
    move.l  %d0,(%a4)
    moveq   #1,%d0
    bra.s   8f
9:  moveq   #0,%d0
8:  movem.l (%sp)+,%d2-%d4/%a2-%a4
    rts

/* Handler wyjatku 4.  Ramka: [SR][PC]; SP wskazuje SR, PC jest pod 2(sp).
 * Obsluguje tylko nasz opcode (PC == adres 1f); reszte oddaje prawdziwemu
 * handlerowi, przywracajac wektor i NIE ruszajac ramki. */
    .type  ksnd_illegal_handler,@function
ksnd_illegal_handler:
    movem.l %d0-%d1/%a0-%a1,-(%sp)  /* zachowujemy wszystko, czego uzyjemy */
    move.l  16+2(%sp),%d1           /* PC z ramki wyjatku (16 B = 4 rejestry) */
    cmpi.l  #ksnd_probe_opcode,%d1
    bne.s   2f                      /* to NIE nasza sonda */

    /* Nasz opcode: ustaw flage i pomin rozkaz. */
    lea     ksnd_probe_flag,%a0
    move.l  #1,(%a0)
    addq.l  #2,16+2(%sp)            /* PC += 2 */
    movem.l (%sp)+,%d0-%d1/%a0-%a1
    rte

2:  /* Obcy wyjatek 4: przywracamy prawdziwy wektor i wracamy BEZ ZMIAN ramki,
     * zeby rozkaz wykonal sie ponownie i trafil do prawdziwego handlera
     * z poprawnym PC.  Stary wektor jest w A3 (przerwany kod to nasza sonda,
     * a handler nie rusza A3). */
    move.l  %a3,0x10
    movem.l (%sp)+,%d0-%d1/%a0-%a1
    rte

    .bss
    .align 2
ksnd_probe_flag:
    .space 4
