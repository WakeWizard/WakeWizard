#!/usr/bin/env python3
"""Validate the distributable image, ESP32 partition bounds and source provenance."""
import hashlib
import json
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1]
site = root / 'web-installer'
for manifest_path in (site / 'firmware').glob('*/manifest.json'):
    manifest = json.loads(manifest_path.read_text())
    assert manifest['new_install_prompt_erase'] is False, 'Full image requires full erase'
    assert manifest['new_install_improv_wait_time'] == 0, 'No Improv protocol in this firmware'
    build, = manifest['builds']
    assert build['chipFamily'] == 'ESP32'
    part, = build['parts']
    assert part['offset'] == 0
    image_path = manifest_path.parent / part['path']
    image = image_path.read_bytes()
    expected_hash, filename = (manifest_path.parent / 'SHA256SUMS.txt').read_text().split()
    assert filename == image_path.name
    assert hashlib.sha256(image).hexdigest() == expected_hash, 'Image checksum mismatch'
    assert len(image) <= 4 * 1024 * 1024
    assert image[0x1000] == 0xe9 and image[0x10000] == 0xe9, 'Missing ESP image headers'
    assert image[0x1002:0x1004] == bytes([2, 0x20]), 'Expected DIO / 4 MB / 40 MHz'
    entries = {}
    previous_end = 0x9000
    table = image[0x8000:0x9000]
    for start in range(0, len(table), 32):
        entry = table[start:start + 32]
        if entry[:2] != b'\xaaP':
            assert entry[:2] == b'\xeb\xeb', 'Missing partition checksum'
            assert hashlib.md5(table[:start]).digest() == entry[16:32], 'Partition checksum mismatch'
            break
        _, typ, subtype, offset, size, label, _ = struct.unpack('<HBBII16sI', entry)
        name = label.split(b'\0')[0].decode()
        assert offset >= previous_end and offset + size <= 4 * 1024 * 1024
        entries[name] = (offset, size)
        previous_end = offset + size
    assert entries['app0'] == (0x10000, 0x140000)
    assert entries['spiffs'] == (0x290000, 0x160000)
    provenance = json.loads((manifest_path.parent / 'provenance.json').read_text())
    for name, source in provenance['components'].items():
        component = image[source['offset']:source['offset'] + source['size']]
        assert hashlib.sha256(component).hexdigest() == source['sha256'], name
    assert len(image) == entries['spiffs'][0] + entries['spiffs'][1]
    assert set(image[0x9000:0xe000]) == {255}, 'NVS must be empty'
    print(f'PASS {manifest_path.relative_to(root)}: checksums, headers, partitions, components, empty NVS')
