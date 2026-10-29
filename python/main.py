import gc
import sys
import uasyncio as asyncio
from machine import ADC, Pin, Timer, RTC, PWM
import bluetooth
#import ble_uart_peripheral
import utime
import time
from neopixel import Neopixel
import random
import network
import socket
import ntptime
import struct
import _thread
import re
import ujson as json
import math
import datetime
import suncalc
import settings as _settings
import onewire, ds18x20
import rtc_lib
import urequests

red = (255, 0, 0)
green = (0, 255, 0)
blue = (0, 0, 255)
white = (255,255,255)
black = (0,0,0)

PIN_NEOPIXEL = 7
PIN_ONBOARD_LED = "LED"
PIN_ONBOARD_TEMP = 4
PIN_TEMP = 26
PIN_PHOTORESISTOR = 27
PIN_GROWLIGHT = 8
PIN_RTC_SDA = 16
PIN_RTC_SCL = 17
PIXEL_COUNT = 85
DEFAULT_GROWLIGHT_DELAY = 60*60 #1hr
DEFAULT_LIGHTS_DELAY = 60*60 #1hr

WIFI_SSID = ""
WIFI_PWD = ""
myIp = '0.0.0.0'
rgbOn = False
growLightOn = False

onboard_led = Pin(PIN_ONBOARD_LED, Pin.OUT)
onboard_led.value(1)
time.sleep(0.2)
onboard_led.value(0)
onboard_temp = machine.ADC(PIN_ONBOARD_TEMP)
growLight = Pin(PIN_GROWLIGHT, Pin.OUT)
strip = Neopixel(PIXEL_COUNT, 0, PIN_NEOPIXEL, "GRB")
ambientSensor = ADC(Pin(PIN_PHOTORESISTOR))
ds_sensor = ds18x20.DS18X20(onewire.OneWire(Pin(PIN_TEMP)))
rtc2 = rtc_lib.ds3231(PIN_RTC_SCL,PIN_RTC_SDA,0)
rtc = RTC()
wlan = network.WLAN(network.STA_IF)
sunlight=suncalc.sun()

lights = [None]*PIXEL_COUNT
nightlights = [None]*PIXEL_COUNT
for i in range(len(lights)):
    lights[i] = (255,255,255,255)
    nightlights[i] = (0,0,0,255)
#lights[:] = (0,0,0)
#print (str(lights))
    
lightsUpdated = False
lightsOn = False
lightsDelay = 0
lightsForceOff = False
growlightDelay = 0
waterTemp = -1
logTemp = True
logSchedule = True
schedule = None
onboardTemp = 0



for i in range(PIXEL_COUNT):
    val = lights[i]
    strip.brightness(val[3])
    strip[i] = (val[0],val[1],val[2]) #38:
#strip.show()
onboard_led.value(0)
#growLight.value(0)

lv = round(ambientSensor.read_u16()/65535*100)
print('light: ' + str(lv))

settings = _settings.getSettings()
if ("lights" not in settings or len(settings["lights"]) != PIXEL_COUNT):
    print('saving settings: no lights')
    settings["lights"] = lights
    _settings.saveSettings(settings)
else:
    lights = settings["lights"]

if ("nightlights" not in settings or len(settings["nightlights"]) != PIXEL_COUNT):
    print('saving settings: no nightlights')
    settings["nightlights"] = nightlights
    _settings.saveSettings(settings)
else:
    nightlights = settings["nightlights"]


WIFI_SSID = settings["wifi-sid"]
WIFI_PWD = settings["wifi-pwd"]

#ble = ble_uart_peripheral.BLEUART(bluetooth.BLE(), "Fishlamp")

def printTime(info, dt):
    timeFormat = "{}-{:02d}-{:02d} {:02d}:{:02d}"
    if (isinstance(dt,int)):
        dt = time.gmtime(dt)
    print(info,": ",timeFormat.format(dt[0],dt[1],dt[2],dt[3],dt[4]))

def getFormatedTime(dt):
    timeFormat = "{}-{:02d}-{:02d} {:02d}:{:02d}"
    if (isinstance(dt,int)):
        dt = time.gmtime(dt)
    return timeFormat.format(dt[0],dt[1],dt[2],dt[3],dt[4])


def printHours(info, dt):
    timeFormat = "{:02d}:{:02d}"
    if (isinstance(dt,int)):
        dt = time.gmtime(dt)
    print(info,": ",timeFormat.format(dt[3],dt[4]))

    
def IsDst(month, day, dayofweek, hour):
    if (month > 11 or month < 3): return True
    if (month < 11 and month > 3): return False
    dstd = 1
    roll = day
    wd = dayofweek
    while(roll > 1):
        roll = roll-1
        wd = wd-1
        if (wd < 0): wd = 6
    if (month == 11):
        while(wd > 0 and wd < 7):
            dstd = dstd + 1
            wd = wd + 1
        return day > dstd or (day == dstd and hour > 1)
    while(wd < 7):
        dstd = dstd + 1
        wd = wd + 1
    return day < dstd or (day == dstd and hour < 2)

