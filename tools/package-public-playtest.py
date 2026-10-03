#!/usr/bin/env python3
"""Create a resignable, data-free COD4iOS playtest package from a built app."""
import argparse, hashlib, json, os, plistlib, re, shutil, struct, subprocess, tempfile, zipfile
from pathlib import Path

def cstrings(data):
    magic, _, _, _, count, _, _, _ = struct.unpack_from('<8I', data)
    if magic != 0xfeedfacf:
        raise ValueError('Expected a thin arm64 Mach-O')
    offset = 32
    for _ in range(count):
        command, size = struct.unpack_from('<II', data, offset)
        if command == 0x19:
            sections = struct.unpack_from('<I', data, offset + 64)[0]
            for index in range(sections):
                section = offset + 72 + index * 80
                name = data[section:section+16].split(b'\0')[0]
                if name == b'__cstring':
                    length = struct.unpack_from('<Q', data, section + 40)[0]
                    start = struct.unpack_from('<I', data, section + 48)[0]
                    yield start, start + length
        offset += size

def sanitize_binary(path, source_root):
    signature = subprocess.run(['codesign', '-d', str(path)], capture_output=True)
    if signature.returncode == 0:
        subprocess.run(['codesign', '--remove-signature', str(path)], check=True)
    subprocess.run(['xcrun', 'strip', '-S', str(path)], check=True)
    data = bytearray(path.read_bytes())
    prefix = str(source_root).encode()
    replacement = b'/build/public-source'.ljust(len(prefix), b'_')
    if len(replacement) != len(prefix):
        raise ValueError('Source prefix too short')
    replaced = 0
    for start, end in cstrings(data):
        offset = start
        while offset < end:
            stop = data.index(0, offset, end)
            literal = bytes(data[offset:stop])
            if literal.startswith(prefix + b'/'):
                # Equal length keeps all string offsets, including shared suffixes.
                data[offset:offset+len(prefix)] = replacement
                replaced += 1
            offset = stop + 1
    path.write_bytes(data)
    if b'/Users/' in data:
        raise ValueError('Unreviewed home path remains in ' + path.name)
    return replaced

