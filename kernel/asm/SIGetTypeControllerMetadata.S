#include <asm.h>
.include "constants.inc"

#  Hooked inside PADOriginCallback at 0x0034CF4C via PatchBL.
#  Replaces "lwz r31, -0x5A68(r13)" (load ResettingChan).
#
#  Sends CMD 0xA000, reads response. Every response has:
#    byte 0: total number of SI transfers (1-8)
#    byte 1: which transfer this is (0-based, 0-7)
#  If total > 8 or current >= total, treat as normal controller (abort).
#  Then sends 0xA001 .. 0xA0(total-1) to collect remaining chunks.
#  Each chunk is 80 bytes; data starts at byte 2 (78 data bytes/chunk).
#  Max total data = 8 * 78 = 624 bytes.
#
#  Per-channel shared memory layout (base +0x20, stride 0x2B0):
#    +0x000  tag        u32   (TAG_OK / TAG_NODATA / error tags)
#    +0x004  calls      u32   (monotonic "data ready" counter)
#    +0x008  n_chunks   u32   (number of 80-byte chunks, 1-8)
#    +0x00C  dbg_comcsr u32   (raw COMCSR value on error/completion)
#    +0x010  chunk[0]   80 bytes  (raw CMD 0xA000 response)
#    +0x060  chunk[1]   80 bytes  (raw CMD 0xA001 response)
#    ...
#    +0x230  chunk[7]   80 bytes  (raw CMD 0xA007 response)
#    +0x290  dbg_iobuf0 u32
#    +0x294  dbg_cmdidx u32
#    +0x298  dbg_id     12 bytes (3 words, reserved; previously CMD 0x00 probe)
#
#  Chan N base = EXTRA_DATA_BASE + 0x20 + (N * 0x2B0)

.set SI_HW_COMCSR,       0xCD806434
.set SI_HW_IO_BUF,       0xCD806480
.set EXTRA_DATA_BASE,    0xD3080000

.set CHAN_BASE_OFF,       0x20
.set CHAN_STRIDE,         0x02B0   # 688 bytes per channel
.set XOFF_TAG,           0x00
.set XOFF_CALLS,         0x04
.set XOFF_NCHUNKS,       0x08
.set XOFF_DBG_COMCSR,    0x0C
.set XOFF_CHUNKS,        0x10     # 8 chunks × 80 bytes = 640 (0x280)
.set XOFF_DBG_IOBUF0,    0x290
.set XOFF_DBG_CMDIDX,    0x294
.set XOFF_DBG_ID,        0x298    # reserved (was CMD 0x00 identity probe result)
.set CHUNK_SIZE,          80
.set MAX_CHUNKS,          8

SIGetTypeControllerMetadata:
    lwz     r31, -0x5A68(r13)

    stwu    r1, -48(r1)
    stw     r5, 8(r1)
    mfcr    r5
    stw     r5, 40(r1)          # save CR before any compare

    cmpwi   r31, 3
    bgt     .Learly_restore     # channels > 3: restore CR/r5 and return

    stw     r6, 12(r1)
    stw     r7, 16(r1)
    stw     r8, 20(r1)
    stw     r9, 24(r1)
    stw     r10, 28(r1)
    stw     r11, 32(r1)
    stw     r12, 36(r1)

    # r5 = per-channel shared memory base
    lis     r5, EXTRA_DATA_BASE@h
    mulli   r6, r31, CHAN_STRIDE     # chan * 688 = chan * 0x2B0
    addi    r6, r6, CHAN_BASE_OFF
    add     r5, r5, r6

    # r6 = COMCSR address (constant)
    lis     r6, SI_HW_COMCSR@h
    ori     r6, r6, SI_HW_COMCSR@l

    # === Drain any in-flight transfer ===
    li      r10, 0
.Ldrain_loop:
    lwz     r7, 0(r6)
    eieio
    andi.   r9, r7, 1
    beq     .Ldrain_done
    addi    r10, r10, 1
    cmpwi   r10, 20000
    blt     .Ldrain_loop

    stw     r7, XOFF_DBG_COMCSR(r5)
    li      r9, -1
    stw     r9, XOFF_DBG_CMDIDX(r5)
    lis     r9, 0xDEAD
    ori     r9, r9, 0x0004
    stw     r9, XOFF_TAG(r5)
    b       .Lrestore

.Ldrain_done:
    andis.  r9, r7, 0x8000
    beq     .Lsetup
    lis     r9, 0x8000
    stw     r9, 0(r6)
    eieio

.Lsetup:
    # r8 = IO_BUF address (constant)
    lis     r8, SI_HW_IO_BUF@h
    ori     r8, r8, SI_HW_IO_BUF@l
    li      r12, 0                  # cmd_idx = 0
    li      r11, 0                  # expected_total (set after first response)
    li      r9, 0
    stw     r9, XOFF_DBG_COMCSR(r5) # init: 0 = no COMERR

    # NOTE: No CMD 0x00 identity probe — issuing an extra SI transaction
    # inside the PADOriginCallback ISR perturbs the bus for normal GC
    # controllers and delays the SDK's subsequent SIEnablePolling. The first
    # CMD 0xA0xx below is itself the probe: a real GC controller will reply
    # with COMERR which is caught in .Lcomerr_as_nodata.

    # ========== Command loop ==========
.Lcmd_loop:
    # Write CMD 0xA0xx to IO_BUF word 0 (xx = cmd_idx)
    lis     r9, 0xA000              # 0xA0000000
    slwi    r10, r12, 16            # cmd_idx << 16
    or      r9, r9, r10
    stw     r9, 0(r8)
    eieio

    # COMCSR: TCINT_clear | OUTLNGTH=2 | INLNGTH=80 | chan | TSTART
    # INLNGTH field: 7 bits (14-8). Max reliable value.
    lis     r9, 0x8002
    ori     r9, r9, 0x5001
    slwi    r7, r31, 1
    or      r9, r9, r7
    stw     r9, 0(r6)
    eieio

    # Poll for TCINT
    li      r10, 0
