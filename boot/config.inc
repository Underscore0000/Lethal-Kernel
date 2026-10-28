; ============================================================================
; Lethal Kernel - Shared boot-time configuration
; ============================================================================

; --- Real mode load addresses -----------------------------------------------
STAGE1_LOAD_OFF   equ 0x7C00      ; where the BIOS drops the boot sector
STAGE2_LOAD_SEG   equ 0x0000
STAGE2_LOAD_OFF   equ 0x8000      ; stage2 is loaded right after stage1

; --- Disk layout (in 512-byte LBA sectors) ----------------------------------
STAGE2_SECTORS    equ 32          ; 16 KiB reserved for stage2 on disk
DISK_LBA_STAGE2   equ 1           ; stage2 starts immediately after the MBR
DISK_LBA_KERNEL   equ 1 + STAGE2_SECTORS

; --- Kernel staging (real-mode temporary load buffer) -----------------------
KERNEL_TEMP_SEG    equ 0x1000     ; linear 0x10000
KERNEL_TEMP_LINEAR equ 0x00010000
KERNEL_LOAD_LIMIT  equ 0x00080000 ; max 512 KiB kernel image in v0.1

; --- Final kernel location ---------------------------------------------------
KERNEL_LINK_ADDR  equ 0x00100000  ; 1 MiB - standard freestanding kernel base

; --- Protected/long mode scratch stack --------------------------------------
PM_STACK_TOP      equ 0x0009F000  ; just below the EBDA, above the temp buffer

; --- Framebuffer (VBE) ------------------------------------------------------
FB_INFO_ADDR      equ 0x00009000
FB_VBE_MODEINFO   equ 0x00009100
FB_VBE_MODE       equ 0x00000118