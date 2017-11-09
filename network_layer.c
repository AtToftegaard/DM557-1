#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "rdt.h"
#include "subnetsupport.h"
#include "subnet.h"
#include "fifoqueue.h"
#include "debug.h"
#include "eventDefinitions.h"

FifoQueue from_transport_layer_queue;
FifoQueue for_transport_layer_queue;
mlock_t *transport_layer_lock;

long int events_we_handle;

void* give_me_message(int i){    
      char *msg;
      msg = (char *) malloc(5*sizeof(char));
      sprintf(msg,"%d" , i);
      return (void*) msg;
}

void fakeTransportLayer(){

   int Receiver = 3;
   char *buffer;
   int i;
   packet *p;

    printf("%s\n", "setting up messages");
    // Setup some messages
    for( i = 0; i < 1; i++ ) {
      p = (packet *) malloc( sizeof(packet) );
      buffer = (char *) malloc(sizeof(char *) * (SIZE_OF_SEGMENT));

      sprintf( buffer, "D: %d", i );
      strcpy(p->data, buffer);
      EnqueueFQ( NewFQE( (void *) p ), from_transport_layer_queue );
      Signal(transport_layer_ready, give_me_message(Receiver) );
    }
    if (i == 1) {
      sleep(10);
      logLine(succes, "Stopping - Sent %d messages to station %d\n", i, Receiver );
      /* A small break, so all stations can be ready */
      Stop();
    }
}

void initialize_locks_and_queues(){
   
   transport_layer_lock = malloc(sizeof(mlock_t));
   Init_lock(transport_layer_lock);

   from_transport_layer_queue = InitializeFQ();
   for_transport_layer_queue = InitializeFQ();
}

int forward(int toAddress){
   return ((ThisStation%4)+1);
}

void network_layer_main_loop(){

//int globalDestination;
packet *p;
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
            printf("%s\n","CASE: network_layer_allowed_to_send" );
            // Layer below says it is ok to send
            // Lets send if there is something to send to that neighbour
            if ((EmptyFQ(from_network_layer_queue)) == 0){ // 1 = tom, 0 = ikke tom
               // Signal element is ready
               Signal(network_layer_ready, give_me_message(forward(0)));
            }

   			break;
   		case data_for_network_layer:
			// Layer below has provided data for us - lets process
			// Either it should go up or be shipped on
         // Retrieve from link_layer
            printf("%s %d\n", "CASE: data_for_network_layer", __LINE__);
            Lock(network_layer_lock);
            e = DequeueFQ(for_network_layer_queue);
            Unlock(network_layer_lock);
            printf("e->val->data: %s\n",((char*)((packet*) e->val)->data));
            if(!e) {
               logLine(error, "ERROR: We did not receive anything from the queue, like we should have\n");
            } else {
               memcpy(p, (char *)ValueOfFQE( e ), sizeof(packet));
               free( (void *)ValueOfFQE( e ) );
               DeleteFQE( e );
               
               printf("Packet has destination: %d\n",p->globalDestination );
               if (p->globalDestination == ThisStation){ // This is final destination
                  printf("%s\n", "We are destination" );
                  Lock(transport_layer_lock);
                  EnqueueFQ( NewFQE( (void *) p ), for_transport_layer_queue );
                  Unlock(transport_layer_lock);
               } else {                                  // forward it
                  printf("%s\n","Package must be forwarded" );
                  Lock(network_layer_lock);
                  EnqueueFQ( NewFQE( (void *) p ), from_network_layer_queue );
                  Signal(network_layer_ready, give_me_message(forward(0)));
                  Unlock(network_layer_lock);
               }
            }
   			break;
   		case transport_layer_ready:
   		    // Data arriving from above - do something with it
            Lock(network_layer_lock);
            printf("%s\n","CASE: transport_layer_ready" );
            Lock(transport_layer_lock);
            e = DequeueFQ(from_transport_layer_queue);
            Unlock(transport_layer_lock);
            if(!e) {
               logLine(succes, "ERROR: We did not receive anything from the queue, like we should have\n");
            } else {
               memcpy(p, (char *)ValueOfFQE( e ), sizeof(packet));
               free( (void *)ValueOfFQE( e ) );
               DeleteFQE( e );
            }                                 
            p->globalDestination = atoi( (char *) event.msg ); //nothing more than int in message, ever.
            p->globalSender = ThisStation;
            p->kind = DATAGRAM;
            printf("Sending packet with globalDestination: %d and localdestination: %d globalSender: %d\n",p->globalDestination, forward(0), p->globalSender );
            EnqueueFQ( NewFQE( (packet *) p ), from_network_layer_queue );
            /*
            FifoQueueEntry n = DequeueFQ(from_network_layer_queue);
            packet *k = (packet*) malloc(sizeof(packet));
            memcpy(k, (packet*) ValueOfFQE(n), sizeof(packet));
            printf("k->data: %s k->globalDestination %d k->globalSender %d\n", k->data, k->globalDestination, k->globalSender );
            */

            Signal(network_layer_ready, give_me_message(forward(0)));
            Unlock(network_layer_lock);
            break;
         }
   }
}
void signal_link_layer_if_allowed(int address){

}

