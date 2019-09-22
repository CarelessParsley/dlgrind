#pragma once

#include <kj/array.h>

#include <vector>
#include <cstdint>

struct PackedInverse {
  PackedInverse() {}

  KJ_DISALLOW_COPY(PackedInverse);

  kj::Array<uint32_t> states_;
  kj::Array<uint8_t> actions_;
  kj::Array<uint32_t> index_;

  kj::ArrayPtr<const uint32_t> getStates() const { return states_; }
  kj::ArrayPtr<const uint8_t> getActions() const { return actions_; }
  kj::ArrayPtr<const uint32_t> getIndex() const { return index_; }

  kj::ArrayPtr<uint32_t> initStates(size_t n) { states_ = kj::heapArray<uint32_t>(n); return states_; }
  kj::ArrayPtr<uint8_t> initActions(size_t n) { actions_ = kj::heapArray<uint8_t>(n); return actions_; }
  kj::ArrayPtr<uint32_t> initIndex(size_t n) { index_ = kj::heapArray<uint32_t>(n); return index_; }
};

struct HopcroftInput {
  HopcroftInput() {}

  KJ_DISALLOW_COPY(HopcroftInput);

  uint32_t numStates_;
  uint8_t numActions_;
  PackedInverse inverse_;
  kj::Array<uint32_t> initialPartition_;

  uint32_t getNumStates() const { return numStates_; }
  uint8_t getNumActions() const { return numActions_; }
  const PackedInverse& getInverse() const { return inverse_; }
  kj::ArrayPtr<const uint32_t> getInitialPartition() const { return initialPartition_; }

  void setNumStates(uint32_t numStates) { numStates_ = numStates; }
  void setNumActions(uint8_t numActions) { numActions_ = numActions; }
  PackedInverse& initInverse() { return inverse_; }
  kj::ArrayPtr<uint32_t> initInitialPartition(size_t n) { initialPartition_ = kj::heapArray<uint32_t>(n); return initialPartition_; }
};

struct HopcroftOutput {
  HopcroftOutput() {}

  KJ_DISALLOW_COPY(HopcroftOutput);

  kj::Array<uint32_t> partition_;
  uint32_t numPartitions_;

  kj::ArrayPtr<const uint32_t> getPartition() const { return partition_; }
  uint32_t getNumPartitions() const { return numPartitions_; }

  void setNumPartitions(uint32_t numPartitions) { numPartitions_ = numPartitions; }
  kj::ArrayPtr<uint32_t> initPartition(size_t n) { partition_ = kj::heapArray<uint32_t>(n); return partition_; }
};

void hopcroft(const HopcroftInput& input, HopcroftOutput* output);
