/*
 * rdt.h
 *
 *  Created on: Aug 18, 2017
 *      Author: jacob
 */
#include "subnetsupport.h"
#include "network_layer.h"
#ifndef RDT_H_
#define RDT_H_

#define frame_timer_timeout_millis  250
#define act_timer_timeout_millis     50

FifoQueue from_network_layer_queue;               /* Queue for data from network layer */
FifoQueue for_network_layer_queue;    /* Queue for data for the network layer */

typedef enum {false, true} boolean;        /* boolean type */
typedef unsigned int seq_nr;        /* sequence or ack numbers */
typedef enum {DATA, ACK, NAK} frame_kind;        /* frame_kind definition */
int ThisStation;
mlock_t *network_layer_lock;
boolean network_layer_enabled[4]; //What neighbours are layer enabled for

typedef struct {        /* frames are transported in this layer */
  frame_kind kind;        /* what kind of a frame is it? */
  seq_nr seq;           /* sequence number */
  seq_nr ack;           /* acknowledgement number */
  packet info;          /* the network layer packet */
  int sendTime;
  int recvTime;
  int sender;
  int destination;
} frame;

/* init_frame fills in default initial values in a frame. Protocols should
 * call this function before creating a new frame. Protocols may later update
 * some of these fields. This initialization is not strictly needed, but
 * makes the simulation trace look better, showing unused fields as zeros.
 */
void init_frame(frame *s, int count);

/* Fetch a packet from the network layer for transmission on the channel. */
void from_network_layer(packet *p);

/* Deliver information from an inbound frame to the network layer. */
void to_network_layer(packet *p);

/* Go get an inbound frame from the physical layer and copy it to r. */
int from_physical_layer(frame *r);

/* Pass the frame to the physical layer for transmission. */
void to_physical_layer(frame *s, int send_to);

/* Start the clock running and enable the timeout event. */
void start_timer(seq_nr k, int station);

/* Stop the clock and disable the timeout event. */
void stop_timer(seq_nr k, int station);

/* Start an auxiliary timer and enable the ack_timeout event. */
void start_ack_timer(int station);

/* Stop the auxiliary timer and disable the ack_timeout event. */
void stop_ack_timer(int station);

/* Allow the network layer to cause a network_layer_ready event. */
void enable_network_layer(int station);

/* Forbid the network layer from causing a network_layer_ready event. */
void disable_network_layer(int station);

/* In case of a timeout event, it is possible to find out the sequence
 * number of the frame that timed out (this is the sequence number parameter
 * in the start_timer function). For this, the simulator must know the maximum
 * possible value that a sequence number may have. Function init_max_seqnr()
 * tells the simulator this value. This function must be called before
 * start_simulator() function. When a timeout event occurs, function
 * get_timedout_seqnr() returns the sequence number of the frame that timed out.
 */
void init_max_seqnr(unsigned int o);
unsigned int get_timedout_seqnr(void);

/* Macro inc is expanded in-line: Increment k circularly. */
#define inc(k) if (k < MAX_SEQ) k = k + 1; else k = 0

#endif /* RDT_H_ */
