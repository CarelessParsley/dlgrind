#!/usr/bin/env python3

import os
import sys
import capnp
import argparse
import importlib.util

# Do argument parsing first, as we must clear sys.argv
# to import dl sim modules
parser = argparse.ArgumentParser(description='Extract configuration from b1ueb1ues sim')
parser.add_argument('adv', metavar='ADV', help='Adventurer script in dl/adv/ to get config from')
args = parser.parse_args()

# Preload "common" modules from dl sim
BASE = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(BASE, "dl"))
sys.path.insert(0, os.path.join(BASE, "dl", "adv"))
sys.argv = []  # Necessary for core.log
import core.timeline  # Required! (Must populate core)
import core.condition  # Required! (Must populate core)
import core
import conf as globalconf

# Import adventurer module
search_candidates = [
    args.adv,
    os.path.join(BASE, "dl", "adv", args.adv),
    os.path.join(BASE, "dl", "adv", args.adv + ".py")
]
adv_fn = None
for candidate_adv_fn in search_candidates:
    if os.path.exists(candidate_adv_fn):
        adv_fn = candidate_adv_fn
        break
if adv_fn is None:
    raise RuntimeError(f"Could not find {args.adv}")
spec = importlib.util.spec_from_file_location("__adventurer__", adv_fn)
adventurer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adventurer)

# Do initial configuration (this is cribbed from Adv.run)
this = adventurer.module()()
this.ctx.on()
this.doconfig()
for i in this.conf.mod:
    v = this.conf.mod[i]
    if type(v) == tuple:
        this.slots.c.mod.append(v)
    if type(v) == list:
        this.slots.c.mod += v
if this.a1 :
    this.slots.c.a.append(this.a1)
if this.a2 :
    this.slots.c.a.append(this.a2)
if this.a3 :
    this.slots.c.a.append(this.a3)
this.equip()
this.setup()
this.d_slots()
this.slot_backdoor()
this.base_att = this.slots.att(globalconf.forte)
this.slots.oninit(this)
# NB: don't initialize any time zero buffs; the sim
# will manage that

# Load capnp schema
schema_capnp = capnp.load(os.path.join(BASE, "dlgrind/schema.capnp"))

# Do serialization
wout = schema_capnp.Config.new_message()

def setTimingStat(stat, prefix):
    stat.recovery = round(this.conf[prefix + '.recovery'] * 60)
    stat.startup = round(this.conf[prefix + '.startup'] * 60)

def setActionStat(stat, prefix):
    stat.dmg = round(this.conf[prefix + '.dmg'] * 100)
    stat.sp = this.conf[prefix + '.sp']
    setTimingStat(stat.timing, prefix)

def to_camel_case(snake_str):
    components = snake_str.split('_')
    return components[0].lower() + ''.join(x.title() for x in components[1:])

setActionStat(wout.adventurer.s1Stat, 's1')
setActionStat(wout.adventurer.s2Stat, 's2')
wout.adventurer.name = to_camel_case(this.__class__.__name__)
wout.adventurer.baseStrength = int(this.base_att)
for mod in this.all_modifiers:
    if mod.mod_type == 'crit':
        if mod.mod_order == 'chance':
            wout.adventurer.modifiers.critRate += mod.mod_value
        elif mod.mod_order == 'damage':
            wout.adventurer.modifiers.critDmg += mod.mod_value
        else:
            assert False, mod.mod_order
    elif mod.mod_type == 'att':
        assert mod.mod_order == 'passive'
        wout.adventurer.modifiers.strength += mod.mod_value
    elif mod.mod_type == 's':
        wout.adventurer.modifiers.skillDmg += mod.mod_value
    elif mod.mod_type == 'fs':
        wout.adventurer.modifiers.fsDmg += mod.mod_value
    else:
        assert False, mod.mod_type

setActionStat(wout.weapon.s3Stat, 's3')
wout.weapon.wtype = this.slots.w.wt
wout.weapon.name = this.slots.w.__class__.__name__

wout.weaponClass.wtype = this.slots.w.wt  # TODO: get rid of dupe
xStats = wout.weaponClass.init('xStats', 5)
if 'x1fs' in this.conf:
    xfsStartups = wout.weaponClass.init('xfsStartups', 5)
for i in range(5):
    fs_prefix = "x{}fs".format(i + 1)
    setActionStat(xStats[i], "x{}".format(i + 1))
    if fs_prefix in this.conf:
        xfsStartups[i] = round(getattr(this.conf, fs_prefix).startup * 60)
setActionStat(wout.weaponClass.fsStat, 'fs')
setTimingStat(wout.weaponClass.fsfTiming, 'fsf')
if 'dfs' in this.conf:
    setTimingStat(wout.weaponClass.dfsTiming, 'dfs')
wout.weaponClass.dodgeRecovery = round(this.conf.dodge.recovery * 60)

# Dump the binary, or print it human readably

if sys.stdout.isatty():
    print("# Detected terminal, printing in human readable format", file=sys.stderr)
    print(wout)
else:
    wout.write(sys.stdout)
