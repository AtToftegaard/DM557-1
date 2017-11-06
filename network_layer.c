
void initialize_locks_and_queues(){
   
   mlock_t *lock = malloc(sizeof(mlock_t));
   Init_lock(lock);
   from_transport_layer_queue = InitializeFQ();
   for_transport_layer_queue = InitializeFQ();

};

int forward(toAddress){

};

void networl_layer_main_loop(){

//The main loop of the network layer would likely look something along the lines of: (pseudo code)

while( true )
   	// Wait until we are allowed to transmit
   	Wait(&event, events_we_handle);
   	switch(event.type) {
   		case network_layer_allowed_to_send:
            // Layer below says it is ok to send
            // Lets send if there is something to send to that neighbour
   			break
   		case data_for_network_layer:
			// Layer below has provided data for us - lets process
			// Either it should go up or be shipped on
   			break
   		case transport_layer_ready:
   		    // Data arriving from above - do something with it
         }
};

void signal_link_layer_if_allowed(){

};