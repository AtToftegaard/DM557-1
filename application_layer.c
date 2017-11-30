#include "application_layer.h"
#include "subnet.h"

void Station1_application_layer(void){
	sleep(5);
	char *mess = malloc(sizeof(char) * 12);
	sprintf(mess, "%s", "Hello World");
	int ID;
	ID = connect(3, 1, 1);
	sleep(20);
	send(ID, mess, 12);
	sleep(30);
	disconnect(ID);
	sleep(40);
	Stop();
}
void Station3_application_layer(void){
	sleep(5);
	char* message = receive();
	printf("Final Message: %s !\n",message );
	sleep(10);
}


