/* SPDX-License-Identifier: GPL-2.0-only
 * Native host-only integration: real exported ROM bytes and real port code,
 * with the upstream fake EFI PCI/MMIO/DMA services. Never attached to a GPU.
 */
#define main unused_original_fixture_main
#include "../port/source/unlock/tests/test_efi_transaction.c"
#undef main
#include "../port/source/unlock/prom_profile.h"
/* Test translation unit only. Do not fill or enable the production profile. */
#undef GU_PROM_VIEW_BYTES
#define GU_PROM_VIEW_BYTES (1024U*1024U)
#include "../port/source/unlock/vendor/unlock_610.c"
int main(int argc,char **argv) {
  CHECK(argc==2);
  size_t bytes;UINT8 *source=read_file(argv[1],&bytes);
  CHECK(bytes<=sizeof(prom));
  memset(prom,0xff,sizeof(prom));memcpy(prom,source,bytes);free(source);
  struct gpu_unlock_fwsec fw={0};
  CHECK(!gpu_unlock_vbios_fwsec(prom,sizeof(prom),&fw));
  UINTN descriptor=fw.image_off-44-fw.sig_bytes;
  UINT32 original_pa=le32(prom+descriptor+16),original_va=le32(prom+descriptor+24);
  CHECK(!original_pa && !original_va);
  for(unsigned mode=0;mode<3;mode++) {
    setup(mode==2?BME_READBACK_FAIL:SUCCESS);
    *(UINT32 *)(config+0)=GU_BOARD_ID;*(UINT32 *)(config+0x2c)=GU_BOARD_SUBSYSTEM;
    registers[0]=GU_BOARD_CHIP;registers[0x1183a4/4]=0x5ff4;
    UINT8 *mapped=(UINT8 *)registers+0x300000;
    if(mode==0)*(UINT32 *)(mapped+descriptor+16)=1;
    if(mode==1)*(UINT32 *)(mapped+descriptor+24)=0x40000000;
    UINT64 mask=0xffffffffULL;
    struct pci_dev device={.dev={.dma_mask=&mask,.coherent_dma_mask=0xffffffffULL},.pm_cap=0x60,.refs=1};
    struct unlock_transaction_work work={.pdev=&device,.bar=(void *)(UINTN)Gu.bar_base,
                                        .bar_len=sizeof(registers),.mapped=true};
    work.layout.frts_offset=0x5ff200000ULL;
    int result=unlock_transaction_fwsec(&work);
    CHECK(result==(mode==0?-EINVAL:mode==1?-ERANGE:-EIO));
    CHECK(!work.firmware_started && !Gu.started && !fake.gsp_starts && !fake.sec2_starts);
    CHECK(!fake.mmio_writes && !fake.flrs);
    if(mode<2) {
      CHECK(!work.master && !(config[4]&4));
      CHECK(strstr(fake.log,"DMA geometry rejected before launch"));
    } else {
      CHECK(work.master); /* Geometry passed; the mocked BME readback failed. */
    }
    CHECK(!EFI_ERROR(GuClearMaster()));
    if(work.fwsec_cpu)
      dma_free_coherent(&device.dev,work.fwsec_size,work.fwsec_cpu,work.fwsec_dma);
    CHECK(!Gu.allocations && !Gu.dma_count && !fake.live_maps && !fake.live_buffers && !fake.pool_live);
    printf("Real-ROM FWSEC prelaunch mode=%u result=%d: no MMIO writes, no firmware starts, resources released PASS\n",mode,result);
    dispose_host_fixture();
  }
  return 0;
}
