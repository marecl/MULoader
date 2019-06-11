#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

int main(){
	DDRC = 0x01;
	while(1){
		PORTC = 0x01;
		_delay_ms(1000);
		PORTC = 0x00;
		_delay_ms(1000);
	}
}
