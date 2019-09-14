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
