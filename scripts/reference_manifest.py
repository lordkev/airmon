#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[1]
repos=[]
for folder in ['esp-idf','gas-index-algorithm','scd4x-pcb','sgp41-pcb','pmsa003i-pcb','scd4x-driver','sgp41-driver','sht4x-driver']:
 p=ROOT/'.tools'/folder
 if (p/'.git').exists():
  def git(*args):return subprocess.check_output(['git','-C',str(p),*args],text=True).strip()
  repos.append(dict(name=folder,url=git('remote','get-url','origin'),commit=git('rev-parse','HEAD')))
files=[]
urls={'display-size.pdf':'https://www.lcdwiki.com/res/E32R28T/E32R28T_Size.pdf','pmsa003i.pdf':'https://cdn-shop.adafruit.com/product-files/4632/4505_PMSA003I_series_data_manual_English_V2.6.pdf'}
for name,url in urls.items():
 p=ROOT/'.tools/references'/name
 files.append(dict(name=name,url=url,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
meta=dict(retrieved_on='2026-09-06',note='Source documents are linked, not redistributed. Purchased revisions require confirmation.',repositories=repos,documents=files,links=[
'https://www.keyestudio.com/products/28-inch-esp32-32e-display-screen-lcd-tft-module-with-touch-wroom-for-arduino',
'https://www.lcdwiki.com/2.8inch_ESP32-32E_Display',
'https://www.lcdwiki.com/res/E32R28T/2.8inch_ESP32-32_Display_Schematic.pdf',
'https://sensirion.com/media/documents/33FD6951/6A7C10A0/HT_DS_Datasheet_SHT4x_V7.3.pdf',
'https://sensirion.com/media/documents/5FE8673C/61E96F50/Sensirion_Gas_Sensors_Datasheet_SGP41.pdf',
'https://sensirion.com/products/catalog/SCD40',
'https://www.diodes.com/assets/Datasheets/AP2112.pdf',
'https://www.adafruit.com/product/4505','https://www.adafruit.com/product/5187','https://www.adafruit.com/product/6455'],tools=dict(kicad='10.0.6',openscad='2026.09.05',idf='v5.5.1',esptool='4.12.0',bambu_studio='02.08.02.61'))
out=ROOT/'hardware/references';out.mkdir(exist_ok=True);(out/'manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
