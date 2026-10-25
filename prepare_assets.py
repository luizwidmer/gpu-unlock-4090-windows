# SPDX-License-Identifier: MIT
"""Prepare hash-pinned firmware locally. Never execute firmware or install it."""
from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parent
TOOLS = ROOT / 'port/source/firmware-tools'
OUT = ROOT / 'assets'
CACHE = ROOT / 'local/cache'

def module(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

def main():
    if sys.version_info < (3, 14):
        raise SystemExit('Use Python 3.14+ for native tar.zst support; API environments stay unchanged.')
    subprocess.run([sys.executable, str(TOOLS / 'fetch_artifacts.py'), str(CACHE)], check=True)
    generator = module('generate_v67')
    def extract(package, name):
        with tarfile.open(package, 'r:*') as archive:
            return generator.read_member(archive, name)
    generator.extract_member = extract
    manifest = json.loads((TOOLS / 'artifacts-610.43.03.json').read_text())
    sources = manifest['source_artifacts']
    OUT.mkdir(exist_ok=True)
    sys.argv = ['generate_v67', str(CACHE / sources[2]['filename']), str(OUT)]
    if generator.main() != 0:
        raise SystemExit('Firmware generation failed')
    payload = module('ad102_payload').payload()
    (OUT / 'v67-ad102-610.43.03-four-plm.bin').write_bytes(payload)
    extracted = subprocess.run([sys.executable, str(TOOLS / 'extract_boot_assets.py'),
                    str(CACHE / sources[0]['filename']), str(OUT), '--arch', 'ad102',
                    '--gsp', str(OUT / 'gsp_ga10x.bin')], check=True, capture_output=True, text=True)
    (OUT / 'extraction-report.json').write_text(extracted.stdout)
    pins = json.loads((ROOT / 'firmware-manifest.json').read_text())
    result = {}
    for name, pin in pins.items():
        if not isinstance(pin, dict) or 'sha256' not in pin:
            continue
        data = (OUT / name).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if len(data) != pin['bytes'] or digest != pin['sha256']:
            raise SystemExit('Asset mismatch: ' + name)
        result[name] = {'bytes': len(data), 'sha256': digest}
    (ROOT / 'assets-verified.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Verified five AD102 firmware assets. Nothing installed or executed.')

if __name__ == '__main__':
    main()
