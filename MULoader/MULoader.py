import serial
import serial.tools.list_ports
import sys
from enum import Enum
import time

class Codes(Enum):
	COMMAND_OK = 'A'
	COMMAND_ERROR = 'B'
	CODE_META = 'C'
	CODE_BEGIN = 'D'
	TAG_OK = 'E'
	TAG_ERROR_WRITE = 'F'
	TAG_ERROR_READ = 'G'
	TAG_ERROR_VERIFY = 'H'
	TAG_NEXT = 'I'
	TAG_WAITING = 'J'
	TAG_FIRST = 'K'
	TAG_NOT_FIRST = 'L'
	BUFFER_OK = 'M'
	BUFFER_ERROR = 'N'
	BUFFER_REPEAT = 'O'
	BUFFER_WAITING = 'P'
	ALL_DONE = 'Q'

if len(sys.argv) != 2:
	print "Input file not specified"
	sys.exit(1)

port = list(serial.tools.list_ports.comports())

for a in port:
	print a[0] + " " + a[1]

selected = raw_input("Port: ").upper()
print("Selected port "+selected)

try:
	file = open(sys.argv[1] ,"rb")
except:
	print "Error opening file"

'''
try:
	ser = serial.Serial(selected, 115200)
	time.sleep(2)
	size = "12345";
	ser.write(Codes.CODE_META.value+size)
	ser.close()
except KeyboardInterrupt:
        ser.close()
        print ("Zamknieto port")
'''