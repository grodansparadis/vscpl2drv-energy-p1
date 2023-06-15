# Simulate HAN P1 electric meter interface by sending P1 data on a serial channel  Simulated
# or real with a set interval
# 
# simhanp1 device interval
#

import time
import serial

ser = serial.Serial('/dev/ttyS10', 115200, timeout=0.050)
count = 0

while 1:
  #ser.write(b'Sent %d time(s)\r\n')
  ser.write(open("hanp1_1.data","rb").read())
  time.sleep(1)
  ser.write(open("hanp1_2.data","rb").read())
  time.sleep(1)
  count += 1