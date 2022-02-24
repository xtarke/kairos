#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Aug  8 10:29:01 2018

@author: Renan Augusto Starke
"""

import serial
import argparse
import time

def main():
    parser = argparse.ArgumentParser(description='Modbus Simple Package Sniffer')
    parser.add_argument('port',
                        help='The serial port to read from')
    parser.add_argument('-b', '--baudrate', default=9600, type=int,
                        help='The baudrate to use for the serial port (defaults to %(default)s)')
    parser.add_argument('-a', '--address', default=7, type=int,
                        help='The slave address whose packages you want to sniff (defaults to %(default)s)')
    
    
    options = parser.parse_args();
    
    # ser = serial.Serial(options.port, options.baudrate)                        
    print("Opened {} at {}. Slave address {}".format(options.port, options.baudrate, options.address))
  
    pkg = bytearray([options.address, 0x1e, 0x83, 0x88])    
    
    try:
        while (True):    
            with serial.Serial(options.port, options.baudrate, timeout=1) as ser:
                ser.write(pkg)
                
                s = ser.read(13)        # read up to 13 bytes (timeout)
                
                if (len(s) > 5):
                    temperature = (s[3] << 8) | s[4]
                    
                print(temperature/10)
                time.sleep(1)
             
    except KeyboardInterrupt:
        print('Ending...')        
    
    ser.close()

if __name__ == '__main__':
    main()
