from Benchmarking import Suite
from jetstream.float_mm.benchmark import float_mm
from jetstream.gcc_loops.benchmark import gcc_loops
from jetstream.hashset.benchmark import hashset
from jetstream.quicksort.benchmark import quicksort
from jetstream.tsf.benchmark import tsf

jetstream = Suite('JetStream', [float_mm, gcc_loops, hashset, quicksort, tsf])

if __name__ == '__main__':
    print("hello")
