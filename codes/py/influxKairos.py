#!/usr/bin/env python3

from influxdb import InfluxDBClient
import paho.mqtt.client as mqtt
import time
import logging
import argparse

logFilename = '/home/pi/influxKairos-' + time.strftime('%Y%m%d-%H-%M-%S') + '.log'
logging.basicConfig(filename=logFilename,level=logging.INFO)

class Data:
    """Class for storing MQQT data with timestamp"""
    def __init__(self, database, cloud_broker):        
        self.broker = cloud_broker
        self.database = database
        self.timestamp = 0
        self.dbdata = [
        {
            "measurement": "temp",
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

    def add_record(self, measurement, data, error):
        self.dbdata[0]['measurement'] = measurement
        self.dbdata[0]['fields']['Float_value'] = data
        self.dbdata[0]['fields']['error'] = error
        self.timestamp = time.time()
     
        self.database.write_points(self.dbdata)

        print(self.dbdata)


    def set_error(self, error):
        self.dbdata[0]['fields']['error'] = error

    def is_new(self, timestamp):
        if self.timestamp == timestamp:
            return False
        else:
            return True

def on_connect(client, userdata, flags, rc):
    
    logging.warning('Connected to localhost with result code: ' + str(rc) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
    print('Connected to localhost with result code ' + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    # client.subscribe("$SYS/#")
    
    client.subscribe('tanque/#')

def on_connect_cloud(client, userdata, flags, rc):
    
    logging.warning('Connected to cloud with result code: ' + str(rc) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
    print('Connected to cloud with result code ' + str(rc))

def on_disconnect(client, userdata, rc):
    if rc != 0:
        print('Unexpected disconnection: ' + str(rc) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
        logging.warning('Unexpected disconnection: ' + str(rc) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S'))

    # client.reconnect()
        
        
def on_message(client, userdata, message):
    #global temperature# = Data()
   
    # Publish messages on the cloud
    ret = userdata.broker.publish(message.topic, message.payload)

    # Add values to the databse
    topic_split = str(message.topic).split('/')
    payload_split = list(message.payload.decode('ASCII'))
    data_value = (''.join(payload_split).split(';',1))

    measurement = ''.join(topic_split)
    userdata.add_record(measurement,float(data_value[0]), int(data_value[1]))    
    
def main():
    
    myData = []
   
    timestamp = []
    timestamp.append(0)
    timestamp.append(0)
    timestamp.append(0)
    
    parser = argparse.ArgumentParser(description='Simple mqtt to influx gateWay')
    parser.add_argument('broker', help='Broker address')
    parser.add_argument('user', help='Broker user')
    parser.add_argument('password', help='Broker password')
    parser.add_argument('DBuser', help='InfluxDB user')
    parser.add_argument('DBpassword', help='InfluxDB password')

    options = parser.parse_args()
        
    print('Connecting to {} with user {}.'.format(options.broker, options.user))
    logging.info('Connecting to {} with user {}.'.format(options.broker, options.user))
    
    cloud_mqtt = mqtt.Client()
    cloud_mqtt.username_pw_set('juca',  'password')
    # cloud_mqtt.connect('m14.cloudmqtt.com')
    cloud_mqtt.connect('192.168.1.2')
    cloud_mqtt.on_connect = on_connect_cloud
  
    local_mqtt = mqtt.Client()
    local_mqtt.username_pw_set(options.user,  options.password)
    local_mqtt.connect(options.broker)    
    local_mqtt.on_disconnect = on_disconnect
    local_mqtt.on_message = on_message
    local_mqtt.on_connect = on_connect
    
    client = InfluxDBClient('localhost', 8086, options.DBuser, options.DBpassword)
    client.switch_database('kairos')

    data = Data(client, cloud_mqtt)
    local_mqtt.user_data_set(data)

    local_mqtt.loop_start()
    cloud_mqtt.loop_start()

    
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

#            i = 0
#
#            for data in myData:
#                if (not (data.is_new(timestamp[i]))):
#                    print('Failed writing fresh data:' + str(i) + ' ' + time.strftime('%Y-%m-%dT%H:%M:%S') )
#                    logging.warning('Failed writing fresh data: ' + time.strftime('%Y-%m-%dT%H:%M:%S'))
#                    data.set_error(9)
#
#                # print(data.dbdata)
#                client.write_points(data.dbdata)
#
#                timestamp[i] = data.timestamp
#                i = i + 1            
               
    except KeyboardInterrupt:
        print('Ending...')
        local_mqtt.disconnect()
        client.close()
         
            
if __name__ == '__main__':
    main()