/* Modified 2026-10-21: experimental AD103 32GB GA102-pair port; see PORT-NOTES.txt. */
/* Derived from the v0.3 handoff path exercised on this user's single AD103. */
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
static EFI_GUID FsGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static const CHAR16 WindowsPath[]=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
static EFI_HANDLE WindowsImage,WindowsVolume;
static EFI_DEVICE_PATH_PROTOCOL *WindowsDevicePath;
static EFI_STATUS MakeWindowsPath(VOID) {
  EFI_DEVICE_PATH_PROTOCOL *base=NULL;
  EFI_STATUS s=Gu.bs->HandleProtocol(WindowsVolume,&PathGuid,(VOID**)&base);
  if(EFI_ERROR(s)||!base)return EFI_NOT_FOUND;
  UINTN off=0;BOOLEAN end=FALSE;
  for(UINTN i=0;i<128 && off<=4092;i++) {
    EFI_DEVICE_PATH_PROTOCOL *n=(VOID*)((UINT8*)base+off);
    UINTN len=n->Length[0]+((UINTN)n->Length[1]<<8);
    if(len<4||len>4096-off)return EFI_COMPROMISED_DATA;
    if(n->Type==END_DEVICE_PATH_TYPE) {
      if(n->SubType!=END_ENTIRE_DEVICE_PATH_SUBTYPE||len!=4)return EFI_UNSUPPORTED;
      end=TRUE;break;
    }
    off+=len;
  }
  if(!end)return EFI_COMPROMISED_DATA;
  UINTN node=4+sizeof(WindowsPath),total=off+node+4;
  UINT8 *b=GuAlloc(total);if(!b)return EFI_OUT_OF_RESOURCES;
  memcpy(b,base,off);b[off]=MEDIA_DEVICE_PATH;b[off+1]=MEDIA_FILEPATH_DP;
  b[off+2]=(UINT8)node;b[off+3]=(UINT8)(node>>8);memcpy(b+off+4,WindowsPath,sizeof(WindowsPath));
  b[off+node]=END_DEVICE_PATH_TYPE;b[off+node+1]=END_ENTIRE_DEVICE_PATH_SUBTYPE;b[off+node+2]=4;
  WindowsDevicePath=(VOID*)b;return EFI_SUCCESS;
}
static EFI_STATUS PreloadWindows(VOID) {
  EFI_HANDLE *h=NULL;UINTN n=0,found=0;
  EFI_STATUS s=Gu.bs->LocateHandleBuffer(ByProtocol,&FsGuid,NULL,&n,&h);
  if(EFI_ERROR(s))return s;
  if(!h||n>256){GuFree(h);return EFI_COMPROMISED_DATA;}
  for(UINTN i=0;i<n;i++) {
    EFI_DEVICE_PATH_PROTOCOL *volume_path=NULL;
    EFI_STATUS path_status=Gu.bs->HandleProtocol(h[i],&PathGuid,(VOID**)&volume_path);
    BOOLEAN matched=!EFI_ERROR(path_status) && GuWindowsEspPath(volume_path,4096);
    GuLog("Windows volume[%zu] handle=%#llx path_status=%#llx pinned_esp=%u\n",
      (size_t)i,(unsigned long long)(UINTN)h[i],(unsigned long long)path_status,matched);
    if(!GuLogReady()){s=EFI_DEVICE_ERROR;break;}
    if(!matched)continue;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;EFI_FILE_PROTOCOL *root=NULL,*file=NULL;
    EFI_STATUS x=Gu.bs->HandleProtocol(h[i],&FsGuid,(VOID**)&fs);
    GuLog("Windows target filesystem: status=%#llx valid=%u\n",(unsigned long long)x,fs!=NULL);
    if(!EFI_ERROR(x)&&fs) {
      x=fs->OpenVolume(fs,&root);
      GuLog("Windows target OpenVolume: status=%#llx valid=%u\n",(unsigned long long)x,root!=NULL);
    }
    if(EFI_ERROR(x)||!root){s=EFI_ERROR(x)?x:EFI_DEVICE_ERROR;break;}
    x=root->Open(root,&file,(CHAR16*)WindowsPath,EFI_FILE_MODE_READ,0);
    GuLog("Windows target bootmgfw Open: status=%#llx valid=%u\n",(unsigned long long)x,file!=NULL);
    if(!EFI_ERROR(x)&&file) {
      UINT8 mz[2];UINTN bytes=2;EFI_STATUS y=file->Read(file,&bytes,mz);
      EFI_STATUS closed=file->Close(file);
      GuLog("Windows target Read: status=%#llx bytes=%zu Close=%#llx\n",
        (unsigned long long)y,(size_t)bytes,(unsigned long long)closed);
      if(EFI_ERROR(y)||EFI_ERROR(closed)||bytes!=2||mz[0]!='M'||mz[1]!='Z')s=EFI_COMPROMISED_DATA;
      else {found++;WindowsVolume=h[i];}
    } else if(x!=EFI_NOT_FOUND)s=EFI_ERROR(x)?x:EFI_DEVICE_ERROR;
    x=root->Close(root);GuLog("Windows target root Close: status=%#llx\n",(unsigned long long)x);
    if(EFI_ERROR(x))s=x;
    if(!GuLogReady())s=EFI_DEVICE_ERROR;
    if(EFI_ERROR(s))break;
  }
  GuFree(h);GuLog("Windows candidates=%zu status=%#llx\n",(size_t)found,(unsigned long long)s);
  if(EFI_ERROR(s))return s;
  if(found!=1)return found?EFI_NO_MAPPING:EFI_NOT_FOUND;
  s=MakeWindowsPath();if(EFI_ERROR(s))return s;
  s=Gu.bs->LoadImage(FALSE,Gu.image,WindowsDevicePath,NULL,0,&WindowsImage);
  GuLog("Windows LoadImage=%#llx\n",(unsigned long long)s);
  if(EFI_ERROR(s))return s;
  EFI_LOADED_IMAGE_PROTOCOL *li=NULL;
  s=Gu.bs->HandleProtocol(WindowsImage,&LoadedGuid,(VOID**)&li);
  if(EFI_ERROR(s)||!li||li->DeviceHandle!=WindowsVolume||li->ImageCodeType!=EfiLoaderCode)return EFI_COMPROMISED_DATA;
  return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS VerifySelectors(const char *phase) {
  UINT32 ss0=GuRead32(0x82381c),ss1=GuRead32(0x823820);
  UINT32 plm=GuRead32(0x823804),effective=GuRead32(0x823818);
  BOOLEAN good=Gu4090SelectorsValid(plm,ss0,ss1,effective);
  GuLog("%s PLM=%#x SS0=%#x SS1=%#x effective=%#x matched=%u\n",phase,plm,ss0,ss1,effective,good);
  return !Gu.fault && GuLogReady() && good?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS RecoveryPciCheck(VOID) {
  EFI_STATUS s=GuRefreshTargetPci();if(EFI_ERROR(s))return s;
  UINT16 command=0;UINT32 value=0;
  s=GuConfigRead(EfiPciIoWidthUint16,4,&command);
  if(EFI_ERROR(s)||command!=(UINT16)InitialConfig[1]||(command&4))return EFI_DEVICE_ERROR;
  for(UINT32 offset=0x10;offset<=0x24;offset+=4) {
    s=GuConfigRead(EfiPciIoWidthUint32,offset,&value);
    if(EFI_ERROR(s)||value!=InitialConfig[offset/4])return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}
/* Find the freshly published output even if its framebuffer is still inactive.
 * Only the unique original GPU/output route may receive a SetMode call. */
static EFI_STATUS RecoveryOutput(EFI_HANDLE *Output,EFI_GRAPHICS_OUTPUT_PROTOCOL **Gop) {
  EFI_HANDLE *handles=NULL;UINTN count=0,targets=0,matches=0;
  *Output=NULL;*Gop=NULL;
  EFI_STATUS s=Gu.bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&handles);
  if(EFI_ERROR(s)){if(handles)GuFree(handles);return s;}
  if(count>128 || (count && !handles)){GuFree(handles);return EFI_COMPROMISED_DATA;}
  for(UINTN i=0;i<count;i++) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;EFI_DEVICE_PATH_PROTOCOL *path=NULL;UINT8 hash[32];
    EFI_STATUS gs=Gu.bs->HandleProtocol(handles[i],&GopGuid,(VOID**)&g);
    EFI_STATUS ps=Gu.bs->HandleProtocol(handles[i],&PathGuid,(VOID**)&path);
    BOOLEAN target=!EFI_ERROR(ps) && DisplayRoute(path,hash,NULL,NULL);
    BOOLEAN same=target && !memcmp(hash,SavedDisplay.path_hash,sizeof(hash));
    GuLog("GOP recovery[%zu] handle=%#llx protocol=%#llx path=%#llx target=%u saved_route=%u\n",
      (size_t)i,(unsigned long long)(UINTN)handles[i],(unsigned long long)gs,(unsigned long long)ps,target,same);
    if(!EFI_ERROR(gs) && g && g->Mode)GuLog("GOP recovery mode=%u max=%u info_bytes=%zu framebuffer=%#llx bytes=%zu\n",
      g->Mode->Mode,g->Mode->MaxMode,(size_t)g->Mode->SizeOfInfo,
      (unsigned long long)g->Mode->FrameBufferBase,(size_t)g->Mode->FrameBufferSize);
    if(target)targets++;
    if(same && !EFI_ERROR(gs) && g && g->Mode){matches++;*Output=handles[i];*Gop=g;}
  }
  GuFree(handles);
  GuLog("GOP recovery targets=%zu saved_matches=%zu\n",(size_t)targets,(size_t)matches);
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  return targets==1 && matches==1?EFI_SUCCESS:EFI_NO_MAPPING;
}
static EFI_STATUS RestoreDisplayForWindows(const GU_RELEASE_OWNER *Owner) {
  if(!WindowsImage || !Owner || !Owner->agent || !SavedDisplay.valid || SavedDisplay.remaining_bytes<4 ||
     !GuScreenDetached || !GuSafeToReturn() || !Gu.quiesced ||
     Gu.flrs!=1 || Gu.allocations || Gu.dma_count || !Gu.owned || Gu.fault)return EFI_NOT_READY;
  EFI_STATUS s=VerifySelectors("POST-FLR");if(EFI_ERROR(s))return s;
  GuLogText("stage=before_GOP_reconnect; transaction quiesced, DMA released, PCI restored\n");
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  s=Gu.bs->CloseProtocol(Gu.handle,&PciGuid,Gu.image,NULL);if(EFI_ERROR(s))return s;
  Gu.owned=FALSE;
  EFI_HANDLE drivers[2]={Owner->agent,NULL};
  /* An End-only RemainingDevicePath can return success without starting a driver. */
  EFI_DEVICE_PATH_PROTOCOL *route=SavedDisplay.remaining_bytes>4?(VOID*)SavedDisplay.remaining:NULL;
  GuLog("GOP reconnect saved_mode=%u child_route_bytes=%zu directed=%u\n",
    SavedDisplay.mode,(size_t)SavedDisplay.remaining_bytes,route!=NULL);
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  s=Gu.bs->ConnectController(Gu.handle,drivers,route,TRUE);
  GuLog("ConnectController status=%#llx\n",(unsigned long long)s);
  if(EFI_ERROR(s))return s;
  s=GuVerifyRecoveryOwner(Owner);if(EFI_ERROR(s))return s;
  s=RecoveryPciCheck();if(EFI_ERROR(s))return s;
  s=VerifySelectors("PRE-SETMODE");if(EFI_ERROR(s))return s;
  EFI_HANDLE output=NULL;EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;
  s=RecoveryOutput(&output,&g);if(EFI_ERROR(s))return s;
  if(!g->QueryMode || !g->SetMode || SavedDisplay.mode>=g->Mode->MaxMode)return EFI_UNSUPPORTED;
  UINTN bytes=0;EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info=NULL;
  s=g->QueryMode(g,SavedDisplay.mode,&bytes,&info);
  BOOLEAN compatible=!EFI_ERROR(s) && info && bytes>=sizeof(*info) && SameVideoInfo(info,&SavedDisplay.info);
  /* EFI_NOT_STARTED is explicitly repaired by SetMode, using the saved mode.
   * All geometry and framebuffer checks remain mandatory after initialization. */
  BOOLEAN needs_init=s==EFI_NOT_STARTED;
  GuLog("GOP QueryMode mode=%u status=%#llx bytes=%zu compatible=%u needs_init=%u\n",
    SavedDisplay.mode,(unsigned long long)s,(size_t)bytes,compatible,needs_init);
  if(info){EFI_STATUS freed=Gu.bs->FreePool(info);if(EFI_ERROR(freed))return freed;}
  if((!compatible && !needs_init) || !GuLogReady())return EFI_UNSUPPORTED;
  s=RecoveryPciCheck();if(EFI_ERROR(s))return s;
  s=GuVerifyRecoveryOwner(Owner);if(EFI_ERROR(s))return s;
  s=VerifySelectors("PRE-SETMODE-RECHECK");if(EFI_ERROR(s))return s;
  EFI_HANDLE checked_output=NULL;EFI_GRAPHICS_OUTPUT_PROTOCOL *checked_gop=NULL;
  s=RecoveryOutput(&checked_output,&checked_gop);if(EFI_ERROR(s))return s;
  if(checked_output!=output || checked_gop!=g || !g->Mode || SavedDisplay.mode>=g->Mode->MaxMode)return EFI_NOT_READY;
  GuLog("stage=GOP_SetMode mode=%u\n",SavedDisplay.mode);if(!GuLogReady())return EFI_DEVICE_ERROR;
  s=g->SetMode(g,SavedDisplay.mode);
  GuLog("GOP SetMode status=%#llx\n",(unsigned long long)s);if(EFI_ERROR(s))return s;
  EFI_HANDLE fresh_output=NULL;g=NULL;
  s=RecoveryOutput(&fresh_output,&g);if(EFI_ERROR(s))return s;
  if(fresh_output!=output || g->Mode->Mode!=SavedDisplay.mode || !g->Mode->Info ||
     g->Mode->SizeOfInfo<sizeof(*g->Mode->Info) || !SameVideoInfo(g->Mode->Info,&SavedDisplay.info))return EFI_COMPROMISED_DATA;
  ExpectDetached=FALSE;
  if(!DisplayCheck())return EFI_DEVICE_ERROR;
  s=GuVerifyRecoveryOwner(Owner);if(EFI_ERROR(s))return s;
  s=RecoveryPciCheck();if(EFI_ERROR(s))return s;
  s=VerifySelectors("POST-GOP");if(EFI_ERROR(s))return s;
  GuLogText("stage=before_StartImage; POST-GOP selectors and PCI BAR/command verified\n");
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  GuScreenDetached=FALSE;
  return EFI_SUCCESS;
}
