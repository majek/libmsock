#!/usr/bin/env python
import os, sys, math

times = []

for i in range(10):
    p = os.popen("./example05")
    d = p.read()
    p.close()
    del p
    total, message, process = d.split(",")
    str_ns, _ = message.split("ns")
    ns = float(str_ns)
    times.append( ns )
    os.write(1, ".")

print

def avg_dev(arr):
    avg = sum(arr) / len(arr)
    sum_sq = sum( (x*x for x in arr) )
    dev = math.sqrt( (sum_sq / len(arr)) - (avg*avg) )
    return avg, dev


avg, dev = avg_dev(times)
print "avg:%.3fns dev:%.3fns" % (avg, dev)
