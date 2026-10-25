/* SPDX-License-Identifier: MIT
 * PNY RTX 4090 cold USB CAP00, 2026-10-24. Exact board/ROM profile.
 */
#pragma once
#define GU_PROFILE_READY 1
#define GU_PROFILE_BOARD_ID 0x268410deU
#define GU_PROFILE_SUBSYSTEM 0x13ba196eU
#define GU_PROFILE_CHIP 0x192000a1U
#define GU_PROFILE_BAR0_BASE 0x76000000ULL
#define GU_PROFILE_BAR0_BYTES 0x1000000ULL
#define GU_PROFILE_CONFIG_C8 0x50011U
#define GU_PROFILE_CONFIG_SHA256 "a3cea968c1c9ddbe40428adc45785b553e9d5f87432ecdd52ddb97eff4f07b02"
#define GU_PROFILE_VRAM_MIB_REG 0x59f4U
#define GU_PROFILE_SS0 0x63703070U
#define GU_PROFILE_SS1 3U
#define GU_PROFILE_EFFECTIVE 0x410080U
#define GU_PROFILE_PCI_ROM_BYTES 150016U
#define GU_PROFILE_PCI_ROM_SHA256 "9235ed51f237ae4e4699468015e968b38978ba5731838f59e2bb7509c4cfa327"
/* Private, measured ESP configuration is deliberately excluded from Git. */
#if __has_include("local_board.h")
#include "local_board.h"
#else
#define GU_PROFILE_WINDOWS_ESP_GUID {0,0,0,{0,0,0,0,0,0,0,0}}
#define GU_PROFILE_WINDOWS_ESP_PARTITION 1U
#define GU_PROFILE_WINDOWS_ESP_START_LBA 2048ULL
#define GU_PROFILE_WINDOWS_ESP_SIZE_LBA 204800ULL
#endif

/* Default builds stay inert. The separate soft-unlock builder explicitly
 * enables the entry only after the captured-board suites pass. Manual builds
 * require U; the separate automatic variant uses a cancellable countdown. */
#if defined(GU_SOFT_UNLOCK_BUILD) && GU_SOFT_UNLOCK_BUILD == 1
static inline int GuPortHardwareEnabled(void) { return 1; }
#else
static inline int GuPortHardwareEnabled(void) { return 0; }
#endif

/* The automatic USB variant must also explicitly enable the soft entry. */
static inline int GuPortAutomaticBoot(void) {
#if defined(GU_AUTO_BOOT_BUILD) && GU_AUTO_BOOT_BUILD == 1
  return GuPortHardwareEnabled();
#else
  return 0;
#endif
}
