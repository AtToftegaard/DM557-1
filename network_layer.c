#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "subnetsupport.h"
#include "subnet.h"
#include "fifoqueue.h"
#include "debug.h"
#include "eventDefinitions.h"

#include "network_layer.h"

long int events_we_handle;

void* give_me_message(int i){    
      char *msg;
      msg = malloc(5*sizeof(char));
      sprintf(msg,"%d" , i);
      return (void*) msg;
}
/*
void fakeTransportLayer(){

   int Receiver = 3;
   char *buffer;
   int i;
   packet *p;
   sleep(5); //wait for things initializing
   Lock(transport_layer_lock);
    printf("%s\n", "setting up messages");
    // Setup some messages
    for( i = 0; i < 2; i++ ) {
      p = (packet *) malloc( sizeof(packet) );
      buffer = (char *) malloc(sizeof(char *) * (SIZE_OF_SEGMENT));

      sprintf( buffer, "D: %d", i );
      strcpy(p->data, buffer);
      EnqueueFQ( NewFQE( (void *) p ), from_transport_layer_queue );
      Signal(transport_layer_ready, give_me_message(Receiver) );
    }
    
    if (i == 2) {
      sleep(15);
      logLine(succes, "Stopping - Sent %d messages to station %d\n", i, Receiver );
      Stop();
    }
   Unlock(transport_layer_lock);
}
*/
void initialize_locks_and_queues(){

   from_transport_layer_queue = InitializeFQ();
   for_transport_layer_queue = InitializeFQ();
}

int forward(int toAddress){
   return ((ThisStation%4)+1);
}

void network_layer_main_loop(){

packet *p;
packet *temp;
FifoQueueEntry e;
//The main loop of the network layer would likely look something along the lines of: (pseudo code)
event_t event;
events_we_handle = transport_layer_ready | network_layer_allowed_to_send | data_for_network_layer;

p = (packet *) malloc(sizeof(packet));
initialize_locks_and_queues();

   while( true ){
   	// Wait until we are allowed to transmit
   	Wait(&event, events_we_handle);
   	switch(event.type) {
   		case network_layer_allowed_to_send:
            Lock(network_layer_lock);
            printf("%s\n","CASE: network_layer_allowed_to_send" );
            // Layer below says it is ok to send
            // Lets send if there is something to send to that neighbour
            if (EmptyFQ(from_network_layer_queue) == 0){ // 1 = tom, 0 = ikke tom
               // Signal element is ready
               printf("%d Signalling queue not empty. Is now: %d \n", __LINE__, EmptyFQ(from_network_layer_queue) );

               Signal(network_layer_ready, give_me_message(forward(0)));
            }
            Unlock(network_layer_lock);

   			break;
   		case data_for_network_layer:
			// Layer below has provided data for us - lets process
			// Either it should go up or be shipped on
         // Retrieve from link_layer
            printf("%s %d\n", "CASE: data_for_network_layer", __LINE__);
            Lock(network_layer_lock);
            e = DequeueFQ(for_network_layer_queue);
            if(!e) {
               printf("data_for_network_layer nothing in queue\n");
               logLine(error, "ERROR: We did not receive anything from the queue, like we should have\n");
            } else {
               memcpy(p, (char *)ValueOfFQE( e ), sizeof(packet));
               free( (void *)ValueOfFQE( e ) );
               DeleteFQE( e );
               
               printf("Packet destination: %d\n",p->globalDestination );
               if (p->globalDestination == ThisStation){ // This is final destination
                  printf("Arrived at destination with msg: %s\n", p->data);
                  EnqueueFQ( NewFQE( (void *) p->data ), for_transport_layer_queue );
                  Signal(data_for_transport_layer, give_me_message(p->globalSender));
               } else {                                  // forward it
                  printf("%s\n","Package must be forwarded" );
                  EnqueueFQ( NewFQE( (void *) p ), from_network_layer_queue );
                  Signal(network_layer_ready, give_me_message(forward(0)));
               }
            }
            Unlock(network_layer_lock);
   			break;
   		case transport_layer_ready:
   		    // Data arriving from above - do something with it
            Lock(network_layer_lock);
            temp = (packet*) malloc(sizeof(packet));
            printf("%s\n","CASE: transport_layer_ready" );
            e = DequeueFQ(from_transport_layer_queue);
            if(!e) {
               logLine(succes, "ERROR: We did not receive anything from the queue, like we should have\n");
            } else {
               memcpy(temp->data, (char *)ValueOfFQE( e ), TPDU_SIZE);
               free( (void *)ValueOfFQE( e ) );
               DeleteFQE( e );
            }                                 
            temp->globalDestination = atoi( (char *) event.msg ); //nothing more than int in message, ever.
            temp->globalSender = ThisStation;
            temp->kind = DATAGRAM;
            printf("Packet enqueued with globalDestination: %d and localdestination: %d globalSender: %d\n",temp->globalDestination, forward(0), temp->globalSender );
            EnqueueFQ( NewFQE( (packet *) temp ), from_network_layer_queue );
            printf("Queue is: %d\n", EmptyFQ(from_network_layer_queue));

            printf("Transport_case signalling network_layer_ready\n");
            signal_link_layer_if_allowed(temp->globalDestination);
            Unlock(network_layer_lock);
            break;
         }
   }
}
void signal_link_layer_if_allowed(int address){
   if (EmptyFQ(from_network_layer_queue) == 0 && network_layer_enabled[address-1] == true) {
      Signal(network_layer_ready, give_me_message(address));
   }
}

