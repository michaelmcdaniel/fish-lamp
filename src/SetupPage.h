#pragma once
// All assets are served locally; no CDN or Internet connection required.
inline constexpr char SetupPage[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Fish Lamp setup</title><style>
body{font:16px system-ui,sans-serif;background:#edf5f6;color:#17353b;margin:0;padding:20px}main{max-width:620px;margin:auto;background:white;padding:24px;border-radius:16px}h1{margin-top:0}fieldset{border:1px solid #bbd0d3;border-radius:10px;margin:20px 0;padding:16px}legend{font-weight:700}label{display:block;margin:12px 0}input,select,button{font:inherit;box-sizing:border-box;width:100%;padding:10px;border:1px solid #8aa7ad;border-radius:6px}input[type=checkbox]{width:auto}input[type=color]{height:46px}button{background:#155e69;color:white;font-weight:700;cursor:pointer}button:disabled{opacity:.6}small{display:block;line-height:1.5;color:#46636a}.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}#message{white-space:pre-wrap;padding:12px;background:#edf5f6;border-radius:6px}details{margin:18px 0}@media(max-width:430px){.row{grid-template-columns:1fr}main{padding:16px}body{padding:10px}}
</style></head><body><main><h1>Set up your Fish Lamp</h1>
<p>Connect your lamp, choose its lighting, and set its clock. Your settings stay saved after a restart.</p>
<p id="message" role="status" aria-live="polite">Loading setup session…</p>
<form id="setup"><fieldset><legend>Home Wi-Fi</legend>
<label><input id="offline" type="checkbox"> Use lighting offline without home Wi-Fi</label>
<label>Network name (2.4 GHz)<input id="ssid" maxlength="32" autocomplete="off" required></label>
<label>Network password<input id="password" type="password" maxlength="64" autocomplete="new-password"></label>
<small>Leave the password empty only for an open home network. A secured network needs 8–63 characters or a 64-digit hexadecimal key.</small>
</fieldset><fieldset><legend>Location &amp; time</legend>
<div class="row"><label>Latitude<input id="latitude" type="number" min="-90" max="90" step="any" required placeholder="North is positive"></label>
<label>Longitude<input id="longitude" type="number" min="-180" max="180" step="any" required placeholder="East is positive"></label></div>
<label>Standard UTC offset in hours<input id="offset" type="number" min="-12" max="14" step="0.25" required placeholder="Example: -6 for US Central"></label>
<label>Daylight saving time<select id="dst"><option value="none">No daylight saving time</option><option value="us">US rules (2007 onward)</option></select></label>
<small>Use the standard offset, before daylight saving. Other daylight saving rules are not supported yet.</small>
<p>Your device clock: <strong id="clock"></strong></p>
<label><input id="confirm" type="checkbox" required> My device's date and time are correct. Use it to set the lamp's clock.</label>
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
<p>Saving initializes storage if needed. The lamp tests home Wi-Fi, verifies its RTC, saves settings, then restarts. Lights stay off until setup succeeds.</p>
<button id="save" disabled>Save settings &amp; restart</button></form>
<script>
const $=id=>document.getElementById(id);let token='';
const message=text=>$('message').textContent=text;
const clock=()=>$('clock').textContent=new Date().toLocaleString();clock();setInterval(clock,1000);
$('offline').onchange=()=>{const disabled=$('offline').checked;$('ssid').disabled=$('password').disabled=disabled;$('ssid').required=!disabled;};
async function session(){try{const r=await fetch('/session',{cache:'no-store'});if(!r.ok)throw Error('Could not open setup session.');const s=await r.json();token=s.token;message(s.error||'Ready. Enter your configuration below.');$('save').disabled=false;}catch(e){message(e.message+' Reconnect to FishLamp-Setup and reload.');}}session();
const number=id=>Number($(id).value);
const seconds=id=>{const [h,m]=$(id).value.split(':').map(Number);return h*3600+m*60;};
function pixels(color,brightness){const hex=$(color).value;const p=[1,3,5].map(i=>parseInt(hex.slice(i,i+2),16));p.push(number(brightness));return Array.from({length:85},()=>p.slice());}
$('setup').onsubmit=async event=>{event.preventDefault();$('save').disabled=true;try{
 let c={};const file=$('import').files[0];if(file){if(file.size>16384)throw Error('Imported file must be at most 16 KiB.');c=JSON.parse(await file.text());if(!c||Array.isArray(c)||typeof c!=='object')throw Error('Import must be a JSON object.');}
 else{c.lights=pixels('dayColor','dayBrightness');c.nightlights=pixels('nightColor','nightBrightness');for(const [key,prefix] of [['lights','lights'],['growlight','grow']]){c[key+'-sunrise-offset']=number(prefix+'Rise')*60;c[key+'-sunset-offset']=number(prefix+'Set')*60;c[key+'-max-offset']=seconds(prefix+'Max');}}
 Object.assign(c,{'schema-version':1,'rtc-utc-confirmed':true,'setup-utc':Math.floor(Date.now()/1000),latitude:number('latitude'),longitude:number('longitude'),'timezone-offset-minutes':number('offset')*60,'timezone-dst':$('dst').value,'wifi-sid':$('offline').checked?'':$('ssid').value,'wifi-pwd':$('offline').checked?'':$('password').value});
 const data=JSON.stringify(c);if(new TextEncoder().encode(data).length>16384)throw Error('Configuration must be at most 16 KiB.');
 const r=await fetch('/configure',{method:'POST',headers:{'Content-Type':'application/json','X-Setup-Token':token},body:data});const text=await r.text();if(!r.ok)throw Error(text);message(text);$('password').value='';
 }catch(e){message(e.message+' If the connection was lost, reconnect to the setup network and reload to check its state.');$('save').disabled=false;}};
</script></main></body></html>)HTML";
