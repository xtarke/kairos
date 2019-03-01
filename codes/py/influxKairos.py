#!/usr/bin/env python3

from influxdb import InfluxDBClient
import paho.mqtt.client as mqtt
import time
import logging
import argparse

logFilename = '/home/starke/influxKairos-' + time.strftime('%Y%m%d-%H-%M-%S') + '.log'
logging.basicConfig(filename=logFilename,level=logging.INFO)

class Data:
    """Class for storing MQQT data with timestamp"""
    def __init__(self, measurement):        
        self.timestamp = 0
        self.dbdata = [
        {
            "measurement": measurement,
            "tags": {                
                "region": "florianopolis"
            },            
            "fields": {
                "Float_value": 0.0,
                "error": 0
            }
        }
    ]

    def add_value(self, data, error):
        self.dbdata[0]['fields']['Float_value'] = data
        self.dbdata[0]['fields']['error'] = error
        self.timestamp = time.time()        

    def set_error(self, error):
        self.dbdata[0]['fields']['error'] = error

    def is_new(self, timestamp):
        if self.timestamp == timestamp:
            return False
        else:
            return True

def on_connect(client, userdata, flags, rc):
    
    logging.warning('Connected with result code: ' + str(rc) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
    print('Connected with result code ' + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    # client.subscribe("$SYS/#")

    client.subscribe('k2/temperatura')
    logging.info('Subscribing to: ' + 'k2/temperatura')

    client.subscribe('k58/temperatura/0')
    logging.info('Subscribing to: ' + 'k58/temperatura/0')

    client.subscribe('k58/temperatura/1')
    logging.info('Subscribing to: ' + 'k58/temperatura/1')

def on_disconnect(client, userdata, rc):
    if rc != 0:
        print('Unexpected disconnection: ' + str(rc) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
        logging.warning('Unexpected disconnection: ' + str(rc) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S'))

    # client.reconnect()
        
        
def on_message(client, userdata, message):
    #global temperature# = Data()
        
    if (message.topic == 'k2/temperatura'):
        data_list = list(message.payload.decode('ASCII'))
               
        while True:
            try:
                data_list.remove('\x00')
            except ValueError:
                break
        
        # temperature.add_value(float(''.join(data_list)))
        # print(float(''.join(data_list)))
        userdata[0].add_value(float(''.join(data_list)),0)
        

    if (message.topic == 'k58/temperatura/0'):
        data_list = list(message.payload.decode('ASCII'))
        
        while True:
            try:
                data_list.remove('\x00')
            except ValueError:
                break

        data = (''.join(data_list).split(';',1))
        
        #temperature.add_value(float(''.join(data_list)))
        userdata[1].add_value(float(data[0]),int(data[1]))
        # print(float(''.join(data[0])))

    if (message.topic == 'k58/temperatura/1'):
        data_list = list(message.payload.decode('ASCII'))
        
        while True:
            try:
                data_list.remove('\x00')
            except ValueError:
                break

        data = (''.join(data_list).split(';',1))
        
        #temperature.add_value(float(''.join(data_list)))
        userdata[2].add_value(float(data[0]),int(data[1]))
        #print(float(''.join(data_list)))
    
    
def main():
    
    myData = []
    myData.append(Data('k2_temp'))
    myData.append(Data('k58-0'))
    myData.append(Data('k58-1'))
    timestamp = []
    timestamp.append(0);
    timestamp.append(0);
    timestamp.append(0);
    
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

    client = InfluxDBClient('localhost', 8086, options.DBuser, options.DBpassword)
    client.switch_database('kairos')
    
    
    try:
        while True:
            # Check DST for influx timestamp
            #if (time.daylight):     
            #    timezoneinfo = time.tzname[1]
            #else:
            #    timezoneinfo = time.tzname[0]
 
            # print(myData.value)
            # print(myData.timestamp)
            # print('----')
                 
            time.sleep(10)
            # json_body[0]['fields']['Float_value'] = float(myData.value)
            # json_body[0]['time'] = time.strftime('%Y-%m-%dT%H:%M:%S') + str(timezoneinfo) + ':00'

            i = 0

            for data in myData:
                if (not (data.is_new(timestamp[i]))):
                    print('Failed writing fresh data:' + str(i) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S') )
                    logging.warning('Failed writing fresh data: ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
                    data.set_error(9)

                # print(data.dbdata)
                client.write_points(data.dbdata)

                timestamp[i] = data.timestamp
                i = i + 1
            
               
    except KeyboardInterrupt:
        print('Ending...')
        mqttc.disconnect()
        client.close()
         
            
if __name__ == '__main__':
    main()

