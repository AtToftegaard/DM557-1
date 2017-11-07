#include "eventDefinitions.h"

void log_event_received(long int event) {
	char *event_name;
	switch(event) {
		case 1:
			event_name = "frame_arrival";
			break;
		case 2:
			event_name = "timeout";
			break;
		case 4:
			event_name = "network_layer_allowed_to_send";
			break;
		case 8:
			event_name = "network_layer_ready";
			break;
		case 16:
			event_name = "data_for_network_layer";
			break;
		case 32: 
			event_name = "transport_layer_ready";
			break;
		case 64:
			break;
		case 128:
			break;
		default:
			event_name = "unknown";
			break;
	}
	logLine(trace, "Event received %s\n", event_name);

}