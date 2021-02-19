#include "client.hh"

using namespace std;
using namespace chrono;

using Option = RubberBand::RubberBandStretcher::Option;

uint64_t Client::client_mix_cursor() const
{
  return mix_cursor_;
}

uint64_t Client::server_mix_cursor() const
{
  return mix_cursor_ + outbound_frame_offset_.value() * opus_frame::NUM_SAMPLES_MINLATENCY;
}

Client::Client( const uint8_t node_id, const uint8_t ch1_num, const uint8_t ch2_num, CryptoSession&& crypto )
  : connection_( 0, node_id, move( crypto ) )
  , cursor_( 960, 1920 )
  , stretcher_( 48000,
                2,
                Option::OptionProcessRealTime | Option::OptionThreadingNever | Option::OptionPitchHighConsistency
                  | Option::OptionWindowShort )
  , ch1_num_( ch1_num )
  , ch2_num_( ch2_num )
{
  stretcher_.setMaxProcessSize( opus_frame::NUM_SAMPLES_MINLATENCY );
  stretcher_.calculateStretch();
}

bool Client::receive_packet( const Address& source, const Ciphertext& ciphertext, const uint64_t clock_sample )
{
  if ( connection_.receive_packet( ciphertext, source ) ) {
    if ( ( not outbound_frame_offset_.has_value() ) and connection_.has_destination() ) {
      outbound_frame_offset_ = clock_sample / opus_frame::NUM_SAMPLES_MINLATENCY;
    }
    return true;
  }
  return false;
}

void Client::decode_audio( const uint64_t cursor_sample, AudioBoard& board )
{
  ChannelPair& output = board.buffer( ch1_num_, ch2_num_ );

  cursor_.sample( connection_.frames(),
                  cursor_sample,
                  connection_.unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES_MINLATENCY,
                  connection_.next_frame_needed() * opus_frame::NUM_SAMPLES_MINLATENCY,
                  decoder_,
                  stretcher_,
                  output );

  connection_.pop_frames( min( cursor_.ok_to_pop( connection_.frames() ),
                               connection_.next_frame_needed() - connection_.frames().range_begin() ) );
}

void Client::mix_and_encode( const vector<mix_gain>& gains, const AudioBoard& board, const uint64_t cursor_sample )
{
  if ( not outbound_frame_offset_.has_value() ) {
    return;
  }

  while ( server_mix_cursor() + opus_frame::NUM_SAMPLES_MINLATENCY <= cursor_sample ) {
    span<float> ch1_target = mixed_audio_.ch1().region( client_mix_cursor(), opus_frame::NUM_SAMPLES_MINLATENCY );
    span<float> ch2_target = mixed_audio_.ch2().region( client_mix_cursor(), opus_frame::NUM_SAMPLES_MINLATENCY );

    for ( uint8_t channel_i = 0; channel_i < board.num_channels(); channel_i++ ) {
      if ( channel_i == ch1_num_ or channel_i == ch2_num_ ) {
        continue;
      }

      const span_view<float> other_channel
        = board.channel( channel_i ).region( server_mix_cursor(), opus_frame::NUM_SAMPLES_MINLATENCY );

      const float gain_into_1 = gains.at( channel_i ).first;
      const float gain_into_2 = gains.at( channel_i ).second;
      for ( uint8_t sample_i = 0; sample_i < opus_frame::NUM_SAMPLES_MINLATENCY; sample_i++ ) {
        const float value = other_channel[sample_i];
        const float orig_1 = ch1_target[sample_i];
        const float orig_2 = ch2_target[sample_i];

        ch1_target[sample_i] = orig_1 + gain_into_1 * value;
        ch2_target[sample_i] = orig_2 + gain_into_2 * value;
      }
    }

    mix_cursor_ += opus_frame::NUM_SAMPLES_MINLATENCY;
  }

  /* encode audio */
  while ( encoder_.min_encode_cursor() + opus_frame::NUM_SAMPLES_MINLATENCY <= client_mix_cursor() ) {
    encoder_.encode_one_frame( mixed_audio_.ch1(), mixed_audio_.ch2() );
    connection_.push_frame( encoder_ );
  }

  /* pop used mixed audio */
  mixed_audio_.pop_before( encoder_.min_encode_cursor() );
}

