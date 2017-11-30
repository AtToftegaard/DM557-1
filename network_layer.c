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

long int network_loop_events;

void* give_me_message(int i){    
      char *msg;
      msg = malloc(5*sizeof(char));
      sprintf(msg,"%d" , i);
      return (void*) msg;
}

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
network_loop_events = transport_layer_ready | network_layer_allowed_to_send | data_for_network_layer;

p = (packet *) malloc(sizeof(packet));
initialize_locks_and_queues();

   while( true ){
   	// Wait until we are allowed to transmit
   	Wait(&event, network_loop_events);
      //printf("network_layer_loop awakened type: %ld \n", event.type);
   	switch(event.type) {
   		case network_layer_allowed_to_send:
            Lock(network_layer_lock);
            //printf("%s\n","CASE: network_layer_allowed_to_send" );
            // Layer below says it is ok to send
            // Lets send if there is something to send to that neighbour
            if ((EmptyFQ(from_network_layer_queue) == 0) && (network_layer_enabled[atoi((char*) event.msg)-1] == true)){ // 1 = tom, 0 = ikke tom
               // Signal element is ready
               signal_link_layer_if_allowed(forward(0));
            }
            Unlock(network_layer_lock);
   			break;

   		case data_for_network_layer:
			// Layer below has provided data for us - lets process
			// Either it should go up or be shipped on
         // Retrieve from link_layer
            printf("%d %s\n", __LINE__,"CASE: data_for_network_layer");
            Lock(network_layer_lock);
            e = DequeueFQ(for_network_layer_queue);
            if(!e) {
               printf("data_for_network_layer nothing in queue\n");
               logLine(error, "ERROR: We did not receive anything from the queue, like we should have\n");
            } else {
               memcpy(p, (char *)ValueOfFQE( e ), sizeof(packet));
               free( (void *)ValueOfFQE( e ) );
               DeleteFQE( e );
               
               printf("%d Packet destination: %d \n", __LINE__,p->globalDestination);
               if (p->globalDestination == ThisStation){ // This is final destination
                  printf("Arrived at destination \n");
                  EnqueueFQ( NewFQE( (void *) p->data ), for_transport_layer_queue );
                  Signal(data_for_transport_layer, give_me_message(p->globalSender));
               } else if (p->globalDestination ==  3) {       // forward it
                  printf("%s\n","Package must be forwarded" );
                  temp = (packet*) malloc(sizeof(packet));
                  memcpy(temp, p, sizeof(packet));
                  EnqueueFQ( NewFQE( (void *) temp ), from_network_layer_queue );
                  Signal(network_layer_ready, give_me_message(forward(0)));
               } else if (p->globalDestination ==  1) {       // forward it
                  printf("%s\n","Package must be forwarded" );
                  temp = (packet*) malloc(sizeof(packet));
                  memcpy(temp, p, sizeof(packet));
                  EnqueueFQ( NewFQE( (void *) temp ), from_network_layer_queue );
                  Signal(network_layer_ready, give_me_message(forward(0)));
               }
            }
            Unlock(network_layer_lock);
   			break;

   		case transport_layer_ready:
   		    // Data arriving from above - do something with it
            printf("%s\n","CASE: transport_layer_ready" );
            Lock(network_layer_lock);
            Lock(transport_layer_lock);
            temp = (packet*) malloc(sizeof(packet));
            
            e = DequeueFQ(from_transport_layer_queue);
            if(!e) {
               logLine(succes, "ERROR: We did not receive anything from the queue, like we should have\n");
            } else {
               memcpy(temp->data, (char *)ValueOfFQE( e ), TPDU_SIZE);
               free( (void *)ValueOfFQE( e ) );
               DeleteFQE( e );
            }  
            Unlock(transport_layer_lock);
            temp->globalDestination = atoi( (char *) event.msg ); //nothing more than int in message, ever.
            temp->globalSender = ThisStation;
            temp->kind = DATAGRAM;
            printf("Packet enqueued with globalDestination: %d and localdestination: %d globalSender: %d \n",temp->globalDestination, forward(0), temp->globalSender );
            EnqueueFQ( NewFQE( (packet *) temp ), from_network_layer_queue );
            printf("Queue is: %d \n", EmptyFQ(from_network_layer_queue));

            signal_link_layer_if_allowed(forward(0));
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

