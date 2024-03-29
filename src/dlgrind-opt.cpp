#include <dlgrind/main.h>
#include <dlgrind/schema.capnp.h>
#include <dlgrind/hopcroft.h>
#include <dlgrind/simulator.h>
#include <dlgrind/action_string.h>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <magic_enum.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <iostream>
#include <chrono>

// Absolute tolerance when comparing DPS floating point for equality.
constexpr double EPSILON = 0.01;

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

// Return the index of an enum in magic_enum::enum_values
template <typename T>
size_t enum_index(T val) {
  size_t i = 0;
  for (auto v : magic_enum::enum_values<T>()) {
    if (v == val) return i;
    i++;
  }
  KJ_ASSERT(0, val, "invalid enum");
}


using state_code_t = uint64_t;
using action_code_t = uint8_t;
using partition_t = uint32_t;

using InverseMap = AdventurerStateMap<std::vector<std::pair<AdventurerState, Action>>>;

struct StateCode {
  AdventurerStateMap<state_code_t> encode_;
  std::vector<AdventurerState> decode_;
};

struct ActionCode {
  std::unordered_map<Action, action_code_t> encode_;
  std::vector<Action> decode_;
};

class DLGrindOpt : DLGrind {
public:
  explicit DLGrindOpt(kj::ProcessContext& context)
      : DLGrind(context) {}
  kj::MainFunc getMain() {
    return kj::MainBuilder(context_, "dlgrind-opt",
        "Compute optimal rotations for characters in Dragalia Lost")
      .addOptionWithArg({'c', "config"}, KJ_BIND_METHOD(*this, setConfig),
          "<filename>", "Read config from <filename>.")
      .addOptionWithArg({"skill-prep"}, KJ_BIND_METHOD(*this, setSkillPrep),
          "<percent>", "Skill prep percentage (e.g., 75).")
      .addOptionWithArg({"num-skills"}, KJ_BIND_METHOD(*this, setNumSkills),
          "<number>", "Number of skills to consider in optimization (e.g. 2 or 3).")
      .addOptionWithArg({"projectile-delay"}, KJ_BIND_METHOD(*this, setProjectileDelay),
          "<frames>", "Frames of delay behind projectile cast and hit (enables precharge).")
      .expectOptionalArg("<frames>", KJ_BIND_METHOD(*this, setFrames))
      .callAfterParsing(KJ_BIND_METHOD(*this, run))
      .build();
  }

  kj::MainBuilder::Validity setFrames(kj::StringPtr frames) {
    frames_ = frames.parseAs<uint32_t>();
    return true;
  }

  kj::MainBuilder::Validity setNumSkills(kj::StringPtr num_skills) {
    sim_.setNumSkills(num_skills.parseAs<size_t>());
    return true;
  }

