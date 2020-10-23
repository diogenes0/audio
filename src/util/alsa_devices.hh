
#include <alsa/asoundlib.h>
#include <dbus/dbus.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ALSADevices
{
public:
  struct Device
  {
    std::string name;
    std::vector<std::pair<std::string, std::string>> interfaces;
  };

  static std::vector<Device> list();
};

class AudioDeviceClaim
{
  class DBusConnectionWrapper
  {
    struct DBusConnection_deleter
    {
      void operator()( DBusConnection* x ) const;
    };
    std::unique_ptr<DBusConnection, DBusConnection_deleter> connection_;

  public:
    DBusConnectionWrapper( const DBusBusType type );
    operator DBusConnection*();
  };

  DBusConnectionWrapper connection_;

  std::optional<std::string> claimed_from_ {};

public:
  AudioDeviceClaim( const std::string_view name );

  const std::optional<std::string>& claimed_from() const { return claimed_from_; }
};

class AudioInterface
{
  std::string interface_name_, annotation_;
  snd_pcm_t* pcm_;
  snd_pcm_uframes_t buffer_size_;

  void check_state( const snd_pcm_state_t expected_state );

public:
  class Buffer
  {
    snd_pcm_t* pcm_;
    const snd_pcm_channel_area_t* areas_;
    unsigned int sample_count_;
    snd_pcm_uframes_t offset_;

  public:
    Buffer( AudioInterface& interface, const unsigned int sample_count );
    void commit();
    ~Buffer();

    int32_t& sample( const bool right_channel, const unsigned int sample_num )
    {
      return *( static_cast<int32_t*>( areas_[0].addr ) + offset_ + right_channel + 2 * sample_num );
    }

    /* can't copy or assign */
    Buffer( const Buffer& other ) = delete;
    Buffer& operator=( const Buffer& other ) = delete;
  };

  AudioInterface( const std::string_view interface_name,
                  const std::string_view annotation,
                  const snd_pcm_stream_t stream );

  void start();
  void drop();
  void loopback_to( AudioInterface& other );
  void write_silence( const unsigned int sample_count );

  void debug();

  std::string name() const;

  ~AudioInterface();

  void link_with( AudioInterface& other );

  operator snd_pcm_t*() { return pcm_; }

  /* can't copy or assign */
  AudioInterface( const AudioInterface& other ) = delete;
  AudioInterface& operator=( const AudioInterface& other ) = delete;
};
