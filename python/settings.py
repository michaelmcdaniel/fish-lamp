import time
import json


def getSettings():
    
    try:
        with open('config.json', 'r') as f:
            settings = json.load(f)
    except:
        settings = {}
        settings['createDate'] = time.time()
        settings['modDate'] = time.time()
        saveSettings(settings)
    return settings

def saveSettings(settings):
    try:
        settings['modDate'] = time.time()
        with open('config.json', 'w') as f:
            json.dump(settings, f)
    except Exception as e:
        print('save failed: ' + str(e))


if __name__ == "__main__":
    settings = getSettings()
    print(str(settings))
    saveSettings(settings)
    