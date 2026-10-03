#!/usr/bin/env python3
"""Check the source export for generated binaries, retail files and local paths."""
from pathlib import Path
import subprocess, sys, re

root=Path(__file__).resolve().parents[1]
blocked_suffixes={'.ipa','.ff','.iwd','.bik','.mp4','.mov','.mobileprovision',
                  '.p12','.pfx','.cer','.pem','.key','.dll','.lib','.dylib','.so',
                  '.o','.obj','.a','.asi','.flt','.exp','.log','.ips','.crash'}
blocked_roots={'main','zone','usermaps','mods','players','profiles','documents','work'}
if (root/'.git').exists():
    names=subprocess.check_output(['git','-C',str(root),'ls-files','-z']).decode().split('\0')
    files=[root/name for name in names if name]
else:
    files=[p for p in root.rglob('*') if p.is_file() and
           not any(x=='.git' or x=='__pycache__' or x.startswith('build')
                   for x in p.relative_to(root).parts[:-1])]
errors=[]
private_paths=[re.compile(rb'/Users/[A-Za-z0-9_.-]+/'),
               re.compile(rb'/(?:private/)?var/folders/[A-Za-z0-9_-]+/')]
for path in files:
    rel=path.relative_to(root)
    if rel.parts[0].lower() in blocked_roots or path.suffix.lower() in blocked_suffixes:
        errors.append(f'Excluded runtime/build/signing file: {rel}');continue
    if path.name in {'.DS_Store','localization.txt','cod4x_servers.txt'} or path.name.startswith('._'):
        errors.append(f'Local metadata/runtime file: {rel}');continue
    if path.is_symlink():
        errors.append(f'Symlink must be reviewed before publication: {rel}');continue
    data=path.read_bytes()
    if any(value.search(data) for value in private_paths):
        errors.append(f'Build-machine path found: {rel}')
    if data[:4] in {b'\xcf\xfa\xed\xfe',b'\xfe\xed\xfa\xcf',b'\x7fELF',b'\xca\xfe\xba\xbe'} or data[:2]==b'MZ':
        errors.append(f'Compiled executable content: {rel}')
    if path.suffix=='.svg' and not data.lstrip().startswith((b'<svg',b'<?xml',b'<!--')):
        errors.append(f'Possible binary save file: {rel}')
    if len(data)>95*1024*1024:
        errors.append(f'File too large for normal source publication: {rel}')
if errors:
    print('\n'.join(errors),file=sys.stderr);sys.exit(1)
print(f'Checked {len(files)} source files: no excluded runtime/build/signing files or local paths')
