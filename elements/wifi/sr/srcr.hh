#ifndef CLICK_SRCR_HH
#define CLICK_SRCR_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/wifi/linktable.hh>
#include <elements/wifi/arptable.hh>
#include <elements/wifi/sr/path.hh>
#include "srcr.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * SRCR(IP, ETH, ETHERTYPE, SRCR element, LinkTable element, ARPtable element, 
 *    [METRIC GridGenericMetric], [WARMUP period in seconds])
 * =d
 * DSR-inspired end-to-end ad-hoc routing protocol.
 * Input 0: ethernet packets 
 * Input 1: ethernet data packets from device 
 * Input 2: IP packets from higher layer, w/ ip addr anno.
 * Input 3: IP packets from higher layer for gw, w/ ip addr anno.
 * Output 0: ethernet packets to device (protocol)
 * Output 1: ethernet packets to device (data)
 *
 */


class SRCR : public Element {
 public:
  
  SRCR();
  ~SRCR();
  
  const char *class_name() const		{ return "SRCR"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();
  static int static_clear(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void clear();
  static String static_print_debug(Element *f, void *);
  static int static_write_debug(const String &arg, Element *e,
				void *, ErrorHandler *errh); 
  static int static_start(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void start(IPAddress dst);



  static int static_link_failure(const String &arg, Element *e,
				 void *, ErrorHandler *errh); 
  void link_failure(EtherAddress dst);



  static String static_print_stats(Element *e, void *);
  String print_stats();

  static String static_print_ip(Element *e, void *);
  static String static_print_path_cache(Element *e, void *);
  String print_path_cache();

  void push(int, Packet *);
  void run_timer();

  static unsigned int jiff_to_ms(unsigned int j)
  { return (j * 1000) / CLICK_HZ; }

  static unsigned int ms_to_jiff(unsigned int m)
  { return (CLICK_HZ * m) / 1000; }

  int get_fwd_metric(IPAddress other);
  int get_rev_metric(IPAddress other);
  bool update_link(IPAddress from, IPAddress to, int metric);
  void forward_query_hook();
  IPAddress get_random_neighbor();
private:

  class Query {
  public:
    Query() {memset(this, 0, sizeof(*this)); }
    Query(IPAddress ip) {memset(this, 0, sizeof(*this)); _ip = ip;}
    IPAddress _ip;
    u_long _seq;
    int _metric;
    int _count;
    struct timeval _last_query;

  };

  
  // List of query sequence #s that we've already seen.
  class Seen {
  public:
    IPAddress _src;
    IPAddress _dst;
    u_long _seq;
    int _fwd_metric;
    int _rev_metric;
    int _count;
    struct timeval _when; /* when we saw the first query */
    struct timeval _to_send;
    bool _forwarded;
    Vector<IPAddress> _hops;
    Vector<int> _fwd_metrics;
    Vector<int> _rev_metrics;
    Seen(IPAddress src, IPAddress dst, u_long seq, int fwd, int rev) {
      _src = src; _dst = dst; _seq = seq; _count = 0; _rev_metric = rev; _fwd_metric = fwd;
    }
    Seen();
  };

  class BadNeighbor {
  public:
    IPAddress _ip;
    struct timeval _when; 
    struct timeval _timeout;
    typedef HashMap<IPAddress, int> IPCount;
    IPCount _errors_sent;

    BadNeighbor() : _ip(), _when(), _timeout() { }
    BadNeighbor(IPAddress ip) : _ip(ip), _when(), _timeout() { }

    bool still_bad() {
      struct timeval expire;
      struct timeval now;
      click_gettimeofday(&now);
      timeradd(&_when, &_timeout, &expire);
      return timercmp(&now , &expire, <);
    }


  };

  class PathInfo {
  public:
    Path _p;
    struct timeval _last_packet;
    int count;
    PathInfo() {memset(this,0,sizeof(*this)); }
    PathInfo(Path p) { _p = p; }
  };


  class CurrentPath {
  public:
    Path _p;
    struct timeval _last_switch;    // last time we picked a new best route
    struct timeval _first_selected; // when _p was first selected as best route
    CurrentPath() { }
    CurrentPath(Path p) { _p = p; }
  };

  typedef HashMap<IPAddress, CurrentPath> PathCache;
  PathCache _path_cache;

  typedef HashMap<Path, PathInfo> PathTable;
  PathTable _paths;


  typedef HashMap<IPAddress, BadNeighbor> BlackList;
  BlackList _black_list;

  typedef HashMap<IPAddress, Query> QueryTable;
  QueryTable _queries;

  typedef HashMap<IPAddress, bool> IPMap;
  IPMap _neighbors;
  Vector<IPAddress> _neighbors_v;

  DEQueue<Seen> _seen;

  int MaxSeen;   // Max size of table of already-seen queries.
  int MaxHops;   // Max hop count for queries.
  struct timeval _query_wait;
  struct timeval _black_list_timeout;
  struct timeval _rev_path_update;
  u_long _seq;      // Next query sequence number to use.
  Timer _timer;
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint32_t _et;     // This protocol's ethertype

  IPAddress _bcast_ip;

  EtherAddress _bcast;

  class SRForwarder *_sr_forwarder;
  class LinkTable *_link_table;
  class LinkMetric *_metric;
  class ARPTable *_arp_table;

  // Statistics for handlers.
  int _num_queries_tx;
  int _bytes_queries_tx;
  int _num_replies_tx;
  int _bytes_replies_tx;

  int _num_queries_rx;
  int _bytes_queries_rx;
  int _num_replies_rx;
  int _bytes_replies_rx;

  bool _route_dampening;
  bool _debug;

  int _time_before_switch_sec;


  int find_dst(IPAddress ip, bool create);
  EtherAddress find_arp(IPAddress ip);
  void got_arp(IPAddress ip, EtherAddress en);
  void got_srpacket(Packet *p_in);
  void start_query(IPAddress);
  void process_query(struct srpacket *pk);
  void forward_query(Seen *s);
  void start_reply(struct srpacket *pk);
  void forward_reply(struct srpacket *pk);
  void got_reply(struct srpacket *pk);
  void start_data(const u_char *data, u_long len, Vector<IPAddress> r);
  void send(WritablePacket *);
  void process_data(Packet *p_in);
  static void static_forward_query_hook(Timer *, void *e) { 
    ((SRCR *) e)->forward_query_hook(); 
  }
};


CLICK_ENDDECLS
#endif