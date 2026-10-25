/* SPDX-License-Identifier: GPL-2.0-only */
#include "../port/source/unlock/app.h"
#include "../port/source/unlock/selector_check.h"
#include "../port/source/unlock/checks.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  /* An invalid non-null table would crash if the entry touched any services.
   * Execute native host code only, never the EFI artifact.
   */
  assert(EfiMain(NULL,(EFI_SYSTEM_TABLE *)(UINTN)1)==EFI_ACCESS_DENIED);
  assert(EfiMain(NULL,NULL)==EFI_ACCESS_DENIED);
  assert(!GuPortHardwareEnabled() && GU_PROFILE_READY);
  assert(!GuPortAutomaticBoot());
  assert(GU_BOARD_ID==0x268410deU && GU_BOARD_SUBSYSTEM==0x13ba196eU);
  assert(GU_BOARD_CHIP==0x192000a1U);
  assert(Gu4090ColdSelectorsClosed(0x63703070,3));
  assert(Gu4090ColdSelectorsClosed(0x23703070,1));
  for(unsigned value=0;value<8;value++)assert(Gu4090ColdSelectorsClosed(0x77777777,value));
  for(unsigned nibble=0;nibble<8;nibble++)
    assert(!Gu4090ColdSelectorsClosed(8U<<(4*nibble),0));
  for(unsigned bit=3;bit<32;bit++)assert(!Gu4090ColdSelectorsClosed(0,1U<<bit));
  assert(!Gu4090ColdSelectorsClosed(UINT32_MAX,UINT32_MAX));
  assert(Gu4090SelectorsValid(0xffffffff,0x88888888,8,0));
  for(unsigned bit=0;bit<27;bit++)
    assert(!Gu4090SelectorsValid(0xffffffff,0x88888888,8,1U<<bit));
  assert(!Gu4090SelectorsValid(0xffffff8f,0x88888888,8,0));
  assert(!Gu4090SelectorsValid(0xffffffff,0,8,0));
  assert(!Gu4090SelectorsValid(0xffffffff,0x88888888,7,0));
  assert(!Gu4090SelectorsValid(0xffffffff,0x88888888,8,0xffffffff));
  assert(!Gu4090SelectorsValid(0xffffffff,0x88888888,8,0xbadf5620));
  UINT8 path[96]={1,1,6,0,0,1};
  HARDDRIVE_DEVICE_PATH disk={0};EFI_GUID esp=GU_PROFILE_WINDOWS_ESP_GUID;
  disk.Header.Type=MEDIA_DEVICE_PATH;disk.Header.SubType=MEDIA_HARDDRIVE_DP;
  disk.Header.Length[0]=sizeof(disk);disk.MBRType=MBR_TYPE_EFI_PARTITION_TABLE_HEADER;
  disk.SignatureType=SIGNATURE_TYPE_GUID;disk.PartitionNumber=GU_PROFILE_WINDOWS_ESP_PARTITION;
  disk.PartitionStart=GU_PROFILE_WINDOWS_ESP_START_LBA;disk.PartitionSize=GU_PROFILE_WINDOWS_ESP_SIZE_LBA;
  memcpy(disk.Signature,&esp,sizeof(esp));memcpy(path+6,&disk,sizeof(disk));
  UINTN end=6+sizeof(disk);path[end]=END_DEVICE_PATH_TYPE;path[end+1]=END_ENTIRE_DEVICE_PATH_SUBTYPE;path[end+2]=4;
  assert(GuWindowsEspPath((VOID*)path,end+4));assert(GuWindowsEspPath((VOID*)path,4096));
  assert(!GuWindowsEspPath(NULL,4096));assert(!GuWindowsEspPath((VOID*)path,4097));
  assert(!GuWindowsEspPath((VOID*)path,end+3));
  const unsigned corrupt[]={6,7,8,10,14,22,30,46,47};
  for(unsigned i=0;i<sizeof(corrupt)/sizeof(*corrupt);i++) {
    path[corrupt[i]]^=1;assert(!GuWindowsEspPath((VOID*)path,end+4));path[corrupt[i]]^=1;
  }
  path[end+1]=1;assert(!GuWindowsEspPath((VOID*)path,end+4));path[end+1]=END_ENTIRE_DEVICE_PATH_SUBTYPE;
  memcpy(path+end,&disk,sizeof(disk));UINTN second=end+sizeof(disk);
  path[second]=END_DEVICE_PATH_TYPE;path[second+1]=END_ENTIRE_DEVICE_PATH_SUBTYPE;path[second+2]=4;
  assert(!GuWindowsEspPath((VOID*)path,second+4));
  puts("Default entry remains inert; board, selectors and pinned Windows ESP path tests passed.");
  return 0;
}
