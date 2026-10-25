# SPDX-License-Identifier: MIT
"""Test automatic USB entry on the native host. Never execute EFI or hardware I/O."""
import json
import subprocess
from build import ROOT, UNLOCK, SOURCES, compiler_flags, zig_path
from test_board import CAPTURE, source_hashes

def main():
    if not (CAPTURE/'PROM.BIN').is_file() or not (CAPTURE/'PCI-BEFORE.BIN').is_file():
        raise SystemExit('Private matching board capture and firmware assets are required; see README.md.')
    out=ROOT/'build/auto-boot-tests';out.mkdir(parents=True,exist_ok=True)
    before=source_hashes();results={}
    core=['port','compat','vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval']
    parts=['app','app_io','ownership','format','sha256','checks',*core]
    for name,defines,test,sources,args in (
        ('automatic',['-DGU_SOFT_UNLOCK_BUILD=1','-DGU_AUTO_BOOT_BUILD=1'],
         UNLOCK/'tests/test_pny_app.c',parts,
         [str(CAPTURE/'PROM.BIN'),str(ROOT/'assets'),str(CAPTURE/'PCI-BEFORE.BIN')]),
        ('auto-flag-alone-locked',['-DGU_AUTO_BOOT_BUILD=1'],ROOT/'tests/test_entry_lock.c',
         [s for s in SOURCES if s!='runtime'],[])):
        exe=out/(name+'.exe')
        command=[zig_path(),'cc','-target','x86_64-windows-gnu',*compiler_flags(),*defines,
                 '-Wno-unused-function','-Wno-misleading-indentation','-Wno-format','-O1',str(test),
                 *[str(UNLOCK/(s+'.c')) for s in sources],'-o',str(exe)]
        for phase,cmd in [('build',command),('run',[str(exe),*args])]:
            result=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
            (out/(name+'-'+phase+'.txt')).write_text(result.stdout+result.stderr,encoding='utf-8')
            if result.returncode:
                print((result.stdout+result.stderr)[-5000:]);raise SystemExit(name+' '+phase+' failed')
        results[name]=result.stdout.strip().splitlines()[-1]
        print(results[name],flush=True)
    assert before==source_hashes(),'Sources changed during automatic tests'
    (out/'result.json').write_text(json.dumps({'passed':True,'hardware_io':False,
        'source_sha256':before,'results':results},indent=2)+'\n')

if __name__=='__main__':main()