.Lpoll:
    lwz     r7, 0(r6)
    eieio
    andis.  r9, r7, 0x8000
    bne     .Lxfer_done
    addi    r10, r10, 1
    cmpwi   r10, 20000
    blt     .Lpoll

    # Timeout — store debug info
    stw     r7, XOFF_DBG_COMCSR(r5)
    lwz     r9, 0(r8)
    stw     r9, XOFF_DBG_IOBUF0(r5)
    stw     r12, XOFF_DBG_CMDIDX(r5)
    lis     r9, 0x8000
    stw     r9, 0(r6)
    eieio
    lis     r9, 0xDEAD
    ori     r9, r9, 0x0001
    stw     r9, XOFF_TAG(r5)
    b       .Lrestore

.Lxfer_done:
    # Check COMERR
    andis.  r9, r7, 0x2000
    beq     .Lno_comerr

    # COMERR on first probe (cmd_idx==0) → normal GC controller, treat as NODATA
    cmpwi   r12, 0
    beq     .Lcomerr_as_nodata
    b       .Lstore_comerr

.Lcomerr_as_nodata:
    # Store COMCSR (has COMERR bit) so ARM side can see it was COMERR
    stw     r7, XOFF_DBG_COMCSR(r5)
    # Clear TCINT
    lis     r9, 0x8000
    stw     r9, 0(r6)
    eieio
    b       .Lnodata

.Lno_comerr:

    # Clear TCINT
    lis     r9, 0x8000
    stw     r9, 0(r6)
    eieio

    # Validate header bytes from IO_BUF word 0 (still valid after copy)
    lwz     r9, 0(r8)
    eieio
    # r10 = byte 0 (total), r7 = byte 1 (current)
    srwi    r10, r9, 24            # total = byte 0
    rlwinm  r7, r9, 16, 24, 31    # current = byte 1

    # If total < 1 or total > 8 → not an RP2040, abort with NODATA
    cmpwi   r10, 1
    blt     .Lnodata
    cmpwi   r10, MAX_CHUNKS
    bgt     .Lnodata

    # If current != cmd_idx → protocol mismatch, abort with NODATA
    cmpw    r7, r12
    bne     .Lnodata

    # First response: save expected total in r11
    cmpwi   r12, 0
    bne     .Lskip_save_total
    mr      r11, r10
.Lskip_save_total:

    # Verify total matches what first response said
    cmpw    r10, r11
    bne     .Lnodata

    # Copy 80 bytes (20 words) from IO_BUF to chunk[cmd_idx]
    mulli   r7, r12, CHUNK_SIZE     # cmd_idx * 80
    addi    r7, r7, XOFF_CHUNKS
    add     r7, r5, r7              # dest ptr
    li      r10, 0
.Lcopy:
    lwzx    r9, r8, r10
    stwx    r9, r7, r10
    addi    r10, r10, 4
    cmpwi   r10, CHUNK_SIZE
    blt     .Lcopy

    # Check if more chunks needed
    addi    r12, r12, 1
    cmpw    r12, r11
    blt     .Lcmd_loop

    # All chunks received — store success
    stw     r11, XOFF_NCHUNKS(r5)
    lis     r9, 0xCA11
    stw     r9, XOFF_TAG(r5)
    b       .Lrestore

.Lnodata:
    # Normal controller or protocol mismatch — store IO_BUF[0] for debug
    lwz     r9, 0(r8)
    stw     r9, XOFF_DBG_IOBUF0(r5)
    stw     r12, XOFF_DBG_CMDIDX(r5)
    lis     r9, 0xCA11
    ori     r9, r9, 0x0001
    stw     r9, XOFF_TAG(r5)
    li      r9, 0
    stw     r9, XOFF_NCHUNKS(r5)
    b       .Lrestore

.Lstore_comerr:
    # Store debug: COMCSR, IO_BUF[0], cmd_idx
    stw     r7, XOFF_DBG_COMCSR(r5)
    lwz     r9, 0(r8)
    eieio
    stw     r9, XOFF_DBG_IOBUF0(r5)
    stw     r12, XOFF_DBG_CMDIDX(r5)
    # Clear TCINT
    lis     r9, 0x8000
    stw     r9, 0(r6)
    eieio
    lis     r9, 0xDEAD
    ori     r9, r9, 0x0003
    stw     r9, XOFF_TAG(r5)

.Lrestore:
    # Restore SI error state to entry: on entry COMERR/TCINT are clean
    # (PADOriginCallback was invoked with error=0 and the SDK ISR cleared
    # TCINT before dispatch), so unconditionally W1C both bits in COMCSR.
    # This prevents any sticky COMERR from our CMD 0xA0 probes from
    # being observed by the SDK's subsequent SIEnablePolling.
    lis     r9, 0xA000              # 0x80000000 (TCINT W1C) | 0x20000000 (COMERR W1C)
    stw     r9, 0(r6)
    eieio

    lwz     r9, XOFF_CALLS(r5)
    addi    r9, r9, 1
    stw     r9, XOFF_CALLS(r5)

    lwz     r6, 12(r1)
    lwz     r7, 16(r1)
    lwz     r8, 20(r1)
    lwz     r9, 24(r1)
    lwz     r10, 28(r1)
    lwz     r11, 32(r1)
    lwz     r12, 36(r1)
.Learly_restore:
    lwz     r5, 40(r1)          # restore CR
    mtcr    r5
    lwz     r5, 8(r1)
    addi    r1, r1, 48
    blr
