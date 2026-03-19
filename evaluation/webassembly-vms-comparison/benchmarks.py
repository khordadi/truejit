from dataclasses import dataclass
from pathlib import Path


@dataclass
class BenchmarkSuite:
    name: str
    base_dir: Path
    benchmarks: list[object]


spec = BenchmarkSuite(
    "spec",
    Path("/truejit/benchmarks/spec"),
    [
        ("lbm", ["64", Path("reference.dat"), "0", "1", Path("100_100_130_ldc.of")]),
        ("mcf", [Path("inp.in")]),
        (
            "x264",
            [
                "--dumpyuv",
                "50",
                "--frames",
                "142",
                "-o",
                Path("in.264"),
                Path("in.yuv"),
                "1280x720",
            ],
        ),
        ("nab", ["aminos", "391519156", "1000"]),
        ("namd", ["--input", Path("apoa1.input"), "--iterations", "2"]),
    ],
)

wabench = BenchmarkSuite(
    "wabench",
    Path("/truejit/benchmarks/wabench"),
    [
        ("bzip2", ["-k", "-f", "-z", Path("bzip2.wasm")]),
        (
            "espeak",
            ["-f", Path("input.txt"), "-s", "120", "-w", Path("output_file.wav")],
        ),
        ("facedetection", [Path("input.png")]),
        ("gnuchess", ["<", Path("input")]),
        ("mnist", []),
        ("snappy", []),
        ("whitedb", []),
    ],
)

mibench = BenchmarkSuite(
    "mibench",
    Path("/truejit/benchmarks/mibench"),
    [
        ("basicmath", []),
        ("bitcount", ["1125000"]),
        (
            "cjpeg",
            [
                "-dct",
                "int",
                "-progressive",
                "-opt",
                "-outfile",
                Path("output_large_encode.jpeg"),
                Path("input_large.ppm"),
            ],
        ),
        (
            "djpeg",
            [
                "-dct",
                "int",
                "-ppm",
                "-outfile",
                Path("output_large_decode.ppm"),
                Path("input_large.jpg"),
            ],
        ),
        ("stringsearch", []),
        (
            "blowfish-decode",
            [
                "d",
                Path("input_large.enc"),
                Path("output_large.asc"),
                "1234567890abcdeffedcba0987654321",
            ],
        ),
        (
            "blowfish-encode",
            [
                "e",
                Path("input_large.asc"),
                Path("output_large.enc"),
                "1234567890abcdeffedcba0987654321",
            ],
        ),
        (
            "rijndael-decode",
            [
                Path("input_large.enc"),
                Path("output_large.dec"),
                "d",
                "1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321",
            ],
        ),
        (
            "rijndael-encode",
            [
                Path("input_large.asc"),
                Path("output_large.enc"),
                "e",
                "1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321",
            ],
        ),
        ("sha", [Path("input_large.asc")]),
        ("rawcaudio", ["<", Path("large.pcm")]),
        ("rawdaudio", ["<", Path("large.adpcm")]),
        ("crc32", [Path("large.pcm")]),
    ],
)

polybench = BenchmarkSuite(
    "polybench",
    # Path("/truejit/benchmarks/polybench/bin/extra-large"),
    Path("/truejit/benchmarks/polybench/bin/mini"),
    [
        (f"{benchmark}", [])
        for benchmark in [
        "2mm",
        "3mm",
        "adi",
        "atax",
        "bicg",
        "cholesky",
        "correlation",
        "covariance",
        "deriche",
        "doitgen",
        "durbin",
        "fdtd-2d",
        "floyd-warshall",
        "gemm",
        "gemver",
        "gesummv",
        "gramschmidt",
        "heat-3d",
        "jacobi-1d",
        "jacobi-2d",
        "lu",
        "ludcmp",
        "mvt",
        "nussinov",
        "seidel-2d",
        "symm",
        "syr2k",
        "syrk",
        "trisolv",
        "trmm",
    ]
    ],
)

npb = BenchmarkSuite(
    "npb",
    Path("/truejit/benchmarks/npb/bin"),
    [(f"{benchmark}", []) for benchmark in ["bt", "cg", "ep", "ft", "is", "lu", "mg", "sp"]],
)

coremark = BenchmarkSuite("coremark", Path("/truejit/benchmarks/coremark"), [("coremark", [])])

jetstream = BenchmarkSuite(
    "jetstream",
    Path("/truejit/benchmarks/jetstream/bin"),
    [(f"{benchmark}", []) for benchmark in ["gcc-loops", "float-mm", "HashSet", "quicksort"]] + [("tsf", ["100"])],
)

ffmpeg = BenchmarkSuite(
    "benchmark",
    Path("/truejit/benchmarks"),
    [
        ("benchmark", ["-y", "-opt", Path("in.mov"), Path("out.mp4")]),
        # ("benchmark", ["-version"]),
    ],
)

sqlite = BenchmarkSuite(
    "sqlite",
    Path("/truejit/benchmarks/sqlite"),
    [
        ("sqlite", [Path("test.db"), Path("query.sql")]),
    ],
)