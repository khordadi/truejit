from pathlib import Path


class Recorder:
    class Record:
        def __init__(self, collectables):
            self.collectables = collectables
            self.metrics = {}
            for collectable in self.collectables:
                self.metrics[collectable] = metric(collectable)

        def __str__(self, delimiter=';'):
            return delimiter.join([f"{self.metrics[k]}" for k in self.collectables])

    def __init__(self, config, collectables):
        self.config = config
        self.collectables = collectables

    delimiter = ';'

    def header(self):
        config_hdr = self.delimiter.join(self.config)
        collectables_hdr = self.delimiter.join(self.collectables)
        return config_hdr + self.delimiter + collectables_hdr

    def record(self, config):
        record = Recorder.Record(self.collectables)
        return self.delimiter.join([str(c) for c in config]) + self.delimiter + str(record)


class Reporter:
    def __init__(self, path="/tmp/report.csv"):
        self.path = Path(path)
        if self.path.exists():
            self.path.unlink()
            self.path.touch()

    def append(self, content):
        with open(self.path, 'a') as file:
            file.write(content + '\n')


def metric(name):
    path = Path(f"/tmp/{name}.txt")
    if path.exists():
        return path.read_text().strip()
    else:
        raise FileNotFoundError(f"Metric file {path} does not exist.")
