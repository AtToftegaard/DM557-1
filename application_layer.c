#include "application_layer.h"
#include "subnet.h"

void Station1_application_layer(void){
	sleep(5);
	/*
	FifoQueueEntry e;
	char *mess = malloc(sizeof(char) * 12);
	sprintf(mess, "%s", "Hello World");
	FifoQueue test = dividemessage(mess, 4	, 4);
	e = DequeueFQ(test);
	printf("%s\n", (char*) ValueOfFQE(e) );
	e = DequeueFQ(test);
	printf("%s\n", (char*) ValueOfFQE(e) );
	e = DequeueFQ(test);
	printf("%s\n", (char*) ValueOfFQE(e) );
	*/
	int ID;
	ID = connect(3, 1, 1);
	sleep(5);
	disconnect(ID);
	sleep(5);
	Stop();
}
void Station3_application_layer(void);


