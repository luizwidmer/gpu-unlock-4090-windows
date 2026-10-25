/* SPDX-License-Identifier: GPL-2.0-only
 * Offline file parser. No driver, MMIO, GPU reset or firmware execution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port/source/unlock/vendor/vbios_610.h"
#include "port/source/unlock/vendor/prep_610.h"
int main(int argc,char **argv) {
  if(argc!=2)return 2;
  FILE *file=fopen(argv[1],"rb");if(!file)return 3;
  if(fseek(file,0,SEEK_END))return 4;
  long length=ftell(file);if(length<512 || length>1024*1024)return 5;
  if(fseek(file,0,SEEK_SET))return 6;
  u8 *rom=malloc((size_t)length);if(!rom)return 7;
  if(fread(rom,1,(size_t)length,file)!=(size_t)length)return 8;
  fclose(file);
  struct gpu_unlock_fwsec fw={0};u32 transfer=0;
  int result=gpu_unlock_vbios_fwsec(rom,(size_t)length,&fw);
  if(result || fw.engine_mask!=0x400)return 9;
  result=gpu_unlock_fwsec_dmem_transfer(&fw,&transfer);if(result)return 10;
  const u64 addresses[]={0x12000000ULL,0x100000000ULL,0x100000000000ULL};
  for(unsigned i=0;i<sizeof(addresses)/sizeof(*addresses);i++)
    if(gpu_unlock_falcon_hs_geometry(addresses[i],0,fw.imem_size,fw.imem_va,
                                    fw.imem_pa,fw.data_off,transfer,fw.dmem_pa))return 11;
  unsigned signatures=0;
  for(unsigned version=0;version<16;version++)if(fw.sig_versions&(1U<<version)) {
    u32 offset=0;
    if(gpu_unlock_fwsec_sig_offset(fw.sig_versions,version,384,&offset) ||
       offset>fw.sig_bytes || fw.sig_bytes-offset<384)return 12;
    signatures++;
  }
  if(signatures!=fw.sig_count)return 13;
  u8 *image=malloc(fw.image_size);if(!image)return 14;
  memcpy(image,rom+fw.image_off,fw.image_size);
  if(gpu_unlock_fwsec_patch_frts(image,&fw,0x5ff200000ULL))return 15;
  free(image);free(rom);
  printf("{\"offline_only\":true,\"hardware_io\":false,\"fwsec\":{"
         "\"image_offset\":%u,\"image_bytes\":%u,\"imem_bytes\":%u,"
         "\"imem_va\":%u,\"imem_pa\":%u,\"data_offset\":%u,"
         "\"dmem_declared_bytes\":%u,\"dmem_transfer_bytes\":%u,\"dmem_pa\":%u,"
         "\"engine_mask\":%u,\"ucode_id\":%u,\"signature_count\":%u,"
         "\"signature_versions\":%u},\"sample_dma_mappings_validated\":3,"
         "\"signature_offsets_validated\":%u,\"frts_patch_on_copy_validated\":true,"
         "\"live_dma_mapping_validated\":false}\n",
         fw.image_off,fw.image_size,fw.imem_size,fw.imem_va,fw.imem_pa,fw.data_off,
         fw.dmem_size,transfer,fw.dmem_pa,fw.engine_mask,fw.ucode_id,fw.sig_count,
         fw.sig_versions,signatures);
  return 0;
}
