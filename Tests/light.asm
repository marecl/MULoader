.include "m328Pdef.inc"

	ldi r16, 0x01
	out DDRC, r16
	out PortC, r16

Start:
	rjmp Start
