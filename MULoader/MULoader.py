import serial
import serial.tools.list_ports
import sys
import os
from enum import Enum
import time
from binascii import hexlify

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
	INIT_OK = 'R'

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
	sys.exit(1)

codeSize = os.path.getsize(sys.argv[1])
print "Code size: %d bytes"%codeSize
if codeSize == 0:
	print "Why would you upload nothing?"
	sys.exit(1)
print "Content will be aligned to 16 bytes"
addr = 0

try:
	ser = serial.Serial(selected, 115200, timeout = 2)
except:
	print "Error opening Serial port"
	sys.exit(1)

while ser.readline().strip() != Codes.INIT_OK.value:
	pass

ser.write(Codes.CODE_META.value+str(codeSize))

retcode = ser.read(1)
if retcode != Codes.COMMAND_OK.value:
	print "Error"






#Iterating through tags
print "Please scan tag"
while ser.in_waiting == 0:
	pass

for x in range(0,3):
	print ser.readline().strip()
	while ser.in_waiting == 0:
		pass

tokencap = int(ser.readline(),10)
print "Token capacity: %d bytes"%tokencap

while ser.in_waiting == 0:
	pass

retcode = ser.read(1)
if retcode == Codes.TAG_ERROR_WRITE.value:
	print "Error writing to block"

while ser.in_waiting == 0:
	pass
	
retcode = ser.read(1)
if retcode == Codes.TAG_ERROR_READ.value:
	print "Error reading block"
if retcode == Codes.TAG_ERROR_VERIFY.value:
	print "Error verifying block"
elif retcode == Codes.TAG_OK.value:
	print "Block updated"
'''
#Uploading code
while True:
	code = file.read(16)
	if code == "":
		break;
	while len(code) < 16:
		code += '\xFF'
	print '%8X: %r'% (addr,hexlify(code))
	ser.write(code)
	addr += 16
	'''
file.close()
ser.close()