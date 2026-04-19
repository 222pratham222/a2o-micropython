# head.S — MicroPython on A2O
# Based on the working boot.s from a2o/test3
#
# ERAT layout:
#   D-ERAT (MMUCR0=0x8C00, TLBSEL=11):
#     31: 1MB @ 0x00000000  (firmware)
#     30: 1MB @ 0x00100000  (headroom)
#     29: 1MB @ 0x03000000  (stack)
#     28: 64KB @ 0xFFF00000 (UART I/O, CI+G)
#   I-ERAT (MMUCR0=0x8800, TLBSEL=10):
#     15: 1MB @ 0x00000000  (firmware code)
#     13: 1MB @ 0x00100000  (headroom)
#
# W0: epn=32:51 class=52:53 v=54 x=55 size=56:59 thrd=60:63
#     size: 0001=4K 0011=64K 0101=1M 0111=16M 1010=1G
# W1: rpn(32:51)=32:51 rpn(22:31)=54:63
# W2: wlc=40:41 u=44:47 r=48 c=49 wimge=52:56
#     vf=57 ux/sx=58:59 uw/sw=60:61 ur/sr=62:63

#define STACK_TOP 0x03100000

#define LOAD_IMM64(r, e)        \
    lis     r,(e)@highest;      \
    ori     r,r,(e)@higher;     \
    rldicr  r,r, 32, 31;        \
    oris    r,r, (e)@h;         \
    ori     r,r, (e)@l;

.section ".head","ax"

# ---------------------------------------------------------------
# Exception vectors (A2O layout: 0x20 spacing)
# ---------------------------------------------------------------
    . = 0x0
    .global _start
_start:
    b       boot_start
    . = 0x020
    b .
    . = 0x040
    b .
    . = 0x060
    b .
    . = 0x080
    b .
    . = 0x0A0
    b .
    . = 0x0C0
    b .
    . = 0x0E0
    b .
    . = 0x100
    b .
    . = 0x120
    b .
    . = 0x140
    b .
    . = 0x160
    b .
    . = 0x180
    b .
    . = 0x1A0
    b .
    . = 0x1C0
    b .
    . = 0x1E0
    b .
    . = 0x200
    b .
    . = 0x220
    b .
    . = 0x240
    b .
    . = 0x260
    b .
    . = 0x280
    b .
    . = 0x2A0
    b .
    . = 0x2C0
    b .
    . = 0x2E0
    b .
    . = 0x300
    b .
    . = 0x320
    b .
    . = 0x340
    b .

# ---------------------------------------------------------------
# Boot: ERAT setup + MSR + BSS zero + call main
# ---------------------------------------------------------------
    . = 0x400
    .global boot_start
