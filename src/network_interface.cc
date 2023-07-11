#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
// https://zhuanlan.zhihu.com/p/642466343
// use: auto const move & {}
// const and constexpr
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const auto dst_ip = next_hop.ipv4_numeric();
  if ( ip2mac.contains( dst_ip ) ) {
    EthernetFrame frame { { ip2mac[dst_ip].first, ethernet_address_, EthernetHeader::TYPE_IPv4 },
                          serialize( dgram ) };
    send_frame.push( move( frame ) );
  } else if ( !arp_timer.contains( dst_ip ) ) {
    ARPMessage arp_msg;
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    arp_msg.sender_ethernet_address = ethernet_address_;
    arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
    arp_msg.target_ip_address = dst_ip;
    EthernetFrame frame { { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP },
                          serialize( arp_msg ) };
    send_frame.push( move( frame ) );
    waited_dgrams[dst_ip].push_back( dgram );
    arp_timer[dst_ip] = time;
  } else {
    waited_dgrams[dst_ip].push_back( dgram );
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return {};
  }
  auto const& type = frame.header.type;
  if ( type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) {
      return dgram;
    }
  } else if ( type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_msg;
    if ( !parse( arp_msg, frame.payload ) ) {
      return {};
    }
    auto const& send_ip = arp_msg.sender_ip_address;
    ip2mac[send_ip] = { arp_msg.sender_ethernet_address, time };
    arp_timer.erase( send_ip );
    auto const& dgrams = waited_dgrams[send_ip];
    for ( auto const& dgram : dgrams ) {
      send_datagram( dgram, Address::from_ipv4_numeric( send_ip ) );
    }
    waited_dgrams.erase( send_ip );
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST ) {
      if ( arp_msg.target_ip_address == ip_address_.ipv4_numeric() ) {
        ARPMessage reply_msg;
        reply_msg.opcode = ARPMessage::OPCODE_REPLY;
        reply_msg.sender_ip_address = ip_address_.ipv4_numeric();
        reply_msg.sender_ethernet_address = ethernet_address_;
        reply_msg.target_ethernet_address = arp_msg.sender_ethernet_address;
        reply_msg.target_ip_address = send_ip;
        EthernetFrame reply_frame {
          { arp_msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP },
          serialize( reply_msg ) };
        send_frame.push( move( reply_frame ) );
      }
    }
  }
  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  time += ms_since_last_tick;
  for ( auto it = ip2mac.begin(); it != ip2mac.end(); ) {
    if ( time >= it->second.second + IP_MAP_TTL ) {
      it = ip2mac.erase( it );
    } else {
      it++;
    }
  }

  for ( auto it = arp_timer.begin(); it != arp_timer.end(); ) {
    if ( time >= it->second + ARP_REQUEST_TTL ) {
      it = arp_timer.erase( it );
    } else {
      it++;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if ( !send_frame.empty() ) {
    auto frame = send_frame.front();
    send_frame.pop();
    return frame;
  } else {
    return {};
  }
}
