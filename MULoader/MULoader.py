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
	TAG_UPDATING_META = 'M'
	BUFFER_OK = 'N'
	BUFFER_ERROR = 'O'
	BUFFER_REPEAT = 'P'
	BUFFER_WAITING = 'Q'
	ALL_DONE = 'R'
	INIT_OK = 'S'


if len(sys.argv) != 2:
	print 'Input file not specified'
	sys.exit(1)

port = list(serial.tools.list_ports.comports())

for a in port:
	print a[0] + ' ' + a[1]

selected = raw_input('Port: ').upper()
print 'Selected port ' + selected

try:
	file = open(sys.argv[1], 'rb')
except:
	print 'Error opening file'
	sys.exit(1)

codeSize = os.path.getsize(sys.argv[1])
print 'Code size: %d bytes' % codeSize

if codeSize == 0:
	print 'Why would you upload nothing?'
	sys.exit(1)

# Adjusting code size to the nearest block
if codeSize % 16 != 0:
	codeSize += 16-(codeSize%16)
	print "Adjusted code size: %d" % codeSize

try:
	ser = serial.Serial(selected, 115200, timeout=2)
except:
	print 'Error opening Serial port'
	sys.exit(1)

while ser.read(1) != Codes.INIT_OK.value:
	pass

ser.write(Codes.CODE_META.value + str(codeSize))

retcode = ser.read(1)
if retcode != Codes.COMMAND_OK.value:
	print 'Error'

# Bytes of code written
addr = 0

retcode = ser.read(1)
while True:
	if retcode != Codes.TAG_WAITING.value:
		print "Wait for tag error"
	raw_input('[Press any key]')

	ser.write(Codes.TAG_NEXT.value)
	print 'Scan tag'

	while ser.in_waiting == 0:
		pass

	for x in range(0, 3):
		print ser.readline().strip()
		while ser.in_waiting == 0:
			pass

	tokencap = int(ser.readline(), 10)
	print 'Token capacity: %d bytes' % tokencap

	while True:
		retcode = ser.read(1)
		if retcode == Codes.TAG_FIRST.value:
			break
		if retcode == Codes.TAG_WAITING.value:
			break
		if retcode == Codes.TAG_UPDATING_META.value:
			print "Updating tag metadata"
		elif retcode == Codes.BUFFER_WAITING.value:
			ser.write(Codes.CODE_BEGIN.value)

			# Write to buffer
			code = file.read(16)
			while len(code) < 16:
				code += '\xFF'
			print '%8X: %r' % (addr, hexlify(code))

			while True:
				# come back here if there is an error with buffer
				ser.write(code)
				retcode = ser.read(1)
				if retcode != Codes.BUFFER_OK.value:
					print "buffer fill error"
				while ser.in_waiting < 16:
					pass

				isError = False
			
				for x in range(0, 16, 1):
					retcode = ser.read(1)
					if code[x] != retcode:
						print 'Buffer error %d!=%d!' % (ord(code[x]), ord(retcode))
						ser.write(Codes.BUFFER_ERROR.value)
						isError = True
						break
				if isError == False: break

			ser.write(Codes.BUFFER_OK.value)
			addr += 16
		
		while ser.in_waiting == 0:
			pass
		retcode = ser.read(1)
		if retcode == Codes.TAG_ERROR_WRITE.value:
			print 'Error writing to block'

		retcode = ser.read(1)
		if retcode == Codes.TAG_ERROR_READ.value:
			print 'Error reading block'
		if retcode == Codes.TAG_ERROR_VERIFY.value:
			print 'Error verifying block'
		elif retcode == Codes.TAG_OK.value: pass

	if retcode == Codes.TAG_FIRST.value: break

# We're done with this file
file.close()

while True:
	# Ask for first tag
	if retcode == Codes.TAG_FIRST.value:
		print 'Prepare first tag'
		raw_input('[Press any key]')
		ser.write(Codes.TAG_NEXT.value)
	print 'Scan first tag'

	# What if tag is not first
	while ser.in_waiting == 0: pass
	retcode = ser.read(1)
	if retcode != Codes.TAG_OK.value:
		if retcode == Codes.TAG_NOT_FIRST.value:
			print 'Tag is not first'
		else:
			print 'Unknown error %d' % ord(retcode)
		retcode = ser.read(1)
	elif retcode == Codes.TAG_OK.value: break



# Waiting for end of communication
while ser.in_waiting == 0:
	pass
retcode = ser.read(1)
if retcode == Codes.ALL_DONE.value:
	print 'Done programming'

if codeSize != addr:
	print 'Written different amount of data (%d | %d)' % (addr, codeSize)
ser.close()