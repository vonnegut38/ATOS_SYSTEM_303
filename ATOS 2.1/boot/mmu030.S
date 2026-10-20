        .text
        .globl  mmu_boot_code
        .globl  mmu_boot_code_end
mmu_boot_code:
        moveq   #0,%d0
        movec   %d0,%cacr
        lea     mmu_crp_block,%a0
        pmove   %a0@,%crp
        move.l  #0x80F09800,%d0
        pmove   %d0,%tc
        move.l  #0x00E0002C,%a0
        move.l  (%a0),%d1
        cmpi.l  #0x45544F53,%d1
        beq.s   1f
0:      bra.s   0b
1:      move.l  #0x00E00004,%a0
        move.l  (%a0),%a1
        jmp     (%a1)
mmu_boot_code_end:
