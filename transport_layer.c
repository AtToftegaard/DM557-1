#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "subnetsupport.h"
#include "subnet.h"
#include "fifoqueue.h"
#include "debug.h"
#include "eventDefinitions.h"

#include "transport_layer.h"

// Global variables
int currentFreePort;	//next free port on this station
int remoteFreePort;		//next port to ask for connection to
int connectionid;		//currently usable connectionid
long int transport_loop_events;
event_t event;
FifoQueueEntry e;		//scratch
tpdu_t *t, *l;			//scratch
FifoQueue data_queue;

int connindex;	//current index in connectionarray
connection_t connectionArray[4]; //array for connections

int get_connect_index( int ID ){
	int i, index = -1;
	for (i = 0; i < 4; i++){
		if (connectionArray[i].connectionID == ID){
			index = i;
			break;
		}
	}
	return index;
}

FifoQueue dividemessage( FifoQueue queue ,char * mess, int nr_of_pieces, int piece_size){
	int n;
	char *msg;

	for (n = 0; n < nr_of_pieces; n++){
		msg = malloc(piece_size * sizeof(char));
		memcpy(msg, mess, piece_size);
		mess = mess + piece_size;
		EnqueueFQ(NewFQE(msg), queue);
	}
	return queue;
}

/*
 * Listen for incomming calls on the specified transport_address.
 * Blocks while waiting
 */