void Client::send_packet( UDPSocket& socket )
{
  if ( connection_.has_destination() ) {
    connection_.send_packet( socket );
  }
}

void Client::summary( ostream& out ) const
{
  if ( connection_.has_destination() ) {
    out << " (" << connection_.destination().to_string() << ") ";
  }
  cursor_.summary( out );
  connection_.summary( out );
}

void KnownClient::summary( ostream& out ) const
{
  out << name_ << ":";
  out << " requests=" << stats_.key_requests;
  out << " responses=" << stats_.key_responses;
  out << " new_sessions=" << stats_.new_sessions;
  if ( current_session_.has_value() ) {
    current_session_->summary( out );
  }
}

bool KnownClient::try_keyrequest( const Address& src, const Ciphertext& ciphertext, UDPSocket& socket )
{
  Plaintext plaintext;
  if ( long_lived_crypto_.decrypt( ciphertext, { &KeyMessage::keyreq_id, 1 }, plaintext )
       and ( plaintext.length() == 0 ) ) {
    stats_.key_requests++;
    if ( steady_clock::now() < next_reply_allowed_ ) {
      return true;
    }

    /* reply with keys to next session */
    Plaintext outgoing_keys;
    {
      Serializer s { outgoing_keys.mutable_buffer() };
      s.object( KeyMessage { id_, next_keys_ } );
      outgoing_keys.resize( s.bytes_written() );
    }
    Ciphertext outgoing_ciphertext;
    long_lived_crypto_.encrypt( { &KeyMessage::keyreq_server_id, 1 }, outgoing_keys, outgoing_ciphertext );
    socket.sendto( src, outgoing_ciphertext );
    next_reply_allowed_ = steady_clock::now() + milliseconds( 250 );

    stats_.key_responses++;
    return true;
  }
  return false;
}

KnownClient::KnownClient( const uint8_t node_id,
                          const uint8_t num_channels,
                          const uint8_t ch1_num,
                          const uint8_t ch2_num,
                          const LongLivedKey& key )
  : id_( node_id )
  , name_( key.name() )
  , long_lived_crypto_( key.key_pair().downlink, key.key_pair().uplink, true )
  , next_reply_allowed_( steady_clock::now() )
  , next_session_( CryptoSession { next_keys_.downlink, next_keys_.uplink } )
  , ch1_num_( ch1_num )
  , ch2_num_( ch2_num )
{
  /* set default gains */
  for ( uint8_t channel_i = 0; channel_i < num_channels; channel_i++ ) {
    gains_.push_back( { 2.0, 2.0 } );
  }
}

void KnownClient::receive_packet( const Address& src, const Ciphertext& ciphertext, const uint64_t clock_sample )
{
  if ( current_session_.has_value() and current_session_->receive_packet( src, ciphertext, clock_sample ) ) {
    return;
  }

  Plaintext throwaway_plaintext;
  if ( next_session_.value().decrypt( ciphertext, { &id_, 1 }, throwaway_plaintext ) ) {
    /* new session established */
    current_session_.emplace( id_, ch1_num_, ch2_num_, move( next_session_.value() ) );

    next_keys_ = KeyPair {};
    next_session_.emplace( next_keys_.downlink, next_keys_.uplink );
    stats_.new_sessions++;

    /* actually use packet */
    current_session_->receive_packet( src, ciphertext, clock_sample );
  }
}

void Client::set_cursor_lag( const uint16_t num_samples )
{
  cursor_.set_target_lag( num_samples );
  cursor_.reset();
}
