@0xe990a23bc37ab754;

# A representation of the "inverse" transition function: given a state,
# compute all states which lead to it (and by what action they came.)
# We store this in a packed representation to improve locality.
#
# states/actions = A1 A2 A3 B1 B2 C1 C2 C3
# index          = 0        3     5        8
#
# We store these as two separate lists so states/actions can avoid
# padding.
struct PackedInverse {
  states @0 :List(UInt32);
  actions @1 :List(UInt8);
  index @2 :List(UInt32);
}

struct HopcroftInput {
  numStates @0 :UInt32;
  numActions @1 :UInt8;
  # NB: We don't ask for the non-inverted transition matrix, because
  # Hopcroft doesn't actually need it, and sometimes there is a
  # compressed form of the forward transition matrix, but not the
  # backwards one.
  inverse @2 :PackedInverse;
  # Indexing: state (to partition id)
  initialPartition @3 :List(UInt32);
}

struct HopcroftOutput {
  partition @0 :List(UInt32);
  numPartitions @1 :UInt32;
}

