#ifndef __NETOWRK_LAYER_H__
#define __NETOWRK_LAYER_H__
#include "fifoqueue.h"
#include "subnetsupport.h"
#include "transport_layer.h"
#include "rdt.h"

FifoQueue from_transport_layer_queue;
FifoQueue for_transport_layer_queue;

/* Make sure all locks and queues are initialized properly */
void initialize_locks_and_queues();

/* Where should a datagram be sent to. Transport layer knows only end hosts,
 * but we will need to inform the link layer where it should forward it to */
int forward(int toAddress);

/* Listen to relevant events for network layer, and act upon them */
void network_layer_main_loop();

/* If there is data that should be sent, this function will check that the
 * relevant queue is not empty, and that the link layer has allowed us to send
 * to the neighbour  */
void signal_link_layer_if_allowed( int address);

void fakeTransportLayer();

void* give_me_message(int address);

#endif /* __NETWORK_LAYER_H__ */