  kj::MainBuilder::Validity run() {
    readConfig();

    // apply skill prep
    init_state_ = sim_.applyPrep(init_state_, skill_prep_);

    ActionCode action_code;

    PackedInverse inverse;
    std::vector<AdventurerState> partition_reps;
    uint32_t numPartitions;
    partition_t initialPartition;
    {
      StateCode state_code;
      HopcroftInput hopcroft_input;
      {
        auto [ inverse_map, inverse_size ] = computeReachableStates();
        std::tie(state_code, action_code) = numberStatesAndActions(inverse_map);

        // Minimize states
        {
          hopcroft_input.setNumStates(state_code.decode_.size());
          hopcroft_input.setNumActions(action_code.decode_.size());

          {
            auto& inverse = hopcroft_input.initInverse();
            auto states = inverse.initStates(inverse_size);
            auto actions = inverse.initActions(inverse_size);
            auto index = inverse.initIndex(state_code.decode_.size() + 1);
            size_t inverse_index = 0;
            for (state_code_t i = 0; i < state_code.decode_.size(); i++) {
              index[i] = inverse_index;
              for (const auto& sa : inverse_map[state_code.decode_[i]]) {
                state_code_t s_code = state_code.encode_[sa.first];
                action_code_t a_code = action_code.encode_[sa.second];
                states[inverse_index] = s_code;
                actions[inverse_index] = a_code;
                inverse_index++;
              }
            }
            KJ_ASSERT(inverse_index == inverse_size, inverse_index, inverse_size);
            index[state_code.decode_.size()] = inverse_index;
          }

          auto initialPartition = hopcroft_input.initInitialPartition(state_code.decode_.size());
          AdventurerStateMap<partition_t> partition_map;
          for (state_code_t i = 0; i < state_code.decode_.size(); i++) {
            AdventurerState s = state_code.decode_[i];
            // coarsen the state
            for (size_t i = 0; i < 3; i++) {
              s.sp_[i] = 0;
              s.energy_ = s.energy_ == 5;
              s.buffFramesLeft_[i] = s.buffFramesLeft_[i] != 0;
            }
            auto it = partition_map.find(s);
            partition_t v;
            if (it == partition_map.end()) {
              v = partition_map.size();
              partition_map.emplace(s, v);
            } else {
              v = it->second;
            }
            initialPartition[i] = v;
          }
          KJ_LOG(INFO, partition_map.size(), "initial number of partitions");
        }
      }
      HopcroftOutput hopcroft_output;
      hopcroft(hopcroft_input, &hopcroft_output);
      auto partition = hopcroft_output.getPartition();
      numPartitions = hopcroft_output.getNumPartitions();
      initialPartition = partition[state_code.encode_[init_state_]];

      // Redo inverse transition table for partitions
      {
        // Compute it first with shitty data structures.
        // Even if states are equivalent, the states that feed to them
        // may not be: equivalence is a statement about future
        // evolution, not the past!
        std::unordered_map<partition_t, std::unordered_set<std::pair<partition_t, action_code_t>>> inverse_map;
        const auto& old_inverse = hopcroft_input.getInverse();
        auto old_states = old_inverse.getStates();
        auto old_actions = old_inverse.getActions();
        auto old_index = old_inverse.getIndex();
        size_t inverse_size = 0;
        partition_reps.resize(numPartitions);
        for (state_code_t s = 0; s < state_code.decode_.size(); s++) {
          partition_t p = partition[s];
          partition_reps[p] = state_code.decode_[s];  // last one wins
          for (uint32_t i = old_index[s]; i < old_index[s+1]; i++) {
            std::pair<partition_t, action_code_t> pair = {
              partition[old_states[i]],
              old_actions[i]
            };
            auto r = inverse_map[p].emplace(pair);
            if (r.second) {
              inverse_size++;
            }
          }
        }

        // Pack the map now
        auto states = inverse.initStates(inverse_size);
        auto actions = inverse.initActions(inverse_size);
        auto index = inverse.initIndex(numPartitions + 1);
        size_t inverse_index = 0;
        for (partition_t i = 0; i < numPartitions; i++) {
          index[i] = inverse_index;
          for (const auto& pa : inverse_map[i]) {
            states[inverse_index] = pa.first;
            actions[inverse_index] = pa.second;
            inverse_index++;
          }
        }
        KJ_ASSERT(inverse_index == inverse_size, inverse_index, inverse_size);
        KJ_LOG(INFO, inverse_size, "reduced inverse transition matrix");
        index[numPartitions] = inverse_index;
      }
    }

    auto inverse_states = inverse.getStates();
    auto inverse_actions = inverse.getActions();
    auto inverse_index = inverse.getIndex();

    // Compute necessary frame window
    frames_t max_frames = 1;
    for (partition_t p = 0; p < numPartitions; p++) {
      for (size_t i = inverse_index[p]; i < inverse_index[p+1]; i++) {
        auto prev = partition_reps[inverse_states[i]];
        auto a = action_code.decode_[inverse_actions[i]];
        frames_t frames = sim_.computeFrames(prev, a);
        if (frames > max_frames) {
          max_frames = frames + 1;
        }
      }
    }

    int buffer_size = max_frames * numPartitions;
    std::vector<float> best_dps(buffer_size, -1);
    std::vector<ActionString> best_sequence(buffer_size);

    auto dix = [&](int frame, int state_ix) {
      return (frame % max_frames) * numPartitions + state_ix;
    };

    best_dps[dix(0, initialPartition)] = 0;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_print_time = start_time;

    float last_best = 0;
    // This is the bottleneck!
    //  - Snapshotting (so I can continue computing later)
    //  - More state reduction?
    //    - Unsound approximations; e.g., quantize buff / SP time
    //  - Branch bound (we KNOW that this is provably worse,
    //    prune it)
    //    - Same combo, same buff, dps is less, SP is less
    //    - Best case "catch up" for states
    //    - Problem: How to know you've been dominated?  Not so easy
    //      to tell without more scanning.
    //  - [TODO] Improve locality of access?
    //    - Only five actions: bucket them together
    //    - Lay out action_inverses contiguously, so we don't
    //      thrash cache
    //  - Parallelize/Vectorize...
    //    - ...computation of all incoming actions (no
    //      data dependency, reduction at the end)
    //    - ...computation of all states at the same
    //      frame (no data dependency, reduction at the end)
    //    - ...all frames within the minimum frame window
    //      (provably no data dependency.)
    //  - Small optimizations
    //    - Compute best as we go (in the main loop), rather
    //      than another single loop at the end
    for (int f = 1; f < frames_; f++) {
      auto cur_time = std::chrono::high_resolution_clock::now();
      if (cur_time > last_print_time + 1 * std::chrono::seconds(60)) {
        std::cerr << "fpm: " << (f * std::chrono::minutes(1)) / (cur_time - start_time) << "\n";
        last_print_time = cur_time;
      }
      #pragma omp parallel for
      for (int p = 0; p < numPartitions; p++) {
        auto& cur = best_dps[dix(f, p)];
        auto& cur_seq = best_sequence[dix(f, p)];

        // Consider all states which could have lead here
        for (int j = inverse_index[p]; j < inverse_index[p+1]; j++) {
          partition_t prev_p = inverse_states[j];
          AdventurerState prev = partition_reps[prev_p];
          Action a = action_code.decode_[inverse_actions[j]];

          frames_t frames;
          double dmg;
          auto r = sim_.applyAction(prev, a, &frames, &dmg);
          KJ_ASSERT(!!r);

          if (f >= frames) {
            auto z = dix(f - frames, prev_p);
            if (best_dps[z] >= 0) {
              auto tmp = best_dps[z] + dmg;
              if (tmp >= 0 && tmp > cur + EPSILON) {
                cur = tmp;
                cur_seq = best_sequence[z];
                cur_seq.push(a);
              } else if (tmp >= 0 && tmp > cur - EPSILON) {
                ActionString tmp_seq = best_sequence[z];
                tmp_seq.push(a);
                // The idea here is that there are often moves which
                // have transpositions (end up with the same dps and
                // end state); let's define an ordering on our move
                // set and prefer moves that frontload combos to make
                // the chosen combos deterministic.  This helps in
                // testing.
                if (std::lexicographical_compare(
                      cur_seq.buffer_.begin(), cur_seq.buffer_.end(),
                      tmp_seq.buffer_.begin(), tmp_seq.buffer_.end())) {
                  cur = tmp;
                  cur_seq = std::move(tmp_seq);
                }
              }
            }
          }
        }
      }
      float best = -1;
      int best_index = -1;
      int density = 0;
      for (int p = 0; p < numPartitions; p++) {
        auto tmp = best_dps[dix(f, p)];
        if (tmp > best + EPSILON) {
          best = tmp;
          best_index = dix(f, p);
        }
        if (tmp > 0) {
          density++;
        }
      }
      if (best >= 0) {
        if (best >= 0 && best > last_best + EPSILON) {
          std::cout << best_sequence[best_index];
          std::cout << "=> " << best << " dmg in " << f << " frames\n";
          last_best = best;
        }
      }
    }

    auto cur_time = std::chrono::high_resolution_clock::now();
    std::cerr << "fpm: " << (frames_ * std::chrono::minutes(1)) / (cur_time - start_time) << "\n";

    return true;
  }

private:

