#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

class Timer
{
  uint64_t time = 0;
  bool active = false;

public:
  bool expire( uint64_t rto );
  uint64_t get_time();
  void tick( uint64_t ms_since_last_tick );
  void reset();
  void stop();
  bool running();
};

class TCPSender
{
  bool syn_ = false;
  bool fin_ = false;
  Wrap32 isn_;

  uint64_t initial_RTO_ms_;
  uint64_t rto = initial_RTO_ms_;

  Timer timer_ {};
  uint64_t window_size_ = 1;
  uint64_t recv_no_ = 0;
  uint64_t next_no_ = 0;

  uint64_t consecutive_retransmissions_ = 0;
  uint64_t bytes_in_flight_ = 0;

  std::deque<TCPSenderMessage> messages_ {};
  std::deque<TCPSenderMessage> outstanding_messages_ {};

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );
  // void add_message( TCPSenderMessage& msg );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
