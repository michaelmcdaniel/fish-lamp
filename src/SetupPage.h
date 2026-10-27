#pragma once
// All assets are served locally; no CDN or Internet connection required.
inline constexpr char SetupPage[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Fish Lamp setup</title><style>
body{font:16px system-ui,sans-serif;background:#edf5f6;color:#17353b;margin:0;padding:20px}main{max-width:620px;margin:auto;background:white;padding:24px;border-radius:16px}h1{margin-top:0}fieldset{border:1px solid #bbd0d3;border-radius:10px;margin:20px 0;padding:16px}legend{font-weight:700}label{display:block;margin:12px 0}input,select,button{font:inherit;box-sizing:border-box;width:100%;padding:10px;border:1px solid #8aa7ad;border-radius:6px}input[type=checkbox]{width:auto}input[type=color]{height:46px}button{background:#155e69;color:white;font-weight:700;cursor:pointer}button:disabled{opacity:.6}small{display:block;line-height:1.5;color:#46636a}.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}#message{white-space:pre-wrap;padding:12px;background:#edf5f6;border-radius:6px}details{margin:18px 0}@media(max-width:430px){.row{grid-template-columns:1fr}main{padding:16px}body{padding:10px}}
[hidden]{display:none!important}.secondary{background:white;color:#155e69;margin-top:8px}.field-heading{display:flex;align-items:center;justify-content:space-between;gap:12px}.field-heading button{width:auto;margin:0;padding:6px 12px}
</style></head><body><main><h1>Set up your Fish Lamp</h1>
<p>Connect your lamp, choose its lighting, and set its clock. Your settings stay saved after a restart.</p>
<p id="message" role="status" aria-live="polite">Loading setup session…</p>
<form id="setup"><fieldset><legend>Home Wi-Fi</legend>
<div class="field-heading"><label for="network">SSID (2.4 GHz)</label><button id="refreshNetworks" type="button" class="secondary">Refresh</button></div>
<select id="network" required aria-describedby="networkStatus"><option value="" disabled selected>Choose a network</option><option value="manual">Manual</option><option value="none">None (offline lighting)</option></select>
<label id="manualNetwork" hidden>Network name<input id="ssid" maxlength="32" autocomplete="off" disabled placeholder="Enter the exact SSID"></label>
<small id="networkStatus" role="status">Loading available networks…</small>
<div id="networkCredentials"><label>Network password<input id="password" type="password" maxlength="64" autocomplete="new-password"></label>
<small>Leave empty for an open home network. A secured network needs 8–63 characters or a 64-digit hexadecimal key.</small></div>
</fieldset><fieldset><legend>Location &amp; time</legend>
<p id="locationStatus" role="status">Checking browser location…</p>
<button id="deviceLocation" type="button" class="secondary">Use device location</button>
<p>Clock: <strong id="clock"></strong></p><p id="zoneStatus"></p>
<button id="advancedToggle" type="button" class="secondary" aria-expanded="false" aria-controls="advanced">Advanced overrides</button>
<div id="advanced" hidden>
<div class="row"><label>Latitude<input id="latitude" type="number" min="-90" max="90" step="any" placeholder="North is positive"></label>
<label>Longitude<input id="longitude" type="number" min="-180" max="180" step="any" placeholder="East is positive"></label></div>
<label>Standard UTC offset in hours<input id="offset" type="number" min="-12" max="14" step="0.25" placeholder="Example: -6 for US Central"></label>
<label>Daylight saving time<select id="dst"><option value="" disabled>Choose a supported rule</option><option value="none">No daylight saving time</option><option value="us">US rules (2007 onward)</option></select></label>
<small>The browser selects fixed-offset or US-rule zones automatically. Other seasonal rules require review; they are not silently treated as US rules.</small>
<label><input id="overrideClock" type="checkbox"> Override the browser clock</label>
<label id="clockOverrideField" hidden>UTC date and time to set when saving<input id="utcOverride" type="datetime-local" step="1" disabled></label>
<button id="browserDefaults" type="button" class="secondary">Restore browser time and zone</button>
</div>
</fieldset><fieldset><legend>Aquarium strip</legend>
<div class="row"><label>Day color<input id="dayColor" type="color" value="#ffffff"></label><label>Day brightness (0–255)<input id="dayBrightness" type="number" min="0" max="255" value="64" required></label>
<label>Night color<input id="nightColor" type="color" value="#000000"></label><label>Night brightness (0–255)<input id="nightBrightness" type="number" min="0" max="255" value="0" required></label></div>
<small>These colors apply to all 85 pixels. Zero brightness means off.</small>
</fieldset><fieldset><legend>Lighting schedule</legend><small>Offsets are minutes relative to sunrise or sunset. Negative means earlier; positive means later. The off limit is local wall time.</small>
<div class="row"><label>RGB sunrise offset<input id="lightsRise" type="number" min="-720" max="720" value="0" required></label><label>RGB sunset offset<input id="lightsSet" type="number" min="-720" max="720" value="0" required></label></div>
<label>RGB off limit<input id="lightsMax" type="time" value="23:00" required></label>
<div class="row"><label>Grow sunrise offset<input id="growRise" type="number" min="-720" max="720" value="0" required></label><label>Grow sunset offset<input id="growSet" type="number" min="-720" max="720" value="0" required></label></div>
<label>Grow off limit<input id="growMax" type="time" value="23:00" required></label></fieldset>
<details><summary>Import existing LED/settings JSON (optional)</summary><label>Configuration file<input id="import" type="file" accept="application/json,.json"></label>
<small>Preserves imported per-pixel day/night arrays, schedule offsets and extra fields. Location, timezone, Wi-Fi and clock are taken from this form. Imported lighting overrides the lighting fields above.</small></details>
<p>Saving initializes storage if needed. The lamp tests home Wi-Fi, verifies its RTC, saves settings, then restarts. Existing configured lighting continues during setup; a new lamp stays off until setup succeeds.</p>
<button id="save" disabled>Save settings &amp; restart</button></form>
<script>
const $=id=>document.getElementById(id);let token='';
const message=text=>$('message').textContent=text;
let networkNames=[],locationEdited=false;
function showAdvanced(){ $('advanced').hidden=false;$('advancedToggle').setAttribute('aria-expanded','true'); }
$('advancedToggle').onclick=()=>{const hidden=!$('advanced').hidden;$('advanced').hidden=hidden;$('advancedToggle').setAttribute('aria-expanded',String(!hidden));};
function updateNetwork(){const choice=$('network').value,manual=choice==='manual',none=choice==='none';$('manualNetwork').hidden=!manual;$('ssid').disabled=!manual;$('ssid').required=manual;$('networkCredentials').hidden=none;$('password').disabled=none;}
$('network').onchange=updateNetwork;updateNetwork();
function selectedSSID(){const choice=$('network').value;if(choice==='none')return '';if(choice==='manual')return $('ssid').value;const index=Number(choice.slice(1));if(!/^n\d+$/.test(choice)||!networkNames[index])throw Error('Choose a network, Manual, or None.');return networkNames[index];}
async function timedFetch(url,options={},timeout=5000){const controller=new AbortController();const timer=setTimeout(()=>controller.abort(),timeout);try{return await fetch(url,{...options,signal:controller.signal});}finally{clearTimeout(timer);}}
async function getJSON(url){for(let attempt=0;attempt<3;attempt++){try{const r=await timedFetch(url,{cache:'no-store'});if(!r.ok)throw Error('Could not load '+url+'.');return await r.json();}catch(e){if(attempt===2)throw e;await new Promise(resolve=>setTimeout(resolve,250));}}}
async function loadNetworks(){
 $('refreshNetworks').disabled=true;
 try{const scan=await getJSON('/networks');if(!Array.isArray(scan.networks))throw Error('Invalid network list.');
  const previous=$('network').value,selected=previous.startsWith('n')?networkNames[Number(previous.slice(1))]:null;
  networkNames=scan.networks.filter(name=>typeof name==='string'&&name.length>0);
  const select=$('network');select.textContent='';const option=(text,value,disabled=false)=>{const o=document.createElement('option');o.textContent=text;o.value=value;o.disabled=disabled;select.appendChild(o);};
  option('Choose a network','',true);networkNames.forEach((name,i)=>option(name,'n'+i));option('Manual','manual');option('None (offline lighting)','none');
  select.value=previous==='manual'||previous==='none'?previous:selected&&networkNames.includes(selected)?'n'+networkNames.indexOf(selected):networkNames.length?'':'manual';updateNetwork();
  $('networkStatus').textContent=scan.status==='ready'?(networkNames.length+' nearby network(s). Choose one, Manual, or None.'):(networkNames.length+' network(s); scan '+scan.status+' (code '+(scan.error??'?')+'). Manual entry is available.');
  if(!networkNames.length)$('networkStatus').textContent+=' Only 2.4 GHz is supported. Enter the SSID manually if needed.';
 }catch(e){$('networkStatus').textContent='Could not load the list. Choose Manual or None, or reload the list.';if(!$('network').value)$('network').value='manual';updateNetwork();}
 finally{$('refreshNetworks').disabled=false;}
}
$('refreshNetworks').onclick=()=>token?loadNetworks():session();
function detectBrowserZone(){
 const year=new Date().getUTCFullYear(),samples=[];
 for(let y=year;y<=year+1;y++)for(let t=Date.UTC(y,0,1,12);t<Date.UTC(y+1,0,1);t+=86400000)samples.push([t,-new Date(t).getTimezoneOffset()]);
 const standard=Math.min(...samples.map(sample=>sample[1]));
 if(samples.every(sample=>sample[1]===standard))return {offset:standard,dst:'none'};
 const boundaries=y=>{const sunday=(month)=>1+(7-new Date(Date.UTC(y,month,1)).getUTCDay())%7;return [Date.UTC(y,2,sunday(2)+7,2)-standard*60000,Date.UTC(y,10,sunday(10),2)-(standard+60)*60000];};
 const expected=t=>{const [start,end]=boundaries(new Date(t).getUTCFullYear());return standard+(t>=start&&t<end?60:0);};
 for(let y=year;y<=year+1;y++)for(const boundary of boundaries(y))for(const delta of [-60000,0,60000]){const t=boundary+delta;samples.push([t,-new Date(t).getTimezoneOffset()]);}
 return {offset:standard,dst:samples.every(sample=>sample[1]===expected(sample[0]))?'us':''};
}
function browserDefaults(){const zone=detectBrowserZone();$('offset').value=zone.offset/60;$('dst').value=zone.dst;$('overrideClock').checked=false;updateClockOverride();let name='local timezone';try{name=Intl.DateTimeFormat().resolvedOptions().timeZone||name;}catch(e){}$('zoneStatus').textContent='Browser zone: '+name+'. '+(zone.dst?'Time and zone filled from this device.':'Seasonal rules need review in Advanced.');if(!zone.dst)showAdvanced();}
function updateClockOverride(){$('clockOverrideField').hidden=!$('overrideClock').checked;$('utcOverride').disabled=!$('overrideClock').checked;if($('overrideClock').checked&&!$('utcOverride').value)$('utcOverride').value=new Date().toISOString().slice(0,19);}
$('overrideClock').onchange=updateClockOverride;$('browserDefaults').onclick=browserDefaults;
function clock(){$('clock').textContent=$('overrideClock').checked?($('utcOverride').value||'Choose a time')+' UTC (override)':new Date().toLocaleString();}
function requestDeviceLocation(replace=false){
 if(!window.isSecureContext||!navigator.geolocation){$('locationStatus').textContent='This browser blocks device location on the HTTP setup network. Enter latitude and longitude in Advanced.';showAdvanced();return;}
 if(replace)locationEdited=false;$('locationStatus').textContent='Waiting for device location permission…';
 navigator.geolocation.getCurrentPosition(position=>{if(locationEdited){$('locationStatus').textContent='Manual coordinates kept. Use device location again to replace them.';return;}const {latitude,longitude}=position.coords;if(!Number.isFinite(latitude)||Math.abs(latitude)>90||!Number.isFinite(longitude)||Math.abs(longitude)>180){$('locationStatus').textContent='Device returned invalid coordinates. Enter them in Advanced.';showAdvanced();return;}$('latitude').value=latitude.toFixed(6);$('longitude').value=longitude.toFixed(6);$('locationStatus').textContent='Using device location: '+$('latitude').value+', '+$('longitude').value;},()=>{$('locationStatus').textContent='Device location unavailable or permission denied. Enter coordinates in Advanced.';showAdvanced();},{enableHighAccuracy:false,timeout:8000,maximumAge:300000});
}
$('latitude').oninput=$('longitude').oninput=()=>{locationEdited=true;};$('deviceLocation').onclick=()=>requestDeviceLocation(true);
browserDefaults();clock();setInterval(clock,1000);requestDeviceLocation();
async function session(){try{const s=await getJSON('/session');if(typeof s.token!=='string'||s.token.length!==32)throw Error('Invalid setup session.');token=s.token;message(s.error||'Ready. Enter your configuration below.');$('save').disabled=false;await loadNetworks();}catch(e){message(e.message+' Reconnect to FishLamp-Setup and use Refresh to retry.');}}session();
const number=id=>{if($(id).value.trim()==='')throw Error('Enter '+id+' in the form (location and time overrides are in Advanced).');const value=Number($(id).value);if(!Number.isFinite(value))throw Error('Invalid '+id+'.');return value;};
const seconds=id=>{const [h,m]=$(id).value.split(':').map(Number);return h*3600+m*60;};
function pixels(color,brightness){const hex=$(color).value;const p=[1,3,5].map(i=>parseInt(hex.slice(i,i+2),16));p.push(number(brightness));return Array.from({length:85},()=>p.slice());}
$('setup').onsubmit=async event=>{event.preventDefault();$('save').disabled=true;try{
 let c={};const file=$('import').files[0];if(file){if(file.size>16384)throw Error('Imported file must be at most 16 KiB.');c=JSON.parse(await file.text());if(!c||Array.isArray(c)||typeof c!=='object')throw Error('Import must be a JSON object.');}
 else{c.lights=pixels('dayColor','dayBrightness');c.nightlights=pixels('nightColor','nightBrightness');for(const [key,prefix] of [['lights','lights'],['growlight','grow']]){c[key+'-sunrise-offset']=number(prefix+'Rise')*60;c[key+'-sunset-offset']=number(prefix+'Set')*60;c[key+'-max-offset']=seconds(prefix+'Max');}}
 const latitude=number('latitude'),longitude=number('longitude');if(Math.abs(latitude)>90||Math.abs(longitude)>180)throw Error('Latitude or longitude is out of range.');if(!['none','us'].includes($('dst').value))throw Error('Choose supported daylight-saving rules in Advanced.');
 const ssid=selectedSSID();if($('network').value==='manual'&&!ssid)throw Error('Enter a network name, or choose None.');if(new TextEncoder().encode(ssid).length>32)throw Error('The SSID must be at most 32 UTF-8 bytes.');
 const utc=$('overrideClock').checked?Date.parse($('utcOverride').value+'Z'):Date.now();if(!Number.isFinite(utc))throw Error('Enter a valid UTC time in Advanced.');
 Object.assign(c,{'schema-version':1,'rtc-utc-confirmed':true,'setup-utc':Math.floor(utc/1000),latitude,longitude,'timezone-offset-minutes':number('offset')*60,'timezone-dst':$('dst').value,'wifi-sid':ssid,'wifi-pwd':$('network').value==='none'?'':$('password').value});
 const data=JSON.stringify(c);if(new TextEncoder().encode(data).length>16384)throw Error('Configuration must be at most 16 KiB.');
 const r=await timedFetch('/configure',{method:'POST',headers:{'Content-Type':'application/json','X-Setup-Token':token},body:data},15000);const text=await r.text();if(!r.ok)throw Error(text);message(text);$('password').value='';
 }catch(e){showAdvanced();message(e.message+' If the connection was lost, reconnect to the setup network and reload to check its state.');$('save').disabled=false;}};
</script></main></body></html>)HTML";
