#include <dlgrind/main.h>
#include <dlgrind/schema.capnp.h>
#include <dlgrind/hopcroft.h>
#include <dlgrind/simulator.h>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <magic_enum.h>

#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>

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

class DLGrindOpt : DLGrind {
public:
  explicit DLGrindOpt(kj::ProcessContext& context)
      : DLGrind(context) {}
  kj::MainFunc getMain() {
    return kj::MainBuilder(context_, "dlgrind-opt",
        "Compute optimal rotations for characters in Dragalia Lost")
      .addOptionWithArg({'c', "config"}, KJ_BIND_METHOD(*this, setConfig),
          "<filename>", "Read config from <filename>.")
      .callAfterParsing(KJ_BIND_METHOD(*this, run))
      .build();
  }

  kj::MainBuilder::Validity run() {
    readConfig();

    // Make IO faster
    std::ios_base::sync_with_stdio(false);

    // Compute reachable states
    using InverseMap = AdventurerStateMap<std::vector<std::pair<AdventurerState, Action>>>;
    InverseMap inverse_map;
    size_t inverse_size = 0;
    {
      AdventurerState init_st;
      std::vector<AdventurerState> todo{init_st};
      inverse_map[init_st];
      while (todo.size()) {
        auto s = todo.back();
        // KJ_LOG(INFO, s.afterAction_, s.uiHiddenFramesLeft_, s.sp_[0], s.sp_[1], s.sp_[2], "loop");
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

    // Number states
    AdventurerStateMap<state_code_t> state_encode;
    std::vector<AdventurerState> state_decode;

    for (const auto& kv : inverse_map) {
      state_encode.emplace(kv.first, state_decode.size());
      state_decode.emplace_back(kv.first);
    }

    // Number actions
    std::unordered_map<Action, action_code_t> action_encode;
    std::vector<Action> action_decode;

    for (auto val : magic_enum::enum_values<Action>()) {
      action_encode.emplace(val, action_decode.size());
      action_decode.emplace_back(val);
    }

    // Minimize states
    capnp::MallocMessageBuilder hopcroft_input_msg;
    auto hopcroft_input = hopcroft_input_msg.initRoot<HopcroftInput>();
    {
      hopcroft_input.setNumStates(state_decode.size());
      hopcroft_input.setNumActions(action_decode.size());

      {
        auto inverse = hopcroft_input.initInverse();
        auto states = inverse.initStates(inverse_size);
        auto actions = inverse.initActions(inverse_size);
        auto index = inverse.initIndex(state_decode.size() + 1);
        size_t inverse_index = 0;
        for (state_code_t i = 0; i < state_decode.size(); i++) {
          index.set(i, inverse_index);
          for (const auto& sa : inverse_map[state_decode[i]]) {
            state_code_t s_code = state_encode[sa.first];
            action_code_t a_code = action_encode[sa.second];
            states.set(inverse_index, s_code);
            actions.set(inverse_index, a_code);
            inverse_index++;
          }
        }
        KJ_ASSERT(inverse_index == inverse_size, inverse_index, inverse_size);
        index.set(state_decode.size(), inverse_index);
      }

      auto initialPartition = hopcroft_input.initInitialPartition(state_decode.size());
      AdventurerStateMap<uint32_t> partition_map;
      for (state_code_t i = 0; i < state_decode.size(); i++) {
        AdventurerState s = state_decode[i];
        // coarsen the state
        s.sp_[0] = 0;
        s.sp_[1] = 0;
        s.sp_[2] = 0;
        s.buffFramesLeft_ = s.buffFramesLeft_ != 0;
        auto it = partition_map.find(s);
        uint32_t v;
        if (it == partition_map.end()) {
          v = partition_map.size();
          partition_map.emplace(s, v);
        } else {
          v = it->second;
        }
        initialPartition.set(i, v);
      }
      KJ_LOG(INFO, partition_map.size(), "initial number of partitions");
    }
    capnp::MallocMessageBuilder hopcroft_output_msg;
    auto hopcroft_output = hopcroft_output_msg.initRoot<HopcroftOutput>();

    hopcroft(hopcroft_input, &hopcroft_output);

    return true;
  }
};

KJ_MAIN(DLGrindOpt);