def IsLocalDst(dt):
    if (isinstance(dt,int)):
        dt = time.gmtime(dt)
    year = dt[0]; month = dt[1];day = dt[2];dayofweek=dt[3];h = dt[4];m = dt[5];s = dt[6]
    if (len(dt) == 8): h = dt[3];m = dt[4];s = dt[5];dayofweek=dt[6];
    if (month > 11 or month < 3): return True
    if (month < 11 and month > 3): return False
    dstd = 1
    roll = day
    wd = dayofweek
    while(roll > 1):
        roll = roll-1
        wd = wd-1
        if (wd < 0): wd = 6
    if (month == 11):
        while(wd > 0 and wd < 7):
            dstd = dstd + 1
            wd = wd + 1
        return day > dstd or (day == dstd and h > 2)
    while(wd < 7):
        dstd = dstd + 1
        wd = wd + 1
    return day < dstd or (day == dstd and h < 1)


def getTZOffset(dt, offset=-5,dst=True):
    outputInt = False
    if (isinstance(dt,int)):
        outputInt = True
        dt = time.gmtime(dt)
    dstOffset = 0
    year = dt[0]; month = dt[1];day = dt[2];wd = dt[3];h = dt[4];m = dt[5];s = dt[6]
    if (len(dt) == 8):
        wd = dt[6];h = dt[3];m = dt[4];s = dt[5]
    mds = [31,28,31,30,31,30,31,31,30,31,30,31]
    if (year % 4 == 0): mds = [31,27,31,30,31,30,31,31,30,31,30,31] # leapyear
    cd = 0;cm = 0
    while(cm < month):
        cd = cd + mds[cm]
        cm += 1
    cm += day
    dt = time.gmtime(time.mktime((year,month,day,h,m,s,wd,cm)) + (offset*3600))
    year = dt[0]; month = dt[1];day = dt[2];wd = dt[6];h = dt[3];m = dt[4];s = dt[5]
    if (dst == True and IsDst(month, day, wd, h) == True): offset += -1
    
    return offset

def toLocaltime(dt,offset=-5,dst=True):
    outputInt = False
    if (isinstance(dt,int)):
        outputInt = True
        dt = time.gmtime(dt)
    year = dt[0]; month = dt[1];day = dt[2];wd = dt[3];h = dt[4];m = dt[5];s = dt[6] # from RTC
    if (len(dt) == 8): wd = dt[6];h = dt[3];m = dt[4];s = dt[5] # from gmtime
    mds = [31,28,31,30,31,30,31,31,30,31,30,31]
    if (year % 4 == 0): mds = [31,27,31,30,31,30,31,31,30,31,30,31] # leapyear
    cd = 0;cm = 0
    while(cm < month):
        cd = cd + mds[cm]
        cm += 1
    cm += day
    dstOffset = getTZOffset((year,month,day,h,m,s,wd,cm), offset, dst)
    dt = time.gmtime(time.mktime((year,month,day,h,m,s,wd,cm)) + (dstOffset*3600))
    
    if (outputInt): return time.mktime(dt)
    return dt
    
def toUtctime(dt,offset=-5,dst=True):
    outputInt = False
    if (isinstance(dt,int)):
        outputInt = True
        dt = time.gmtime(dt)
    year = dt[0]; month = dt[1];day = dt[2];wd = dt[3];h = dt[4];m = dt[5];s = dt[6] # from RTC
    if (len(dt) == 8): wd = dt[6];h = dt[3];m = dt[4];s = dt[5] # from gmtime
    if (dst and IsLocalDst(dt)):
        offset += -1
    mds = [31,28,31,30,31,30,31,31,30,31,30,31]
    if (year % 4 == 0): mds = [31,27,31,30,31,30,31,31,30,31,30,31] # leapyear
    cd = 0;cm = 0
    while(cm < month):
        cd = cd + mds[cm]
        cm += 1
    cm += day
    dt = time.mktime((year,month,day,h,m,s,wd,cm)) + (offset*-3600)
    if (outputInt): return dt
    return time.gmtime(dt)
    
    #determine if dt[] is 

