#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
	double taxrate = 7.3, discountrate;
	char buyer[100], seller[100];
	
	double * tmpPtr = &taxrate;
	
	printf("*tmpPtr = %f\n", *tmpPtr);
	
	discountrate = *tmpPtr;
	
	printf("discountrate = %.2f\n", discountrate);
	
	printf("&taxrate = %p\n", &taxrate);
	
	printf("tmpPtr = %p\n", tmpPtr);

	printf("equal? => %s\n", tmpPtr == &taxrate ? "yes" : "no");

	strncpy(buyer, "Hello World!", sizeof(buyer));
	
	strncpy(seller, buyer, sizeof(seller));
	
	printf("buyer == seller? => %s\n", 
		!strcmp(buyer, seller) ? "yes" : "no");
		
	strncat(buyer, seller, sizeof(buyer));
		
	printf("strlen(buyer) = %u\n", strlen(buyer));

	return 0;
}
