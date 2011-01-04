/*
 * linktablemulti.{cc,hh}
 * John Bicket, Stefano Testi, Roberto Riggio
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2009 CREATE-NET
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "linktablemulti.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
CLICK_DECLS

LinkTableMulti::LinkTableMulti() : _beta(80)
{
}

LinkTableMulti::~LinkTableMulti()
{
}

void *
LinkTableMulti::cast(const char *n) {
    if (strcmp(n, "LinkTableMulti") == 0)
        return (LinkTableMulti *) this;
    else if (strcmp(n, "LinkTableBase") == 0)
        return (LinkTableBase<NodeAddress, PathMulti> *) this;
    else
        return 0;
}

int 
LinkTableMulti::configure(Vector<String> &conf, ErrorHandler *errh) {
    int stale_period = 120;
    IPAddress ip;
    String ifaces;
    if (cp_va_kparse(conf, this, errh, 
            "IP", cpkM, cpIPAddress, &ip,
            "IFACES", cpkM, cpString, &ifaces,
            "BETA", 0, cpUnsigned, &_beta, 
            "STALE", 0, cpUnsigned, &stale_period, 
            cpEnd) < 0)
        return -1;

    _ip = NodeAddress(ip, 0);
    _hosts.insert(_ip, HostInfo(_ip));

    Vector<String> args;
    cp_spacevec(ifaces, args);
    if (args.size() < 1) {
        return errh->error("Error param IFACES must have > 0 arg, given %u", args.size());
    }
    for (int x = 0; x < args.size(); x++) {
        int iface;
        if (!cp_unsigned(args[x], &iface)) {
            errh->error("argument %s should be an unsigned", args[x].c_str());
        }
        NodeAddress node = NodeAddress(ip, iface);
        _hosts.insert(node, HostInfo(node));
    }
    _stale_timeout.assign(stale_period, 0);
    return 0;
}

Vector<int>
LinkTableMulti::get_interfaces(NodeAddress ip) {
    Vector<int> ifaces;
    for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
        if ((iter.key()._ip == ip._ip) && (iter.key()._iface != 0)) {
            ifaces.push_back(iter.key()._iface);
        }
    }
    return ifaces;
}

PathMulti 
LinkTableMulti::best_route(NodeAddress dst, bool from_me)
{
    PathMulti reverse_route;
    if (!dst) {
        return reverse_route;
    }
    HostInfo *nfo = _hosts.findp(dst);
    if (from_me) {
        Vector<NodeAddress> raw_path;
        while (nfo && nfo->_metric_from_me != 0) {
            if (nfo->_address._iface != 0) {
                raw_path.push_back(nfo->_address);
            }
            nfo = _hosts.findp(nfo->_prev_from_me);
        }
        if (raw_path.size() < 2) {
            return reverse_route;
        }
        reverse_route.push_back(NodeAirport(raw_path[0]._ip, raw_path[0]._iface, 0));
        for (int x = 1; x < raw_path.size() - 1; x++) {
            reverse_route.push_back(NodeAirport(raw_path[x]._ip, raw_path[x + 1]._iface, raw_path[x]._iface));
            while ((x < raw_path.size() - 1) && (raw_path[x + 1]._ip == raw_path[x]._ip)) {
                x++;
            }
        }
        reverse_route.push_back(NodeAirport(raw_path[raw_path.size() - 1]._ip, 0, raw_path[raw_path.size() - 1]._iface));
    } else {
        Vector<NodeAddress> raw_path;
        while (nfo && nfo->_metric_to_me != 0) {
            if (nfo->_address._iface != 0) {
                raw_path.push_back(nfo->_address);
            }
            nfo = _hosts.findp(nfo->_prev_to_me);
        }
        if (raw_path.size() < 2) {
            return reverse_route;
        }
        reverse_route.push_back(NodeAirport(raw_path[0]._ip, 0, raw_path[0]._iface));
        for (int x = 1; x < raw_path.size() - 1; x++) {
            reverse_route.push_back(NodeAirport(raw_path[x]._ip, raw_path[x]._iface, raw_path[x + 1]._iface));
            while ((x < raw_path.size() - 1) && (raw_path[x + 1]._ip == raw_path[x]._ip)) {
                x++;
            }
        }
        reverse_route.push_back(NodeAirport(raw_path[raw_path.size() - 1]._ip, raw_path[raw_path.size() - 1]._iface, 0));
    }
    if (from_me) {
        Vector<NodeAirport> route;
        for (int i = reverse_route.size() - 1; i >= 0; i--) {
            route.push_back(reverse_route[i]);
        }
        return route;
    }
    return reverse_route;
}

String 
LinkTableMulti::route_to_string(PathMulti p) {
    StringAccum sa;
    int hops = p.size() - 1;
    int metric = 0;
    StringAccum sa2;
    for (int i = 0; i < p.size(); i++) {
        sa2 << p[i].unparse();
        if (i != p.size()-1) {
            NodeAddress nfrom = NodeAddress(p[i]._ip, p[i]._dep);
            NodeAddress nto = NodeAddress(p[i+1]._ip, p[i+1]._arr);
            int m = get_link_metric(nfrom, nto);
            sa2 << " (" << get_link_metric(nfrom, nto) << ", " << get_link_channel(nfrom, nto) << ") ";
            metric += m;
        }
    }
    sa << p[p.size()-1]._ip.unparse() << " hops " << hops << " metric " << metric << " " << sa2;
    return sa.take_string();
}

String 
LinkTableMulti::print_routes(bool from_me, bool pretty)
{
    StringAccum sa;
    Vector<NodeAddress> addrs;
    for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
        if (iter.key()._iface == 0) {
            addrs.push_back(iter.key());
        }
    }
    click_qsort(addrs.begin(), addrs.size(), sizeof(NodeAddress), addr_sorter<NodeAddress>);
    for (int x = 0; x < addrs.size(); x++) {
        NodeAddress address = addrs[x];
        PathMulti r = best_route(address, from_me);
        if (valid_route(r)) {
            if (pretty) {
                sa << route_to_string(r) << "\n";
            } else {
                sa << r[r.size()-1] << "  ";
                for (int a = 0; a < r.size(); a++) {
                    sa << r[a].unparse();
                    if (a != r.size()-1) {
                        NodeAddress nfrom = NodeAddress(r[a]._ip, r[a]._dep);
                        NodeAddress nto = NodeAddress(r[a+1]._ip, r[a+1]._arr);
                        sa << " " << get_link_metric(nfrom,nto);
                        sa << " (" << get_link_seq(nfrom,nto) << "," << get_link_age(nfrom,nto) << ") ";
                    }
                }
                sa << "\n";
            }
        }
    }
    return sa.take_string();
}

uint32_t 
LinkTableMulti::get_route_metric(const PathMulti &route) {
    unsigned metric = 0;
    for (int i = 0; i < route.size() - 1; i++) {
        NodeAddress nfrom = NodeAddress(route[i]._ip, route[i]._dep);
        NodeAddress nto = NodeAddress(route[i+1]._ip, route[i+1]._arr);
        unsigned m = get_link_metric(nfrom, nto);
        if (m == 0) {
            return 0;
        }
        metric += m;
    }
    return metric;
}

enum { H_HOST_IP, H_HOST_INTERFACES };

String 
LinkTableMulti::read_handler(Element *e, void *thunk) {
  LinkTableMulti *td = (LinkTableMulti *) e;
  switch ((uintptr_t) thunk) {
    case H_HOST_IP:  return td->_ip.unparse() + "\n";
    case H_HOST_INTERFACES: {
      Vector<int> ifaces = td->get_local_interfaces();
      StringAccum sa;
      for (int x = 0; x < ifaces.size(); x++) {
        sa << ifaces[x] << " ";
      }
      sa << "\n";
      return sa.take_string();
    }
    default:
      return LinkTableBase<NodeAddress, PathMulti>::read_handler(e, thunk);
    }
}

void
LinkTableMulti::add_handlers() {
    add_read_handler("ip", read_handler, H_HOST_IP);
    add_read_handler("interfaces", read_handler, H_HOST_INTERFACES);
    LinkTableBase<NodeAddress, PathMulti>::add_handlers();
}

void
LinkTableMulti::dijkstra(bool from_me)
{
  Timestamp start = Timestamp::now();

  typedef HashMap<NodeAddress, bool> AddressMap;
  typedef AddressMap::const_iterator AMIter;

  AddressMap addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    addrs.insert(iter.value()._address, true);
  }

  for (AMIter i = addrs.begin(); i.live(); i++) {
    /* clear them all initially */
    HostInfo *n = _hosts.findp(i.key());
    n->clear(from_me);
  }
  HostInfo *root_info = _hosts.findp(_ip);

  assert(root_info);

  if (from_me) {
    root_info->_prev_from_me = root_info->_address;
    root_info->_metric_from_me = 0;
  } else {
    root_info->_prev_to_me = root_info->_address;
    root_info->_metric_to_me = 0;
  }

  NodeAddress current_min_address = root_info->_address;

  while (current_min_address) {
    HostInfo *current_min = _hosts.findp(current_min_address);
    assert(current_min);
    if (from_me) {
      current_min->_marked_from_me = true;
    } else {
      current_min->_marked_to_me = true;
    }


    for (AMIter i = addrs.begin(); i.live(); i++) {
      HostInfo *neighbor = _hosts.findp(i.key());
      assert(neighbor);
      bool marked = neighbor->_marked_to_me;
      if (from_me) {
	marked = neighbor->_marked_from_me;
      }

      if (marked) {
	continue;
      }

      AddressPair pair = AddressPair(neighbor->_address, current_min_address);
      if (from_me) {
	pair = AddressPair(current_min_address, neighbor->_address);
      }
      LinkInfo *lnfo = _links.findp(pair);
      if (!lnfo || !lnfo->_metric) {
	continue;
      }
      uint32_t neighbor_metric = neighbor->_metric_to_me;
      uint32_t current_metric = current_min->_metric_to_me;

      if (from_me) {
	neighbor_metric = neighbor->_metric_from_me;
	current_metric = current_min->_metric_from_me;
      }


      uint32_t adjusted_metric = current_metric + lnfo->_metric;
      if (!neighbor_metric ||
	  adjusted_metric < neighbor_metric) {
	if (from_me) {
	  neighbor->_metric_from_me = adjusted_metric;
	  neighbor->_prev_from_me = current_min_address;
	} else {
	  neighbor->_metric_to_me = adjusted_metric;
	  neighbor->_prev_to_me = current_min_address;
	}

      }
    }

    current_min_address = NodeAddress();
    uint32_t  min_metric = ~0;
    for (AMIter i = addrs.begin(); i.live(); i++) {
      HostInfo *nfo = _hosts.findp(i.key());
      uint32_t metric = nfo->_metric_to_me;
      bool marked = nfo->_marked_to_me;
      if (from_me) {
	metric = nfo->_metric_from_me;
	marked = nfo->_marked_from_me;
      }
      if (!marked && metric &&
	  metric < min_metric) {
        current_min_address = nfo->_address;
        min_metric = metric;
      }
    }

  }

  _dijkstra_time = Timestamp::now() - start;

}

EXPORT_ELEMENT(LinkTableMulti)
CLICK_ENDDECLS
