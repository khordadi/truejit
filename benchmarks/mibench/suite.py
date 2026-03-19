from Benchmarking import Suite
from mibench.basicmath.benchmark import basicmath
from mibench.bitcount.benchmark import bitcount
from mibench.blowfish.benchmark import blowfish_encode, blowfish_decode
from mibench.cjpeg.benchmark import cjpeg
from mibench.crc32.benchmark import crc32
from mibench.djpeg.benchmark import djpeg
from mibench.rawcaudio.benchmark import rawcaudio
from mibench.rawdaudio.benchmark import rawdaudio
from mibench.rijndael.benchmark import rijndael_encode, rijndael_decode
from mibench.sha.benchmark import sha
from mibench.stringsearch.benchmark import stringsearch

mibench = Suite('MiBench',
                [
                    basicmath,
                    bitcount,
                    stringsearch,
                    cjpeg,
                    djpeg,
                    rawcaudio,
                    rawdaudio,
                    blowfish_encode, blowfish_decode,
                    rijndael_encode, rijndael_decode,
                    crc32,
                    sha,
                ])
