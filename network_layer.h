
#include "fifoqueue.h"
#include "eventDefinitions.h"
#include "subnetsupport.h"
#include "transport_layer.h"

/* For now only DATAGRAM is used, but for dynamic routing, ROUTERINFO is defined */
typedef enum {DATAGRAM, ROUTERINFO} datagram_kind;        /* datagram_kind definition */

#define SIZE_OF_SEGMENT 16

typedef struct {                        /* datagrams are transported in this layer */
  char data[TPDU_SIZE];           /* Data from the transport layer segment  */
  datagram_kind kind;                   /* what kind of a datagram is it? */
  int globalSender;                     /* From station address */
  int globalDestination;                /* To station address */
} packet;

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

