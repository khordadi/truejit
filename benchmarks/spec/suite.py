from Benchmarking import Suite
from jetstream.float_mm.benchmark import float_mm
from jetstream.gcc_loops.benchmark import gcc_loops
from jetstream.hashset.benchmark import hashset
from jetstream.quicksort.benchmark import quicksort
from jetstream.tsf.benchmark import tsf
from spec.lbm.benchmark import lbm
from spec.mcf.benchmark import mcf
from spec.nab.benchmark import nab
from spec.namd.benchmark import namd
from spec.x264.benchmark import x264

spec = Suite('SPEC', [lbm, mcf, nab, namd, x264])
