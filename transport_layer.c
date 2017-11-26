#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "transport_layer.h"
#include "eventDefinitions.h"
#include "subnetsupport.h"
#include "subnet.h"
#include "fifoqueue.h"

// Global variables
int currentFreePort;	//next free port on this station
int remoteFreePort;		//next port to ask for connection to
int connectionid;		//currently usable connectionid
long int events_we_handle;
event_t event;
FifoQueueEntry e;		//scratch
tpdu_t *t, *l;			//scratch

int connectIndex;	//current index in connectionarray
connection_t connectionArray[4]; //array for connections

/*
 * Listen for incomming calls on the specified transport_address.
 * Blocks while waiting
 */
int listen(transport_address local_address){
	int for_us = 1;
	while (!for_us){
		Wait(&event, connection_req_answer);
		logLine(trace, "listen waking up");
		if ((int) event.msg == local_address){
			logLine(trace, "data was for port %d", local_address);
			Lock(transport_layer_lock);
			e = DequeueFQ(for_transport_layer_queue);
            if(!e) {
               logLine(succes, "ERROR: We did not receive anything from the queue, like we should have\n");
            } else {
               memcpy(t, (char *)ValueOfFQE( e ), sizeof(packet));
               free( (void *)ValueOfFQE( e ) );
               DeleteFQE( e );
               for_us = 0;
            }

            if (t->bytes){
				logLine(trace, "connection accepted");
				Unlock(transport_layer_lock);
				return 0;
			} else {
				logLine( error, "connection refused");
				Unlock(transport_layer_lock);
				return -1;
			}    
           
		}
	}
		
	
    return -1;
}

/*
 * Try to connect to remote host, on specified transport_address,
 * listening back on local transport address. (full duplex).
 *
 * Once you have used the connection to send(), you can then be able to receive
 * Returns the connection id - or an appropriate error code
 */
int connect(host_address remote_host, transport_address local_ta, transport_address remote_ta){
	
	tpdu_t *dataUnit = malloc(sizeof(tpdu_t));
	dataUnit->type = connection_req;
	dataUnit->returnport = local_ta;
	dataUnit->port = remote_ta;
	EnqueueFQ(NewFQE(dataUnit), from_transport_layer_queue);
	Signal(transport_layer_ready, give_me_message(remote_host));
	if (listen(local_ta)){
		connectionArray[connectIndex % 4].state 		= established;
		connectionArray[connectIndex % 4].local_address = local_ta;
		connectionArray[connectIndex % 4].remote_address= remote_ta;
		connectionArray[connectIndex % 4].remote_host 	= remote_host;
		connectIndex++;
		printf("Connection established");
		return connectionid++;
	} else {
		printf("%s\n", "Connection unsuccesful" );
		return -1;
	}
	
}

/*
 * Disconnect the connection with the supplied id.
 * returns appropriate errorcode or 0 if successfull
 */
int disconnect(int connection_id);

/*
 * Set up a connection, so it is ready to receive data on, and wait for the main loop to signal all data received.
 * Could have a timeout for a connection - and cleanup afterwards.
 */
int receive(char, unsigned char *, unsigned int *);

/*
 * On connection specified, send the bytes amount of data from buf.
 * Must break the message down into chunks of a manageble size, and queue them up.
 */
int send(int connection_id, unsigned char *buf, unsigned int bytes);


void transport_layer_loop(){

	transport_layer_lock = malloc(sizeof(mlock_t));
   	Init_lock(transport_layer_lock);

   	t = (tpdu_t*) malloc(sizeof(tpdu_t));		
   	connectionid = 0;
   	connectIndex = 0;
	events_we_handle = data_from_application_layer | data_for_transport_layer;

	for(;;){	// Begin loop
		Wait(&event, events_we_handle);
		switch(event.type){
			case data_for_transport_layer:
				Lock(transport_layer_lock);

				e = FirstEntryFQ(for_transport_layer_queue);

				switch(( (tpdu_t *) ValueOfFQE(e))->type ){
					case connection_req_reply:	//bytes field is used as accepted/refused indicator
						logLine(trace, "port %d gets connection_req_reply", ((tpdu_t*) ValueOfFQE(e))->port );
						Signal(connection_req_answer, give_me_message(((tpdu_t*) ValueOfFQE(e))->port ));
						break;
					case connection_req:
						break;
					case tcredit:
						break;
					case clear_connection:
						break;
					case clear_conf:
						break;
					case data_tpdu:
						break;
				}

				Unlock(transport_layer_lock);
				break;

			case data_from_application_layer:
				Lock(transport_layer_lock);

				Unlock(transport_layer_lock);
				break;
		}
	}
}