# SPDX-License-Identifier: MIT
"""Build the locked RTX 4090 candidate. No install, USB staging or execution."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import struct
import subprocess

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / 'port/source'
UNLOCK = SOURCE / 'unlock'
SOURCES = ['app', 'app_io', 'ownership', 'format', 'runtime', 'sha256', 'checks',
           'port', 'compat', 'vendor/unlock_610', 'vendor/prep_610',
           'vendor/layout_610', 'vendor/vbios_610', 'vendor/gfw_eval']

def compiler_flags():
    return ['-std=c11', '-fshort-wchar', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-parameter', '-D__KERNEL__', '-DGU_UEFI_PORT',
            '-DGU_TARGET_PCI_ID=0x268410deU', '-I' + str(UNLOCK / 'compat'),
            '-I' + str(SOURCE / 'deps/edk2/MdePkg/Include'),
            '-I' + str(SOURCE / 'deps/edk2/MdePkg/Include/X64')]

def zig_path(value=None):
    return value or os.environ.get('ZIG_EXE') or str(ROOT / 'tools/zig-x86_64-windows-0.14.1/zig.exe')

def inspect_pe(data, require_locked=True):
    u16 = lambda offset: struct.unpack_from('<H', data, offset)[0]
    u32 = lambda offset: struct.unpack_from('<I', data, offset)[0]
    pe = u32(60)
    opt = pe + 24
    if data[:2] != b'MZ' or data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Not PE/COFF')
    if (u16(pe+4), u16(opt), u16(opt+68)) != (0x8664, 0x20b, 10):
        raise ValueError('Not an AMD64 EFI application')
    if u16(opt+70) & 0x140 != 0x140 or not u32(opt+112+5*8+4):
        raise ValueError('Missing relocation/security characteristics')
    for directory in (1, 12, 13):
        if data[opt+112+directory*8:opt+120+directory*8] != b'\0'*8:
            raise ValueError('Unexpected imports')
    entry = u32(opt+16)
    start = opt + u16(pe+20)
    entry_sections = [t for t in [start+40*i for i in range(u16(pe+6))]
                      if u32(t+12) <= entry < u32(t+12)+u32(t+8) and u32(t+36)&0x20000000]
    if len(entry_sections) != 1:
        raise ValueError('Entry point outside executable section')
    section = entry_sections[0]
    entry_offset = u32(section+20) + entry - u32(section+12)
    # Pinned Zig 0.14.1 x64 stub: stack frame, mov EFI_ACCESS_DENIED to RAX,
    # restore frame, ret. No call, pointer dereference, I/O or branch.
    expected = bytes.fromhex('554889e548b80f000000000000805dc3')
    locked=data[entry_offset:entry_offset+len(expected)] == expected
    if require_locked and not locked:
        raise ValueError('Entry is not the reviewed immediate ACCESS_DENIED stub')
    return {'machine': 'AMD64', 'subsystem': 'EFI_APPLICATION',
            'relocations': True, 'imports': False, 'entry_immediate_access_denied': locked,
            'entry_machine_code': data[entry_offset:entry_offset+len(expected)].hex()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zig')
    args = parser.parse_args()
    lock = (UNLOCK / 'board_profile.h').read_text()
    if 'static inline int GuPortHardwareEnabled(void) { return 0; }' not in lock:
        raise SystemExit('This builder requires the source-level hardware entry lock.')
    out = ROOT / 'build/RTX4090-PREP-LOCKED.EFI'
    out.parent.mkdir(exist_ok=True)
    cmd = [zig_path(args.zig), 'cc', '-target', 'x86_64-uefi', *compiler_flags(),
           '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-mno-red-zone',
           '-Os', '-g0', '-nostdlib', *[str(UNLOCK / (s+'.c')) for s in SOURCES], '-o', str(out)]
    subprocess.run(cmd, check=True)
    data = out.read_bytes()
    report = dict(inspect_pe(data), artifact=str(out.relative_to(ROOT)), bytes=len(data),
                  sha256=hashlib.sha256(data).hexdigest(), hardware_execution_enabled=False,
                  hardware_tested=False, boot_settings_changed=False,
                  profile_ready=True, zig_version=subprocess.check_output([zig_path(args.zig), 'version'], text=True).strip(),
                  source_sha256={str(p.relative_to(SOURCE)):hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in sorted(UNLOCK.rglob('*')) if p.is_file() and p.suffix in ('.c','.h','.inc')})
    (ROOT / 'build/build-result.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='source_sha256'}, indent=2))

if __name__ == '__main__':
    main()
