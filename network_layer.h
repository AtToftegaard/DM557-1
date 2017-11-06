

#include "eventDefinitions.h"

/* For now only DATAGRAM is used, but for dynamic routing, ROUTERINFO is defined */
typedef enum {DATAGRAM, ROUTERINFO} datagram_kind;        /* datagram_kind definition */

typedef struct {                        /* datagrams are transported in this layer */
  char data[SIZE_OF_SEGMENT];   /* Data from the transport layer segment  */
  datagram_kind kind;                   /* what kind of a datagram is it? */
  int from;                                                /* From station address */
  int to;                                                /* To station address */
} datagram;


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


