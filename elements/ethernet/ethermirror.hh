#ifndef CLICK_ETHERMIRROR_HH
#define CLICK_ETHERMIRROR_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * EtherMirror()
 * =s Ethernet, modification
 * swaps Ethernet source and destination
 * =d
 *
 * Incoming packets are Ethernet. Their source and destination Ethernet
 * addresses are swapped before they are output.
 * */

class EtherMirror : public Element { public:

  EtherMirror();
  ~EtherMirror();
  
  const char *class_name() const		{ return "EtherMirror"; }
    
  Packet *simple_action(Packet *);
  
};

CLICK_ENDDECLS
#endif // CLICK_ETHERMIRROR_HH
