/* SPDX-License-Identifier: GPL-2.0-only */
#include "../port/source/unlock/port.h"
#include <assert.h>
#include <stdio.h>
#include "../port/source/unlock/vendor/prep_610.h"
#include "../port/source/unlock/vendor/vbios_610.h"
int main(void) {
  unsigned checks=0;
#define CHECK(x) do { assert(x); checks++; } while(0)
  CHECK(gpu_unlock_falcon_dma_valid(0,0,0x12000000,256));
  CHECK(gpu_unlock_falcon_dma_valid(0,0,0x100000000ULL,256));
  CHECK(gpu_unlock_falcon_dma_valid((1U<<24)-256,0,0x1000,256));
  CHECK(!gpu_unlock_falcon_dma_valid(1U<<24,0,0x1000,256));
  CHECK(gpu_unlock_falcon_dma_valid(0,0xffffff00U,0,256));
  CHECK(!gpu_unlock_falcon_dma_valid(0,0xffffff00U,0,512));
  CHECK(gpu_unlock_falcon_dma_valid(0,0,(1ULL<<49)-256,256));
  CHECK(!gpu_unlock_falcon_dma_valid(0,0,1ULL<<49,256));
  CHECK(!gpu_unlock_falcon_dma_valid(0,0,(1ULL<<49)-256,512));
  CHECK(!gpu_unlock_falcon_dma_valid(0,256,(1ULL<<49)-256,256));
  CHECK(!gpu_unlock_falcon_dma_valid(0,0,0x1000,0));
  for(unsigned n=1;n<256;n++) {
    CHECK(!gpu_unlock_falcon_dma_valid(n,0,0x1000,256));
    CHECK(!gpu_unlock_falcon_dma_valid(0,n,0x1000,256));
    CHECK(!gpu_unlock_falcon_dma_valid(0,0,0x1000+n,256));
    CHECK(!gpu_unlock_falcon_dma_valid(0,0,0x1000,256+n));
  }
  CHECK(!gpu_unlock_falcon_hs_geometry(0x12000000,0,0x100,0,0,0x100,0x100,0));
  CHECK(!gpu_unlock_falcon_hs_geometry(0x12000000,0,0x100,0x100,0,0,0,0));
  CHECK(gpu_unlock_falcon_hs_geometry(0,0,0x100,0x100,0,0,0,0)==-34);
  CHECK(gpu_unlock_falcon_hs_geometry(~0ULL-255,256,256,0,0,0,0,0)==-34);
  CHECK(gpu_unlock_falcon_hs_geometry(~0ULL-255,0,256,0,0,256,256,0)==-34);
  CHECK(gpu_unlock_falcon_hs_geometry(0x12000001,0,256,0,0,0,0,0)==-22);
  CHECK(gpu_unlock_falcon_hs_geometry(0x12000000,0,256,0,0,1,256,0)==-22);
  struct gpu_unlock_fwsec fw={.dmem_size=0x980,.data_off=0xe300,.image_size=0xed00};
  u32 rounded=0;
  CHECK(!gpu_unlock_fwsec_dmem_transfer(&fw,&rounded) && rounded==0xa00);
  fw.image_size--;CHECK(gpu_unlock_fwsec_dmem_transfer(&fw,&rounded)==-34);
  fw.image_size=0xed00;fw.data_off++;CHECK(gpu_unlock_fwsec_dmem_transfer(&fw,&rounded)==-22);
  fw.data_off=0xe300;fw.dmem_size=0xffffffff;CHECK(gpu_unlock_fwsec_dmem_transfer(&fw,&rounded)==-22);
  fw.dmem_size=0;CHECK(gpu_unlock_fwsec_dmem_transfer(&fw,&rounded)==-22);
  CHECK(gpu_unlock_fwsec_dmem_transfer(NULL,&rounded)==-22);
  CHECK(gpu_unlock_fwsec_dmem_transfer(&fw,NULL)==-22);
  printf("%u DMA alignment, address, wrap, underflow and DMEM rounding checks passed.\n",checks);
  return 0;
}
