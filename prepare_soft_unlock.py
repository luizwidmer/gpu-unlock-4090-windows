# SPDX-License-Identifier: MIT
"""Test and package a PNY soft-unlock EFI. Never stage or execute it."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import sys
from build import ROOT, UNLOCK, SOURCES, compiler_flags, zig_path, inspect_pe
from test_board import source_hashes

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision',default='r1')
    parser.add_argument('--automatic',action='store_true',help='Five-second Esc cancellation window instead of U')
    args=parser.parse_args()
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,31}',args.revision):raise SystemExit('Invalid revision')
    if args.automatic and args.revision=='r1':raise SystemExit('Automatic builds require a distinct revision')
    if not (UNLOCK/'local_board.h').is_file():raise SystemExit('Missing private local_board.h: configure and review the measured Windows ESP before an enabled build.')
    folder='soft-unlock' if args.revision=='r1' else 'soft-unlock-'+args.revision
    out=ROOT/'build'/folder;out.mkdir(parents=True,exist_ok=True)
    before=source_hashes()
    checks=[('preparation',[sys.executable,str(ROOT/'test.py')]),
            ('board',[sys.executable,str(ROOT/'test_board.py')]),
            ('capture-review',[sys.executable,str(ROOT/'diagnostic/test_verify_capture.py')])]
    if args.automatic:checks.append(('automatic',[sys.executable,str(ROOT/'test_auto_boot.py')]))
    for name,command in checks:
        result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
        (out/(name+'-tests.txt')).write_text(result.stdout+result.stderr,encoding='utf-8')
        if result.returncode:
            print((result.stdout+result.stderr)[-5000:]);raise SystemExit(name+' tests failed; no package prepared')
        print(name+' tests passed',flush=True)
    if before!=source_hashes():raise SystemExit('Source changed while tests ran')
    board=json.loads((ROOT/'build/pny-board-tests/result.json').read_text())
    assert board['passed'] and board['source_sha256']==before
    if args.automatic:
        auto=json.loads((ROOT/'build/auto-boot-tests/result.json').read_text())
        assert auto['passed'] and auto['source_sha256']==before
    package=out/'usb-package';efi=out/'PNY-RTX4090-SOFT-UNLOCK.EFI'
    command=[zig_path(),'cc','-target','x86_64-uefi',*compiler_flags(),
             '-DGU_SOFT_UNLOCK_BUILD=1',*(['-DGU_AUTO_BOOT_BUILD=1'] if args.automatic else []),
             '-ffreestanding','-fno-builtin','-fno-stack-protector',
             '-mno-red-zone','-Os','-g0','-nostdlib',
             *[str(UNLOCK/(s+'.c')) for s in SOURCES],'-o',str(efi)]
    result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
    (out/'build.txt').write_text(result.stdout+result.stderr,encoding='utf-8')
    if result.returncode:raise SystemExit('EFI compile failed: '+result.stderr[-3000:])
    data=efi.read_bytes();pe=inspect_pe(data,require_locked=False)
    if pe['entry_immediate_access_denied']:raise SystemExit('Soft build unexpectedly has inert entry')
    if before!=source_hashes():raise SystemExit('Source changed during build')
    assets=json.loads((ROOT/'assets-verified.json').read_text())
    contents={'EFI/BOOT/BOOTX64.EFI':data}
    for name,expected in assets.items():
        asset=(ROOT/'assets'/name).read_bytes()
        assert len(asset)==expected['bytes'] and hashlib.sha256(asset).hexdigest()==expected['sha256']
        contents['RTX4090-PREP/ASSETS/'+name]=asset
    contents['SOFT-UNLOCK-INSTRUCTIONS.md']=(ROOT/('AUTO-BOOT.md' if args.automatic else 'SOFT-UNLOCK.md')).read_bytes()
    files={}
    for relative,content in contents.items():
        destination=package/relative;destination.parent.mkdir(parents=True,exist_ok=True)
        # A repeated identical build is allowed, differing existing package files
        # need an explicit new output folder rather than silently changing a kit.
        if destination.exists():assert destination.read_bytes()==content, 'Existing kit differs: '+relative
        else:destination.write_bytes(content)
        digest=hashlib.sha256(destination.read_bytes()).hexdigest()
        assert digest==hashlib.sha256(content).hexdigest()
        files[relative]={'bytes':len(content),'sha256':digest}
    report={**pe,'revision':args.revision,'artifact':str(efi.relative_to(ROOT)),'bytes':len(data),
            'sha256':hashlib.sha256(data).hexdigest(),'hardware_entry_enabled':True,
            'manual_U_required':not args.automatic,'automatic_boot':args.automatic,
            'cancel_window_seconds':5 if args.automatic else 0,
            'max_sec2_launches':1,'automatic_retry':False,
            'hardware_unlock_tested':False,'live_dma_mapping_validated':False,
            'boot_settings_changed':False,'usb_staged':False,'unlock_executed':False,
            'tests':board['results'],'source_sha256':before,'files':files}
    if args.automatic:report['automatic_tests']=auto['results']
    (out/'build-result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('source_sha256','files')},indent=2))

if __name__=='__main__':main()
