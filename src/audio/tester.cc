#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
 
#include "socket.hh"
#include "alsa_devices.hh"
#include "eventloop.hh"
#include "exception.hh"

using namespace std;
using namespace std::chrono;

pair<string, string> find_device( const string_view expected_description )
{
  ALSADevices devices;
  bool found = false;

  string name, interface_name;

  for ( const auto& dev : devices.list() ) {
    for ( const auto& interface : dev.interfaces ) {
      if ( interface.second == expected_description ) {
        if ( found ) {
          throw runtime_error( "Multiple devices matching description" );
        } else {
          found = true;
          name = dev.name;
          interface_name = interface.first;
        }
      }
    }
  }

  if ( not found ) {
    throw runtime_error( "Device \"" + string( expected_description ) + "\" not found" );
  }

  return { name, interface_name };
}

void program_body()
{
  ios::sync_with_stdio( false );

  const auto [name, interface_name] = find_device( "UAC-2, USB Audio" );

  cout << "Found " << interface_name << " as " << name << "\n";

  try {
    AudioDeviceClaim ownership { name };

    cout << "Claimed ownership of " << name;
    if ( ownership.claimed_from() ) {
      cout << " from " << ownership.claimed_from().value();
    }
    cout << endl;
  } catch ( const exception& e ) {
    cout << "Failed to claim ownership: " << e.what() << "\n";
  }

  AudioPair uac2 { interface_name };
  uac2.initialize();

  EventLoop loop;

  FileDescriptor input { CheckSystemCall( "dup STDIN_FILENO", dup( STDIN_FILENO ) ) };
  UDPSocket udpSocket; /*Dummy variables to satisfy interface change*/
  auto loopback_rule = loop.add_rule(
    "audio loopback",
    uac2.fd(),
    Direction::In,
    [&] { uac2.loopback(udpSocket); },
    [] { return true; },
    [] {},
    [&] {
      uac2.recover();
      return true;
    } );

  loop.add_rule( "exit on keystroke", input, Direction::In, [&] {
    loopback_rule.cancel();
    input.close();
  } );

  auto next_stats_print = steady_clock::now() + seconds( 3 );
  loop.add_rule(
    "print statistics",
    [&] {
      cout << "recov=" << uac2.statistics().recoveries;
      cout << " skipped=" << uac2.statistics().samples_skipped;
      cout << " empty=" << uac2.statistics().empty_wakeups << "/" << uac2.statistics().total_wakeups;
      cout << " mic<=" << uac2.statistics().max_microphone_avail;
      cout << " phone>=" << uac2.statistics().min_headphone_delay;
      cout << " comb<=" << uac2.statistics().max_combined_samples;
      cout << "\n";
      cout << loop.summary() << "\n";
      cout << global_timer().summary() << endl;
      uac2.reset_statistics();
      next_stats_print = steady_clock::now() + seconds( 3 );
    },
    [&] { return steady_clock::now() > next_stats_print; } );

  uac2.start();
  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  cout << loop.summary() << "\n";
}

int main()
{
  try {
    program_body();
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << "\n";
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
