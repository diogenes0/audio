#pragma once

#include <chrono>
#include <memory>
#include <sstream>

#include "audio_task.hh"
#include "eventloop.hh"
#include "file_descriptor.hh"
#include "ring_buffer.hh"
#include "sender.hh"

class StatsPrinterTask
{
  std::shared_ptr<AudioDeviceTask> device_;
  std::shared_ptr<NetworkSender> sender_;
  std::shared_ptr<EventLoop> loop_;

  FileDescriptor standard_output_;
  RingBuffer output_rb_;

  using time_point = decltype( std::chrono::steady_clock::now() );

  time_point next_stats_print, next_stats_reset;

  static constexpr auto stats_print_interval = std::chrono::milliseconds( 500 );
  static constexpr auto stats_reset_interval = std::chrono::seconds( 10 );

  std::ostringstream ss_ {};

public:
  StatsPrinterTask( const std::shared_ptr<AudioDeviceTask> device,
                    const std::shared_ptr<NetworkSender> sender,
                    const std::shared_ptr<EventLoop> loop );
};
