#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>

namespace saasy::common {

template <typename TResponse>
using CommandCallback = std::function<void(const TResponse&, std::optional<std::string>)>;

class CommandWrapper {
 public:
  virtual ~CommandWrapper() = default;

  virtual void Execute() = 0;
};

class CommandQueue {
 public:
  void Push(std::unique_ptr<CommandWrapper> wrapper);

  std::unique_ptr<CommandWrapper> TryPop();

  void Stop();

 private:
  std::queue<std::unique_ptr<CommandWrapper>> queue_;
  mutable std::mutex mutex_;
  bool stopped_ = false;
};

template <typename TResponse>
class CommandWithResponse {
 public:
  explicit CommandWithResponse(CommandCallback<TResponse> callback)
      : callback_(std::move(callback)) {}

  virtual ~CommandWithResponse() = default;

  virtual void Execute() = 0;

 protected:
  void InvokeCallback(const TResponse& response) {
    if (callback_) {
      callback_(response, std::nullopt);
    }
  }

  void InvokeCallback(const std::string& error) {
    if (callback_) {
      callback_(TResponse{}, error);
    }
  }

 private:
  CommandCallback<TResponse> callback_;
};

template <typename TResponse>
class ResponseCommandWrapper : public saasy::common::CommandWrapper {
 public:
  explicit ResponseCommandWrapper(std::unique_ptr<CommandWithResponse<TResponse>> cmd)
      : command_(std::move(cmd)) {}

  void Execute() override { command_->Execute(); }

 private:
  std::unique_ptr<CommandWithResponse<TResponse>> command_;
};

class CommandProcessor {
 public:
  explicit CommandProcessor(const std::string& session_id = "");
  ~CommandProcessor();

  void Start();
  void Stop();

  std::shared_ptr<saasy::common::CommandQueue> GetCommandQueue() { return command_queue_; }
  const std::string& GetSessionId() const { return session_id_; }

 private:
  std::string session_id_;
  std::thread command_thread_;
  std::atomic<bool> running_{false};
  std::shared_ptr<saasy::common::CommandQueue> command_queue_;

  void RunLoop();
};

}  // namespace saasy::common
