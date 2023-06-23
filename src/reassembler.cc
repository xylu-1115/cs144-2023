#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  // if (data.empty()) return;

  uint64_t max_size = output.bytes_pushed() + output.available_capacity();
  uint64_t min_size = output.bytes_pushed();

  if ( first_index >= max_size || first_index + data.size() <= min_size )
    data = "";

  if ( first_index + data.size() > max_size ) { // 数据超过容量
    data = data.substr( 0, max_size - first_index );
  }
  if ( min_size > first_index && first_index + data.size() >= min_size ) { // 部分数据已经push完成
    data = data.substr( min_size - first_index );
    first_index = min_size;
  }

  if ( is_last_substring ) {
    tot_size = first_index + data.size();
    set_last = true;
  }

  // if (data.size() == 0) return;

  // pend_size += data.size();
  // mp[first_index] = move(data);

  auto p1 = mp.lower_bound( first_index );

  auto adjust = [&]( std::map<uint64_t, std::string>::iterator p ) {
    auto p2 = p++;
    while ( p != mp.end() ) {
      int64_t t = p2->first + p2->second.size() - p->first;
      if ( p2->first + p2->second.size() > p->first + p->second.size() ) {
        pend_size -= p->second.size();
        mp.erase( p++ );
      } else if ( t >= 0 ) {
        pend_size -= t;
        p2->second.append( move( p->second.substr( t ) ) );
        mp.erase( p++ );
      } else
        break;
    }
  };

  if ( data.size() == 0 ) {
  } else if ( p1 != mp.end() && p1->first == first_index ) {
    if ( data.size() > p1->second.size() ) {
      pend_size += data.size() - p1->second.size();
      p1->second = move( data );
      adjust( p1 );
    }
  } else {
    if ( p1 != mp.begin() ) {
      p1--;
      pend_size += data.size();
      auto flag = mp.insert( make_pair( first_index, move( data ) ) );
      if ( p1->first + p1->second.size() >= first_index )
        adjust( p1 );
      else
        adjust( flag.first );
    } else {
      pend_size += data.size();
      auto flag = mp.insert( make_pair( first_index, move( data ) ) );
      adjust( flag.first );
    }
  }

  uint64_t nxt_idx = output.bytes_pushed(), nnxt_idx;
  while ( mp.count( nxt_idx ) ) {
    output.push( mp[nxt_idx] );
    pend_size -= mp[nxt_idx].size();
    nnxt_idx = nxt_idx + mp[nxt_idx].size();
    mp.erase( nxt_idx );
    nxt_idx = nnxt_idx;
  }

  if ( output.bytes_pushed() == tot_size && set_last ) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return pend_size;
}
