"""Run real CE binary UI/storage smoke checks in the CEdev CEmu autotester.
The ROM stays local and is never copied into a release.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--rom', required=True, type=Path)
parser.add_argument('--measure', action='store_true', help='Read timing counters using diagnostic memory hashes')
args = parser.parse_args()
build = ROOT / 'build'
build.mkdir(exist_ok=True)
mapping = (ROOT / 'bin/NEONDASH.map').read_text()

def address(symbol):
    return re.search(r'(0x[0-9a-f]+)\s+_' + symbol + r'\b', mapping).group(1)

def crc(value, size):
    # CEmu uses CRC32C (Castagnoli), not zlib's IEEE CRC32.
    result = 0xffffffff
    for byte in value.to_bytes(size, 'little'):
        result ^= byte
        for _ in range(8):
            result = (result >> 1) ^ (0x82f63b78 if result & 1 else 0)
    return f'{result ^ 0xffffffff:08X}'

hashes = {}
for name, value in [('home', 1), ('campaign', 2), ('play', 3), ('pause', 4),
                    ('editor', 5), ('message', 6), ('slots', 7), ('custom', 8), ('mode', 9)]:
    hashes[name] = {'description': f'Actual UI state: {name}', 'start': address('nd_screen'),
                    'size': 1, 'expected_CRCs': [crc(value, 1)], 'timeout': 10000 if name in ('message', 'slots') else 2000}
for name, lo, hi in [('nd_frames', 95, 110), ('nd_ticks', 190, 210), ('nd_max_frame_clocks', 1, 1092)]:
    hashes[name] = {'description': name, 'start': address(name), 'size': 4,
                    'expected_CRCs': ['00000000'] if args.measure else [crc(v, 4) for v in range(lo, hi + 1)]}
sequence = [
    'action|launch', 'delay|500', 'hashWait|home',
    'key|enter', 'hashWait|campaign', 'key|enter', 'hashWait|mode',
    'key|enter', 'delay|3000', 'hash|play', 'key|clear', 'hashWait|pause',
    'hash|nd_frames', 'hash|nd_ticks', 'hash|nd_max_frame_clocks',
    'key|down', 'key|down', 'key|enter', 'hashWait|campaign',
    'key|clear', 'hashWait|home', 'key|down', 'key|down', 'key|enter', 'hashWait|slots',
    'key|enter', 'hashWait|custom', 'key|enter', 'hashWait|editor',
    'key|right', 'key|2nd', 'key|y=', 'hashWait|message', 'key|enter', 'hashWait|editor',
    'key|zoom', 'hashWait|message', 'key|enter', 'hashWait|editor',
    'key|trace', 'hashWait|message', 'key|enter', 'hashWait|editor',
    'key|graph', 'delay|500', 'hashWait|play', 'key|clear', 'hashWait|pause',
    'key|down', 'key|down', 'key|enter', 'hashWait|editor',
    'key|clear', 'hashWait|slots', 'key|clear', 'hashWait|home',
    'hash|storage',
]
hashes['storage'] = {'description': 'CE save/profile/export/import all succeeded',
                     'start': address('nd_storage_ok'), 'size': 1, 'expected_CRCs': [crc(15, 1)]}
for i, command in enumerate(sequence):
    operation, _, name = command.partition('|')
    if operation in ('hash', 'hashWait') and not name.startswith('nd_'):
        unique = f'{name}_step{i}'
        hashes[unique] = hashes[name].copy()
        sequence[i] = f'{operation}|{unique}'
config = {
    'rom': str(args.rom.resolve()),
    'transfer_files': [str(ROOT / 'bin/NEONDASH.8xp')] + [str(ROOT / '.tools/clibs/clibs' / f'{n}.8xv') for n in ['libload', 'graphx', 'keypadc', 'fileioc']],
    'target': {'name': 'NEONDASH', 'isASM': True}, 'sequence': sequence, 'hashes': hashes,
}
path = build / 'emulator-test.json'
path.write_text(json.dumps(config, indent=2))
result = subprocess.run([str(ROOT / '.tools/CEdev/bin/cemu-autotester.exe'), str(path)], cwd=build, text=True, capture_output=True)
output = result.stdout + result.stderr
(build / ('emulator-metrics-raw.log' if args.measure else 'emulator-test.log')).write_text(output)
if not args.measure:
    print(output)
# Decode diagnostic CRCs for actionable timing failures rather than opaque hashes.
metrics = {}
for symbol in ['nd_frames', 'nd_ticks', 'nd_max_frame_clocks']:
    match = re.search(r'Hash #' + symbol + r'.*?got ([0-9A-F]+)', output)
    if match:
        for value in range(100000):
            if crc(value, 4) == match.group(1).zfill(8):
                metrics[symbol] = value
                if not args.measure:
                    print(f'{symbol}: observed {value}')
                break
if args.measure:
    failures = re.findall(r'\[Test failed!\] Hash #(\S+)', output)
    assert sorted(failures) == sorted(metrics) and len(metrics) == 3, output
    metrics['fps'] = metrics['nd_frames'] * 60 / metrics['nd_ticks']
    metrics['max_draw_ms'] = metrics['nd_max_frame_clocks'] * 1000 / 32768
    assert 29.5 <= metrics['fps'] <= 30.5, metrics
    assert metrics['max_draw_ms'] < 1000 / 30, metrics
    (build / 'metrics.json').write_text(json.dumps(metrics, indent=2))
    print(json.dumps(metrics, indent=2))
    raise SystemExit(0)
raise SystemExit(result.returncode)