  // returns inverse_map, inverse_size (number of transitions)
  std::pair<InverseMap, size_t> computeReachableStates() {
    // Compute reachable states
    InverseMap inverse_map;
    size_t inverse_size = 0;
    {
      std::vector<AdventurerState> todo{init_state_};
      inverse_map[init_state_];
      while (todo.size()) {
        auto s = todo.back();
        //KJ_LOG(INFO, s, "loop");
        //std::cout << kj::str(s).cStr() << "\n";
        todo.pop_back();
        auto push = [&](AdventurerState n_s, Action a) {
          if (inverse_map.count(n_s) == 0) {
            todo.emplace_back(n_s);
          }
          inverse_map[n_s].emplace_back(s, a);
          inverse_size++;
        };
        for (auto a : magic_enum::enum_values<Action>()) {
          auto mb_n_s = sim_.applyAction(s, a);
          if (mb_n_s) {
            push(*mb_n_s, a);
          }
        }
      }
    }
    KJ_LOG(INFO, inverse_map.size(), "initial states");
    return {inverse_map, inverse_size};
  }

  std::pair<StateCode, ActionCode> numberStatesAndActions(const InverseMap& inverse_map) {

    StateCode state_code;
    ActionCode action_code;

    // Number states
    for (const auto& kv : inverse_map) {
      state_code.encode_.emplace(kv.first, state_code.decode_.size());
      state_code.decode_.emplace_back(kv.first);
    }

    // Number actions
    for (auto val : magic_enum::enum_values<Action>()) {
      action_code.encode_.emplace(val, action_code.decode_.size());
      action_code.decode_.emplace_back(val);
    }

    return { std::move(state_code), std::move(action_code) };
  }

  frames_t frames_ = 3600;
  AdventurerState init_state_;

};

KJ_MAIN(DLGrindOpt);
