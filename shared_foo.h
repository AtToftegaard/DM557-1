#ifndef __SHARED_FOO_H__
#define __SHARED_FOO_H__

#define SIZE_OF_SEGMENT 10
#define TPDU_SIZE sizeof(tpdu_t)

/* For now only DATAGRAM is used, but for dynamic routing, ROUTERINFO is defined */
typedef enum {DATAGRAM, ROUTERINFO} datagram_kind;        /* datagram_kind definition */

typedef enum
{
    connection_req,
    connection_req_reply,
    tcredit,
    clear_connection,
    clear_conf,
    data_tpdu,
    data_notif
} tpdu_type;

typedef struct tpdu_s
{
    char            m;
    int             segment;
    int             port;
    int             returnport;
    tpdu_type       type;
    unsigned int    bytes;
    char            payload[SIZE_OF_SEGMENT];
} tpdu_t;


typedef struct {                        /* datagrams are transported in this layer */
  char data[TPDU_SIZE];           /* Data from the transport layer segment  */
  datagram_kind kind;                   /* what kind of a datagram is it? */
  int globalSender;                     /* From station address */
  int globalDestination;                /* To station address */
} packet;

#endif /* __SHARED_FOO_H__ */