def Index():
    global waterTemp
    global schedule
    global lights
    global growlightDelay
    global lightsDelay
    global lightsOn
    global onboardTemp
    clv = round(ambientSensor.read_u16()/65535*100)
    
    picoTempDisplay = str(onboardTemp) + ' &deg;F'
    waterTempDisplay = '-'
    if (waterTemp > 0):
        waterTempDisplay = str(waterTemp) + ' &deg;F'
    sunrise = sunlight.sunrise()
    sunset = sunlight.sunset()
    sunrise = getFormatedTime(toLocaltime(sunrise))
    sunset = getFormatedTime(toLocaltime(sunset))
    scheduledLightsOn = ''
    scheduledLightsOff = ''
    scheduledGrowlightOn = ''
    scheduledGrowlightOff = ''
    nextcheck = ''
    if (schedule != None):
        scheduledLightsOn = getFormatedTime(toLocaltime(schedule["lightsOn"]))
        scheduledLightsOff = getFormatedTime(toLocaltime(schedule["lightsOff"]))
        scheduledGrowlightOn = getFormatedTime(toLocaltime(schedule["growlightOn"]))
        scheduledGrowlightOff = getFormatedTime(toLocaltime(schedule["growlightOff"]))
        nextcheck = getFormatedTime(toLocaltime(schedule["nextCheck"]))
    glOnValue = ''
    glOnValueBool = 'false'
    
    rgbOnValue = ''
    rgbOnValueBool = 'false'
    
    utcTime = getFormatedTime(time.gmtime())
    localTime = getFormatedTime(toLocaltime(time.gmtime()))
    areLightsOn = anyLightsOn()
    if (lightsOn == True):
        rgbOnValue = ''
        rgbOnValueBool = 'true'
    if (growLightOn == True):
        glOnValue = ''
        glOnValueBool = 'true'
    nowSec = time.time()
    if (nowSec < growlightDelay and growlightDelay > 0):
        if (growLightOn):
            scheduledGrowlightOff = getFormatedTime(toLocaltime(growlightDelay))
            if (growlightDelay != schedule["growlightOff"]):
                glOnValue = ' (delayed)'
        else:
            scheduledGrowlightOn = getFormatedTime(toLocaltime(growlightDelay))
            if (growlightDelay != schedule["growlightOn"]):
                glOnValue = ' (delayed)'
    if (nowSec < lightsDelay and lightsDelay > 0):
        if (lightsOn):
            scheduledLightsOff = getFormatedTime(toLocaltime(lightsDelay))
            if (lightsDelay != schedule["lightsOff"]):
                rgbOnValue = ' (delayed)'
        else:
            scheduledLightsOn = getFormatedTime(toLocaltime(lightsDelay))
            if (lightsDelay != schedule["lightsOn"]):
                rgbOnValue = ' (delayed)'
            
    
    lightValues = json.dumps(lights)
    nightlightValues = json.dumps(nightlights)
    html = f"""
<!DOCTYPE html>
<html lang="en">
    <head>
        <title>Fishlamp</title>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <script src="https://cdnjs.cloudflare.com/ajax/libs/vue/3.5.4/vue.global.prod.js"></script>
        <script src="https://cdnjs.cloudflare.com/ajax/libs/jscolor/2.5.2/jscolor.min.js"></script>
        <script src="https://mcdaniel.ws/js/components.min.js"></script>
        <link href="https://mcdaniel.ws/css/fishlamp.css" rel="stylesheet">
        <style>
            *[v-cloak]{{display:none!important;}}
            .switch {{ display: inline-block; }}
        </style>
    </head>
    <body>
        <div id="root">
            <div v-cloak>
                <h2>Day time Lights</h2>
            <div v-for="(l) in lights" class="light-container" :key="index" :class="{selected:isSelected(l.index)}">
                <span class="light" :style="getBG(l)" v-on:mousedown="mdown($event.target, l.index)" v-on:mousemove="mmove($event.target, l.index)" v-on:mouseup="mup($event.target, l.index)"></span>
                <span :id="'picker'+l.index" data-jscolor="{}"></span>
            </div>
                <!--<h2>Night time lights</h2>
                <div v-for="(l,i) in nightlights" class="light-container" :tabindex="i">
                    <span class="light" :style="getBG(l)" v-on:click="change($event.target, i, false)"></span>
                </div>-->
            </div>
            <div><label>Time:</label><span class="value time">{utcTime}</span></div>
            <div><label>Local:</label><span class="value time">{localTime}</span></div><br/>
            <div><label>Sunrise:</label><span class="value time">{sunrise}</span></div>
            <div><label>Sunset:</label><span class="value time">{sunset}</span></div><br/>
            <div><label>RGB Light:</label><span class="value"><core-switch v-model="lightsOn" label-on="Day" label-off="Night"></core-switch>{rgbOnValue}</span></div>
            <div style="padding-left:10px;"><label>Daytime:</label><span class="value time">{scheduledLightsOn}</span></div>
            <div style="padding-left:10px;"><label>Nighttime:</label><span class="value time">{scheduledLightsOff}</span></div><br/>
            <div><label>Grow Light:</label><span class="value"><core-switch v-model="growlightOn" label-on="On" label-off="Off"></core-switch>{glOnValue}</span></div>
            <div style="padding-left:10px;"><label>On:</label><span class="value time">{scheduledGrowlightOn}</span></div>
            <div style="padding-left:10px;"><label>Off:</label><span class="value time">{scheduledGrowlightOff}</span></div><br/>
            <div><label>Next Check:</label><span class="value time">{nextcheck}</span></div><br/>
            <div><label>Ambient:</label><span class="value">{clv}</span></div>
            <div><label>Water Temp:</label><span class="value">{waterTempDisplay}</span></div>
            <div><label>Light Temp:</label><span class="value">{picoTempDisplay}</span></div>
        </div>
        <script type="text/javascript">
            window.page = {{
                model: {{
                    lights: {lightValues},
                    nightlights: {nightlightValues},
                    lightsOn: {rgbOnValueBool},
                    growlightOn: {glOnValueBool}
                }}
            }}
        </script>
        <script type="text/javascript" src="https://mcdaniel.ws/js/fishlamp/core.js"></script>
    </body>
</html>
"""
    #            <div>Temp: {temp_fahrenheit} &deg;F {temperature_celcius} &deg;C</div>

    #print(str(html))
    return str(html)

def get_our_ip():
    global myIp
    
    status = wlan.ifconfig()
    print( 'ip = ' + status[0] )
    try: 
      myIp = status[0].split('.')[3]
    except:
      myIp = status[0]
#endof def get_our_ip():


async def connect_wifi():
    global wifi_connected
    
    if wlan.status() == 3:
      print('already connected to', str(WIFI_SSID) )
      get_our_ip()
      wifi_connected = True
        
    while wlan.status() != 3:
      onboard_led.value(0)
      await connect_to_network()

      if wlan.status() != 3:
        print("Status:", wlan.status(), " Resting.")
        await asyncio.sleep(5.0)
    print('Getting time')
    try:
        #rtc.datetime((2024,2,20,2,23,59,50,0))
        #rtc2.set_time('23:59:50,Tuesday,2024-02-20')
        ntptime.settime()
        printTime('utc time', time.time())
        printTime('local time', toLocaltime(time.time()))
    except Exception as e:
        print("unable to get current time.")
        sys.print_exception(e)
        
