#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  // Your code here.
  const uint64_t l = min( data.size(), capacity_ - written_size_ );
  if ( l == 0 ) {
    return;
  }
  if ( l < data.size() ) {
    data = data.substr( 0, l );
  }
  written_size_ += l;
  strq.push_back( std::move( data ) );
  viewq.emplace_back( strq.back().c_str(), l );
}

void Writer::close()
{
  // Your code here.
  close_ = true;
}

void Writer::set_error()
{
  // Your code here.
  error_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return close_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - written_size_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return written_size_ + read_size_;
}

string_view Reader::peek() const
{
  // Your code here.
  if ( written_size_ == 0 ) {
    return {};
  }
  return viewq.front();
}

bool Reader::is_finished() const
{
  // Your code here.
  return close_ && written_size_ == 0;
}

bool Reader::has_error() const
{
  // Your code here.
  return error_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t l = min( len, written_size_ );
  written_size_ -= l;
  read_size_ += l;
  while ( l > 0 ) {
    const uint64_t n = viewq.front().size();
    if ( l < n ) {
      viewq.front().remove_prefix( l );
      return;
    }
    viewq.pop_front();
    strq.pop_front();
    l -= n;
  }
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return written_size_;
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return read_size_;
}