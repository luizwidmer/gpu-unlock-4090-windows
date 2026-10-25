# SPDX-License-Identifier: MIT
"""PNY captured-board integration tests. Native host only; all EFI I/O is fake."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
from build import ROOT, UNLOCK, compiler_flags, zig_path

CAPTURE=ROOT/'inputs/captures/20261024-CAP00'

def source_hashes():
    paths=[p for p in UNLOCK.rglob('*') if p.is_file() and p.suffix in ('.c','.h','.inc')]
    paths += list(ROOT.glob('*.py'))+list((ROOT/'tests').glob('*.c'))+list((ROOT/'tests').glob('*.py'))
    paths += list((ROOT/'diagnostic').glob('*.py'))
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}

def main():
    if not (CAPTURE/'PROM.BIN').is_file() or not (ROOT/'inputs/PNY-RTX4090.rom').is_file():
        raise SystemExit('Private matching board capture and VBIOS ROM are required; see README.md.')
    sys.path.insert(0,str(ROOT/'diagnostic'))
    from verify_capture import verify
    review=verify(CAPTURE,ROOT/'inputs/PNY-RTX4090.rom')
    assert review['capture_structure_valid'] and review['active_rom_chain_matches_prom']
    assert review['efi_rom_matches_exact_legacy_efi_prefix']
    assert review['sha256']['PCI-BEFORE.BIN']=='a3cea968c1c9ddbe40428adc45785b553e9d5f87432ecdd52ddb97eff4f07b02'
    assert review['sha256']['PROM.BIN']=='1388f2b43a6dedf65c2fad8a055591c8cabaf37391ae54fd658ea43a38f2c136'
    out=ROOT/'build/pny-board-tests';out.mkdir(parents=True,exist_ok=True)
    core=['port','compat','vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval']
    reports={}
    for name,extra,args in (
        ('core',[],[str(CAPTURE/'PROM.BIN'),str(ROOT/'assets')]),
        ('app',['app','app_io','ownership','format','sha256','checks'],
         [str(CAPTURE/'PROM.BIN'),str(ROOT/'assets'),str(CAPTURE/'PCI-BEFORE.BIN')])):
        exe=out/(name+'.exe')
        command=[zig_path(),'cc','-target','x86_64-windows-gnu',*compiler_flags(),
                 '-DGU_SOFT_UNLOCK_BUILD=1','-Wno-unused-function','-Wno-misleading-indentation',
                 '-Wno-format','-O1',str(UNLOCK/('tests/test_pny_'+name+'.c')),
                 *[str(UNLOCK/(s+'.c')) for s in extra+core],'-o',str(exe)]
        for phase,cmd in [('build',command),('run',[str(exe),*args])]:
            result=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
            (out/(name+'-'+phase+'.txt')).write_text(result.stdout+result.stderr,encoding='utf-8')
            if result.returncode:
                print((result.stdout+result.stderr)[-4500:]);raise SystemExit(name+' '+phase+' failed')
        reports[name]=result.stdout.strip().splitlines()[-1]
        print(reports[name],flush=True)
    report={'passed':True,'hardware_io':False,'efi_services_simulated':True,
            'live_dma_validated':False,'layout_inputs':'VGA/MMU inputs simulated; not included in CAP00',
            'capture_sha256':review['sha256'],'source_sha256':source_hashes(),'results':reports}
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