boot_start:

    # ====== D-ERAT (MMUCR0=0x8C00 -> derat) ======

    .long 0x3c608c00    /* lis r3, 0x8C00                            */
    .long 0x7c7cfba6    /* mtspr mmucr0, r3                          */

    # D-ERAT 31: 1MB @ VA=0x00000000 (firmware code+data)
    .long 0x3800001f    /* li r0, 31            entry number          */
    .long 0x3900025f    /* li r8, 0x025F        W0: v=1 sz=1M th=all */
    .long 0x38800000    /* li r4, 0             W1: RPN=0             */
    .long 0x38400015    /* li r2, 0x15          W2: sup RWX           */
    .long 0x7c4011a6    /* eratwe r2, r0, 2                           */
    .long 0x7c8009a6    /* eratwe r4, r0, 1                           */
    .long 0x7d0001a6    /* eratwe r8, r0, 0                           */
    .long 0x4c00012c    /* isync                                      */

    # D-ERAT 30: 1MB @ VA=0x00100000 (headroom)
    .long 0x3800001e    /* li r0, 30                                  */
    .long 0x39000000    /* li r8, 0                                   */
    .long 0x65080010    /* oris r8, r8, 0x0010                        */
    .long 0x6108025f    /* ori  r8, r8, 0x025F  W0=0x0010025F         */
    .long 0x38800000    /* li r4, 0                                   */
    .long 0x64840010    /* oris r4, r4, 0x0010  W1=0x00100000         */
    .long 0x38400015    /* li r2, 0x15          W2: sup RWX           */
    .long 0x7c4011a6    /* eratwe r2, r0, 2                           */
    .long 0x7c8009a6    /* eratwe r4, r0, 1                           */
    .long 0x7d0001a6    /* eratwe r8, r0, 0                           */
    .long 0x4c00012c    /* isync                                      */

    # D-ERAT 29: 1MB @ VA=0x03000000 (stack)
    .long 0x3800001d    /* li r0, 29                                  */
    .long 0x39000000    /* li r8, 0                                   */
    .long 0x65080300    /* oris r8, r8, 0x0300                        */
    .long 0x6108025f    /* ori  r8, r8, 0x025F  W0=0x0300025F         */
    .long 0x38800000    /* li r4, 0                                   */
    .long 0x64840300    /* oris r4, r4, 0x0300  W1=0x03000000         */
    .long 0x38400015    /* li r2, 0x15          W2: sup RWX           */
    .long 0x7c4011a6    /* eratwe r2, r0, 2                           */
    .long 0x7c8009a6    /* eratwe r4, r0, 1                           */
    .long 0x7d0001a6    /* eratwe r8, r0, 0                           */
    .long 0x4c00012c    /* isync                                      */

    # D-ERAT 28: 64KB @ VA=0xFFF00000 (UART I/O, cache-inhibited + guarded)
    .long 0x3800001c    /* li r0, 28                                  */
    .long 0x39000000    /* li r8, 0                                   */
    .long 0x6508fff0    /* oris r8, r8, 0xFFF0                        */
    .long 0x6108023f    /* ori  r8, r8, 0x023F  W0=0xFFF0023F         */
    .long 0x38800000    /* li r4, 0                                   */
    .long 0x6484fff0    /* oris r4, r4, 0xFFF0  W1=0xFFF00000         */
    .long 0x38400000    /* li r2, 0                                   */
    .long 0x644200a0    /* oris r2, r2, 0x00A0                        */
    .long 0x60420015    /* ori  r2, r2, 0x0015  W2=0x00A00015 (I+G)   */
    .long 0x7c4011a6    /* eratwe r2, r0, 2                           */
    .long 0x7c8009a6    /* eratwe r4, r0, 1                           */
    .long 0x7d0001a6    /* eratwe r8, r0, 0                           */
    .long 0x4c00012c    /* isync                                      */

    # ====== I-ERAT (MMUCR0=0x8800 -> ierat) ======

    .long 0x3c608800    /* lis r3, 0x8800                             */
    .long 0x7c7cfba6    /* mtspr mmucr0, r3                           */

    # I-ERAT 15: 1MB @ VA=0x00000000 (firmware code)
    .long 0x3800000f    /* li r0, 15                                  */
    .long 0x3900025f    /* li r8, 0x025F        W0: v=1 sz=1M th=all  */
    .long 0x38800000    /* li r4, 0             W1: RPN=0              */
    .long 0x3840003f    /* li r2, 0x3F          W2: u+s RWX            */
    .long 0x7c4011a6    /* eratwe r2, r0, 2                            */
    .long 0x7c8009a6    /* eratwe r4, r0, 1                            */
    .long 0x7d0001a6    /* eratwe r8, r0, 0                            */
    .long 0x4c00012c    /* isync                                       */

    # I-ERAT 13: 1MB @ VA=0x00100000 (firmware code headroom)
    .long 0x3800000d    /* li r0, 13                                   */
    .long 0x39000000    /* li r8, 0                                    */
    .long 0x65080010    /* oris r8, r8, 0x0010                         */
    .long 0x6108025f    /* ori  r8, r8, 0x025F  W0=0x0010025F          */
    .long 0x38800000    /* li r4, 0                                    */
    .long 0x64840010    /* oris r4, r4, 0x0010  W1=0x00100000          */
    .long 0x3840003f    /* li r2, 0x3F          W2: u+s RWX            */
    .long 0x7c4011a6    /* eratwe r2, r0, 2                            */
    .long 0x7c8009a6    /* eratwe r4, r0, 1                            */
    .long 0x7d0001a6    /* eratwe r8, r0, 0                            */
    .long 0x4c00012c    /* isync                                       */

    # ====== MSR: 64-bit mode ======
    # CM=1 CE=1 EE=1 PR=0 FP=1 ME=1 = 0x8002B000
    .long 0x39400000    /* li r10, 0                                   */
    .long 0x654a8002    /* oris r10, r10, 0x8002                       */
    .long 0x614ab000    /* ori  r10, r10, 0xB000                       */
    .long 0x7d400124    /* mtmsr r10                                   */
    .long 0x4c00012c    /* isync                                       */

    # ====== Zero BSS ======
    LOAD_IMM64(%r3, __bss_start)
    LOAD_IMM64(%r4, __bss_end)
    li      %r5, 0
1:  cmpld   %r3, %r4
    bge     2f
    std     %r5, 0(%r3)
    addi    %r3, %r3, 8
    b       1b
2:

    # ====== Stack + TOC + call main ======
    LOAD_IMM64(%r1, STACK_TOP - 0x100)
    LOAD_IMM64(%r2, .TOC.)
    bl      .main
    nop
1:  b 1b
