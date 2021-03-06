/*
    ir_rc6.c - phillips rc6 protocoll encoder
    Copyright (C) 2008  p.c.squirrel <pcs@gmx.at>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "lpc2220.h"
#include "infrared.h"
#include "ir_rc6.h"
#include "encoders.h"
#include "codes.h"

const struct irModule RC6_Module =
{
	RC6_Encode,
	RC6_Send,
	RC6_Repeat,
	RC6_Stop,
	136,	// carrier
	1,	// carrier-on
	3	// carrier-off
};

extern volatile unsigned char mod_enable;
extern volatile unsigned int cycles;
extern volatile unsigned long keyMap[42];

#define RC6_IDLE	        0x00
#define RC6_LEADER_A	    0x01
#define RC6_LEADER_B	    0x02
#define RC6_HEADER_BIT_A	0x03
#define RC6_HEADER_BIT_B	0x04
#define RC6_TRAILER_BIT_A	0x05
#define RC6_TRAILER_BIT_B	0x06
#define RC6_BIT_A	        0x07
#define RC6_BIT_B	        0x08
#define RC6_WAIT	        0x09


#define RC6_BITTIME	    16
#define RC6_WAITTIME_M0	188  //~80 ms
#define RC6_WAITTIME_M6	155

void __attribute__ ((section(".text.fastcode"))) RC6_Encode (void)
{
	cycles = RC6_BITTIME; //?
	switch(ir.state)
	{
		case RC6_IDLE:
			mod_enable = 0;
			break;

		case RC6_LEADER_A:
			mod_enable = 1;
			cycles = RC6_BITTIME*6;
			ir.state++;
			break;

		case RC6_LEADER_B:
			mod_enable = 0;
			cycles = RC6_BITTIME*2;
			ir.state++;
			break;

		case RC6_HEADER_BIT_A:
			if(ir.general.header_cmd & 0x8) //0b1000 4. bit  SB, mb2, mb1, mb0
				mod_enable = 1;
			else
				mod_enable = 0;
			ir.general.header_cmd <<= 1;
			cycles = RC6_BITTIME;
			ir.state++;
			break;

		case RC6_HEADER_BIT_B:
			if(mod_enable == 0)
				mod_enable = 1;
			else
				mod_enable = 0;
			ir.general.header_bit++;
			cycles = RC6_BITTIME;
			if(ir.general.header_bit == 4)
				ir.state++;
			else
				ir.state--;
			break;

		case RC6_TRAILER_BIT_A:
            if(ir.general.trail)
                mod_enable = 1;
            else
                mod_enable = 0;
			cycles = RC6_BITTIME*2;
			ir.state++;
			break;

		case RC6_TRAILER_BIT_B:
			if(ir.general.trail)
				mod_enable = 0;
			else
				mod_enable = 1;
            cycles = RC6_BITTIME*2;
			ir.state++;
			break;

		case RC6_BIT_A:
			if(ir.cmd & 0x80000000)
				mod_enable = 1;
			else
				mod_enable = 0;
			ir.cmd <<= 1;
			ir.state++;
			break;

		case RC6_BIT_B:
			if(mod_enable == 0)
				mod_enable = 1;
			else
				mod_enable = 0;
			ir.general.bit++;
			if(ir.general.bit == ir.general.numbits)
				ir.state++;
			else
				ir.state--;
			break;

		case RC6_WAIT:
			mod_enable = 0;
			ir.general.wait++;
			if(ir.general.wait >= ir.general.waittime)
			{
				ir.general.bit = 0;
				ir.general.header_bit = 0;
				ir.general.wait = 0;
				ir.state = RC6_IDLE;
			}
			break;
	}
	T1MR0 = cycles * RC6_Module.lo_border * RC6_Module.tval;
}

void RC6_Init(unsigned char map)
{
	if(map < RC6.num_tables)
	{
		ir.state	= RC6_IDLE;
		ir.cmd		= 0x0000;
		ir.actcmd	= 0x0000;
		ir.general.wait	= 0;
		ir.general.bit		= 0;
		copyMapI((unsigned int*)RC6.table[map].codes);
		setIR(RC6_Module);
		cycles = RC6_BITTIME;
	}
}

void RC6_Send(unsigned long cmd)
{
	ir.actcmd = cmd;

	setIRspeed(RC6_Module);
	
//	if(ir.general.toggle & 0x01)
	//	ir.actcmd &= 0xF7FF;
	RC6_Repeat();
}

void RC6_Repeat(void)
{
	if(ir.actcmd != 0x0000)
	{
		if(ir.state == RC6_IDLE)
		{
			ir.cmd = ir.actcmd;
            if(ir.cmd > 0xffff) {
				if(ir.toggle & 0x01)
					ir.cmd &= 0xFFFF7FFF;
				else
					ir.cmd |= 0x00008000;
				ir.general.numbits = 32;
				ir.general.header_cmd = 0x0E;  	//0b1110 ->Mode 6;
				ir.general.trail = 0;
				ir.general.waittime = RC6_WAITTIME_M6;
			}
			else {
				ir.cmd <<=16;
				ir.general.numbits = 16;
				ir.general.header_cmd = 0x08;  		//0b1000 ->Mode 0;
				ir.general.trail = ir.toggle & 0x01;
				ir.general.waittime = RC6_WAITTIME_M0;
			}
            ir.general.header_bit = 0;
			ir.state++;
			runIR();
		}
	}
}

void RC6_Stop(void)
{
    if(ir.actcmd != 0x00000000)
	{
waitend:
		if(ir.state != RC6_IDLE)
			goto waitend;
	}

	ir.toggle++;
	ir.actcmd = 0x0000;
	stopIR();
}


