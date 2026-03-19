import json
import os

import torch
from torch.utils.data import Dataset, DataLoader
from torch import nn, optim
from torch.nn import functional as F


class TraceDataset(Dataset):
    def __init__(self, traces, window_size=3):
        self.data = []
        self.label = []
        for trace in traces:
            for i in range(len(trace) - window_size):
                self.data.append(trace[i:i + window_size])
                self.label.append(trace[i + window_size])

    def __len__(self):
        return len(self.data)

    def __getitem__(self, idx):
        return torch.tensor(self.data[idx], dtype=torch.long), torch.tensor(self.label[idx], dtype=torch.long)


class Vocabulary:
    def __init__(self, traces):
        self.function_to_index = {}
        self.index_to_function = {}
        vocab = set()
        for trace in traces:
            for function in trace:
                vocab.add(function)
        vocab = list(vocab)
        for i, function in enumerate(vocab):
            self.function_to_index[function] = i
            self.index_to_function[i] = function

    def __len__(self):
        return len(self.function_to_index)


class LSTMModel(nn.Module):
    def __init__(self, vocab_size, embed_size, hidden_size):
        super(LSTMModel, self).__init__()
        self.embedding = nn.Embedding(vocab_size, embed_size)
        self.lstm = nn.LSTM(embed_size, hidden_size, batch_first=True)
        self.fc = nn.Linear(hidden_size, vocab_size)

    def forward(self, x):
        x = self.embedding(x)
        x, (h, c) = self.lstm(x)
        x = self.fc(x[:, -1, :])  # Select the last timestep output.
        return x


def train(model, device, train_loader, optimizer, criterion):
    model.train()
    total_loss = 0
    for data, target in train_loader:
        data, target = data.to(device), target.to(device)
        optimizer.zero_grad()
        output = model(data)
        loss = criterion(output, target)
        loss.backward()
        optimizer.step()
        total_loss += loss.item()
    return total_loss / len(train_loader)


def validate(model, device, test_loader):
    model.eval()
    total_loss = 0
    with torch.no_grad():
        for data, target in test_loader:
            data, target = data.to(device), target.to(device)
            output = model(data)
            loss = F.cross_entropy(output, target)
            total_loss += loss.item()
    return total_loss / len(test_loader)


def main():
    device = torch.device("cuda")

    trace_json_path = '/truejit/evaluation/prediction-accuracy/profiling/502.gcc_r.json'
    traces = []
    with open(trace_json_path, 'r') as trace_json_file:
        lines = trace_json_file.readlines()
        # lines = lines[:5]  # development only
        for line in lines:
            traces.append(json.loads(line))

    train_traces = traces[:int(len(traces) * 0.95)]
    test_traces = traces[int(len(traces) * 0.95):]
    print('Train traces:', len(train_traces))
    print('Test traces:', len(test_traces))

    vocab = Vocabulary(traces)
    vocab_size = len(vocab.function_to_index)
    print('Vocab size:', vocab_size)

    train_traces = [list(map(vocab.function_to_index.get, trace)) for trace in train_traces]
    test_traces = [list(map(vocab.function_to_index.get, trace)) for trace in test_traces]

    # if '/truejit/evaluation/prediction-accuracy/model.pth' exists, load the model and return
    if os.path.exists('/truejit/evaluation/prediction-accuracy/model.pth'):
        model = LSTMModel(vocab_size, 128, 256).to(device)
        model.load_state_dict(torch.load('/truejit/evaluation/prediction-accuracy/model.pth'))

    else:
        train_dataset = TraceDataset(train_traces)
        test_dataset = TraceDataset(test_traces)
        print('Train dataset:', len(train_dataset))
        print('Test dataset:', len(test_dataset))

        train_loader = DataLoader(train_dataset, batch_size=4096, shuffle=True)
        test_loader = DataLoader(test_dataset, batch_size=4096, shuffle=False)

        embed_size = 128
        hidden_size = 256

        model = LSTMModel(vocab_size, embed_size, hidden_size).to(device)
        optimizer = optim.Adam(model.parameters(), lr=0.001)
        criterion = nn.CrossEntropyLoss()

        num_epochs = 100
        for epoch in range(num_epochs):
            train_loss = train(model, device, train_loader, optimizer, criterion)
            test_loss = validate(model, device, test_loader)
            print(f'Epoch: {epoch + 1}, Train Loss: {train_loss:.4f}, Test Loss: {test_loss:.4f}')

        # Save the model
        torch.save(model.state_dict(), '/truejit/evaluation/prediction-accuracy/model.pth')

    # Evaluate the model
    model.eval()
    test_dataset = TraceDataset(test_traces)
    test_loader = DataLoader(test_dataset, batch_size=4096, shuffle=False)
    test_loss = validate(model, device, test_loader)
    print(f'Test Loss: {test_loss:.4f}')


if __name__ == "__main__":
    main()
