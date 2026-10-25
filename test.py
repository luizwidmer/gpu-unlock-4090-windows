# SPDX-License-Identifier: MIT
"""Native host and file-only tests. No GPU APIs, EFI execution or hardware I/O."""
from pathlib import Path
import json
import subprocess
import sys
from build import ROOT, UNLOCK, SOURCES, compiler_flags, zig_path

def run(command, name):
    result = subprocess.run(command, capture_output=True, text=True, cwd=ROOT)
    (ROOT / ('build/' + name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        print((result.stdout + result.stderr)[-6000:])
        raise SystemExit(name + ' failed')
    return (result.stdout.strip() or result.stderr.strip())[-1000:]

def main():
    (ROOT / 'build').mkdir(exist_ok=True)
    results = {}
    out = ROOT / 'build/entry-lock-host.exe'
    run([zig_path(), 'cc', '-target', 'x86_64-windows-gnu', *compiler_flags(), '-O0',
         str(ROOT/'tests/test_entry_lock.c'),
         *[str(UNLOCK/(s+'.c')) for s in SOURCES if s != 'runtime'], '-o', str(out)], 'entry-lock-build')
    results['entry_lock_and_selectors'] = run([str(out)], 'entry-lock')
    for name in ('dma_geometry','dma_early_reject'):
        out=ROOT/('build/'+name+'-host.exe')
        sources=[s for s in SOURCES if s!='runtime' and (name!='dma_early_reject' or s!='vendor/unlock_610')]
        run([zig_path(),'cc','-target','x86_64-windows-gnu',*compiler_flags(),
             '-Wno-unused-function','-O0',str(ROOT/('tests/test_'+name+'.c')),
             *[str(UNLOCK/(s+'.c')) for s in sources],'-o',str(out)],name+'-build')
        results[name]=run([str(out)],name)
    kernel = ROOT / 'upstream-4090/kernel/gpu_unlock'
    tests = ROOT / 'upstream-4090/tests'
    cases = {'gfw_eval':['gfw_eval.c'], 'layout_610':['layout_610.c'],
             'prep_610':['layout_610.c','prep_610.c'], 'vbios_610':['vbios_610.c'],
             'ad102_kernel':['gfw_eval.c','prep_610.c','layout_610.c','vbios_610.c']}
    for name, sources in cases.items():
        out = ROOT / ('build/upstream-'+name+'.exe')
        shim = ['-I'+str(tests/'unlock_shim')] if name=='ad102_kernel' else []
        run([zig_path(), 'cc', '-target', 'x86_64-windows-gnu', '-std=c11',
             '-Wall','-Wextra','-Werror','-Wno-unused-parameter', '-I'+str(ROOT/'tests/native_include'), '-I'+str(kernel), *shim,
             *[str(kernel/s) for s in sources], str(tests/('test_'+name+'.c')), '-o', str(out)], name+'-build')
        # The public checkout has no proprietary firmware. Its synthetic AD102
        # tests still run; exact-asset transaction cases require all local pins.
        pins=json.loads((ROOT/'firmware-manifest.json').read_text())
        have_assets=all((ROOT/'assets'/asset).is_file() for asset in pins)
        arguments = ['-', str(ROOT/'assets')] if name=='ad102_kernel' and have_assets else []
        results[name] = run([str(out), *arguments], name)
    results['rom_parser'] = run([sys.executable, '-m', 'unittest', 'discover', '-s', str(ROOT/'tests'), '-v'], 'rom-parser')
    rom=ROOT/'inputs/PNY-RTX4090.rom'
    if rom.is_file():
        results['exported_rom_dma']=run([sys.executable,str(ROOT/'validate_rom_dma.py'),str(rom)],'exported-rom-dma')
        out=ROOT/'build/fwsec-prelaunch-host.exe'
        parts=['port','compat','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval']
        run([zig_path(),'cc','-target','x86_64-windows-gnu',*compiler_flags(),
             '-Wno-unused-function','-Wno-misleading-indentation','-Wno-format','-O0',
             str(ROOT/'tests/test_fwsec_prelaunch.c'),*[str(UNLOCK/(s+'.c')) for s in parts],
             '-o',str(out)],'fwsec-prelaunch-build')
        results['fwsec_prelaunch_integration']=run([str(out),str(ROOT/'build/exported-rom-first-bank-view.bin')],'fwsec-prelaunch')
    (ROOT/'build/tests-result.json').write_text(json.dumps({'hardware_io':False,
        'exact_firmware_asset_cases_run':have_assets,'private_rom_cases_run':rom.is_file(),
        'results':results},indent=2)+'\n')
    print(json.dumps({'hardware_io':False,'passed_groups':list(results)},indent=2))

if __name__=='__main__':
    main()
