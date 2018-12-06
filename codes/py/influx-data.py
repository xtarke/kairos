from influxdb import InfluxDBClient
import paho.mqtt.client as mqtt
import time

luz = 0

def on_disconnect(client, userdata, rc):
    if rc != 0:
        print("Unexpected disconnection.")
        
def on_message_print(client, userdata, message):
    global luz
 
    # print("%s %s" % (message.topic, message.payload))    
    
    luz = str(int(message.payload))
    print(luz)

    
def main():
    json_body = [
        {
            "measurement": "luz",
            "tags": {                
                "region": "florianopolis"
            },
            "time": "2018-11-18T09:35:00Z",
            "fields": {                
                "value": 23.0
            }
        }
    ]
    mqttc = mqtt.Client()

    mqttc.connect("150.162.29.60")
    mqttc.loop_start()
    mqttc.on_disconnect = on_disconnect
    mqttc.on_message = on_message_print

    mqttc.subscribe("test/luz")
    
    print(json_body)

    client = InfluxDBClient(host='localhost', port=8086)
    client.switch_database('solartracker')
       
    try:
        while True:
            time.sleep(10)
            json_body[0]['fields']['Float_value'] = luz
            json_body[0]['time'] = time.strftime("%Y-%m-%dT%H:%M:%SZ")
            
            # print(json_body)
            
            client.write_points(json_body)
            
               
    except KeyboardInterrupt:
        print('Ending...')
        mqttc.disconnect()
        client.close()
         
            
if __name__ == '__main__':
    main()



