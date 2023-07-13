#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  if ( prefix_length > 32U ) {
    return;
  }
  auto node = root;
  for ( int i = 31; i >= 32 - prefix_length; --i ) {
    if ( node->next[route_prefix >> i & 1].get() == nullptr ) {
      node->next[route_prefix >> i & 1] = make_shared<Trie>();
    }
    node = node->next[route_prefix >> i & 1];
  }
  RouteItem item { route_prefix, prefix_length, next_hop, interface_num };
  node->item = make_shared<RouteItem>( move( item ) );
}

void Router::route()
{
  for ( auto& itfc : interfaces_ ) {
    auto recv_dgram = itfc.maybe_receive();
    if ( recv_dgram.has_value() ) {
      auto& dgram = recv_dgram.value();
      if ( dgram.header.ttl > 1 ) {
        dgram.header.ttl--;
        dgram.header.compute_checksum(); // important!
        auto dst_ip = dgram.header.dst;
        auto node = root;
        shared_ptr<RouteItem> rec( nullptr );
        for ( int i = 31; i && node.get(); --i ) {
          if ( node->item.get() != nullptr ) {
            rec = node->item;
            printf( "%s\n", Address::from_ipv4_numeric( rec->route_prefix ).ip().c_str() );
          }
          node = node->next[dst_ip >> i & 1];
        }

        if ( rec.get() != nullptr ) {
          auto& target_interface = interface( rec->interface_num );
          target_interface.send_datagram( dgram, rec->next_hop.value_or( Address::from_ipv4_numeric( dst_ip ) ) );
        }
      }
    }
  }
}