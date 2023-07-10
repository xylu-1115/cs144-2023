#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <cmath>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return bytes_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( messages_.empty() ) {
    return {};
  }

  TCPSenderMessage msg = messages_.front();
  messages_.pop_front();
  return msg;
}

void TCPSender::push( Reader& outbound_stream )
{
  if ( fin_ ) {
    return;
  }

  int64_t available_size = recv_no_ + ( window_size_ ? window_size_ : 1 ) - next_no_;
  while ( available_size > 0 && !fin_ ) {
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_no_, isn_ );
    // add syn
    if ( !syn_ ) {
      syn_ = true;
      msg.SYN = true;
      available_size--;
    }

    Buffer& buffer = msg.payload;
    uint64_t len = min( static_cast<uint64_t>( available_size ), TCPConfig::MAX_PAYLOAD_SIZE );
    len = min( len, outbound_stream.bytes_buffered() );
    read( outbound_stream, len, buffer );
    available_size -= len;

    // add fin
    if ( outbound_stream.is_finished() && available_size ) {
      fin_ = true;
      available_size--;
      msg.FIN = true;
    }

    len = msg.sequence_length();
    if ( !len ) {
      return;
    }
    next_no_ += len;
    bytes_in_flight_ += len;
    messages_.push_back( msg );
    outstanding_messages_.push_back( msg );
    if ( !timer_.running() ) {
      timer_.reset();
    }
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( next_no_, isn_ );
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size_ = msg.window_size;
  if ( !msg.ackno.has_value() ) {
    return;
  }
  bool has_pop = false;
  recv_no_ = msg.ackno.value().unwrap( isn_, recv_no_ );
  // wrong recv_no_
  if ( recv_no_ > next_no_ ) {
    return;
  }
  while ( !outstanding_messages_.empty() ) {
    auto m = outstanding_messages_.front();
    // pop seg from segments_outstanding
    // deduct bytes_inflight
    if ( m.seqno.unwrap( isn_, next_no_ ) + m.sequence_length() <= recv_no_ ) {
      bytes_in_flight_ -= m.sequence_length();
      outstanding_messages_.pop_front();
      has_pop = true;
    } else {
      break;
    }
  }

  // reset rto, reset _consecutive_retransmissions
  // reset timer
  // stop timer if bytes_inflight == 0
  if ( has_pop ) {
    timer_.reset();
    rto = initial_RTO_ms_;
    if ( outstanding_messages_.empty() ) {
      timer_.stop();
    }
    consecutive_retransmissions_ = 0;
  }
}

void TCPSender::tick( const uint64_t ms_since_last_tick )
{
  if ( !timer_.running() ) {
    return;
  }
  timer_.tick( ms_since_last_tick );
  if ( !timer_.expire( rto ) || outstanding_messages_.empty() ) {
    return;
  }
  messages_.push_back( outstanding_messages_.front() );
  if ( window_size_ != 0 ) {
    consecutive_retransmissions_++;
    rto <<= 1;
  }
  timer_.reset();
}

void Timer::tick( uint64_t ms_since_last_tick )
{
  time += ms_since_last_tick;
}

bool Timer::expire( uint64_t rto )
{
  return active && ( time >= rto );
}

void Timer::reset()
{
  time = 0;
  active = true;
}

void Timer::stop()
{
  active = false;
}

bool Timer::running()
{
  return active;
}

uint64_t Timer::get_time()
{
  return time;
}
