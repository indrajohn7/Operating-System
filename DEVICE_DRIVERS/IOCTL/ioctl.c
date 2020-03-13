#include<linux/kernel.h>
#include<linux/module.h>


#define MAGIC 'P'
#define NUM 0
#define SET_FONT _IOW(MAGIC,NUM,unsigned long)