async def connect_to_network():
    global wifi_connected
    global onboard_led
    global WIFI_SSID
    global WIFI_PWD
    
# LEDs on while attempting connection
    onboard_led.value(1)
    #ssled.value(1)
    if (WIFI_SSID == None or WIFI_SSID == ""):
        print("no wifi sid specified")
        return
    if (WIFI_PWD == None or WIFI_PWD == ""):
        print("no wifi password specified")
        return
    
    if wlan.status() == 3:
      print( "wifi already connected" )
    else:
      print( "connecting to", WIFI_SSID, str(wlan.status()) )
      
      wlan.active(True)
#      wlan.config(pm = 0xa11140) # disable low power mode default=0xa11142 aggressive_pm=0xa11c82 performance_pm=0x111022
      wlan.connect( WIFI_SSID, WIFI_PWD )

# Wait for connect or fail
      max_wait = 90
      while max_wait > 0:
        if wlan.status() < 0 or wlan.status() >= 3:
            onboard_led.value(0)
            break
        max_wait -= 1
        onboard_led.value(max_wait % 2)
        print('waiting for connection...',max_wait)
        await asyncio.sleep(1)

# Handle connection error
    if wlan.status() != 3:
      wlan.disconnect()
      wlan.active(False)
      print('connection timed out to',WIFI_SSID)
      print('status code',wlan.status())
    else:
      print('connected to',WIFI_SSID)
      get_our_ip()
      #print('localizing time')
      #set_utc(rtc, -5, True)
      #now = rtc.datetime()

      wifi_connected = True
      launch_webserver()

# Connection attempt is over, lights out.
    await asyncio.sleep(1)
    onboard_led.value(0)
    #ssled.value(0)





