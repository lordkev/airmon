#!/usr/bin/env python3
"""Collect the built classic ESP32 images and verify OTA partition capacity."""
from pathlib import Path
import hashlib,json,shutil
ROOT=Path(__file__).resolve().parents[1];build=ROOT/'firmware/build';out=ROOT/'release/firmware';out.mkdir(parents=True,exist_ok=True)
files={'bootloader/bootloader.bin':('bootloader.bin',0x1000),'partition_table/partition-table.bin':('partition-table.bin',0x8000),'ota_data_initial.bin':('ota_data_initial.bin',0xf000),'airmon.bin':('airmon.bin',0x20000)}
rows=[]
for source,(name,offset) in files.items():
 p=out/name;shutil.copyfile(build/source,p)
 rows.append(dict(file=name,offset=hex(offset),size_bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
assert (out/'airmon.bin').stat().st_size<=0x1e0000
manifest=dict(project='airmon',version='0.1.0',status='prototype; physical validation pending',idf='v5.5.1',chip='esp32',flash_size='4MB',flash_mode='dio',flash_frequency='40m',ota_partition_bytes=0x1e0000,ota_free_bytes=0x1e0000-(out/'airmon.bin').stat().st_size,files=rows)
manifest['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ROOT/'firmware').rglob('*')) if p.is_file() and not any(x in p.relative_to(ROOT/'firmware').parts for x in ['build','managed_components','sdkconfig','sdkconfig.old'])}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(out/'SHA256SUMS').write_text(''.join(r['sha256']+'  '+r['file']+'\n' for r in rows))
(out/'flash_args').write_text('--flash_mode dio --flash_size 4MB --flash_freq 40m\n'+''.join(r['offset']+' '+r['file']+'\n' for r in rows))
print('Packaged firmware:',manifest['ota_free_bytes'],'bytes free per OTA slot')
