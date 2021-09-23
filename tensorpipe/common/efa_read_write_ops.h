/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

#include <tensorpipe/common/defs.h>
#include <tensorpipe/common/error.h>
#include <tensorpipe/common/optional.h>

namespace tensorpipe {

// The read operation captures all state associated with reading a
// fixed length chunk of data from the underlying connection. All
// reads are required to include a word-sized header containing the
// number of bytes in the operation. This makes it possible for the
// read side of the connection to either 1) not know how many bytes
// to expected, and dynamically allocate, or 2) know how many bytes
// to expect, and preallocate the destination memory.
class EFAReadOperation {
 public:
  enum Mode {
    WAIT_TO_POST,
    WAIT_TO_COMPLETE,
    COMPLETE,
  };

 public:
  using read_callback_fn =
      std::function<void(const Error& error, const void* ptr, size_t len)>;

  explicit inline EFAReadOperation(read_callback_fn fn, uint64_t handlerId);

  inline EFAReadOperation(
      void* ptr,
      size_t length,
      read_callback_fn fn,
      uint64_t handlerId);

  // Called when a buffer is needed to read data from stream.
  inline void allocFromLoop();

  // Called when data has been read from stream.
  //   inline void readFromLoop();

  // Returns if this read operation is complete.
  inline bool completed() const;
  inline bool posted() const;

  inline void setCompleted();

  inline void setWaitToCompleted();

  inline size_t getReadLength();

  inline size_t* getLengthPtr();
  inline char* getBufferPtr();

  // Invoke user callback.
  inline void callbackFromLoop(const Error& error);
  uint64_t handlerId;

 private:
  Mode mode_{WAIT_TO_POST};
  char* ptr_{nullptr};

  // Number of bytes as specified by the user (if applicable).
  optional<size_t> givenLength_;

  // Number of bytes to expect as read from the connection.
  size_t readLength_{0};

  // Number of bytes read from the connection.
  // This is reset to 0 when we advance from READ_LENGTH to READ_PAYLOAD.
  size_t bytesRead_{0};

  // Holds temporary allocation if no length was specified.
  std::unique_ptr<char[]> buffer_{nullptr};

  // User callback.
  read_callback_fn fn_;
};

EFAReadOperation::EFAReadOperation(read_callback_fn fn, uint64_t handlerId)
    : fn_(std::move(fn)), handlerId(handlerId) {}

EFAReadOperation::EFAReadOperation(
    void* ptr,
    size_t length,
    read_callback_fn fn,
    uint64_t handlerId)
    : ptr_(static_cast<char*>(ptr)),
      givenLength_(length),
      fn_(std::move(fn)),
      handlerId(handlerId) {}

void EFAReadOperation::allocFromLoop() {
  if (givenLength_.has_value()) {
    TP_DCHECK(ptr_ != nullptr || givenLength_.value() == 0);
    TP_DCHECK_EQ(readLength_, givenLength_.value());
  } else {
    TP_DCHECK(ptr_ == nullptr);
    buffer_ = std::make_unique<char[]>(readLength_);
    ptr_ = buffer_.get();
  }
}

inline size_t* EFAReadOperation::getLengthPtr() {
  return &readLength_;
};
inline char* EFAReadOperation::getBufferPtr() {
  return ptr_;
};

inline size_t EFAReadOperation::getReadLength() {
  return readLength_;
};

bool EFAReadOperation::completed() const {
  return mode_ == COMPLETE;
}

bool EFAReadOperation::posted() const {
  return !(mode_ == WAIT_TO_POST);
}

void EFAReadOperation::setCompleted() {
  mode_ = COMPLETE;
}

void EFAReadOperation::setWaitToCompleted() {
  mode_ = WAIT_TO_COMPLETE;
}

void EFAReadOperation::callbackFromLoop(const Error& error) {
  fn_(error, ptr_, readLength_);
}

// The write operation captures all state associated with writing a
// fixed length chunk of data from the underlying connection. The
// write includes a word-sized header containing the length of the
// write. This header is a member field on this class and therefore
// the instance must be kept alive and the reference to the instance
// must remain valid until the write callback has been called.
class EFAWriteOperation {
 public:
  enum Mode {
    WAIT_TO_POST,
    WAIT_TO_COMPLETE,
    COMPLETE,
  };

  using write_callback_fn = std::function<void(const Error& error)>;

  inline EFAWriteOperation(
      const void* ptr,
      size_t length,
      write_callback_fn fn,
      uint64_t handlerId);

  struct Buf {
    char* base;
    size_t len;
  };

  inline std::tuple<Buf*, size_t> getBufs();

  // Invoke user callback.
  inline void callbackFromLoop(const Error& error);
  // set mode to WAIT_TO_COMPLETE
  inline void setWaitComplete();

  inline bool posted();

  // Returns if this write operation is complete.
  inline bool completed() const;
  // set mode to complete
  inline void setCompleted();
  // get peer address
  inline fi_addr_t getPeerAddr();
  // set peer address
  inline void setPeerAddr(fi_addr_t peer_addr);
  // get length
  inline size_t getLength() const;
  uint64_t handlerId;

 private:
  Mode mode_{WAIT_TO_POST};
  const char* ptr_;
  const size_t length_;
  fi_addr_t peer_addr_;

  // Buffers (structs with pointers and lengths) to write to stream.
  std::array<Buf, 2> bufs_;

  // User callback.
  write_callback_fn fn_;
};

EFAWriteOperation::EFAWriteOperation(
    const void* ptr,
    size_t length,
    write_callback_fn fn,
    uint64_t handlerId)
    : ptr_(static_cast<const char*>(ptr)),
      length_(length),
      fn_(std::move(fn)),
      handlerId(handlerId) {
  bufs_[0].base = const_cast<char*>(reinterpret_cast<const char*>(&length_));
  bufs_[0].len = sizeof(length_);
  bufs_[1].base = const_cast<char*>(ptr_);
  bufs_[1].len = length_;
}

std::tuple<EFAWriteOperation::Buf*, size_t> EFAWriteOperation::getBufs() {
  size_t numBuffers = length_ == 0 ? 1 : 2;
  return std::make_tuple(bufs_.data(), numBuffers);
}

void EFAWriteOperation::callbackFromLoop(const Error& error) {
  fn_(error);
}

bool EFAWriteOperation::posted() {
  return !(mode_ == WAIT_TO_POST);
}

size_t EFAWriteOperation::getLength() const {
  return length_;
}

void EFAWriteOperation::setWaitComplete() {
  mode_ = WAIT_TO_COMPLETE;
}

void EFAWriteOperation::setCompleted() {
  mode_ = COMPLETE;
}

bool EFAWriteOperation::completed() const {
  return mode_ == COMPLETE;
}

void EFAWriteOperation::setPeerAddr(fi_addr_t peer_addr) {
  peer_addr_ = peer_addr;
}

fi_addr_t EFAWriteOperation::getPeerAddr() {
  return peer_addr_;
}

} // namespace tensorpipe
