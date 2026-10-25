# SPDX-License-Identifier: MIT
"""Validate the exported ROM's FWSEC using the port's actual C file/DMA parsers."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from build import ROOT, UNLOCK, zig_path
from check_rom import inspect_rom

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom',type=Path)
    args=parser.parse_args()
    data=args.rom.read_bytes()
    report=inspect_rom(data)
    if not report['contains_4090_id']:
        raise SystemExit('ROM has no NVIDIA RTX 4090 PCI image')
    offset,length=report['file_view_offset'],report['file_view_bytes']
    view=data[offset:offset+length]
    output=ROOT/'build'
    output.mkdir(exist_ok=True)
    view_path=output/'exported-rom-first-bank-view.bin'
    view_path.write_bytes(view)
    exe=output/'rom-dma-inspect-host.exe'
    subprocess.run([zig_path(),'cc','-target','x86_64-windows-gnu','-std=c11',
                    '-Wall','-Wextra','-Werror','-Wno-unused-parameter','-O1',
                    str(ROOT/'rom_dma_inspect.c'),
                    *[str(UNLOCK/('vendor/'+s+'.c')) for s in ('vbios_610','prep_610','layout_610')],
                    '-o',str(exe)],check=True)
    result=subprocess.run([str(exe),str(view_path)],capture_output=True,text=True)
    (output/'rom-dma-inspection.log').write_text(result.stderr,encoding='utf-8')
    if result.returncode:
        raise SystemExit(f'Offline ROM/FWSEC validation failed ({result.returncode}): '+result.stderr[-2000:])
    report['dma_validation']=json.loads(result.stdout)
    report['port_source_sha256']={s:hashlib.sha256((UNLOCK/('vendor/'+s)).read_bytes()).hexdigest()
                                 for s in ('vbios_610.c','prep_610.c','unlock_610.c')}
    (ROOT/'rom-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'rom_sha256':report['sha256'],'file_view_offset':offset,
                      'fwsec':report['dma_validation']['fwsec'],
                      'offline_dma_validation_passed':True,
                      'live_prom_mapping_verified':False,'hardware_execution_enabled':False},indent=2))

if __name__=='__main__':main()
