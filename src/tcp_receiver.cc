#include "tcp_receiver.hh"
#include <cstdint>

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( message.SYN ) {
    zero_point = message.seqno;
  }
  // no SYN, drop it
  if ( !zero_point.has_value() ) {
    return;
  }
  const uint64_t check_point = inbound_stream.bytes_pushed() + 1 - message.SYN; // add SYN
  const uint64_t first_index = message.seqno.unwrap( zero_point.value(), check_point ) + message.SYN - 1;
  reassembler.insert( first_index, message.payload.release(), message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage msg;
  if ( !zero_point.has_value() ) {
    msg.ackno = std::nullopt;
  } else {
    uint64_t written_size = inbound_stream.bytes_pushed() + 1 + inbound_stream.is_closed(); // add SYN and FIN
    msg.ackno = zero_point.value() + written_size;
  }
  static const uint64_t max_window_size = UINT16_MAX;
  msg.window_size = std::min( inbound_stream.available_capacity(), max_window_size );
  return msg;
}
