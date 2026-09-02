#include "command_framework.h"

#include <chrono>
#include <iostream>

namespace saasy::common {

void CommandQueue::Push(std::unique_ptr<CommandWrapper> wrapper) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!stopped_) {
    queue_.push(std::move(wrapper));
  }
}

std::unique_ptr<CommandWrapper> CommandQueue::TryPop() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty() || stopped_) {
    return nullptr;
  }

  auto wrapper = std::move(queue_.front());
  queue_.pop();
  return wrapper;
}

void CommandQueue::Stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  stopped_ = true;

  while (!queue_.empty()) {
    queue_.pop();
  }
}

CommandProcessor::CommandProcessor(const std::string& session_id)
    : session_id_(session_id), command_queue_(std::make_shared<saasy::common::CommandQueue>()) {}

CommandProcessor::~CommandProcessor() { Stop(); }

void CommandProcessor::Start() {
  if (running_.exchange(true)) {
    return;  // Already running
  }

  command_thread_ = std::thread(&CommandProcessor::RunLoop, this);
  std::cout << "[CommandProcessor-" << session_id_ << "] Started\n";
}

void CommandProcessor::Stop() {
  if (!running_.exchange(false)) {
    return;  // Already stopped
  }

  command_queue_->Stop();

  // Make sure we have a valid thread before trying to join
  if (command_thread_.joinable()) {
    command_thread_.join();  // Block until thread completes
  }
  std::cout << "[CommandProcessor-" << session_id_ << "] Stopped\n";
}

void CommandProcessor::RunLoop() {
  std::cout << "[CommandProcessor-" << session_id_ << "] Entering run loop\n";

  while (running_) {
    // Process commands from the orchestrator
    if (auto wrapper = command_queue_->TryPop()) {
      wrapper->Execute();
    }

    // Small sleep prevents busy-spinning
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  std::cout << "[CommandProcessor-" << session_id_ << "] Exiting run loop\n";
}

}  // namespace saasy::common
