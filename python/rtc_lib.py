from machine import Pin, I2C
import utime
import binascii

class ds3231(object):
#            13:45:00 Mon 24 May 2021
#  the register value is the binary-coded decimal (BCD) format
#               sec min hour week day month year

    NowTime = b'\x00\x45\x13\x02\x24\x05\x21'
    w  = ["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];
    m  = [31,28,31,30,31,30,31,31,30,31,30,31]
    lym  = [31,27,31,30,31,30,31,31,30,31,30,31]
    address = 0x68
    start_reg = 0x00
    alarm1_reg = 0x07
    control_reg = 0x0e
    status_reg = 0x0f
    
    def __init__(self,i2c_scl=0,i2c_sda=1,i2c_port=0):
        self.bus = I2C(i2c_port,scl=Pin(i2c_scl),sda=Pin(i2c_sda))

    def set_time(self,new_time):
        #"14:40:00,Tuesday,2024-01-30"
        hour = new_time[0] + new_time[1]
        minute = new_time[3] + new_time[4]
        second = new_time[6] + new_time[7]
        wd = new_time.split(",",2)[1]
        week = "0" + str(self.w.index(wd)+1)
        dt = new_time.split(",",2)[2]
        year = dt[2] + dt[3]
        month = dt[5] + dt[6]
        day = dt[8] + dt[9]
        #print((second + " " + minute + " " + hour + " " + week + " " + day + " " + month + " " + year).replace(' ',''))
        now_time = binascii.unhexlify((second + " " + minute + " " + hour + " " + week + " " + day + " " + month + " " + year).replace(' ',''))
        #print(binascii.unhexlify((second + " " + minute + " " + hour + " " + week + " " + day + " " + month + " " + year).replace(' ','')))
        #print(self.NowTime)
        self.bus.writeto_mem(int(self.address),int(self.start_reg),now_time)
        
    def IsDst(self, month, day, dayofweek, hour):
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
            while(wd != 0 or wd != 7):
                dstd = dstd + 1
                wd = wd + 1
            return day > dstd or (day == dstd and hour > 1)
        while(wd < 7):
            dstd = dstd + 1
            wd = wd + 1
        return day < dstd or (day == dstd and hour < 2)
    
    def set_utc(self,now,offset,dst):
        dstOffset = 0
        year = now[0]
        month = now[1]
        day = now[2]
        wd = now[3]
        h = now[4]
        m = now[5]
        s = now[6]
        if (dst == True and self.IsDst(month, day, wd, h) == True):
            dstOffset = -1
        if (h + offset + dstOffset < 0): # is prev day
            mds = self.m
            if (year % 4 == 0): msd = self.lym # leapyear
            if (day - 1 <= 0):
                if (month == 1): month = 12 # set to dec
                else: month = month-1
                day = mds[month] #set day to last day of month
            else:
                day = day - 1 #decrement day
            wd = wd - 1
            if (wd < 0): wd = 6 #decrement week day
            h = 24 + (h + offset + dstOffset)
        else:
            h = h + offset + dstOffset
        ds = repr("%02d:%02d:%02d,%s,%04d-%02d-%02d" %(h,m,s,self.w[wd],year,month,day))[1:-1]
        #print(ds)
        self.set_time(ds)
    
    def read_time(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),7)
        a = t[0]&0x7F  #second
        b = t[1]&0x7F  #minute
        c = t[2]&0x3F  #hour
        d = t[3]&0x07  #week
        e = t[4]&0x3F  #day
        f = t[5]&0x1F  #month
        #---------------------OLED PRINT----------------------------#
        return repr("20%x/%02x/%02x %02x:%02x:%02x" %(t[6],t[5],t[4],t[2],t[1],t[0]))[1:-1]
        #write15.text(repr("%s" %(self.w[t[3]-1])),10,8)             # Day
        #---------------------OLED PRINT----------------------------#
        
    def year(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),7)
        hi = int((t[6]&0x70)/16) * 10
        lo = t[6]&0x0F
        return hi + lo
        
    def second(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),7)
        hi = int((t[0]&0x70)/16) * 10
        lo = t[0]&0x0F
        return hi + lo
    
    def minute(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),7)
        hi = int((t[1]&0x70)/16) * 10
        lo = t[1]&0x0F
        return hi + lo
    
    def hour(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),7)
        hi = int((t[2]&0x30)/16) * 10
        lo = t[2]&0x0F
        return hi + lo
    
    def dayofweek(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),7)
        lo = t[3]&0x07
        return lo
    
    def day(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),7)
        hi = int((t[4]&0x30)/16) * 10
        lo = t[4]&0x0F
        return hi + lo
    
    def month(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),7)
        hi = int((t[5]&0x10)/16) * 10
        lo = t[5]&0x0F
        return hi + lo
    
    def day_name(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),7)
        return(self.w[t[3]-1])
        
    def temperature(self):
        t = self.bus.readfrom_mem(int(self.address),int(self.start_reg),19)
        whole = t[17]&0xFF        
        decimal = ((t[18]& 192)/64) *0.25
        temp = whole + decimal
        if (t[17]&0xFF) > 127:
            temp = temp * -1
        return temp
