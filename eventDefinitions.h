#ifndef __EVENTDEFINITIONS__
#define __EVENTDEFINITIONS__
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
#define data_for_transport_layer	   0x00000040
#define data_from_application_layer	   0x00000080
#define connection_req_answer	  	   0x00000100
#define test_event					   0x00000200

void log_event_received(long int event);

#endif /* __EVENTDEFINITONS__ */