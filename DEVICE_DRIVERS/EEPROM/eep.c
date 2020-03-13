#include<wire.h>
#include<stdio.h>
#include<stdlib.h>

#define EEP_MEM_ADDR 0X50
#define EEP_HIGH_ADDR 0X2000

void eep_write(int memadddr,int data)
{
	int low_mem_addr,high_mem_addr;
	low_mem_addr=0X00;
	high_mem_addr=eep_high_addr;
	if(memaddr >= 0 && memaddr <= eep_high_addr){
		low_mem_addr=((memaddr)&(0Xooff));
		high_mem_addr=(memaddr & 0Xff00)>>8;

		Wire.beginTransmission(EEP_MEM_ADDR);
		Wire.write((byte)((int)EEP_HIGH_ADDR));
		Wire.write((byte)((int)low_mem_addr));
		Wire.write((byte)((int)data));

		Wire.endTransmission();
		delay(5);

	}
}



byte eep_read(int memadddr)
{
	int low_mem_addr,high_mem_addr;
	low_mem_addr=0X00;
	high_mem_addr=eep_high_addr;
	if(memaddr >= 0 && memaddr <= eep_high_addr){
		low_mem_addr=((memaddr)&(0Xooff));
		high_mem_addr=(memaddr & 0Xff00)>>8;

		Wire.beginTransmission(EEP_MEM_ADDR);
		Wire.write((byte)((int)EEP_HIGH_ADDR));
		Wire.write((byte)((int)low_mem_addr));

		Wire.endTransmission();
		
		Wire.requestFrom(EEP_MEM_ADDR,1);  //reading only 1 BYTE

		while(!Wire.available()){
			//plugin piece of code
		}

		return Wire.read();

	}
}



