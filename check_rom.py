# SPDX-License-Identifier: MIT
"""Read-only ROM-file inventory. This never opens the GPU or approves a port."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def inspect_rom(data):
    if not 512 <= len(data) <= 16 * 1024 * 1024:
        raise ValueError('ROM size outside bounded parser limits')
    container = data[:4] == b'NVGI'
    start = 0
    if container:
        # Locate the first aligned, structurally valid NVIDIA PCI image in
        # this file. This does not establish the GPU's live PROM mapping.
        for candidate in range(0, min(len(data), 1024*1024), 512):
            if candidate + 26 > len(data) or data[candidate:candidate+2] != b'\x55\xaa':
                continue
            pointer = candidate + struct.unpack_from('<H', data, candidate+24)[0]
            if (pointer >= candidate+26 and pointer+24 <= len(data) and
                data[pointer:pointer+4] == b'PCIR' and
                struct.unpack_from('<H', data, pointer+4)[0] == 0x10de):
                start = candidate
                break
        else:
            raise ValueError('NVGI container has no valid aligned NVIDIA PCI image')
    images, offset = [], start
    for _ in range(32):
        if offset + 26 > len(data) or data[offset:offset+2] not in (b'\x55\xaa', b'VN', b'\x77\xbb'):
            raise ValueError('Invalid or truncated expansion-ROM header')
        pcir = offset + struct.unpack_from('<H', data, offset + 24)[0]
        if pcir < offset + 26 or pcir + 24 > len(data):
            raise ValueError('PCI data structure outside ROM')
        signature = data[pcir:pcir+4]
        if signature not in (b'PCIR', b'NPDS', b'RGIS'):
            raise ValueError('Missing PCI data signature')
        vendor, device = struct.unpack_from('<HH', data, pcir + 4)
        size = struct.unpack_from('<H', data, pcir + 16)[0] * 512
        last = data[pcir+21] & 128
        structure_size = struct.unpack_from('<H', data, pcir+10)[0]
        if structure_size < 24 or pcir+structure_size > len(data):
            raise ValueError('Invalid PCI data structure length')
        npde = (pcir + structure_size + 15) & ~15
        if npde+4 <= len(data) and data[npde:npde+4] == b'NPDE':
            if npde+10 > len(data):
                raise ValueError('Truncated NVIDIA extension')
            revision, length, blocks = struct.unpack_from('<HHH', data, npde+4)
            if revision in (0x100, 0x101):
                if length < 10 or npde+length > len(data):
                    raise ValueError('Invalid NVIDIA extension extent')
                subsize = blocks * 512
                last = data[npde+10] & 128 if length > 10 else (0 if subsize < size else last)
                size = subsize
        if not size or offset + size > len(data) or pcir + structure_size > offset + size:
            raise ValueError('PCI image extent invalid')
        images.append({'offset': offset, 'bytes': size, 'vendor': f'{vendor:04x}',
                       'device': f'{device:04x}', 'code_type': data[pcir+20],
                       'data_signature': signature.decode('ascii'),
                       'checksum_mod_256': sum(data[offset:offset+size]) & 255})
        if last:
            break
        offset += size
    else:
        raise ValueError('ROM chain limit exceeded')
    # A later NVGI header bounds the first exported bank. Its position is
    # evidence from the file only, not a claim about BAR0's PROM window.
    boundary = len(data)
    if container:
        for at in range((start+8191)//8192*8192, len(data)-3, 8192):
            if at > start and data[at:at+4] == b'NVGI':
                boundary = at
                break
    view_end = min(boundary, start+1024*1024)
    if images[-1]['offset'] + images[-1]['bytes'] > view_end:
        raise ValueError('ROM chain exceeds bounded first-bank view')
    view = data[start:view_end]
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'container': 'NVGI' if container else 'PCI_ROM',
            'file_view_offset': start, 'file_view_bytes': len(view),
            'file_view_sha256': hashlib.sha256(view).hexdigest(),
            'images': images, 'contains_4090_id': any(i['vendor']=='10de' and i['device']=='2684' for i in images),
            'live_prom_mapping_verified': False, 'hardware_execution_enabled': False,
            'note': 'File inventory only. A cold live PROM and EFI RomImage capture must establish mapping and hashes.'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        result = inspect_rom(args.rom.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, str(error) + '\n')
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    print(text, end='')

if __name__ == '__main__':
    main()
