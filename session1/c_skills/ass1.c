#include <stdio.h>

struct route {
	int routeID;
	char descrp[25];
};

typedef struct route Route;


int main(int argc , char *argv[])
{
	Route route1;
	struct route longroutes[10];
	struct route * routePtr;

	printf("route id = ");
	scanf("%d", &route1.routeID);

	printf("route descrip = ");
	scanf("%24s", route1.descrp);
	
	longroutes[2] = route1;
	routePtr = longroutes;
	
	printf("route id = %d, route descrip = %s\n",
		(routePtr+2)->routeID,
		(routePtr+2)->descrp);

	return 0;
}
