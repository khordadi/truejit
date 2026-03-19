import json
import subprocess
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from pathlib import Path
from Benchmarking import TrueJIT
from coremark.benchmark import benchmark
from ffmpeg.benchmark import ffmpeg
from jetstream.suite import jetstream
from mibench.suite import mibench
from spec.suite import spec
from sqlite.benchmark import sqlite
from wabench.suite import wabench
from polybench.suite import suite as polybench
from npb.suite import suite as npb


def train(application, history_file, model_file, window_size=1):
    torch.cuda.is_available()

    with open(history_file, 'r') as f:
        history = json.load(f)
        histories = [history]

    out = subprocess.run(["wasm-objdump", "-h", application], check=True, text=True, capture_output=True)
    lines = out.stdout.split("\n")
    # get the line that has "Function" in it
    imports = [line for line in lines if "Import" in line][0]
    imports = imports.split(" ")[-1]
    imports = int(imports)
    functions = [line for line in lines if "Function" in line][0]
    functions = functions.split(" ")[-1]
    functions = int(functions)

    functions = functions + imports

    # use gpu
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f"Using {device}")
    # if cude is available, clear the cache
    if torch.cuda.is_available():
        torch.cuda.empty_cache()

    # Custom clazz class
    class HistoryDataset(Dataset):
        def __init__(self, histories, window_size):
            self.data = []
            self.labels = []
            for dataset in histories:
                if len(dataset) == 0:
                    continue

                self.data += [[0] * (window_size - i - 1) + dataset[:i + 1] for i in range(window_size - 1)]
                self.labels += dataset[1:window_size]

                self.data += [dataset[i - window_size:i] for i in range(window_size, len(dataset))]
                self.labels += dataset[window_size:]

            self.data = torch.tensor(self.data, dtype=torch.long)
            self.labels = torch.tensor(self.labels, dtype=torch.long)

        def __len__(self):
            return len(self.data)

        def __getitem__(self, idx):
            return self.data[idx], self.labels[idx]

    # Create clazz and dataloader
    dataset = HistoryDataset(histories, window_size)
    dataloader = DataLoader(dataset, batch_size=128, shuffle=True)

    # The LSTM model
    class LSTMModel(nn.Module):
        def __init__(self, vocab_size, embed_size, hidden_size, output_size):
            super(LSTMModel, self).__init__()
            self.embedding = nn.Embedding(vocab_size, embed_size)
            self.lstm = nn.LSTM(embed_size, hidden_size, batch_first=True)
            self.fc = nn.Linear(hidden_size, output_size)

        def forward(self, x):
            x = x.clamp(0, self.embedding.num_embeddings - 1)
            embeds = self.embedding(x)
            lstm_out, _ = self.lstm(embeds)
            output = self.fc(lstm_out[:, -1, :])
            return output

    # Parameters
    embed_size = 32
    hidden_size = 64

    # Instantiate the model, loss function, and optimizer
    model = LSTMModel(functions, embed_size, hidden_size, functions).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=0.1)

    # Training loop
    epochs = 100
    for epoch in range(epochs):
        model.train()
        for batch_data, batch_labels in dataloader:
            batch_data, batch_labels = batch_data.to(device), batch_labels.to(device)
            optimizer.zero_grad()
            output = model(batch_data)
            loss = criterion(output, batch_labels)
            loss.backward()
            optimizer.step()

        if (epoch + 1) % 10 == 0:
            print(f'Epoch: {epoch + 1:3}\t\tLoss: {loss.item():.10f}')

    # move the model to cpu
    model = model.cpu()

    # Save the model
    torch.jit.save(torch.jit.script(model.eval()), model_file)


if __name__ == '__main__':
    benchmarks = []
    benchmarks.extend(jetstream.benchmarks)
    benchmarks.extend([benchmark])
    benchmarks.extend([sqlite])
    benchmarks.extend([ffmpeg])
    benchmarks.extend(mibench.benchmarks)
    benchmarks.extend(spec.benchmarks)
    benchmarks.extend(npb('S').benchmarks)
    benchmarks.extend(polybench('mini').benchmarks)
    benchmarks.extend(wabench.benchmarks)

    truejit = TrueJIT()

    for benchmark in benchmarks:
        history_file = '/tmp/history.json'
        profile_file = '/tmp/profile.json'
        env = {'HISTORY': history_file, 'PROFILE': profile_file}
        # truejit.run(benchmark.binary, benchmark.workloads[0], env=env)

        # model_path = Path(str(benchmark.binary).replace('benchmarks', 'models'))
        # model_path = model_path.with_suffix('.pt')
        # model_path.parent.mkdir(parents=True, exist_ok=True)
        # train(benchmark.binary, history_file, model_path)

        # oracle_history_path = Path(str(benchmark.binary).replace('benchmarks', 'histories'))
        # oracle_history_path = oracle_history_path.with_suffix('.json')
        # oracle_history_path.parent.mkdir(parents=True, exist_ok=True)
        # cp history_file oracle_history_path
        # Path(history_file).replace(oracle_history_path)

        plan_path = Path(str(benchmark.binary).replace('benchmarks', 'plans'))
        plan_path = plan_path.with_suffix('.json')
        plan_path.parent.mkdir(parents=True, exist_ok=True)
        # /truejit/release/tools/callgraph -i {oracle_history_path} -o {plan_path}
        subprocess.run(['/truejit/release/tools/callgraph', '-i', str(benchmark.binary), '-o', str(plan_path)], check=True)
