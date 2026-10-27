// Execute the embedded browser script with a DOM/fetch stand-in; no dependencies.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(require('node:path').join(__dirname,'../src/DashboardPage.h'),'utf8');
class Element{
 constructor(){this.children=[];this.style={};this.textContent='';}
 append(...nodes){this.children.push(...nodes);}
 prepend(node){this.children.unshift(node);}
 replaceChildren(){this.children=[];}
}
const elements=Object.fromEntries([...source.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const sample={utc:1774008000,timeValid:true,rtcHealthy:true,rtcMocked:false,rtcAgeMs:400,uptimeMs:1000,
 timezoneOffsetSeconds:0,zone:{offsetMinutes:-360,usDst:true},rgbMode:'day',stripReady:true,growlightOn:true,
 lightsDelay:0,growlightDelay:0,wifi:{ssid:'<img src=x onerror=alert(1)>',ip:'192.0.2.42',connected:true,rssiDbm:-55},
 sensors:{waterF:{value:77,healthy:false,mocked:false,ageMs:61000},cpuC:{value:null,healthy:false,ageMs:null},ambientPercent:{value:50,healthy:true,mocked:true,ageMs:100}},
 schedule:{lightsOn:1774008000,lightsOff:1774030000,growlightOn:1774008000,growlightOff:1774030000},
 lights:Array.from({length:85},()=>[16,32,64,128]),nightlights:Array.from({length:85},()=>[0,0,8,20]),
 lightsOffsets:{sunriseSeconds:0,sunsetSeconds:0,absoluteOffSeconds:86400},growlightOffsets:{sunriseSeconds:0,sunsetSeconds:0,absoluteOffSeconds:86400}};
const timers=[],calls=[];let fail=false;
const context=vm.createContext({document:{getElementById:id=>elements[id],createElement:()=>new Element()},Date,AbortController,
 setTimeout(fn,ms){timers.push({fn,ms});return timers.length;},clearTimeout(){},
 async fetch(url,options){calls.push({url,options});if(fail)throw Error('offline');return {ok:true,json:async()=>sample};}});
vm.runInContext(source.match(/<script>([\s\S]*?)<\/script>/)[1],context);
const flush=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
 await flush();assert.equal(calls[0].url,'/status');assert.equal(calls[0].options.cache,'no-store');
 assert.equal(elements.day.children.length,85);assert.equal(elements.night.children.length,85);
 assert.match(elements.sensors.children[0].textContent,/77.0 °F.*Error \/ stale.*61s/);
 assert.match(elements.sensors.children[1].textContent,/Unavailable/);
 assert.equal(elements.wifi.children[1].textContent,sample.wifi.ssid);
 assert.match(elements.connection.textContent,/Live/);
 const row=elements.day.children[0];await vm.runInContext('poll()',context);assert.equal(elements.day.children[0],row);
 fail=true;await vm.runInContext('poll()',context);assert.equal(elements.connection.className,'failed');
 assert.equal(elements.day.children.length,85);assert.match(elements.connection.textContent,/stale/);
 assert.ok(timers.some(t=>t.ms===5000));assert.ok(timers.some(t=>t.ms===3000));
 fail=false;await vm.runInContext('poll()',context);assert.equal(elements.connection.className,'');
 // Event-specific offset across both US DST transitions, independent of phone timezone.
 for(const [date,expected] of [['2026-03-08T07:59:59Z','01:59:59 (UTC−06:00)'],['2026-03-08T08:00:00Z','03:00:00 (UTC−05:00)'],['2026-11-01T06:59:59Z','01:59:59 (UTC−05:00)'],['2026-11-01T07:00:00Z','01:00:00 (UTC−06:00)']]){
   context.eventSeconds=Date.parse(date)/1000;context.data=sample;
   assert.ok(vm.runInContext('time(eventSeconds,data)',context).endsWith(expected));
 }
 console.log('Dashboard script tests passed: rendering, 85 day/night pixels, safe SSID text, polling/retry/stale status and DST event times.');
})().catch(e=>{console.error(e);process.exitCode=1;});
