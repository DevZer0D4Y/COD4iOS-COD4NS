#!/usr/bin/env python3
"""Exercise the native GSC parser on user-owned inline PC RawFile records.

This is a compatibility audit, not the runtime asset loader. Serialized aliases
and script dependencies are not resolved here. Extracted commercial data stays
in the explicitly selected working directory, never in the source tree.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
import zlib

MAX_PACKED = 128 * 1024 * 1024
MAX_EXPANDED = 256 * 1024 * 1024
SCRIPT_NAME = re.compile(rb"(?:maps|common_scripts)/[A-Za-z0-9_/.-]+\.gsc\x00")


def raw_scripts(path):
    if path.stat().st_size > MAX_PACKED:
        raise ValueError("compressed fastfile exceeds audit budget")
    packed = path.read_bytes()
    if packed[:12] != b"IWffu100\x05\x00\x00\x00":
        raise ValueError("expected PC IWffu100 version 5")
    decoder = zlib.decompressobj()
    data = decoder.decompress(packed[12:], MAX_EXPANDED + 1)
    if len(data) > MAX_EXPANDED or not decoder.eof or decoder.unused_data:
        raise ValueError("invalid or oversized zlib stream")
    for match in SCRIPT_NAME.finditer(data):
        if match.start() < 12:
            continue
        name_ref, size, buffer_ref = struct.unpack_from("<III", data, match.start() - 12)
        if name_ref != 0xFFFFFFFF or buffer_ref != 0xFFFFFFFF or not 1 <= size <= 4 * 1024 * 1024:
            continue
        end = match.end() + size
        body = data[match.end():end]
        if len(body) != size or data[end:end + 1] != b"\0" or b"\0" in body:
            continue
        name = match.group()[:-1].decode("ascii")
        relative = PurePosixPath(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("unsafe serialized script path")
        yield name, body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zone-dir", required=True, type=Path)
    parser.add_argument("--parser", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    seen, results, zones = set(), [], []
    for fastfile in sorted(args.zone_dir.glob("*.ff")):
        if fastfile.stem.startswith("mp_"):
            continue
        count = 0
        try:
            for name, body in raw_scripts(fastfile):
                count += 1
                digest = hashlib.sha256(body).hexdigest()
                key = (name, digest)
                if key in seen:
                    continue
                seen.add(key)
                destination = args.work_dir / "scripts" / digest / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(body)
                try:
                    run = subprocess.run([str(args.parser.resolve()), str(destination.resolve())],
                                         capture_output=True, text=True, timeout=30)
                    result = dict(zone=fastfile.name, name=name, sha256=digest,
                                  exit=run.returncode, output=run.stdout, error=run.stderr)
                except subprocess.TimeoutExpired:
                    result = dict(zone=fastfile.name, name=name, sha256=digest,
                                  exit=-1, output="", error="parser timeout after 30 seconds")
                results.append(result)
                if result["exit"]:
                    print("FAIL", fastfile.name, name, result["error"][:160], flush=True)
            zones.append(dict(name=fastfile.name, inline_scripts=count))
            print(f"{fastfile.name}: {count} validated inline script records", flush=True)
        except (OSError, ValueError, zlib.error) as error:
            zones.append(dict(name=fastfile.name, error=str(error)))
            print("ZONE ERROR", fastfile.name, error, flush=True)
    failed = sum(result["exit"] != 0 for result in results)
    report = dict(unique_scripts=len(results), failed_scripts=failed, zones=zones, scripts=results)
    (args.work_dir / "report.json").write_text(json.dumps(report, indent=2))
    print(f"Parsed {len(results) - failed}/{len(results)} unique script records", flush=True)
    return int(failed != 0 or not results or any("error" in zone for zone in zones))


if __name__ == "__main__":
    raise SystemExit(main())
