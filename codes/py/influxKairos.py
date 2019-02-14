#!/usr/bin/env python3

from influxdb import InfluxDBClient
import paho.mqtt.client as mqtt
import time
import logging
import argparse

logFilename = '/home/starke/influxKairos-' + time.strftime('%Y%m%d-%H-%M-%S') + '.log'
logging.basicConfig(filename=logFilename,level=logging.DEBUG)

class Data:
    """Class for storing MQQT data with timestamp"""
    def __init__(self):
        self.value = 0.0
        self.timestamp = 0

    def add_value(self, data):
        self.value = data
        self.timestamp = time.time()

    def is_new(self, timestamp):
        if self.timestamp == timestamp:
            return False
        else:
            return True

def on_connect(client, userdata, flags, rc):
    
    print('Connected with result code ' + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    # client.subscribe("$SYS/#")  

def on_disconnect(client, userdata, rc):
    if rc != 0:
        print('Unexpected disconnection: ' + str(rc) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
        logging.warning('Unexpected disconnection: ' + str(rc) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
        
def on_message(client, userdata, message):
    #global temperature# = Data()
        
    data_list = list(message.payload.decode('ASCII'))
           
    while True:
        try:
            data_list.remove('\x00')
        except ValueError:
            break
    
    #temperature.add_value(float(''.join(data_list)))
    userdata.add_value(float(''.join(data_list)))
    # print(float(''.join(data_list)))
    
def main():
    json_body = [
        {
            "measurement": "k2_temp",
            "tags": {                
                "region": "florianopolis"
            },
            #"time": "2018-11-18T09:35:00Z",
            "fields": {
                "Float_value": 0.64,
                #"value": 23.0
            }
        }
    ]

    myData = Data()
    timestamp = 0
    
    parser = argparse.ArgumentParser(description='Simple mqtt to influx gateWay')
    parser.add_argument('broker', help='Broker address')
    parser.add_argument('user', help='Broker user')
    parser.add_argument('password', help='Broker password')
    parser.add_argument('DBuser', help='InfluxDB user')
    parser.add_argument('DBpassword', help='InfluxDB password')

    options = parser.parse_args();
        
    print('Connecting to {} with user {}.'.format(options.broker, options.user))
    logging.info('Connecting to {} with user {}.'.format(options.broker, options.user))
        
    mqttc = mqtt.Client()
    mqttc.username_pw_set(options.user,  options.password)
    mqttc.connect(options.broker)
 
    mqttc.loop_start()
    mqttc.on_disconnect = on_disconnect
    mqttc.on_message = on_message
    mqttc.on_connect = on_connect
    mqttc.user_data_set(myData)

    mqttc.subscribe("k2/temperatura")
    logging.info('Subscribing to: ' + 'k2/temperatura')

    client = InfluxDBClient('localhost', 8086, options.DBuser, options.DBpassword)
    client.switch_database('kairos')
    
    
    try:
        while True:
            # Check DST for influx timestamp
            if (time.daylight):     
                timezoneinfo = time.tzname[1]
            else:
                timezoneinfo = time.tzname[0]
 
            # print(myData.value)
            # print(myData.timestamp)
            # print('----')
                 
            time.sleep(30)
            json_body[0]['fields']['Float_value'] = float(myData.value)
            json_body[0]['time'] = time.strftime('%Y-%m-%dT%H:%M:%S') + str(timezoneinfo) + ':00'

            if (myData.is_new(timestamp)):            
                # print(json_body)
                client.write_points(json_body)
            else:
                # print('Failed writine fresh data: ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
                logging.warning('Failed writine fresh data: ' + time.strftime('%Y-%m-%dT%H:%M:%S'))

            timestamp = myData.timestamp          
            
               
    except KeyboardInterrupt:
        print('Ending...')
        mqttc.disconnect()
        client.close()
         
            
if __name__ == '__main__':
    main()

