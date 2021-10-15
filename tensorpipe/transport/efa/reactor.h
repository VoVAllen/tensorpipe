/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include <tensorpipe/common/busy_polling_loop.h>
#include <tensorpipe/common/callback.h>
#include <tensorpipe/common/efa.h>
#include <tensorpipe/common/fd.h>
#include <tensorpipe/common/optional.h>
#include <tensorpipe/transport/efa/constants.h>

namespace tensorpipe {
namespace transport {
namespace efa {

class efaEventHandler {
 public:
  virtual void onWriteCompleted() = 0;

  virtual void onReadCompleted() = 0;

  virtual void setReactorId(uint64_t id) = 0;

  virtual ~efaEventHandler() = default;
};

enum efaTag : uint64_t {
  kLength = 1ULL << 32,
  kPayload = 1ULL << 33,
};

// Reactor loop.
//
// Companion class to the event loop in `loop.h` that executes
// functions on triggers. The triggers are posted to a shared memory
// ring buffer, so this can be done by other processes on the same
// machine. It uses extra data in the ring buffer header to store a
// mutex and condition variable to avoid a busy loop.
//
class Reactor final : public BusyPollingLoop {
 public:
  Reactor(EfaLib efaLib, EfaDeviceList efaDeviceList);

  const EfaLib& getefaLib() {
    return efaLib_;
  }

  EfaDomain& getefaDomain() {
    return domain_;
  }

  EfaCompletionQueue& getefaCq() {
    return cq_;
  }

  const EfaAddress& getEfaAddress() {
    return addr_;
  }

  void registerHandler(
      fi_addr_t peer_addr,
      std::shared_ptr<efaEventHandler> eventHandler);

  void unregisterHandler(fi_addr_t peer_addr);

  void postSend(
      void* buffer,
      size_t size,
      uint64_t tag,
      fi_addr_t peer_addr,
      void* context);

  void postRecv(
      void* buffer,
      size_t size,
      uint64_t tag,
      fi_addr_t peer_addr,
      uint64_t ignore,
      void* context);

  fi_addr_t addPeerAddr(EfaAddress& addr);

  void removePeerAddr(fi_addr_t faddr);

  void setId(std::string id);

  void close();

  void join();

  ~Reactor();

 protected:
  bool pollOnce() override;

  bool readyToClose() override;

  class EfaEventDeleter {
   public:
    void operator()(fi_msg_tagged* msg) {
      delete msg->msg_iov;
    }
  };
  using EfaEvent = std::unique_ptr<fi_msg_tagged, EfaEventDeleter>;

 private:
  EfaLib efaLib_;
  EfaFabric fabric_;
  EfaDomain domain_;
  EfaEndpoint ep_;
  EfaCompletionQueue cq_;
  EfaAdressVector av_;
  EfaAddress addr_;

  uint64_t efaEventHandlerCounter_ = 0;

  int postPendingRecvs();
  int postPendingSends();

  std::atomic<bool> closed_{false};
  std::atomic<bool> joined_{false};

  // An identifier for the context, composed of the identifier for the context,
  // combined with the transport's name. It will only be used for logging and
  // debugging purposes.
  std::string id_{"N/A"};

  // The registered event handlers for each queue pair.
  std::unordered_map<fi_addr_t, std::shared_ptr<efaEventHandler>>
      efaEventHandler_;
  std::unordered_map<fi_addr_t, uint32_t>
      efaRecvIdxCount_;

  std::deque<EfaEvent> pendingSends_;
  std::deque<EfaEvent> pendingRecvs_;
};

} // namespace efa
} // namespace transport
} // namespace tensorpipe
