#!/usr/bin/env python3

# Compute how much of the SP value state space is reachable
# based on the SP gains of combo and FS.  The larger the SP
# state space, the harder it is to optimize.

import heapq

def accumu(lis):
    total = 0
    for x in lis:
        total += x
        yield total

sps = {}

# sword
x_sp = [150, 150, 196, 265, 391]
sps['sword'] = list(accumu(x_sp)) + [345]

# blade
x_sp = [130, 130, 220, 360, 660]
sps['blade'] = list(accumu(x_sp)) + [200]

# dagger
x_sp = [144, 144, 264, 288, 288]
sps['dagger'] = list(accumu(x_sp)) + [288]

# axe
x_sp = [200, 240, 360, 380, 420]
sps['axe'] = list(accumu(x_sp)) + [300]

# lance
x_sp = [120, 240, 120, 480, 600]
sps['lance'] = list(accumu(x_sp)) + [400]

# bow
x_sp = [184, 92, 276, 414, 529]
sps['bow'] = list(accumu(x_sp)) + [460]

# wand
x_sp = [130, 200, 240, 430, 600]
sps['wand'] = list(accumu(x_sp)) + [400]

# staff
x_sp = [232, 232, 348, 464, 696]
sps['staff'] = list(accumu(x_sp)) + [580]

for k, the_sps in sps.items():

    pq = the_sps[:]
    heapq.heapify(pq)
    seen = set(pq)

    for _ in range(1000):
        sp = heapq.heappop(pq)
        for delta_sp in the_sps:
            new_sp = sp + delta_sp
            if new_sp not in seen:
                heapq.heappush(pq, new_sp)
                seen.add(new_sp)

    print("{}: {}% coverage at {} SP".format(k, round(len(seen) / max(seen) * 100, 1), max(seen)))
