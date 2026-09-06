#!/usr/bin/env python3
"""Reproducibly compress firmware web assets (no CDN/build dependencies)."""
import gzip
from pathlib import Path
root = Path(__file__).resolve().parents[1]
source = root / 'firmware/main/web/index.html'
target = source.with_suffix('.html.gz')
target.write_bytes(gzip.compress(source.read_bytes(), compresslevel=9, mtime=0))
print(f'{source.name}: {source.stat().st_size} bytes -> {target.stat().st_size} bytes')
