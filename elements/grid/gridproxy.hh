#ifndef CLICK_GRID_PROXY_HH
#define CLICK_GRID_PROXY_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/hashmap.hh>
#include <clicknet/ip.h>
CLICK_DECLS

/*
 * =c
 * GridProxy(IP)
 * =d
 * a simple mobile-ip proxy
 * Input 0: ipip packets from a gateway
 * Input 1: ip packets for a mobile host
 * Output 0: ip packets for the outside world
 * Output 1: ipip packets for current "gateway" machine
 *
 * GridProxy tracks the last gateway a host sent an ip packet
 * through and when it receives packets for that host, it sends
 * then to the gateway it was last heard through using an 
 * ipip tunnel.
 */
class GridProxy : public Element { 

  class DstInfo {
  public:
    IPAddress _ip;
    timeval _last_updated;
    IPAddress _gw;
    DstInfo() {_ip = IPAddress(0); _last_updated.tv_sec = 0; _gw = IPAddress(0); }
    DstInfo(IPAddress ip, IPAddress gw, timeval now) {
      _ip = ip;
      _last_updated = now;
      _gw = gw;
    }

  };
  typedef HashMap<IPAddress, DstInfo> ProxyMap;
  class ProxyMap _map;
  
  click_ip _iph;
  uatomic32_t _id;
  
  void reverse_mapping(Packet *p_in);
  void forward_mapping(Packet *p_in);
  

public:
    GridProxy();
    ~GridProxy();

    const char *class_name() const { return "GridProxy"; }
    void *cast(const char *);
    const char *processing() const { return PUSH; }

    const int ninputs ()   { return 2; }
    const int noutputs ()  { return 2; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();
    void cleanup(CleanupStage);

    void push(int, Packet *);
  
  static String static_print_stats(Element *e, void *);
  String print_stats();

    
};

#include <click/hashmap.cc>
CLICK_ENDDECLS
#endif
