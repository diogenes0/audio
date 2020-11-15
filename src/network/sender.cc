#include <iostream>

#include "sender.hh"

using namespace std;

NetworkSender::NetworkSender( const Address& server, shared_ptr<OpusEncoderProcess> source, EventLoop& loop )
  : server_( server )
  , source_( source )
{
  socket_.set_blocking( false );

  loop.add_rule(
    "network transmit", [&] { push_one_frame(); }, [&] { return source_->has_frame(); } );
}

void NetworkSender::generate_statistics( ostringstream& out ) const
{
  out << "Sender info:";
  if ( frames_dropped_ ) {
    out << " dropped=" << frames_dropped_;
  }

  unsigned int num_outstanding = 0, num_in_flight = 0;
  const span_view<AudioFrameStatus> statuses
    = frame_status_.region( frame_status_.range_begin(), next_frame_index_ - frame_status_.range_begin() );
  for ( const auto& status : statuses ) {
    num_outstanding += status.outstanding;
    num_in_flight += status.in_flight;
  }
  out << " frames outstanding/in-flight=" << num_outstanding << "/" << num_in_flight;

  out << "\n";
}

void NetworkSender::push_one_frame()
{
  OpusEncoderProcess& encoder = *source_;

  if ( encoder.frame_index() != next_frame_index_ ) {
    throw runtime_error( "encoder/sender index mismatch" );
  }

  if ( next_frame_index_ < frames_.range_begin() ) {
    throw runtime_error( "NetworkSender internal error" );
  }

  if ( next_frame_index_ >= frames_.range_end() ) {
    const size_t frames_to_drop = next_frame_index_ - frames_.range_end() + 1;
    frames_.pop( frames_to_drop );
    frame_status_.pop( frames_to_drop );
    frames_dropped_ += frames_to_drop;
  }

  frames_.at( next_frame_index_ ) = { next_frame_index_, encoder.front_ch1(), encoder.front_ch2() };
  frame_status_.at( next_frame_index_ ) = { true, false };
  next_frame_index_++;

  encoder.pop_frame();

  send_packet();
}

void NetworkSender::send_packet()
{
  Packet p;

  p.sequence_number = next_sequence_number_;
  next_sequence_number_++;

  /* always send the most recent frame */
  const auto& most_recent_frame = frames_.at( next_frame_index_ - 1 );
  auto& most_recent_status = frame_status_.at( next_frame_index_ - 1 );
  if ( not most_recent_status.needs_send() ) {
    throw runtime_error( "unexpected frame status" );
  }
  p.frames.length = 1;
  p.frames.elements[0] = most_recent_frame;
  most_recent_status.in_flight = true;

  /* now, attempt to fill up the other slots for frames in the packet */
  uint8_t next_index_to_fill = 1;

  span<AudioFrameStatus> statuses
    = frame_status_.region( frame_status_.range_begin(), next_frame_index_ - frame_status_.range_begin() );
  const span_view<AudioFrame> frames
    = frames_.region( frame_status_.range_begin(), next_frame_index_ - frame_status_.range_begin() );
  for ( uint32_t i = 0; i < statuses.size(); i++ ) {
    auto& status = statuses[i];
    const auto& frame = frames[i];

    if ( status.needs_send() ) {
      p.frames.length = next_index_to_fill + 1;
      p.frames.elements[next_index_to_fill] = frame;
      status.in_flight = true;
      next_index_to_fill++;

      if ( next_index_to_fill >= p.frames.elements.size() ) {
        break;
      }
    }
  }

  /* now, send the packet */
  array<char, 1500> packet_buf;
  Serializer s { { packet_buf.data(), packet_buf.size() } };
  s.object( p );
  socket_.sendto( server_, { packet_buf.data(), s.bytes_written() } );
}