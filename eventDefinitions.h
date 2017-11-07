#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "debug.h"

/* Events */
#define network_layer_allowed_to_send  0x00000004
#define network_layer_ready            0x00000008
#define data_for_network_layer         0x00000010
#define transport_layer_ready 		   0x00000020

void log_event_received(long int event);