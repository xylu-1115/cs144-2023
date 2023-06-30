#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  static const uint64_t two32 = 1ULL << 32;
  static const uint64_t two31 = 1ULL << 31;
  const uint64_t pos = (checkpoint >> 32 << 32) + (raw_value_ - zero_point.raw_value_);
  if (pos > checkpoint + two31 && pos >= two32) {
    return pos - two32;
  } else if (pos < checkpoint - two31 && checkpoint >= two31) {
    return pos + two32;
  } else {
    return pos;
  }
}