def zip_files(path, files):
    temporary = path.with_suffix(path.suffix + '.new')
    with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, data, executable in sorted(files):
            info = zipfile.ZipInfo(name, (2026, 10, 3, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = ((0o100755 if executable else 0o100644) << 16)
            archive.writestr(info, data)
    with zipfile.ZipFile(temporary) as archive:
        assert archive.testzip() is None
    os.replace(temporary, path)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    root = args.source_root.resolve()
    # Never package build folders, Documents, game assets or local test artifacts.
    allow = {'Info.plist', 'PkgInfo', 'KisakCOD', 'Assets.car', 'TouchControls.LICENSE',
             'AppIcon60x60@2x.png', 'AppIcon76x76@2x~ipad.png',
             'Frameworks/libkisakcod_sp.dylib', 'Frameworks/libkisakcod_mp.dylib'}
    report = {'unsigned': True, 'gameFilesIncluded': False, 'signingProfileIncluded': False}
    with tempfile.TemporaryDirectory(prefix='cod4ios-public-') as tmp:
        app = Path(tmp) / 'Payload/KisakCOD.app'
        app.mkdir(parents=True)
        for relative in allow:
            source = args.app / relative
            assert source.is_file(), relative
            target = app / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        replaced = {}
        for relative in ['Frameworks/libkisakcod_sp.dylib', 'Frameworks/libkisakcod_mp.dylib', 'KisakCOD']:
            original_symbols = subprocess.check_output(['nm', '-gU', str(args.app / relative)])
            replaced[relative] = sanitize_binary(app / relative, root)
            symbols = subprocess.check_output(['nm', '-gU', str(app / relative)])
            assert symbols == original_symbols, 'Exported symbols changed: ' + relative
            # Validate the modified Mach-O with an identity-free temporary signature.
            subprocess.run(['codesign', '--force', '--sign', '-', str(app / relative)], check=True, capture_output=True)
            subprocess.run(['codesign', '--verify', '--strict', str(app / relative)], check=True)
            subprocess.run(['codesign', '--remove-signature', str(app / relative)], check=True)
            unsigned = subprocess.run(['codesign', '-d', str(app / relative)], capture_output=True, text=True)
            assert unsigned.returncode and 'not signed at all' in unsigned.stderr
        licenses = app / 'Licenses'; licenses.mkdir()
        license_files = {'GPL-3.0.txt': root / 'LICENSE',
                         'OpenAssetTools.txt': root / 'ports/ios/zoneload/LICENSE.OpenAssetTools',
                         'DXVK.txt': root / 'ports/ios/compat/native/windows/LICENSE.dxvk',
                         'MinGW-w64.txt': root / 'ports/ios/compat/native/directx/COPYING.MinGW-w64.txt'}
        openal_license = root / 'third-party/openal-soft/COPYING'
        if openal_license.exists():license_files['OpenAL-Soft.txt'] = openal_license
        for name, source in license_files.items():shutil.copyfile(source, licenses / name)
        # Verify known local/private data against all payload files without reporting it.
        private = [str(root).encode(), b'/Users/', b'SIGNING_DEVICE_TOKEN',
                   b'SIGNING_TEAM_TOKEN', b'TestUser', b'PRIVATE_ACCOUNT_TOKEN']
        files = []
        for path in app.rglob('*'):
            if path.is_file():
                data = path.read_bytes()
                assert not any(pattern in data for pattern in private), 'Private data in ' + path.name
                files.append(('Payload/KisakCOD.app/' + str(path.relative_to(app)), data,
                              path.name == 'KisakCOD' or path.suffix == '.dylib'))
        info = plistlib.loads((app / 'Info.plist').read_bytes())
        assert info['CFBundleIdentifier'] == 'com.devz.cod4ios'
        ipa = args.output / 'COD4iOS-playtest-unsigned.ipa'
        zip_files(ipa, files)
        report.update(ipaBytes=ipa.stat().st_size, ipaSHA256=hashlib.sha256(ipa.read_bytes()).hexdigest(),
                      sanitizedSourcePaths=replaced, bundleIdentifier=info['CFBundleIdentifier'],
                      version=info['CFBundleShortVersionString'], build=info['CFBundleVersion'],
                      archiveEntries=len(files), exportedSymbolsVerified=True,
                      temporaryAdHocSignatureVerification='passed; removed before packaging')
    # Provide corresponding source separately. Keep build sources and licenses,
    # excluding binaries, caches, game files and private device status notes.
    extensions = {'.c','.cc','.cpp','.cxx','.h','.hpp','.mm','.m','.in','.cmake','.txt','.md',
                  '.py','.sh','.bat','.cmd','.metal','.s','.S','.png','.json','.svg','.ico','.rc','.am','.html','.bmp','.LICENSE','.yml','.yaml'}
    sources = []
    for directory in ['src', 'ports', 'scripts', 'deps', 'docs', 'tools', '.github']:
        for path in (root / directory).rglob('*'):
            if not path.is_file() or any(x in {'__pycache__','.git','build','work'} for x in path.relative_to(root).parts):continue
            if path.name == 'PORTING_STATUS.md' or path.resolve() == Path(__file__).resolve():continue
            if path.suffix not in extensions and not path.name.startswith(('LICENSE','COPYING','README')):continue
            data = path.read_bytes()
            if path.suffix not in {'.png','.ico','.bmp'}:
                data = data.replace(str(root).encode(), b'SOURCE_ROOT')
                data = data.replace(b'TestUser', b'TestUser').replace(b'testuser', b'testuser')
                assert not re.search(rb'/Users/[A-Za-z0-9_.-]+/', data), 'Private source metadata: '+str(path.relative_to(root))
            sources.append(('COD4iOS-source/' + str(path.relative_to(root)), data, path.suffix == '.sh'))
    for name in ['CMakeLists.txt','LICENSE','README.md','.gitignore','.gitattributes','NOTICE.md','CHANGELOG.md','GPLv3_Logo.png','generate-project.bat','build-win.bat']:
        data = (root / name).read_bytes().replace(str(root).encode(), b'SOURCE_ROOT')
        assert not re.search(rb'/Users/[A-Za-z0-9_.-]+/', data)
        sources.append(('COD4iOS-source/' + name, data, False))
    # Include the matching fetched OpenAL source, without its Git metadata.
    openal = root / 'third-party/openal-soft'
    if openal.exists():
        for path in openal.rglob('*'):
            if not path.is_file() or '.git' in path.relative_to(openal).parts:continue
            sources.append(('COD4iOS-source/third-party/openal-soft/' + str(path.relative_to(openal)),path.read_bytes(),False))
    public_tool = Path(__file__).read_bytes()
    # Local scan needles must not become personal metadata in the public tool.
    for before, after in [(b'SIGNING_DEVICE_TOKEN',b'SIGNING_DEVICE_TOKEN'),
                          (b'SIGNING_TEAM_TOKEN',b'SIGNING_TEAM_TOKEN'),
                          (b'TestUser',b'TestUser'),(b'testuser',b'testuser'),
                          (b'PRIVATE_ACCOUNT_TOKEN',b'PRIVATE_ACCOUNT_TOKEN')]:
        public_tool = public_tool.replace(before, after)
    sources.append(('COD4iOS-source/tools/package-public-playtest.py',public_tool,True))
    source_zip = args.output / 'COD4iOS-playtest-source.zip'
    zip_files(source_zip, sources)
    report.update(sourceArchiveBytes=source_zip.stat().st_size, sourceArchiveSHA256=hashlib.sha256(source_zip.read_bytes()).hexdigest(),sourceFiles=len(sources))
    (args.output / 'BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__ == '__main__':main()
