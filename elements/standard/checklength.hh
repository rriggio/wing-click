#ifndef CLICK_CHECKLENGTH_HH
#define CLICK_CHECKLENGTH_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

CheckLength(MAX)

=s checking

drops large packets

=d

CheckLength checks every packet's length against MAX. If the packet has
length MAX or smaller, it is sent to output 0; otherwise, it is sent to
output 1 (or dropped if there is no output 1).
*/

class CheckLength : public Element { public:
  
  CheckLength();
  ~CheckLength();
  
  const char *class_name() const		{ return "CheckLength"; }
  const char *processing() const		{ return "a/ah"; }
  void notify_noutputs(int);
  
  int configure(Vector<String> &, ErrorHandler *);
  
  void push(int, Packet *);
  Packet *pull(int);
  
 protected:
  
  unsigned _max;

};

CLICK_ENDDECLS
#endif
