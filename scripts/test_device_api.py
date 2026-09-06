#!/usr/bin/env python3
"""Read-only acceptance checks against a real, running AirMon.

No configuration, calibration, firmware or network settings are changed.
This does not replace the physical sensor, power, provisioning or OTA tests.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import gzip
import json
import math
from pathlib import Path
import time
import traceback
import urllib.error
import urllib.parse
import urllib.request

UNITS={'temperature':'°C','humidity':'%','pm1':'µg/m³','pm2_5':'µg/m³','pm10':'µg/m³','co2':'ppm','voc_index':'index','nox_index':'index'}
STATES={'absent','warming_up','valid','stale','failed'}

def number(value):
    return type(value) in (int,float) and math.isfinite(value)

def run(base):
    checks=[]
    def fetch(path,expected=200,as_json=True):
        request=urllib.request.Request(base+path,headers={'Accept':'application/json' if as_json else 'text/html'})
        try:
            response=urllib.request.urlopen(request,timeout=15)
        except urllib.error.HTTPError as error:
            response=error
        with response:
            assert response.status==expected,f'{path}: HTTP {response.status}, expected {expected}'
            payload=response.read(2_000_001)
            assert len(payload)<=2_000_000,'Response unexpectedly large'
            if response.headers.get('Content-Encoding')=='gzip':payload=gzip.decompress(payload)
            if as_json:
                assert response.headers.get_content_type()=='application/json',path
                return json.loads(payload)
            return payload.decode('utf-8')
    def readings():
        r=fetch('/api/v1/readings')
        assert number(r['uptime_ms']) and r['uptime_ms']>=0
        assert r['timestamp'] is None or number(r['timestamp'])
        assert set(r['metrics'])==set(UNITS)
        for key,m in r['metrics'].items():
            assert m['unit']==UNITS[key] and m['status'] in STATES,key
            assert m['age_ms'] is None or number(m['age_ms']) and m['age_ms']>=0,key
            if m['status']=='valid':
                assert number(m['value']) and m['age_ms'] is not None,key
                assert m['age_ms']<=((15000 if key=='co2' else 5000)+100),key
            else:assert m['value'] is None,key
        return r
    initial=fetch('/api/v1/status')
    assert initial['board']=='E32R28T'
    assert 0<initial['minimum_free_heap']<=initial['free_heap']
    assert len(initial['sensor_errors'])==4
    assert 0<=initial['history_points']<=1440
    checks.append('status, heap and counter fields')
    readings();checks.append('all metric units, states, freshness and null semantics')
    config=fetch('/api/v1/config')
    assert not {'password','mqtt_password','admin','ap_password'}&set(config)
    assert type(config['wifi_password_set']) is bool and type(config['mqtt_password_set']) is bool
    checks.append('configuration secret redaction')
    html=fetch('/',as_json=False)
    assert '<title>AirMon' in html and 'api/v1/' in html
    checks.append('embedded dashboard served')
    before=None;all_points=[]
    for _ in range(9):
        suffix='' if before is None else '&before='+str(before)
        page=fetch('/api/v1/history?limit=180'+suffix)
        assert page['resolution_seconds']==60 and page['persistent'] is False
        points=page['points'];assert len(points)<=180
        times=[p['uptime_s'] for p in points]
        assert all(type(t) is int and t>=0 and t%60==0 for t in times)
        assert all(a<b for a,b in zip(times,times[1:]))
        if before is not None:assert all(t<before for t in times)
        assert page['next_before']==(times[0] if times else None)
        for point in points:
            assert set(UNITS)<=set(point)
            assert all(point[k] is None or number(point[k]) for k in UNITS)
        all_points=points+all_points
        if not points or len(points)<180:break
        before=page['next_before']
    else:raise AssertionError('Pagination exceeded 1440-point history capacity')
    assert len(all_points)<=1440
    checks.append(f'history pagination and missing-data semantics ({len(all_points)} points available)')
    for query in ['limit=0','limit=181','limit=-1','limit=180x','limit=4294967296','before=-1','before=abc','before=4294967296','before=1.5','before='+('9'*110)]:
        assert isinstance(fetch('/api/v1/history?'+query,400)['error'],str)
    checks.append('malformed and overflowing history query rejection')
    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(lambda _:readings(),range(9)))
    checks.append('three concurrent measurement clients')
    final=fetch('/api/v1/status')
    assert final['device_id']==initial['device_id'] and final['uptime_ms']>=initial['uptime_ms'],'Device rebooted during checks'
    return dict(passed=True,scope='Read-only live ESP32 HTTP acceptance checks',checked_at_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),version=final['version'],device_id=final['device_id'],minimum_free_heap=final['minimum_free_heap'],checks=checks,not_tested=['physical measurement accuracy','Wi-Fi provisioning and recovery','authenticated mutations','MQTT/Home Assistant','OTA and rollback','24-hour endurance'])

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base_url',help='Device HTTP origin, e.g. http://192.168.1.42')
    parser.add_argument('--output',type=Path,default=Path('tmp/device-api-validation.json'))
    args=parser.parse_args();url=urllib.parse.urlsplit(args.base_url)
    if url.scheme!='http' or not url.hostname or url.username or url.password or url.query or url.fragment or url.path not in ('','/'):
        parser.error('Provide a plain device http://host[:port] origin, without credentials or an API path')
    try:report=run(args.base_url.rstrip('/'))
    except Exception as error:
        location=traceback.extract_tb(error.__traceback__)[-1]
        report=dict(passed=False,scope='Read-only live ESP32 HTTP acceptance checks',error=str(error) or type(error).__name__,location=f'{Path(location.filename).name}:{location.lineno}')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    raise SystemExit(0 if report['passed'] else 1)

if __name__=='__main__':main()