int listen(transport_address local_address){
	int for_us = 1;
	while (for_us == 1){	// multiple applications may wait for replies
		Wait(&event, connection_req_answer);
		printf("Listen waking up \n");
		printf("msg -> %d local_address -> %d \n", atoi(event.msg), local_address );
		if (atoi(event.msg) == local_address){
			printf("data was for port %d \n", local_address);
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

            if (t->bytes != -1){
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
	
	Lock(transport_layer_lock);
	tpdu_t *dataUnit = malloc(sizeof(tpdu_t));
	dataUnit->type = connection_req;
	dataUnit->returnport = local_ta;
	dataUnit->port = remote_ta;
	EnqueueFQ(NewFQE(dataUnit), from_transport_layer_queue);
	Signal(transport_layer_ready, give_me_message(remote_host));
	Unlock(transport_layer_lock);
	if (listen(local_ta) == 0){
		connectionid++;
		connectionArray[connindex].state 		= established;
		connectionArray[connindex].local_address = local_ta;
		connectionArray[connindex].remote_address= remote_ta;
		connectionArray[connindex].remote_host 	= remote_host;
		connectionArray[connindex].connectionID = connectionid;
		connindex++;
		printf("Connection established \n");
		return connectionid;
	} else {
		printf("%s\n", "Connection unsuccesful" );
		return -1;
	}
}

/*
 * Disconnect the connection with the supplied id.
 * returns appropriate errorcode or 0 if successfull
 */
int disconnect(int connection_id){
	Lock(transport_layer_lock);
	tpdu_t *dataUnit = malloc(sizeof(tpdu_t));
	int index = get_connect_index(connection_id);
	dataUnit->port = connection_id;		// port used for storing ID since it's not in use for disconnecting
	dataUnit->type = clear_connection;
	EnqueueFQ(NewFQE(dataUnit), from_transport_layer_queue);
	Signal(transport_layer_ready, give_me_message(connectionArray[index].remote_host));
	Unlock(transport_layer_lock);
	return 0;
}

char* receive(){
	tpdu_t *f;
	Wait(&event, data_for_application_layer);
	char* message = malloc(atoi(event.msg)*SIZE_OF_SEGMENT);
	while (EmptyFQ(data_queue) == 0){
		f = (tpdu_t*) ValueOfFQE(DequeueFQ(data_queue));
		memcpy(message + (f->segment * SIZE_OF_SEGMENT) , f->payload , SIZE_OF_SEGMENT);
	}	
	return message;
}

/*
 * On connection specified, send the bytes amount of data from buf.
 * Must break the message down into chunks of a manageble size, and queue them up.
 */
int send(int connection_id, char *buf, unsigned int bytes){
	FifoQueueEntry e;
	tpdu_t *message_unit;
	FifoQueue queue = InitializeFQ();
	int extra = 0, counter = 0;
	int index = get_connect_index(connection_id);

	connectionArray[index].state = sending;
	if (( bytes % SIZE_OF_SEGMENT) != 0){	//do we need extra piece (probably much better way to do this)
		extra = 1;
	}

	message_unit = malloc(sizeof(tpdu_t));	//notification of incoming data
	message_unit->type = data_notif;
	message_unit->port = connectionArray[connindex].remote_host;
	message_unit->segment = ((bytes / SIZE_OF_SEGMENT) + extra);
	EnqueueFQ(NewFQE(message_unit), from_transport_layer_queue);
	Signal(transport_layer_ready, give_me_message(connectionArray[index].remote_host));

	queue = dividemessage( queue ,buf, (bytes / SIZE_OF_SEGMENT) + extra  , SIZE_OF_SEGMENT);	// (src, nr_of_pieces, piece_size)
	
	while (EmptyFQ(queue) == 0){
		e = DequeueFQ(queue);
		message_unit = malloc(sizeof(tpdu_t));
		message_unit->type = data_tpdu;
		message_unit->port = connectionArray[connindex].remote_address;
		message_unit->returnport = connectionArray[connindex].local_address;
		message_unit->bytes = SIZE_OF_SEGMENT;
		message_unit->segment = counter++;
		memcpy(message_unit->payload, ValueOfFQE(e), SIZE_OF_SEGMENT);
		EnqueueFQ(NewFQE(message_unit), from_transport_layer_queue);
		free( (void *)ValueOfFQE( e ) );
		DeleteFQE(e);
		Signal(transport_layer_ready, give_me_message(connectionArray[index].remote_host));
	}
	return 0;
}


void transport_layer_loop(){

	transport_layer_lock = malloc(sizeof(mlock_t));
   	Init_lock(transport_layer_lock);

   	t = (tpdu_t*) malloc(sizeof(tpdu_t));
   	tpdu_t *temp;		
   	connectionid = 0;
   	connindex = 0;
   	int i, allowed, index, nr_of_segments, nr_of_segments_counter = 0;
	transport_loop_events =  data_for_transport_layer | test_event;

	while ( true ){	
		Wait(&event, transport_loop_events);
		//printf("transport_layer_loop awakened type: %ld \n", event.type);
		switch(event.type){
			case data_for_transport_layer:
				printf("%d CASE: data_for_transport_layer\n", __LINE__ );

				Lock(transport_layer_lock);
				l = ValueOfFQE(FirstEntryFQ(for_transport_layer_queue));

				switch( l->type ){
					case connection_req_reply:
						printf("port %d gets connection_req_reply \n", l->port );
						Signal(connection_req_answer, give_me_message(l->port));
						break;
					case connection_req:
						allowed = 0;
						for (i=0; i<connindex; i++){
							if (l->port == connectionArray[i].local_address ){ 
								printf("port number %d in use, return refusal", l->port);
								l->type = connection_req_reply;
								l->port = -1;
								DeleteFQE((DequeueFQ(for_transport_layer_queue)));
								EnqueueFQ(NewFQE(l), from_transport_layer_queue);	// send reply
								Signal(transport_layer_ready, give_me_message(atoi(event.msg)));
								allowed = 1;
								break;
							}
						}
						if (allowed == 0){
							i = atoi(event.msg);							
							connectionArray[connindex].connectionID = connectionid++;
							connectionArray[connindex].state = established;
							connectionArray[connindex].remote_host = i;
							connectionArray[connindex].local_address = l->port;
							connectionArray[connindex].remote_address = l->returnport; 
							
							DeleteFQE((DequeueFQ(for_transport_layer_queue)));
							l->type = connection_req_reply;
							EnqueueFQ(NewFQE(l), from_transport_layer_queue);	// send reply
							Signal(transport_layer_ready, give_me_message(i));
							printf("port number free, return acceptance\n");
						}
						break;
					case tcredit:
						break;
					case clear_connection:
						DeleteFQE((DequeueFQ(for_transport_layer_queue)));
						index = get_connect_index(l->port);
						connectionArray[index].state = disconn;
						printf("%d Connection %d disconnected \n",__LINE__, l->port );
						break;
					case clear_conf:
						break;
					case data_tpdu:
						printf("DATA_TPDU RECIEVED, nr %d!\n", nr_of_segments_counter);
						e = DequeueFQ(for_transport_layer_queue);
						temp = malloc(sizeof(tpdu_t));
						memcpy(temp, (tpdu_t*) ValueOfFQE( e ), sizeof(tpdu_t));
               			free( (void *)ValueOfFQE( e ) );
               			DeleteFQE( e );
						EnqueueFQ(NewFQE(temp), data_queue);
						nr_of_segments_counter++;
						printf("nr_of_segments_counter: %d  nr_of_segments: %d \n",nr_of_segments_counter,nr_of_segments );
						if (nr_of_segments_counter == nr_of_segments){
							printf("%d pieces recieved\n", nr_of_segments );
							Signal(data_for_application_layer, give_me_message(nr_of_segments));
						}
						break;
					case data_notif:
						printf("DATA_NOTIF recieved, expecting %d pieces!\n", l->segment);
						nr_of_segments = l->segment;
						data_queue = InitializeFQ();
						DeleteFQE((DequeueFQ(for_transport_layer_queue)));
						break;
				}

				Unlock(transport_layer_lock);
				break;
			case test_event:
				printf("Test_event\n");
				break;
		}
	}
}