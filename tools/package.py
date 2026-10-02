"""Assemble an offline release from verified build outputs and original sources."""
import hashlib
import json
from pathlib import Path
import struct
import zipfile

root = Path(__file__).resolve().parents[1]
destination = root / 'dist'
destination.mkdir(exist_ok=True)
files = {'calculator/PHASEBND.8xp': root / 'bin/PHASEBND.8xp'}
for name in ('libload', 'graphx', 'keypadc', 'fileioc'):
    files[f'calculator/{name}.8xv'] = root / '.tools/clibs/clibs' / f'{name}.8xv'
for name in ('README.md', 'LICENSE', 'makefile', '.gitignore'):
    files[name] = root / name
for folder in ('src', 'tests', 'tools', 'docs'):
    for path in sorted((root / folder).iterdir()):
        if path.is_file() and path.suffix in ('.c', '.h', '.md', '.txt', '.ps1', '.py'):
            files[f'{folder}/{path.name}'] = path

manifest = {}
for name, path in files.items():
    data = path.read_bytes()
    if name.startswith('calculator/'):
        assert data[:8] == b'**TI83F*', f'Invalid TI header: {path}'
        size = struct.unpack_from('<H', data, 53)[0]
        assert len(data) == 55 + size + 2, f'Invalid TI container size: {path}'
        if name.endswith('.8xp'):
            assert data[60:68] == b'PHASEBND', f'Unexpected calculator program name: {path}'
        assert sum(data[55:-2]) & 0xffff == struct.unpack_from('<H', data, len(data)-2)[0], f'Invalid TI checksum: {path}'
    manifest[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}

start = '''PHASEBOUND - TI-84 PLUS CE RELEASE CANDIDATE
Shift form. Defy gravity.

1. Transfer the five files in calculator/ using TI Connect CE.
2. Follow docs/INSTALL.md for the launch route matching your current OS.
3. Read README.md for controls and docs/EDITOR.md for level creation.

Verified: native build, five host test suites, 25 CEmu checks.
Physical-calculator testing remains outstanding. No ROM is included.
Source, build scripts, tests, and required runtime-library license are included.
'''
archive = destination / 'Phasebound-CE.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, path in files.items():
        z.write(path, name)
    z.writestr('START-HERE.txt', start)
    z.writestr('manifest.json', json.dumps(manifest, indent=2))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert not any(n.endswith(('.rom', '.sav', '.ce')) for n in z.namelist())
print(f'{archive}\n{len(files)} files + instructions and manifest; {archive.stat().st_size:,} bytes; ZIP and TI checksums verified.')
