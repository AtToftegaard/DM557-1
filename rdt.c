/*
 * Reliable data transfer between two stations
 *
 * Author: Jacob Aae Mikkelsen.
 */

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

/* En macro for at lette overførslen af korrekt navn til Activate */
#define ACTIVATE(n, f) Activate(n, f, #f)

#define MAX_SEQ 127        /* should be 2^n - 1 */
#define NR_BUFS 4
#define NR_STATIONS 4
#define NR_MESSAGES 20

/* Globale variable */

char *StationName;         /* Globalvariabel til at overføre programnavn      */
//int ThisStation;           /* Globalvariabel der identificerer denne station. */
log_type LogStyle;         /* Hvilken slags log skal systemet føre            */
boolean network_layer_enabled[NR_STATIONS]; //What neighbours are layer enabled for

LogBuf mylog;                /* logbufferen                                     */

mlock_t *write_lock;

packet ugly_buffer; // TODO Make this a queue

int ack_timer_id[NR_STATIONS]; //Reciever sets an ACK-timer
int timer_ids[NR_STATIONS][NR_BUFS]; //Sender sets timer for FRAMES
boolean no_nak[NR_STATIONS]; /* no nak has been sent yet */

static boolean between(seq_nr a, seq_nr b, seq_nr c)
{
	boolean x = (a <= b) && (b < c);
	boolean y = (c < a) && (a <= b);
	boolean z = (b < c) && (c < a);
	// TODO Omskriv så det er til at fatte!
	logLine(debug, "a==%d, b=%d, c=%d, x=%d, y=%d, z=%d\n", a,b,c,x,y,z);

    return x || y || z;
}

/* Copies package content to buffer, ensuring it has a string end character. */
void packet_to_string(packet* data, char* buffer)
{
	strncpy ( buffer, (char*) data->data, SIZE_OF_SEGMENT );
	buffer[SIZE_OF_SEGMENT] = '\0';

}

static void send_frame(frame_kind fk, seq_nr frame_nr, seq_nr frame_expected, packet buffer[], int destination)
{
	// destination is NOT index
    /* Construct and send a data, ack, or nak frame. */
    frame s;        /* scratch variable */

    s.kind = fk;        /* kind == data, ack, or nak */
    if (fk == DATA)
    {
    	s.info = buffer[frame_nr % NR_BUFS];
    }
    s.seq = frame_nr;        /* only meaningful for data frames */
    s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);
    if (fk == NAK)
    {
    	no_nak[destination-1] = false;        /* one nak per frame, please */
    }
    to_physical_layer(&s, destination);        /* transmit the frame */
    if (fk == DATA)
    {
    	start_timer(frame_nr, destination);
    }
    stop_ack_timer(destination);        /* no need for separate ack frame */
}

