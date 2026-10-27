// Node built-ins only. Exercise the actual embedded script with a small DOM,
// browser permission and HTTP stand-in; no network or real location is used.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(require('node:path').join(__dirname,'../src/SetupPage.h'),'utf8');
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];
class Element {
 constructor(tag,html=''){this.tag=tag;this.children=[];this.attributes={};this._value=html.match(/\bvalue="([^"]*)"/)?.[1]||'';this.hidden=/\bhidden\b/.test(html);this.disabled=/\bdisabled\b/.test(html);this.checked=false;this.files=[];}
 get value(){return this._value;} set value(v){this._value=String(v);}
 get textContent(){return this.text||'';} set textContent(v){this.text=String(v);this.children=[];}
 appendChild(child){this.children.push(child);}
 setAttribute(k,v){this.attributes[k]=v;}
}
async function page({tz='America/Chicago',secure=true,geo='ok',networks=['Manual','None','quoted"network','Caf\u00e9'],failures=0,status='ready'}={}) {
 process.env.TZ=tz;
 const elements={};for(const match of source.matchAll(/<(\w+)\b[^>]*\bid="([^"]+)"[^>]*>/g))elements[match[2]]=new Element(match[1],match[0]);
 const calls=[],saved=[],pending=[],geoOptions=[];let attempts=0;
 const sandbox={console,TextEncoder,AbortController,Intl,Date,Number,Math,Promise,
  document:{getElementById:id=>{assert.ok(elements[id],'Unknown element '+id);return elements[id];},createElement:tag=>new Element(tag)},
  window:{isSecureContext:secure},navigator:{geolocation:{getCurrentPosition(ok,error,options){geoOptions.push(options);if(geo==='ok')ok({coords:{latitude:12.345678,longitude:-98.765432}});else if(geo==='denied')error({code:1});else pending.push(ok);}}},
  setInterval(){},setTimeout(callback,ms){if(ms===250)queueMicrotask(callback);return 1;},clearTimeout(){},
  async fetch(url,options={}){calls.push(url);if(url==='/session')return {ok:true,json:async()=>({token:'0'.repeat(32),error:''})};if(url==='/networks'){if(attempts++<failures)throw Error('Synthetic connection failure');return {ok:true,json:async()=>({networks,status,error:status==='failed'?-1:0})};}assert.equal(url,'/configure');saved.push(JSON.parse(options.body));return {ok:true,text:async()=>'Settings saved'};}
 };
 const context=vm.createContext(sandbox);vm.runInContext(script,context);
 for(let i=0;i<8;i++)await new Promise(resolve=>setImmediate(resolve));
 return {elements,calls,saved,pending,geoOptions,run:text=>vm.runInContext(text,context)};
}
(async()=>{
 const p=await page(),e=p.elements;
 assert.equal(e.offset.value,'-6');assert.equal(e.dst.value,'us');
 assert.equal(p.geoOptions.length,0);assert.equal(e.coordinateFields.hidden,true);assert.equal(e.zip.required,true);
 assert.equal(e.network.children.length,7);assert.equal(e.network.children[1].textContent,'Manual');
 assert.equal(e.network.children[1].value,'n0'); // Real SSIDs cannot collide with controls.
 e.network.value='n0';e.network.onchange();assert.equal(p.run('selectedSSID()'),'Manual');
 e.network.value='manual';e.network.onchange();assert.equal(e.manualNetwork.hidden,false);assert.equal(e.ssid.required,true);
 e.ssid.value='synthetic hidden network';assert.equal(p.run('selectedSSID()'),e.ssid.value);
 e.network.value='none';e.network.onchange();assert.equal(e.networkCredentials.hidden,true);assert.equal(e.password.disabled,true);assert.equal(e.ssid.disabled,true);
 assert.equal(e.coordinateFields.hidden,false);assert.equal(e.zip.disabled,true);assert.equal(e.latitude.required,true);
 e.deviceLocation.onclick();assert.equal(e.latitude.value,'12.345678');assert.equal(e.longitude.value,'-98.765432');
 assert.equal(p.geoOptions[0].enableHighAccuracy,false);assert.equal(p.geoOptions[0].maximumAge,300000);
 const before=p.calls.filter(x=>x==='/networks').length;await e.refreshNetworks.onclick();
 assert.equal(p.calls.filter(x=>x==='/networks').length,before+1);assert.equal(e.network.value,'none');
 await e.setup.onsubmit({preventDefault(){}});assert.equal(p.saved.length,1);
 assert.equal(p.saved[0]['wifi-sid'],'');assert.equal(p.saved[0]['wifi-pwd'],'');
 assert.equal(p.saved[0]['timezone-offset-minutes'],-360);assert.equal(p.saved[0]['timezone-dst'],'us');
 assert.ok(Math.abs(p.saved[0]['setup-utc']-Date.now()/1000)<5);assert.equal(p.saved[0].lights.length,85);
 e.overrideClock.checked=true;e.overrideClock.onchange();e.utcOverride.value='2026-07-04T12:34:56';
 await e.setup.onsubmit({preventDefault(){}});assert.equal(p.saved[1]['setup-utc'],Date.UTC(2026,6,4,12,34,56)/1000);
 const blocked=await page({secure:false});assert.equal(blocked.elements.latitude.value,'');
 blocked.elements.network.value='none';blocked.elements.network.onchange();blocked.elements.deviceLocation.onclick();assert.match(blocked.elements.locationStatus.textContent,/HTTP/);await blocked.elements.setup.onsubmit({preventDefault(){}});
 assert.equal(blocked.saved.length,0);assert.match(blocked.elements.message.textContent,/Enter latitude/);
 const online=await page({secure:false});online.elements.network.value='n0';online.elements.network.onchange();
 await online.elements.setup.onsubmit({preventDefault(){}});assert.equal(online.saved.length,0);assert.match(online.elements.message.textContent,/ZIP/);
 online.elements.zip.value='01234';await online.elements.setup.onsubmit({preventDefault(){}});
 assert.equal(online.saved[0]['zip-code'],'01234');assert.equal(online.saved[0].latitude,35.84562);assert.equal(online.saved[0].longitude,-86.39027);
 assert.equal(online.geoOptions.length,0);assert.equal(online.elements.latitude.disabled,true);
 online.elements.network.value='none';online.elements.network.onchange();online.elements.latitude.value='1';online.elements.longitude.value='2';
 await online.elements.setup.onsubmit({preventDefault(){}});assert.equal(online.saved[1]['zip-code'],'');assert.equal(online.saved[1].latitude,1);
 const denied=await page({geo:'denied'});denied.elements.network.value='none';denied.elements.network.onchange();denied.elements.deviceLocation.onclick();assert.match(denied.elements.locationStatus.textContent,/denied/);
 const race=await page({geo:'pending'});race.elements.network.value='none';race.elements.network.onchange();race.elements.deviceLocation.onclick();race.elements.latitude.value='45';race.elements.latitude.oninput();race.pending.shift()({coords:{latitude:1,longitude:2}});assert.equal(race.elements.latitude.value,'45');
 race.elements.deviceLocation.onclick();race.pending.shift()({coords:{latitude:1,longitude:2}});assert.equal(race.elements.latitude.value,'1.000000');
 const fixed=await page({tz:'Asia/Kathmandu'});assert.equal(fixed.elements.offset.value,'5.75');assert.equal(fixed.elements.dst.value,'none');
 const phoenix=await page({tz:'America/Phoenix'});assert.equal(phoenix.elements.offset.value,'-7');assert.equal(phoenix.elements.dst.value,'none');
 const unsupported=await page({tz:'Europe/London'});assert.equal(unsupported.elements.dst.value,'');assert.equal(unsupported.elements.advanced.hidden,false);
 const retry=await page({failures:2});assert.equal(retry.calls.filter(x=>x==='/networks').length,3);assert.equal(retry.elements.network.children.length,7);
 const failed=await page({failures:9});assert.equal(failed.calls.filter(x=>x==='/networks').length,3);assert.match(failed.elements.networkStatus.textContent,/Could not load/);assert.equal(failed.elements.network.value,'manual');assert.equal(failed.elements.refreshNetworks.disabled,false);
 const empty=await page({networks:[],status:'failed'});assert.equal(empty.elements.network.value,'manual');assert.match(empty.elements.networkStatus.textContent,/code -1/);
 assert.ok(!source.includes('id="offline"'));assert.ok(source.includes('for="network">SSID'));
 console.log('Setup page tests passed: Wi-Fi ZIP/offline coordinates, validation, fetch refresh/retry, browser clock/zone and location permissions.');
})().catch(error=>{console.error(error);process.exitCode=1;});
