/*
 * miEEPROM.cpp
 *
 *  Created on: 6/1/2015
 *      Author: Alkimi
 */

#include "Arduino.h"
#include "miEEPROM.h"
#include <avr/eeprom.h>

uint8_t miEEPROM::read(int address)
{
        return eeprom_read_byte((unsigned char *) address);
}

void miEEPROM::write(int address, uint8_t value)
{
    	eeprom_write_byte((unsigned char *) address, value);

}

char *miEEPROM::lecturaEeprom16(byte posicion, char *direccion){
	byte i;
	if (posicion>63) posicion=63;
	int pos=posicion*16;
	for (i=0;i<16;i++){
		direccion[i]=read(pos+i);
	}
	direccion[i]=0x0;
	return direccion;
}

void miEEPROM::escrituraEeprom16(byte posicion,const char str[]){
	byte i;
	if (posicion>63) posicion=63;
	int pos=posicion*16;
	for (i=0;i<16;i++){
		write((pos+i),str[i]);
	}
}

miEEPROM EEPROM;


