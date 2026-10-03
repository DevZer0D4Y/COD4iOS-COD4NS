#!/usr/bin/env python3
"""Copy a personal CoD4 install into an already signed/installed iOS app.
Run --dry-run first. Does not stage a second copy on the Mac or delete saves.
"""
import argparse
from pathlib import Path
import subprocess

DEFAULT = Path.home() / 'Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam/steamapps/common/Call of Duty 4'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=DEFAULT)
    parser.add_argument('--device', help='Device UDID from Xcode Devices and Simulators')
    parser.add_argument('--bundle-id', default='org.kisakcod.sp')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    root = args.source.resolve()
    localization = root / 'localization.txt'
    if not localization.is_file():
        parser.error(f'Missing {localization}')
    language = localization.read_text(encoding='cp1252').splitlines()[0].strip()
    if not language.isalpha():
        parser.error('Invalid localization language')
    required = [root / 'main' / f'iw_{i:02}.iwd' for i in range(14)]
    required += [root / 'zone' / language / f'{name}.ff' for name in ('code_post_gfx', 'common', 'ui', 'killhouse')]
    for item in required:
        if not item.is_file() or item.stat().st_size == 0:
            parser.error(f'Missing/empty game asset: {item}')
    total = sum(p.stat().st_size for folder in ('main', 'zone') for p in (root / folder).rglob('*') if p.is_file())
    print(f'Validated {language} data: {total / 1024**3:.2f} GiB plus localization.txt', flush=True)
    # Localization is copied last so a fresh installation cannot start while
    # the large asset directories are only partially transferred.
    commands = []
    for name in ('main', 'zone', 'localization.txt'):
        commands.append(['xcrun', 'devicectl', 'device', 'copy', 'to', '--device', args.device or '<device-UDID>',
                         '--source', str(root / name), '--destination', f'Documents/{name}',
                         '--domain-type', 'appDataContainer', '--domain-identifier', args.bundle_id])
    if args.dry_run:
        for cmd in commands:
            print(f'{cmd[8]} -> {cmd[10]}')
        print('No files transferred. Install the signed app first, then run with --device UDID.')
        return
    if not args.device:
        parser.error('--device is required unless using --dry-run')
    for name, cmd in zip(('main', 'zone', 'localization.txt'), commands):
        print(f'Transferring {name}…', flush=True)
        subprocess.run(cmd, check=True)
    print('Transfer commands completed. Reopen KisakCOD on the iPhone.')

if __name__ == '__main__':
    main()
