#include<stdio.h>
#include<stdlib.h>

int* fun1()
{
	int a[] = {1, 2, 3};
	a[0] = 2;
	return a;
}

void fun(int* p)
{
	int b = 5;
	p = &b;
}

void fun2(int** p)
{
	int b = 5;
	*p = &b;
}

int main()
{
	int* x = fun1();
	printf("X:%d\n",x[0]);
	char str[] = "Indra";
//	str = "JIT"; /*This is not pemitted , since str is a const pointer/
	str[1] = '1';
	char* ptr = "Brocade";
//	ptr[0] = '1'; /*Crashed : Memory not available */
	int a = 10;
	int *ptr1 = &a;
	fun(ptr1);
	printf("PTR1:%d\n",*ptr1);
	fun2(&ptr1);

	printf("PTR1:%d\n",*ptr1);
	printf("STR:%s\n",str);
	printf("PTR:%c\n",*ptr);
	return 0;
}
