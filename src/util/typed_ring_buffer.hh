#pragma once

#include "exception.hh"
#include "ring_buffer.hh"
#include "spans.hh"

#include <algorithm>

template<typename T>
class TypedRingBuffer
{
  RingBuffer storage_;

  static constexpr auto elem_size_ = sizeof( T );

public:
  explicit TypedRingBuffer( const size_t capacity )
    : storage_( capacity * elem_size_ )
  {}

  size_t capacity() const { return storage_.capacity() / elem_size_; }

  span<T> writable_region() { return storage_.writable_region(); }
  void push( const size_t num_elems ) { storage_.push( num_elems * elem_size_ ); }

  span_view<T> readable_region() const { return storage_.readable_region(); }
  void pop( const size_t num_elems ) { storage_.pop( num_elems * elem_size_ ); }

  size_t num_pushed() const { return storage_.bytes_pushed() / elem_size_; }
  size_t num_popped() const { return storage_.bytes_popped() / elem_size_; }
  size_t num_stored() const { return storage_.bytes_stored() / elem_size_; }

  span<T> rw_region() { return storage_.rw_region(); }
  span_view<T> rw_region() const { return storage_.rw_region(); }
};

template<typename T>
class EndlessBuffer : TypedRingBuffer<T>
{
  void check_bounds( const size_t pos, const size_t count ) const
  {
    if ( pos < range_begin() ) {
      throw std::out_of_range( std::to_string( pos ) + " < " + std::to_string( range_begin() ) );
    }

    if ( pos + count > range_end() ) {
      throw std::out_of_range( std::to_string( pos ) + " + " + std::to_string( count ) + " > "
                               + std::to_string( range_end() ) );
    }
  }

public:
  using TypedRingBuffer<T>::TypedRingBuffer;

  void pop( const size_t num_elems )
  {
    TypedRingBuffer<T>::pop( num_elems );
    auto region = TypedRingBuffer<T>::writable_region();
    std::fill( region.end() - num_elems, region.end(), T {} );
  }

  size_t range_begin() const { return TypedRingBuffer<T>::num_popped(); }
  size_t range_end() const { return range_begin() + TypedRingBuffer<T>::capacity(); }

  span<T> region( const size_t pos, const size_t count )
  {
    check_bounds( pos, count );
    return TypedRingBuffer<T>::rw_region().substr( pos - range_begin(), count );
  }
};