//move to another file?
/*
void FakeNetworkLayer()
{
	int Receiver = ThisStation+1;
	char *buffer;
	int i,j;
	packet *p;
    long int events_we_handle;
    event_t event;
	FifoQueueEntry e;
	int reciever_idx = Receiver-1;

    

    // Setup some messages
    for( i = 0; i < NR_MESSAGES; i++ ) {
    	p = (packet *) malloc( sizeof(packet) );
		buffer = malloc(sizeof(char *) * (SIZE_OF_SEGMENT - 8));
		p->globalDestination = Receiver;//i % numberOfStations;
		p->globalSender = ThisStation; 

		sprintf( buffer, "D: %d", i );
		strcpy(p->data, buffer);
		EnqueueFQ( NewFQE( (void *) p ), from_network_layer_queue );
    }

    events_we_handle = network_layer_allowed_to_send | data_for_network_layer;

    // Give selective repeat a chance to start
    sleep(2);

    i = 0;
    j = 0;
    while( true ) {
    	// Wait until we are allowed to transmit
    	Wait(&event, events_we_handle);
    	switch(event.type) {
    		case network_layer_allowed_to_send:
				Lock( network_layer_lock );
    			if( i < NR_MESSAGES && network_layer_enabled[reciever_idx]) {
        			// Signal element is ready
        			logLine(info, "Sending signal for message #%d\n", i);
        			network_layer_enabled[reciever_idx] = false;
        			Signal(network_layer_ready, (int*) 1);
        			i++;
    			}
				Unlock( network_layer_lock );
    			break;
    		case data_for_network_layer:
				Lock( network_layer_lock );

				e = DequeueFQ( for_network_layer_queue );
    			//logLine(succes, "Received message: %s\n" ,( (char *) e->val) );

				Unlock( network_layer_lock );

    			j++;
    			break;
    	}

		if( i >= NR_MESSAGES ) {
			sleep(5);
		    logLine(info, "Station %d done. - (\'sleep(5)\')\n", ThisStation);
		    logLine(succes, "Stopping - Sent %d messages to station %d\n", NR_MESSAGES, Receiver );
		    // A small break, so all stations can be ready 
		    Stop();
		}
    }
}
*/
void selective_repeat() {
    seq_nr ack_expected			[NR_STATIONS];             		/* lower edge of sender's window */
    seq_nr next_frame_to_send	[NR_STATIONS];        			/* upper edge of sender's window + 1 */
    seq_nr frame_expected		[NR_STATIONS];         		   	/* lower edge of receiver's window */
    seq_nr too_far				[NR_STATIONS];                  /* upper edge of receiver's window + 1 */
    int i, j, destination, source, s_idx = -1;                       /* index into buffer pool */
    frame r;                          							/* scratch variable */
    packet *p;													// scratch
    packet out_buf				[NR_STATIONS][NR_BUFS];         /* buffers for the outbound stream */
    packet in_buf				[NR_STATIONS][NR_BUFS];         /* buffers for the inbound stream */
    boolean arrived				[NR_STATIONS][NR_BUFS];         /* inbound bit map */
    seq_nr nbuffered			[NR_STATIONS];                  /* how many output buffers currently used */
    event_t event;
    long int events_we_handle;
    unsigned int timer_id;

    write_lock = malloc(sizeof(mlock_t));
    network_layer_lock = (mlock_t *)malloc(sizeof(mlock_t));

    Init_lock(write_lock);
    Init_lock( network_layer_lock );

    from_network_layer_queue = InitializeFQ();
    for_network_layer_queue = InitializeFQ();
    
    for (i=0; i<NR_STATIONS; i++){
    	enable_network_layer(i+1);  /* initialize */
   		ack_expected[i] = 0;        /* next ack expected on the inbound stream */
    	next_frame_to_send[i] = 0;        /* number of next outgoing frame */
    	frame_expected[i] = 0;        /* frame number expected */
    	too_far[i] = NR_BUFS;        /* receiver's upper window + 1 */
    	nbuffered[i] = 0;        /* initially no packets are buffered */
    	ack_timer_id[i] = -1;
    	no_nak[i] = false;
    	
    	for (j=0; j<NR_BUFS; j++){
    		arrived[i][j] = false;
    		timer_ids[j][j] = -1;
    	}
    }

    logLine(trace, "Starting selective repeat %d\n", ThisStation);

    events_we_handle = frame_arrival | timeout | network_layer_ready;

    /*
    // If you are in doubt how the event numbers should be, comment in this, and you will find out.
    printf("%#010x\n", 1);
    printf("%#010x\n", 2);
    printf("%#010x\n", 4);
    printf("%#010x\n", 8);
    printf("%#010x\n", 16);
    printf("%#010x\n", 32);
    printf("%#010x\n", 64);
    printf("%#010x\n", 128);
    printf("%#010x\n", 256);
    printf("%#010x\n", 512);
    printf("%#010x\n", 1024);
	*/
	p = (packet*) malloc(sizeof(packet));

    while (true) {
        // Wait for any of these events

        Wait(&event, events_we_handle);
        log_event_received(event.type);

        switch(event.type) {
            case network_layer_ready:        /* accept, save, and transmit a new frame */
            	logLine(trace, "Network layer delivers frame - lets send it\n");
            	printf("%s\n","CASE: network_layer_ready" );

            	destination = atoi((char*) event.msg);
            	s_idx = destination-1;
            	network_layer_enabled[s_idx] = false;
	            nbuffered[s_idx] = nbuffered[s_idx] + 1;        /* expand the window */
	          
            	from_network_layer(&out_buf[s_idx][next_frame_to_send[s_idx]%NR_BUFS]);
	            send_frame(DATA, next_frame_to_send[s_idx], frame_expected[s_idx], out_buf[s_idx], destination);        /* transmit the frame */
	            inc(next_frame_to_send[s_idx]);        /* advance upper window edge */
	            break;

	        case frame_arrival:        /* a data or control frame has arrived */
	            printf("%s\n", "CASE: frame_arrival");
				from_physical_layer(&r);        /* fetch incoming frame from physical layer */
				source = r.sender;
				s_idx = source-1;
				if (r.kind == DATA) {
					/* An undamaged frame has arrived. */
					if ((r.seq != frame_expected[s_idx]) && no_nak[s_idx]) {
						send_frame(NAK, 0, frame_expected[s_idx], out_buf[s_idx], source);
					} else {
						start_ack_timer(source);
					}
					if (between(frame_expected[s_idx], r.seq, too_far[s_idx]) && (arrived[s_idx][r.seq%NR_BUFS] == false)) {
						/* Frames may be accepted in any order. */
						arrived[s_idx][r.seq % NR_BUFS] = true;        /* mark buffer as full */
						in_buf[s_idx][r.seq % NR_BUFS] = r.info;        /* insert data into buffer */
						while (arrived[s_idx][frame_expected[s_idx] % NR_BUFS]) {
							/* Pass frames and advance window. */
							to_network_layer(&in_buf[s_idx][frame_expected[s_idx] % NR_BUFS]);
							no_nak[s_idx] = true;
							arrived[s_idx][frame_expected[s_idx] % NR_BUFS] = false;
							inc(frame_expected[s_idx]);        /* advance lower edge of receiver's window */
							inc(too_far[s_idx]);        /* advance upper edge of receiver's window */
							start_ack_timer(source);        /* to see if (a separate ack is needed */
						}
					}
				}
				if((r.kind==NAK) && between(ack_expected[s_idx],(r.ack+1)%(MAX_SEQ+1),next_frame_to_send[s_idx])){
					send_frame(DATA, (r.ack+1) % (MAX_SEQ + 1), frame_expected[s_idx], out_buf[s_idx], source);
				}

				logLine(info, "Are we between so we can advance window? ack_expected=%d, r.ack=%d, next_frame_to_send=%d\n", ack_expected, r.ack, next_frame_to_send);
				while (between(ack_expected[s_idx], r.ack, next_frame_to_send[s_idx])) {
					logLine(debug, "Advancing window %d\n", ack_expected);
					nbuffered[s_idx] = nbuffered[s_idx] - 1;        		/* handle piggybacked ack */
					stop_timer(ack_expected[s_idx] % NR_BUFS, source);     /* frame arrived intact */
					inc(ack_expected[s_idx]);        				/* advance lower edge of sender's window */
				}
				break;

	        case timeout: /* Ack timeout or regular timeout*/
	        	// Check if it is the ack_timer
	        	timer_id = event.timer_id;
	        	printf("%s\n", "CASE: timeout");
	        	logLine(succes, "Timeout with id: %d - acktimer_id is acktimer_id[%d][%d][%d][%d] %d\n", timer_id, ack_timer_id[0],ack_timer_id[1],ack_timer_id[2],ack_timer_id[3]);
	        	logLine(succes, "Message from timer: '%s'\n", (char *) event.msg );

	        	s_idx = -1;
	    		for (i = 0; i < NR_STATIONS; i++) {
	    			if (timer_id == ack_timer_id[i]) {
	    				s_idx = i;
	    				logLine(trace, "Timer is an ack timer with id: %d\n", ack_timer_id[s_idx]);
	    				break;
	    			} else {
	    				for (j = 0; j < NR_BUFS; j++) {
	    					if (timer_ids[i][j] == timer_id) {
	    						s_idx = i;
	    						break;
	    					}
	    				}
	    				if (s_idx != -1) {
	    					break;
	    				}
	    			}
	    		}	
	    		if (s_idx == -1){
	    			printf("s_idx == -1 for id: %d DISREGARDING\n", timer_id);
	    			break;
	    		}
	        	if( timer_id == ack_timer_id[s_idx] ) { // Ack timer timer out
	        		logLine(succes, "This was an ack-timer timeout. Sending explicit ack to %d.\n", s_idx+1);
	        		free(event.msg);
	        		ack_timer_id[s_idx] = -1; // It is no longer running
	        		send_frame(ACK,0,frame_expected[s_idx], out_buf[s_idx], s_idx+1);        /* ack timer expired; send ack */
	        	} else {
	        		int timed_out_seq_nr = atoi( (char *) event.msg );
	        		logLine(succes, "Timeout for frame - need to resend frame %d\n", timed_out_seq_nr);
		        	send_frame(DATA, timed_out_seq_nr, frame_expected[s_idx], out_buf[s_idx], s_idx+1);
	        	}
	        	break;
	     }
	     sleep(2);
	     if (nbuffered[s_idx] < NR_BUFS) {
	         enable_network_layer(s_idx+1);
	     } else {
	    	 disable_network_layer(s_idx+1);
	     }
	  }
}

