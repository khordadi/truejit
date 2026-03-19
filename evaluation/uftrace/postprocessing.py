import sys
import json


def main():
    sequences = []
    for line in sys.stdin:
        j = json.loads(line)
        sequences.extend(j)

    # sort by timestamp
    sequences.sort(key=lambda x: x[0])

    # remove timestamps
    sequences = [name for (_, name) in sequences]

    # remove repeated function benchmarks and keep the order
    unique_sequences = []
    for name in sequences:
        if name not in unique_sequences:
            unique_sequences.append(name)

    # to json
    unique_sequences = json.dumps(unique_sequences)

    print(unique_sequences, file=sys.stderr)


if __name__ == '__main__':
    main()
