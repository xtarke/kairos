#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Thu Aug  2 12:52:42 2018

@author: Renan Augusto Starke
"""

import serial
import argparse
import paho.mqtt.client as mqtt

pkg = []

def do_sniff(serPort: serial, slave_address: int) -> int:
    """Sniff all bytes from the serial port."""
    temperature = 0
    not_valid = True
    
    while (not_valid):
        read = serPort.read()
        
        if (read[0] == slave_address):      
            if (len(pkg) > 5):
                temperature = (pkg[3] << 8) | pkg[4]
                not_valid = False
            pkg.clear()
                  
        pkg.append(read[0])
        
    return temperature
    
    
def main():
    parser = argparse.ArgumentParser(description='Modbus Simple Package Sniffer')
    parser.add_argument('port',
                        help='The serial port to read from')
    parser.add_argument('-b', '--baudrate', default=9600, type=int,
                        help='The baudrate to use for the serial port (defaults to %(default)s)')
    parser.add_argument('-a', '--address', default=7, type=int,
                        help='The slave address whose packages you want to sniff (defaults to %(default)s)')
    
    
    options = parser.parse_args();
    
    ser = serial.Serial(options.port, options.baudrate)                        
    print("Opened {} at {}. Slave address {}".format(options.port, options.baudrate, options.address))
    
    broker_address="localhost" 
    client = mqtt.Client("P1") #create new instance
    client.connect(broker_address) #connect to broker
        
    try:
         while True:
            temperature = do_sniff(ser, options.address)
            temp_frac = temperature / 10;
                                   
            
            client.publish("temp", str(temp_frac))   #publish       
            
            print(temp_frac)
            
            
            
    except KeyboardInterrupt:
        print('Ending...')
    
    ser.close()

if __name__ == '__main__':
    main()