void enable_network_layer(int station) {
	int index = station-1;
	Lock( network_layer_lock );
	logLine(trace, "enabling network layer\n");
	network_layer_enabled[index] = true;
	Signal( network_layer_allowed_to_send, NULL );
	Unlock( network_layer_lock );
}

void disable_network_layer(int station){
	int index = station-1;
	Lock( network_layer_lock );
	logLine(trace, "disabling network layer\n");
	network_layer_enabled[index] = false;
	Unlock( network_layer_lock );
}

void from_network_layer(packet *p) {
    FifoQueueEntry e;
	Lock( network_layer_lock );
	e = DequeueFQ( from_network_layer_queue );
	Unlock( network_layer_lock );
	if(!e) {
		logLine(succes, "ERROR: We did not receive anything from the queue, like we should have\n");
	} else {
		memcpy(p, (packet*) ValueOfFQE( e ), sizeof(packet));
		free( (void *)ValueOfFQE( e ) );
		DeleteFQE( e );
	}
}


void to_network_layer(packet *p) {
	
	packet *pack;
    Lock( network_layer_lock );

    /*char * buffer;
    buffer = (char *) malloc ( sizeof(char) * (1+SIZE_OF_SEGMENT));
    packet_to_string(p, buffer);*/

    pack = malloc(sizeof(packet));
    memcpy(pack, p, sizeof(packet));

    EnqueueFQ( NewFQE( (void *) pack ), for_network_layer_queue );

    Unlock( network_layer_lock );

    Signal( data_for_network_layer, NULL);
}


