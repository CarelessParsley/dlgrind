import os
import sys
import capnp

BASE = os.path.realpath(os.path.join(os.path.dirname(__file__), ".."))

schema_capnp = capnp.load(os.path.join(BASE, "dlgrind/schema.capnp"))
sys.path.insert(0, os.path.join(BASE, "dl"))

# Necessary for core.log
sys.argv = []

# Required imports due to import order shenanigans
import core.timeline
import core.condition
import core
import conf as globalconf

import adv.erik as m

# Pasted from run()
this = m.module()()
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

print(this.base_att)
# Need to dump these separately, because may need to apply
# changes to individual brackets from buffs
print(this.all_modifiers)
print(this.def_mod())  # Should always be 1.0
print(this.conf)

"""
import conf
import slot

slots = slot.Slots()
print(slots)
print(conf.get('Erik').slot_common[0](slots))
"""
