// Local UI preview with synthetic networks; never reads or writes lamp settings.
const http=require('node:http'),fs=require('node:fs'),path=require('node:path');
let lists=0;
const server=http.createServer((request,response)=>{
 response.setHeader('Cache-Control','no-store');
 response.setHeader('Permissions-Policy','geolocation=()');
 if(request.url==='/session') {response.setHeader('Content-Type','application/json');response.end(JSON.stringify({token:'0'.repeat(32),error:''}));}
 else if(request.url==='/networks') {response.setHeader('Content-Type','application/json');response.end(JSON.stringify({status:'ready',error:0,networks:++lists>1?['Synthetic One','Synthetic Two','Synthetic Three']:['Synthetic One','Synthetic Two']}));}
 else if(request.url==='/configure') {request.resume();response.end('Preview only. No device settings were written.');}
 else {response.setHeader('Content-Type','text/html; charset=utf-8');const source=fs.readFileSync(path.join(__dirname,'../src/SetupPage.h'),'utf8');response.end([...source.matchAll(/R"HTML\(([\s\S]*?)\)HTML"/g)].map(match=>match[1]).join(''));}
});
server.listen(0,'127.0.0.1',()=>console.log('Preview: http://127.0.0.1:'+server.address().port));
