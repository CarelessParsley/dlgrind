@0xb227cfade86c9b47;

struct TimingStat {
  startup @0 :UInt32;  # frames
  recovery @1 :UInt32;  # frames
}

struct ActionStat {
  dmg @0 :UInt32;  # mod
  # SP gain for x and fs; SP cost for skills
  sp @1 :UInt32;
  timing @2 :TimingStat;
  # what is this exactly?
  iv @3 :UInt32;  # frames (I think), optional (bow, wand, staff)
}

enum MeleeOrRanged {
  melee @0;
  ranged @1;
}

enum WeaponType {
  axe @0;
  blade @1;
  bow @2;
  dagger @3;
  lance @4;
  staff @5;
  sword @6;
  wand @7;
}

struct WeaponClass {
  xtype @0 :MeleeOrRanged;
  xStats @1 :List(ActionStat);
  fsStat @2 :ActionStat;
  xfsStartups @3 :List(UInt32);  # frames, optional (axe, dagger, sword x1fs only)
  fsfTiming @4 :TimingStat;
  dfsTiming @5 :TimingStat;  # optional (bow)
  dodgeRecovery @6 :UInt32;  # frames
  wtype @7 :WeaponType;
}

enum Element {
  flame @0;
  water @1;
  wind @2;
  light @3;
  shadow @4;
}

enum WeaponName {
  axe5b1 @0;
  axe5b2 @1;
}

struct Weapon {
  name @0 :WeaponName;
  wtype @1 :WeaponType;
  s3Stat @2 :ActionStat;
  attack @3 :UInt16;
}

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
}

enum Action {
  x @0;
  fs @1;
  s1 @2;
  s2 @3;
  s3 @4;
}

enum AfterAction {
  afterNothing @0;
  afterS1 @1;
  afterS2 @2;
  afterS3 @3;
  afterC1 @4;
  afterC2 @5;
  afterC3 @6;
  afterC4 @7;
  afterC5 @8;
  afterFs @9;
}

enum AdventurerName {
  erik @0;
}

struct Adventurer {
  s1Stat @0 :ActionStat;
  s2Stat @1 :ActionStat;
  name @2 :AdventurerName;
}

# TODO: Do I ever need this rep for anything?
struct AdventurerState {
  afterAction @0 :AfterAction;
  # UI recovery is 114 frames; therefore, 8-bit is enough
  uiHiddenFramesLeft @1 :UInt8;
  sp @2 :List(UInt16);
}
