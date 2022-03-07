#
# Copyright 2022 Renan Augusto Starke
#
# Record all data from mqtt to influxDB 
#
#!/usr/bin/env python3


import time
import paho.mqtt.client as paho
import argparse
from paho import mqtt

from influxdb import InfluxDBClient


class Data:
    """Class for storing MQQT data with timestamp"""
    def __init__(self, database):        
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

        # print(self.dbdata)


    def set_error(self, error):
        self.dbdata[0]['fields']['error'] = error


# setting callbacks for different events to see if it works, print the message etc.
def on_connect(client, userdata, flags, rc, properties=None):
    print("CONNACK received with code %s." % rc)
    
    client.subscribe('tanque/#')

# with this callback you can see if your publish was successful
def on_publish(client, userdata, mid, properties=None):
    print("mid: " + str(mid))

# print which topic was subscribed to
def on_subscribe(client, userdata, mid, granted_qos, properties=None):
    print("Subscribed: " + str(mid) + " " + str(granted_qos))

# print message, useful for checking if it was successful
def on_message(client, userdata, msg):
    # print(msg.topic + " " + str(msg.qos) + " " + str(msg.payload))

    topic_split = str(msg.topic).split('/')
    payload_split = list(msg.payload.decode('ASCII'))
    data_value = (''.join(payload_split).split(';',1))

    measurement = ''.join(topic_split)
    
    userdata.add_record(measurement,float(data_value[0]), int(data_value[1]))  



def main():
    
    parser = argparse.ArgumentParser(description='Simple mqtt to influx gateWay')    
    parser.add_argument('Cloudpassword', help='Broker password')    
    parser.add_argument('DBpassword', help='InfluxDB password')

    options = parser.parse_args()
    
    dbclient = InfluxDBClient('localhost', 8086, 'kairos', options.DBpassword)
    dbclient.switch_database('kairosDB')
        
    # using MQTT version 5 here, for 3.1.1: MQTTv311, 3.1: MQTTv31
    # userdata is user defined data of any type, updated by user_data_set()
    # client_id is the given name of the client
    client = paho.Client(client_id="", userdata=None, protocol=paho.MQTTv5)
    client.on_connect = on_connect

    # enable TLS for secure connection
    client.tls_set(tls_version=mqtt.client.ssl.PROTOCOL_TLS)
    # set username and password
    client.username_pw_set("kairos", options.Cloudpassword)
    # connect to HiveMQ Cloud on port 8883 (default for MQTT)
    client.connect("2a447abee88546fab229e97127448b10.s1.eu.hivemq.cloud", 8883)

    # setting callbacks, use separate functions like above for better visibility
    client.on_subscribe = on_subscribe
    client.on_message = on_message
    client.on_publish = on_publish

    data = Data(dbclient)
    client.user_data_set(data)

    # loop_forever for simplicity, here you need to stop the loop manually
    # you can also use loop_start and loop_stop
    client.loop_forever()


if __name__ == '__main__':
    main()