void print_frame(frame* s, char *direction) {
	char temp[SIZE_OF_SEGMENT+1];

	switch( s->kind ) {
		case ACK:
			logLine(info, "%s: ACK frame. Ack seq_nr=%d\n", direction, s->ack);
			break;
		case NAK:
			logLine(info, "%s: NAK frame. Nak seq_nr=%d\n", direction, s->ack);
			break;
		case DATA:
			packet_to_string(&(s->info), temp);
			logLine(succes, "%s: DATA frame [seq=%d, ack=%d, kind=%d, (%s)] \n", direction, s->seq, s->ack, s->kind, temp);
			break;
	}
}

int from_physical_layer(frame *r) {
	r->seq = 0;
	r->kind = DATA;
	r->ack = 0;

	int source, dest, length;

	logLine(trace, "Receiving from subnet in station %d\n", ThisStation);
	FromSubnet(&source, &dest, (char *) r, &length);

	r->sender = source;
	//--------------------------------------------------
	if (r->kind == DATA){
		//printf("%s %d %d\n", r->info.data, r->info.globalDestination, r->info.globalSender );
	}
	//--------------------------------------------------
	print_frame(r, "received");

	return 0;
}


void to_physical_layer(frame *s, int send_to)
{
	print_frame(s, "sending");
	ToSubnet(ThisStation, send_to, (char *) s, sizeof(frame));
}


