from coremark.benchmark import benchmark as coremark


def jsonify(benchmark):
    args = [benchmark.binary]
    wl = benchmark.workloads[0]
    args += [str(arg) for arg in wl.args]
    return {
        'Name': benchmark.name,
        'Args': args,
        'PreOpens': [str(pre_open) for pre_open in wl.pre_opens],
    }


def main():
    print(jsonify(coremark))


if __name__ == '__main__':
    main()
