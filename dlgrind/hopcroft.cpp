#include <dlgrind/schema.capnp.h>

#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <kj/debug.h>

using partition_t = uint32_t;
using state_t = uint32_t;
using action_t = uint8_t;

namespace std {
  template<typename T>
  inline void hash_combine(std::size_t& seed, const T& val) {
    std::hash<T> hasher;
    seed ^= hasher(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  // taken from https://stackoverflow.com/a/7222201/916549
  template<typename S, typename T>
  struct hash<std::pair<S, T>> {
    inline size_t operator()(const std::pair<S, T>& val) const {
      size_t seed = 0;
      hash_combine(seed, val.first);
      hash_combine(seed, val.second);
      return seed;
    }
  };
}

struct State {
  partition_t partition_;
  State(partition_t partition) : partition_(partition) {}
};

struct Partition {
  std::unordered_set<state_t> states_;
};

void hopcroft(HopcroftInput::Reader input, HopcroftOutput::Builder* output) {
  auto numStates = input.getNumStates();
  auto numActions = input.getNumActions();
  KJ_LOG(INFO, numStates, numActions);
  auto inverse = input.getInverse();
  auto inverseStates = inverse.getStates();
  auto inverseActions = inverse.getActions();
  auto inverseIndex = inverse.getIndex();

  auto initialPartition = input.getInitialPartition();

  // NB: It's *possible* that you might want to run this state minimizer
  // on state machines with more than two billion states.  But I'm not
  // supporting it.

  // Validate input
  KJ_REQUIRE(numStates + 1 == inverseIndex.size(),
             numStates, inverseIndex.size());
  KJ_REQUIRE(numStates == initialPartition.size(),
             numStates, initialPartition.size());
  KJ_REQUIRE(inverseStates.size() == inverseActions.size(),
             inverseStates.size(), inverseActions.size())
  {
    uint32_t prev_i = 0;
    for (state_t s = 0; s < numStates + 1; s++) {
      KJ_REQUIRE(inverseIndex[s] >= prev_i);
      KJ_REQUIRE(inverseIndex[s] <= inverseStates.size());
      prev_i = inverseIndex[s];
    }
    KJ_ASSERT(prev_i == inverseStates.size());
  }
  for (uint32_t i = 0; i < inverseIndex[numStates]; i++) {
    state_t s = inverseStates[i];
    action_t a = inverseActions[i];
    KJ_REQUIRE(s < numStates, i, s, numStates);
    KJ_REQUIRE(a < numActions, i, a, numActions);
  }

  // Setup partitions
  std::vector<Partition> partitions;
  std::vector<State> states; // mutable copy of initialPartition
  {
    state_t s = 0;
    for (partition_t p : initialPartition) {
      if (p >= partitions.size()) partitions.resize(p + 1);
      partitions[p].states_.insert(s);
      states.emplace_back(p);
      s++;
    }
  }

  // Do Hopcroft's algorithm (with shitty data structures)
  std::unordered_set<std::pair<state_t, action_t>> waiting;
  for (partition_t p = 0; p < partitions.size(); p++) {
    for (action_t a = 0; a < numActions; a++) {
      waiting.insert({p, a});
    }
  }

  // while WAITING not empty do
  while (waiting.size()) {
    // select and delete any integer i from WAITING
    partition_t p;
    action_t a;
    std::tie(p, a) = *waiting.begin();
    waiting.erase({p, a});

    // INVERSE <- f^-1(B[i])
    std::unordered_set<state_t> inverse;
    for (state_t s : partitions[p].states_) {
      for (uint32_t i = inverseIndex[s]; i < inverseIndex[s+1]; i++) {
        if (inverseActions[i] != a) continue;
        inverse.insert(inverseStates[i]);
      }
    }

    // for each j such that B[j] /\ INVERSE != {} and
    // B[j] not subset of INVERSE (this list of j is
    // stored in jlist)
    std::unordered_map<partition_t, std::vector<state_t>> jlist;
    for (state_t s : inverse) {
      partition_t q = states[s].partition_;
      jlist[q].emplace_back(s);
      if (jlist[q].size() == partitions[q].states_.size()) {
        jlist.erase(q);
      }
    }
    for (auto q_qstates : jlist) {
      // q <- q+1
      // create a new block B[q]
      partition_t r = partitions.size();
      partitions.emplace_back();
      partition_t q = q_qstates.first;
      const auto& qstates = q_qstates.second;
      // B[q] <- B[j] /\ INVERSE
      partitions[r].states_.insert(qstates.begin(), qstates.end());
      // B[j] <- B[j] - B[q]
      for (state_t n : qstates) {
        states[n].partition_ = r;
        partitions[q].states_.erase(n);
      }
      // if j is in WAITING, then add q to WAITING
      for (action_t a = 0; a < numActions; a++) {
        if (waiting.count({q, a})) {
          waiting.insert({r, a});
        } else {
          // if |B[j]| <= |B[q]| then
          //  add j to WAITING
          // else add q to WAITING
          if (partitions[r].states_.size() <= partitions[q].states_.size()) {
            waiting.insert({r, a});
          } else {
            waiting.insert({q, a});
          }
        }
      }
    }
  }
  KJ_LOG(INFO, partitions.size(), "after reduction");
  {
    partition_t p = 0;
    for (const auto& partition : partitions) {
      for (state_t s : partition.states_) {
        KJ_LOG(INFO, p, s);
      }
      p++;
    }
  }

  auto partition = output->initPartition(numStates);
  for (state_t s = 0; s < numStates; s++) {
    partition.set(s, states[s].partition_);
  }
}
