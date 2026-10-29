import time
import datetime

class schedule:
    # action is function that is called when schedule goes into and out of effect with argument True = started, False = ended
    # if repeat == False
    #   start = start time in unix epoch
    #   end = end time in unix epoch
    # if repeat == True
    #   start = seconds since 00:00:00.000
    #   end = duration in seconds
    def __init__(self, start=None, end=None, action=None, repeat=False):
        self.action = action
        self.start = start
        self.end = end
        self.lastCheck = None
        self.repeat = repeat
        self.repeatOffset = 0
        
    #checks if schedule is active.  If it hasn't been called, then invokes action with True/False i.e. On Off
    #if last time is executed (action called with false) return true if the schedule is not a repeater
    def execute(self, time):
        start = self.start
        end = self.end
        if (self.repeat):
            now = time.gmtime(time.time())
            nowDate = time.mktime((now[0],now[1],now[2],0,0,0,now[6],now[7]))
            start = start + nowDate + self.repeatOffset
            end = end + start
        #need to account for start date is the same date, just later that duration.    
            
        if (self.lastCheck == None and time >= start and time < end):
            self.lastCheck = time.time()
            self.action(True)
        elif (self.lastcheck == None and (time < start or time >= end)):
            self.lastCheck = time.time()
            self.action(False)
            if (time >= end):
                if (self.repeat == False): return True
                ro = time.gmtime(end)
                self.repeatOffset = time.mktime((now[0],now[1],now[2],23,59,59,now[6],now[7])) + 1 - end
        elif (time >= start and time < self.lastCheck + start):
            self.lastCheck = time.time()
            self.action(True)
        else (time >= end and time < self.lastCheck + end):
            self.lastCheck = time.time()
            self.action(False)
            if (self.repeat == False): return True
            ro = time.gmtime(end)
            self.repeatOffset = time.mktime((now[0],now[1],now[2],23,59,59,now[6],now[7])) + 1 - end
        return False
    
class scheduler:
    def __init__():
        