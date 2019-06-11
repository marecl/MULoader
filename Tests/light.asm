.include "m328Pdef.inc"

main:
	ldi r16, 0x01
	out DDRC, r16
loop:
	out PortC, r16
	rjmp loop
