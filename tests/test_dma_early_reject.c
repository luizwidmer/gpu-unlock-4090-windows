/* SPDX-License-Identifier: GPL-2.0-only */
#include "../port/source/unlock/port.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef EPROTO
#undef EBADMSG
#undef EOVERFLOW
#undef EOPNOTSUPP
#undef ETIMEDOUT
#undef EALREADY
#include "../port/source/unlock/vendor/unlock_610.c"
int main(void) {
  struct unlock_transaction_work work={0};
  struct falcon_eng engine={.base=0x110000,.fbif=0x110600,.falcon2=0x111000};
  /* A mistaken read/reset/submission would dereference these poison pointers.
   * Only native host code runs; no live GPU or EFI services are attached.
   */
  GuInit((EFI_BOOT_SERVICES *)(UINTN)1,(EFI_PCI_IO_PROTOCOL *)(UINTN)1);
  work.bar=(void *)(UINTN)1;work.bar_len=0x1000000;work.mapped=true;
  assert(falcon_hs(&work,&engine,0,256,0,0,0,0,0,0,0x400,9,
                   0x12000001,0,0,false,false)==-EINVAL);
  assert(falcon_hs(&work,&engine,0,256,256,0,0,0,0,0,0x400,9,
                   0,0,0,false,false)==-ERANGE);
  assert(falcon_hs(&work,&engine,0,256,0,0,1,256,0,0,0x400,9,
                   0x12000000,0,0,false,false)==-EINVAL);
  assert(!work.firmware_started && !work.master && !Gu.started && !Gu.flrs);
  puts("Actual Falcon launch rejects invalid DMA before EFI/MMIO/reset; state remains unstarted.");
  return 0;
}