def time_string():
    months = [ "Jan", "Feb", "Mar", "Apr", "May", "Jun",
           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" ]

    t = time.localtime()
    if ( t[1] < 1 ):
      t[1] = 1
    if ( t[1] > 12 ):
      t[1] = 12
    ts = "{}-{}-{} {}:{t4:02d}:{t5:02d}".format( t[0], months[t[1]-1], t[2],
                                                 t[3], t4=t[4], t5=t[5] )
    return ts

requestParser = re.compile("^(GET|POST|HEAD|PUT|DELETE)\s(\/[^\s\?]*)(\?[^\s]*)?\s(HTTP\/\d+(\.\d+)?)\r\n")
headerParser = re.compile("^([^\:]+)\:(.*)\r\n")
colorParser = re.compile("^\/lights\/(\d+)\/([a-fA-F0-9][a-fA-F0-9])([a-fA-F0-9][a-fA-F0-9])([a-fA-F0-9][a-fA-F0-9])([a-fA-F0-9][a-fA-F0-9])?$")
colorRangeParser = re.compile("^\/lights\/(\d+)-(\d+)\/([a-fA-F0-9][a-fA-F0-9])([a-fA-F0-9][a-fA-F0-9])([a-fA-F0-9][a-fA-F0-9])([a-fA-F0-9][a-fA-F0-9])?$")
colorAllParser = re.compile("^\/lights\/([a-fA-F0-9][a-fA-F0-9])([a-fA-F0-9][a-fA-F0-9])([a-fA-F0-9][a-fA-F0-9])([a-fA-F0-9][a-fA-F0-9])?$")
colorBrightnessParser = re.compile("^\/lights\/brightness\/([1-9]?[0-9]|25[0-5]|2[0-4][0-9])$")

async def serve_client( reader, writer ):
    global counter
    global shutdown
    global myIp
    global lightsUpdated
    global growLightOn
    global lightsOn
    global lights
    global schedule
    global waterTemp
    global growlightDelay
    global lightsDelay
    global lightsForceOff
    #print("Client connected")
    onboard_led.value(1)
    try:
        request_line = await reader.readline()
        if (len(request_line) > 2048):
            writer.write('HTTP/1.0 415 URI Too Long\r\nContent-length: 0\r\n\r\n')
            await writer.drain()
            await writer.wait_closed()
            return
            
        header = b""
        headers = {}
        print('Request:',request_line)  
        ticksStart = time.ticks_ms();
        headerByteCount = 0
        while ( header != b"\r\n" ) and ( time.ticks_diff(time.ticks_ms(), ticksStart) < 250 ):
            header = await reader.readline()
            headerByteCount += len(header)
            if (headerByteCount > 1024*16):
                writer.write('HTTP/1.0 431 Request Header Fields Too Large\r\nContent-length: 0\r\n\r\n')
                await writer.drain()
                await writer.wait_closed()
                return
                
            match = headerParser.match(header)
            if (match != None):
                headers[match.group(1)] = match.group(2)
            await asyncio.sleep(0.0001)
            #print('Header:', str(header) )
    
        
    
        match = requestParser.match(request_line)
        if (match == None):
            writer.write('HTTP/1.0 400 Bad Request\r\nContent-length: 0\r\n\r\n')
        else:        
            request = str(request_line[4:17])
            request = request[2:]
            method = match.group(1).lower()
            path = match.group(2).lower()
            querystring = match.group(3)
            if (querystring == None): querystring = ""
            if path != b'/favicon.ico':
                print('request: ' + str(method) + ' ' + str(path) + str(querystring))
            #print(list(headers))
            if (method == b''):
                print('post request')
                writer.write('HTTP/1.0 404 Not Found\r\nContent-length: 0\r\n\r\n')
            elif (method == b'get'):
                if path == b'/favicon.ico':
                    writer.write('HTTP/1.0 200 OK\r\nContent-type: image/x-icon\r\nContent-length: 0\r\nCache-Control: private, max-age: 86400\r\n\r\n')
                elif path == b'/':
                    page = Index()
                    writer.write('HTTP/1.0 200 OK\r\nContent-type: text/html\r\nCache-Control: no-cache\r\n\r\n')
                    writer.write(page)
                elif path == b'/status':
                    writer.write('HTTP/1.0 200 OK\r\nContent-type: application/json\r\nCache-Control: no-cache\r\n\r\n')
                    writer.write(json.dumps({
                        "lightsOn":lightsOn,
                        "growlightOn":growLightOn,
                        "lights":lights,
                        "schedule":schedule,
                        "growlightDelay": growlightDelay,
                        "lightsDelay": lightsDelay,
                        "waterTemp": waterTemp,
                        "ambient": round(ambientSensor.read_u16()/65535*100)
                    }))
                elif path == b'/log/temp' or path == b'/log/temp/on':
                    logTemp = True
                elif path == b'/log/temp/off':
                    logTemp = False
                elif path == b'/log/schedule' or path == b'/log/schedule/on':
                    logSchedule = True
                elif path == b'/log/schedule/off':
                    logSchedule = False
                elif (path.startswith(b'/grow-light/on')):
                    if (growLightOn == False):
                        growlightDelay = max(time.time() + DEFAULT_GROWLIGHT_DELAY, schedule["growlightOff"])
                        #printTime("growligth delay default: ", time.time() + DEFAULT_GROWLIGHT_DELAY)
                        #printTime("growlight next off: ", schedule["growlightOff"])
                        printTime("growlight will turn off",  growlightDelay)
                    
                    growLightOn = True                    
                    writer.write('HTTP/1.0 200 OK\r\nContent-type: text/html\r\nCache-Control: no-cache\r\n\r\n')
                elif (path.startswith(b'/grow-light/off')):
                    if (growLightOn == True):
                        growlightDelay = max(time.time() + DEFAULT_GROWLIGHT_DELAY, schedule["growlightOn"])
                        #printTime("growligth delay default: ", time.time() + DEFAULT_GROWLIGHT_DELAY)
                        #printTime("growlight next on: ", schedule["growlightOn"])
                        printTime("growlight will turn on",  growlightDelay)
                        
                    growLightOn = False
                    writer.write('HTTP/1.0 200 OK\r\nContent-type: text/html\r\nCache-Control: no-cache\r\n\r\n')
                elif (path.startswith(b'/grow-light')):
                    writer.write('HTTP/1.0 200 OK\r\nContent-type: plain/text\r\nCache-Control: no-cache\r\n\r\n')
                    writer.write(str(growLightOn))
                elif (path.startswith(b'/lights/on')):
                    if (anyLightsOn() == False):
                        for i in range(PIXEL_COUNT):
                            lights[i] = (255, 255, 255, 255)
                        print('saving settings')
                        settings["lights"] = lights
                        _settings.saveSettings(settings)

                    if (time.time() + DEFAULT_LIGHTS_DELAY > schedule["lightsOn"] and time.time() + DEFAULT_LIGHTS_DELAY < schedule["lightsOff"]):
                        lightsDelay = schedule["lightsOff"]
                    else:
                        lightsDelay = min(time.time() + DEFAULT_LIGHTS_DELAY, schedule["lightsOn"])
                    printTime("light will turn off",  lightsDelay)
                    lightsUpdated = True
                    
                    writer.write('HTTP/1.0 200 OK\r\nContent-type: text/html\r\nCache-Control: no-cache\r\n\r\n')
                elif (path.startswith(b'/lights/off')):
                    lightsForceOff = True
                    if (time.time() + DEFAULT_LIGHTS_DELAY < schedule["lightsOn"] or time.time() + DEFAULT_LIGHTS_DELAY > schedule["lightsOff"]):
                        lightsDelay = schedule["lightsOff"]
                    else:
                        lightsDelay = min(time.time() + DEFAULT_LIGHTS_DELAY, schedule["lightsOn"])
                    printTime("light will turn on",  lightsDelay)
                    lightsUpdated = True
                    writer.write('HTTP/1.0 200 OK\r\nContent-type: text/html\r\nCache-Control: no-cache\r\n\r\n')
                elif (path.startswith(b'/lights/brightness')):
                    c = colorBrightnessParser.match(path)
                    if (c != None):
                        for i in range(PIXEL_COUNT):
                            lights[i] = (lights[i][0], lights[i][1], lights[i][2], int(c.group(1)))

                        lightsUpdated = True
                    writer.write('HTTP/1.0 200 OK\r\nContent-type: text/html\r\nCache-Control: no-cache\r\n\r\n')
                elif (path.startswith(b'/lights/')):
                    print('set')
                    # 00aabbccdd (index)(red)(blue)(green)(alpha/brightness)
                    c = colorParser.match(path)
                    c2 = colorRangeParser.match(path) 
                    c3 = colorAllParser.match(path)
                    
                    if (c != None):
                        idx = min(PIXEL_COUNT-1,max(0,int(c.group(1))))
                        r = min(255,max(0,int(c.group(2),16)))
                        g = min(255,max(0,int(c.group(3),16)))
                        b = min(255,max(0,int(c.group(4),16)))
                        a = 255
                        if (c.group(5) != None): a = min(255,max(0,int(c.group(5),16)))
                        print('lights[' + str(idx) + '] = (' + str(r) + ',' + str(g) + ',' + str(b) + ') b: ', a)
                        lights[idx] = (r, g, b, a)
                        lightsUpdated = True
                        print('saving settings')
                        settings["lights"] = lights
                        _settings.saveSettings(settings)

                        writer.write('HTTP/1.0 200 OK\r\nContent-type: text/html\r\nCache-Control: no-cache\r\n\r\n')
                    elif c2 != None:
                        c=c2
                        sidx = min(PIXEL_COUNT-1,max(0,int(c.group(1))))
                        eidx = min(PIXEL_COUNT-1,max(0,int(c.group(2))))
                        if (sidx > eidx):
                            tmp = eidx
                            eidx = sidx
                            sidx = tmp
                        r = min(255,max(0,int(c.group(3),16)))
                        g = min(255,max(0,int(c.group(4),16)))
                        b = min(255,max(0,int(c.group(5),16)))
                        a = 255
                        if (c.group(6) != None): a = min(255,max(0,int(c.group(6),16)))
                        print('lights[' + str(sidx) + ':' + str(eidx) + '] = (' + str(r) + ',' + str(g) + ',' + str(b) + ') b: ', a)
                        for i in range(sidx, eidx+1):
                            lights[i] = (r, g, b, a)
                        #lights[sidx:eidx] = (r, g, b, a)
                        print('saving settings')
                        settings["lights"] = lights
                        _settings.saveSettings(settings)
                        lightsUpdated = True
                        writer.write('HTTP/1.0 200 OK\r\nContent-type: text/html\r\nCache-Control: no-cache\r\n\r\n')                    
                    elif c3 != None:
                        c = c3
                        r = min(255,max(0,int(c.group(1),16)))
                        g = min(255,max(0,int(c.group(2),16)))
                        b = min(255,max(0,int(c.group(3),16)))
                        a = 255
                        if (c.group(4) != None): a = min(255,max(0,int(c.group(4),16)))
                        print('lights[:] = (' + str(r) + ',' + str(g) + ',' + str(b) + ') b: ', a)
                        for i in range(PIXEL_COUNT):
                            lights[i] = (r, g, b, a)
                        #lights[:] = (r, g, b, a)
                        print('saving settings')
                        settings["lights"] = lights
                        _settings.saveSettings(settings)
                        lightsUpdated = True
                        writer.write('HTTP/1.0 200 OK\r\nContent-type: text/html\r\nCache-Control: no-cache\r\n\r\n')                    
                    else:
                        writer.write('HTTP/1.0 404 Not Found\r\nContent-length: 0\r\n\r\n')
                else:
                    writer.write('HTTP/1.0 404 Not Found\r\nContent-length: 0\r\n\r\n')
            else:
                writer.write('HTTP/1.0 404 Not Found\r\nContent-length: 0\r\n\r\n')
    except Exception as e:
        sys.print_exception(e)
        gc.collect()
        writer.write('HTTP/1.0 500 internal server error\r\nContent-length: 0\r\n\r\n')
        
    await writer.drain()
    await writer.wait_closed()
    #await asyncio.sleep(0.75)
    #print("Disconnected", request )
    onboard_led.value(0)
    
def webserver_done():
    global ws_launched
    ws_launched = False
    print( "webserver done" )
        
def launch_webserver():
    global ws_launched
    global serverOb
    global myIp
    if wlan.status() != 3:
      print( "Will not launch webserver in wlan.status:", wlan.status() )
    else:  
      serverOb = asyncio.start_server(serve_client, "0.0.0.0", 80)
      asyncio.create_task( serverOb )
      ws_launched = True
      print( "webserver launched" )

def anyLightsOn():
    anyOn = False
    for i in range(PIXEL_COUNT):
        val = lights[i]
        if (val[0] > 0 or val[1] > 0 or val[2] > 0):
            anyOn = True
            break
    return anyOn

def getSchedule(sunrise, sunset):
    now = time.gmtime()
    date = time.mktime((now[0],now[1],now[2],0,0,0,now[6],now[7]))
    nextDay = date + (60*60*(24+4)) # 4am
    localSunrise = toLocaltime(sunrise)
    localNow = toLocaltime(now)
    localDate = time.mktime((localSunrise[0],localSunrise[1],localSunrise[2],0,0,0,localSunrise[6],localSunrise[7]))
    printTime("utc time:", now)
    printTime("utc date:", date)
    printTime("local time:", localNow)
    printTime("local date:", localDate)
    
    growlight_AbsoluteOff_local = localDate + settings["growlight-max-offset"]
    growlight_AbsoluteOff_UTC = toUtctime(localDate + settings["growlight-max-offset"])
    growlight_TimeOn = time.mktime(sunrise) + settings["growlight-sunrise-offset"]
    growlight_TimeOff = min(growlight_AbsoluteOff_UTC, time.mktime(sunset) + settings["growlight-sunset-offset"])
    
    lights_AbsoluteOff_local = localDate + settings["lights-max-offset"]
    lights_AbsoluteOff_UTC = toUtctime(localDate + settings["lights-max-offset"])
    lights_TimeOn = time.mktime(sunrise) + settings["lights-sunrise-offset"]
    lights_TimeOff = min(lights_AbsoluteOff_UTC, time.mktime(sunset) + settings["lights-sunset-offset"])
    
    printTime('sunrise utc', sunrise)
    printTime('sunset utc', sunset)
    printTime('sunrise ', toLocaltime(sunrise))
    printTime('sunset ', toLocaltime(sunset))
    
    printTime('growlight off (suggested): ', time.mktime(sunset) + settings["growlight-sunset-offset"])
    printTime('growlight off (absolute):  ', growlight_AbsoluteOff_local)
    printTime( '   utc: ', growlight_AbsoluteOff_UTC)
    printTime('growlight on: ', toLocaltime(time.gmtime(growlight_TimeOn)))
    printTime('growlight off: ', toLocaltime(time.gmtime(growlight_TimeOff)))

    printHours('lights off (suggested): ', time.mktime(sunset) + settings["lights-sunset-offset"])
    printHours('lights off (absolute):  ', lights_AbsoluteOff_local)
    printTime( '   utc: ', lights_AbsoluteOff_UTC)
    printTime( 'lights on: ', toLocaltime(time.gmtime(lights_TimeOn)))
    printTime( 'lights off: ', toLocaltime(time.gmtime(lights_TimeOff)))    
    
    return {
        'lightsOn': lights_TimeOn,
        'lightsOff': lights_TimeOff,
        'growlightOn': growlight_TimeOn,
        'growlightOff': growlight_TimeOff,
        'nextCheck': nextDay
    }
    

async def lighting():
    global shutdown
    global lightsUpdated
    global growLightOn
    global lightsOn
    global schedule
    global growlightDelay
    global lightsForceOff
    global lightsDelay
    print('starting lighting')
    lastGLOnState = None
    counter = 0
    now = time.gmtime()
    localNow = toLocaltime(now)
    date = time.mktime((now[0],now[1],now[2],0,0,0,now[6],now[7]))
    localDate = time.mktime((localNow[0],localNow[1],localNow[2],0,0,0,localNow[6],localNow[7]))
    
    printTime('Date', time.gmtime(localDate))
    #datetime.datetime(*time.localtime())
    ld = localNow
    dt = datetime.datetime(*(localNow[0],localNow[1],localNow[2],0,0,0,localNow[6],localNow[7]))
    sunrise = sunlight.sunrise(dt)
    sunset = sunlight.sunset(dt)
    
    if (now > sunset):
        print('moving sunset to next day.')
        sunrise = sunlight.sunrise()
        sunset = sunlight.sunset()
        
    
    schedule = getSchedule(sunrise, sunset)
    nextCheck = schedule["nextCheck"]
    printTime("next check", nextCheck)
    printGLDelay = True
    first = True
    
    lightsNow = [None]*PIXEL_COUNT
    for i in range(PIXEL_COUNT):
        lightsNow[i] = lights[i]
    while shutdown == False:
        try:
            now = time.gmtime()
            nowSec = time.time()
            for i in range(PIXEL_COUNT):
                lightsNow[i] = lights[i]
                
            #printTime('now: ', nowSec)
            if (nowSec >= nextCheck):
                sunrise = sunlight.sunrise(datetime.datetime(*(now[0],now[1],now[2],12,0,0,now[6],now[7])))
                sunset = sunlight.sunset(datetime.datetime(*(now[0],now[1],now[2],12,0,0,now[6],now[7])))
                schedule = getSchedule(sunrise, sunset)
                nextCheck = schedule["nextCheck"]
                printTime("next check", nextCheck)
                            
            if ((first or lightsOn == False) and nowSec > lightsDelay and nowSec >= schedule["lightsOn"] and nowSec <= schedule["lightsOff"]):
                print('schedule: lights ON')
                try:                   
                    for i in range(PIXEL_COUNT):
                       cl = settings["lights"][i]
                       lightsNow[i] = (cl[0],cl[1],cl[2],cl[3])
                    if (anyLightsOn() == False):
                        print("no lights on. turning on")
                        for i in range(PIXEL_COUNT):
                            lightsNow[i] = (255,255,255,255)
                            lights[i] = (255,255,255,255)
                        print('saving settings')
                        settings["lights"] = lights
                        _settings.saveSettings(settings)
                except:
                    print('error: turning all lights on')
                    for i in range(PIXEL_COUNT):
                        lightsNow[i] = (255,255,255,255)
                        lights[i] = (255,255,255,255)
                    print('saving settings')
                    settings["lights"] = lights
                    _settings.saveSettings(settings)
                   
                lightsUpdated = True
            elif ((first or lightsOn == True) and nowSec > lightsDelay and (nowSec < schedule["lightsOn"] or nowSec >= schedule["lightsOff"])):
                print('schedule: lights OFF')
                for i in range(PIXEL_COUNT):
                    cl = settings["nightlights"][i]
                    lightsNow[i] = (cl[0],cl[1],cl[2],cl[3])
                lightsUpdated = True
            elif lightsForceOff:
                print('force: lights OFF')
                for i in range(PIXEL_COUNT):
                   lightsNow[i] = (0,0,0,0)
                lightsUpdated = True
            else:
                for i in range(PIXEL_COUNT):
                    cl = lights[i]
                    lightsNow[i] = (cl[0],cl[1],cl[2],cl[3])
             
            lightsForceOff = False
            if (nowSec > growlightDelay and (first or growLightOn == False) and nowSec >= schedule["growlightOn"] and nowSec <= schedule["growlightOff"]):
                growLightOn = True
                printGLDelay = True
                print('schedule: growlight ON', nowSec, growlightDelay)
            elif (nowSec > growlightDelay and (first or growLightOn == True) and nowSec >= schedule["growlightOff"]):
                growLightOn = False
                printGLDelay = True
                print('schedule: growlight OFF', nowSec, growlightDelay)
            #elif nowSec <= growlightDelay:
            #    if (printGLDelay):
            #        toff = "turn on"
            #        if (growLightOn): toff = "turn off"
            #        printTime("growlight will " + toff + " at: ", growlightDelay)
            #        printGLDelay = False
                
                
            
            if (lightsUpdated == True):
                print('updating lights')
                lightsOn = False
                for i in range(PIXEL_COUNT):
                    val = lightsNow[i]
                    #print("setting light value", i, val)
                    strip.brightness(val[3])
                    strip[i] = (val[0],val[1],val[2])
                    if (lightsOn == False and ((val[0] > 0 or val[1] > 0 or val[2] > 0) and val[3] > 0)): lightsOn = True
                lightsUpdated = False
                strip.show()
                if lightsOn: print("lights: ON")
                else: print("lights: OFF")
                
            if (growLightOn != lastGLOnState):
                print("updating growlight")
                if (growLightOn == True):
                    print('growlight: ON')
                    growLight.value(1)
                else:
                    print('growlight: OFF')
                    growLight.value(0)
                lastGLOnState = growLightOn
            first = False
        except Exception as e:
            sys.print_exception(e)
            await asyncio.sleep(5)
        await asyncio.sleep(1)

async def ProcessOnboardTemperature():
    global onboardTemp
    global growLightOn
    global lightsOn
    temps = []
    print('starting onboard temp')
    while shutdown == False:
        offset = 0
        if (growLightOn): offset = offset + 0.05
        if (lightsOn): offset = offset + 0.15
        reading = onboard_temp.read_u16() * ((2.963 + offset) / (65536))
        temperature_celcius = (27 - (reading - 0.706)/0.001721)  * 9 / 5 + 32
        temps.append(temperature_celcius)
        if (len(temps) == 10):
            onboardTemp = (sum(temps)/len(temps))
            temps = []        
        await asyncio.sleep(0.2)

async def ProcessTemperature():
    global settings
    global waterTemp
    try:
        print('starting temperature')
        roms = ds_sensor.scan()
        print('Found Temperature devices')
        printError = 0
        seconds = 0
        errors = 0
        first = True
        while shutdown == False:
            try:
                if (seconds % 60) == 0:
                    ds_sensor.convert_temp()
                    await asyncio.sleep(0.75)
                    tempF = -404
                    for rom in roms:
                        #print(rom)
                        tempC = ds_sensor.read_temp(rom)
                        tempF = tempC * (9/5) +32
                        #print('temperature (ºC):', "{:.2f}".format(tempC))
                    url = settings["temperature-url"]
                    if url==None or url == "":
                        print('temperature (ºF):', "{:.2f}".format(tempF))
                    elif wlan.status() != 3:
                        print('(no wifi) temperature (ºF):', "{:.2f}".format(tempF))
                    elif tempF > 10 and tempF < 100:
                        
                        waterTemp = tempF
                        if (logTemp or first): print('uploading temperature (ºF):', "{:.2f}".format(tempF))
                        payload = {'temperature':tempF,'humidity':100,'pressure':1038}
                        headers = {'Content-Type':'application/json'}
                        data = (json.dumps(payload)).encode()
                        request_url = settings["temperature-url"]
                        r = urequests.post(request_url, data=data, headers=headers)
                        data = ''
                        
                        if (logTemp or first): print('saving temperature: ', r.status_code)
                        r.close()
                        r = None
                        gc.collect()
                        if (printError > 0):
                            print('restored/saved: temperature (ºF):', "{:.2f}".format(tempF))
                        printError = 0
                        errors = 0
                    
            except Exception as e:
                errors = errors + 1
                gc.collect()
                if (printError % 4 == 0):
                    print('failed getting temperature', printError)
                    sys.print_exception(e)
                printError += 1
                seconds = 45
                if (errors > 5):
                    machine.soft_reset()
                roms = ds_sensor.scan()
            seconds += 1
            if (seconds > 60): seconds = 1
            await asyncio.sleep(1)
            first = False
    except Exception as e:
        print('failed getting temp sensor')
        sys.print_exception(e)

async def main():
    print("main() starting")
    global counter
    global shutdown
    global wifi_connected
    global ws_launched
    global serverOb
    global myIp
    wifi_connected = False
    ws_launched = False
    myIp = "."
    counter  = 0
    shutdown = False
    from rp2 import country
    country('US')
    await asyncio.sleep(0.1)
    print("main() loop starting")
    asyncio.create_task(lighting())
    asyncio.create_task(ProcessTemperature())
    asyncio.create_task(ProcessOnboardTemperature())
    while shutdown == False:
      #print('tick')
      if wifi_connected == False:
        print("connecting to wifi")
        await connect_wifi()
      
      if ws_launched == False:
        if wlan.status() == 3:
          print("starting webserver")
          serverOb = asyncio.start_server(serve_client, "0.0.0.0", 80)
          asyncio.create_task( serverOb )
          ws_launched = True
          print("webserver started")
            
      await asyncio.sleep(0)
    try:
      print('shutting down webserver...')

      serverOb.close()
      serverOb.wait_closed()
      print('webserver stopped')
    finally:
      shutdown = True
      print("main() exiting")
      
      await asyncio.sleep(2.0) # let core1 exit first
      onboard_led.value(0)
       
        
try:
  asyncio.run(main())

finally:
  asyncio.new_event_loop()        
        

