import json
import os
import subprocess
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader

window_size = 1

def main(application, history_file, model_file):
    torch.cuda.is_available()

    with open(history_file, 'r') as f:
        history = json.load(f)
        histories = [history]

    cmd = ["wasm-objdump", "-h", application]
    out = subprocess.run(cmd, check=True, text=True, capture_output=True)
    out = out.stdout
    lines = out.split("\n")
    # get the line that has "Function" in it
    functions = [line for line in lines if "Function" in line][0]
    functions = functions.split(" ")[-1]
    functions = int(functions)

    # use gpu
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f"Using {device}")
    # if cude is available, clear the cache
    if torch.cuda.is_available():
        torch.cuda.empty_cache()

    # Custom dataset class
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

    # Create dataset and dataloader
    dataset = HistoryDataset(histories, window_size)
    dataloader = DataLoader(dataset, batch_size=8192, shuffle=True)

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
    epochs = 10000
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
            print(f"Epoch {epoch + 1}/{epochs}, Loss: {loss.item()}")

    # move the model to cpu
    model = model.cpu()

    # Save the model
    torch.jit.save(torch.jit.script(model.eval()), model_file)


if __name__ == '__main__':
    args = os.sys.argv[1:]
    # usage: python3 train.py -opt <history_file> -o <model_file>
    if len(args) != 4 or args[0] != '-opt' or args[2] != '-o':
        print("Usage: python3 train.py -opt <history_file> -o <model_file>")
        exit(1)
    main(args[1], args[3])
