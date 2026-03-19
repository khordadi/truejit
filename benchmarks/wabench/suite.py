from Benchmarking import Suite
from wabench.bzip2.benchmark import bzip2
from wabench.espeak.benchmark import espeak
from wabench.facedetection.benchmark import facedetection
from wabench.gnuchess.benchmark import gnuchess
from wabench.mnist.benchmark import mnist
from wabench.snappy.benchmark import snappy
from wabench.whitedb.benchmark import whitedb

wabench = Suite('WABench',
                [
                    bzip2,
                    espeak,
                    facedetection,
                    gnuchess,
                    mnist,
                    snappy,
                    whitedb
                ])
