// Exercise the actual embedded dashboard against a simulated HTTP contract.
// This does not emulate ESP32 Wi-Fi, NVS, sensor drivers, or OTA flash writes.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
import {chromium} from 'playwright';
const root=new URL('../',import.meta.url);
const html=await readFile(new URL('firmware/main/web/index.html',root));
const keys=['temperature','humidity','pm1','pm2_5','pm10','co2','voc_index','nox_index'];
const units=['°C','%','µg/m³','µg/m³','µg/m³','ppm','index','index'];
const metrics=Object.fromEntries(keys.map((k,i)=>[k,{value:[22.4,45.1,3,8,12,620,null,null][i],unit:units[i],status:i===6?'absent':i===7?'warming_up':'valid',age_ms:i===6?null:100}]));
const config={name:'Living room',ssid:'Home network',brightness:30,dim_seconds:120,temperature_offset:0,altitude_m:0,co2_asc:false,mqtt_enabled:false,mqtt_uri:'',mqtt_user:'',wifi_password_set:true,mqtt_password_set:false};
const history=Array.from({length:361},(_,i)=>({uptime_s:(i+1)*60,timestamp:null,...Object.fromEntries(keys.map((k,j)=>[k,i%20===0||j>5?null:[22,45,3,8,12,620][j]+Math.sin(i/8)]))}));
let updates=[],historyPages=0,offline=false;
const server=createServer(async(req,res)=>{
 if(!req.url.startsWith('/api/')){res.setHeader('Content-Type','text/html');res.end(html);return;}
 if(offline){req.socket.destroy();return;}
 const url=new URL(req.url,'http://localhost');let value;
 if(url.pathname.endsWith('/readings'))value={uptime_ms:22000000,timestamp:null,metrics};
 else if(url.pathname.endsWith('/status'))value={version:'0.1.0',device_id:'aabbcc112233',uptime_ms:22000000,wifi_connected:true,ip:'192.168.1.42',ap_active:false,config_pending:false,config_result:''};
 else if(url.pathname.endsWith('/history')){historyPages++;const before=Number(url.searchParams.get('before')||1e12),n=Number(url.searchParams.get('limit')||180);const points=history.filter(p=>p.uptime_s<before).slice(-n);value={resolution_seconds:60,persistent:false,points,next_before:points[0]?.uptime_s??null};}
 else if(url.pathname.endsWith('/config')&&req.method==='GET')value=config;
 else if(req.method==='PUT'){
  let body='';for await(const data of req)body+=data;
  if(req.headers.authorization!=='Bearer '+'a'.repeat(32)){res.statusCode=401;value={error:'Administrator token required'};}
  else {updates.push(JSON.parse(body));res.statusCode=202;value={status:'pending'};}
 } else{res.statusCode=404;value={error:'Unknown API endpoint'};}
 res.setHeader('Content-Type','application/json');res.end(JSON.stringify(value));
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({channel:'chrome',headless:true});
const page=await browser.newPage({viewport:{width:1200,height:1000}});const errors=[];
page.on('pageerror',e=>errors.push(e.message));
try{
 await page.goto('http://127.0.0.1:'+server.address().port);
 await page.getByText('Living room',{exact:true}).waitFor();
 await page.waitForFunction(()=>document.querySelectorAll('.metric.valid').length===6);
 await page.waitForFunction(()=>document.querySelector('#refresh-history').disabled===false);
 assert.equal(await page.locator('.metric').count(),8);assert.equal(historyPages,3);
 assert.match(await page.locator('[data-key="voc_index"]').innerText(),/Not installed/);
 assert.match(await page.locator('[data-key="nox_index"]').innerText(),/Warming up/);
 await page.locator('[data-key="temperature"]').click();assert.equal(await page.locator('#chart-title').innerText(),'Temperature history');
 await mkdir(new URL('tmp/tests',root),{recursive:true});
 await page.screenshot({path:new URL('tmp/tests/dashboard-desktop.png',root).pathname,fullPage:true});
 await page.setViewportSize({width:390,height:844});
 assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
 await page.screenshot({path:new URL('tmp/tests/dashboard-mobile.png',root).pathname,fullPage:true});
 await page.locator('#settings-tab').click();
 await page.getByRole('button',{name:'Save settings',exact:true}).click();
 await page.getByText('Enter the 32-character administrator token shown on the display.').waitFor();assert.equal(updates.length,0);
 await page.locator('#admin').fill('b'.repeat(32));await page.getByRole('button',{name:'Save settings',exact:true}).click();
 await page.locator('#config-result').filter({hasText:'Administrator token required'}).waitFor();assert.equal(updates.length,0);
 await page.locator('#admin').fill('a'.repeat(32));await page.locator('[name="name"]').fill('Bedroom');
 await page.getByRole('button',{name:'Save settings',exact:true}).click();
 await page.waitForFunction(()=>document.querySelector('#config-result').textContent.includes('Testing and saving'));
 assert.equal(updates.length,1);assert.equal(updates[0].name,'Bedroom');assert.ok(!('password'in updates[0]));assert.ok(!('mqtt_password'in updates[0]));
 await page.locator('#open-wifi').check();await page.getByRole('button',{name:'Save settings',exact:true}).click();
 await page.waitForTimeout(100);assert.equal(updates[1].password,'');
 assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
 await page.screenshot({path:new URL('tmp/tests/settings-mobile.png',root).pathname,fullPage:true});
 offline=true;await page.locator('#dashboard-tab').click();
 await page.getByText('Monitor unreachable',{exact:true}).waitFor({timeout:6000});
 assert.equal(await page.locator('.metric.valid').count(),0);assert.deepEqual(errors,[]);
 const report={passed:true,mode:'Actual embedded HTML in headless Chrome; simulated API server',checks:['eight metric states','three-page history','metric selection','390px responsive layout','token required before mutation','401 handling','preserve omitted passwords','explicit password clearing','offline state','no uncaught browser errors']};
 await writeFile(new URL('docs/web-validation.json',root),JSON.stringify(report,null,2)+'\n');console.log('PASS:',report.checks.length,'browser checks');
}finally{await browser.close();server.close();}