void start_timer(seq_nr k, int station) {

	int index = station-1;
	char *msg;
	msg = (char *) malloc(100*sizeof(char));
	sprintf(msg, "%d", k); // Save seq_nr in message

	timer_ids[index][k % NR_BUFS] = SetTimer( frame_timer_timeout_millis, (void *)msg );
	logLine(succes, "start_timer for seq_nr=%d timer_ids=[%d, %d, %d, %d] %s\n", k, timer_ids[index][0], timer_ids[index][1], timer_ids[index][2], timer_ids[index][3], msg);

}

void stop_timer(seq_nr k, int station) {
	
	int index = station-1;
	int timer_id;
	char *msg;

	timer_id = timer_ids[index][k];
	logLine(trace, "stop_timer for seq_nr %d med id=%d\n", k, timer_id);

    if (StopTimer(timer_id, (void *)&msg)) {
    	logLine(succes, "timer %d stoppet. msg: %s \n", timer_id, msg);
        free(msg);
    } else {
    	logLine(succes, "timer %d kunne ikke stoppes. Måske er den timet ud?timer_ids=[%d, %d, %d, %d] \n", timer_id, timer_ids[index][0], timer_ids[index][1], timer_ids[index][2], timer_ids[index][3]);
    }
}


void start_ack_timer(int station)
{
	int index = station-1;
	if( ack_timer_id[index] == -1 ) {
		logLine(trace, "Starting ack-timer\n");
		char *msg;
		msg = (char *) malloc(100*sizeof(char));
		strcpy(msg, "Ack-timer");
		ack_timer_id[index] = SetTimer( act_timer_timeout_millis, (void *)msg );
		logLine(debug, "Ack-timer startet med id %d\n", ack_timer_id);
	}
}


void stop_ack_timer(int station)
{
	int index = station-1;
	char *msg;

	logLine(trace, "stop_ack_timer\n");
    if (StopTimer(ack_timer_id[index], (void *)&msg)) {
	    logLine(trace, "timer %d stoppet. msg: %s \n", ack_timer_id, msg);
        free(msg);
    }
    ack_timer_id[index] = -1;
}



int main(int argc, char *argv[])
{
  StationName = argv[0];
  ThisStation = atoi( argv[1] );

  if (argc == 3)
    printf("Station %d: arg2 = %s\n", ThisStation, argv[2]);

  mylog = InitializeLB("mytest");

    LogStyle = synchronized;

    printf( "Starting network simulation\n" );


  /* processerne aktiveres. Selective Repeat først. */
  
  ACTIVATE(1, selective_repeat);
  ACTIVATE(2, selective_repeat);
  ACTIVATE(3, selective_repeat);
  ACTIVATE(4, selective_repeat);
  sleep(1);
  ACTIVATE(1, network_layer_main_loop);
  ACTIVATE(2, network_layer_main_loop);
  ACTIVATE(3, network_layer_main_loop);
  ACTIVATE(4, network_layer_main_loop);
  sleep(1);
  ACTIVATE(1, fakeTransportLayer);
  

  /* simuleringen starter */
  Start();
  exit(0);
}


