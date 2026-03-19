import torch
import torch.nn as nn
import torch.optim as optim
import os
import json

# use gpu
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
print(f"Using {device}")

window_size = 5


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


def main():
    # parse dataset
    datasets = []
    with open('/truejit/predictor/dataset.txt', 'r') as f:
        for line in f:
            line = json.loads(line)
            line = [word for word in line]
            datasets.append(line)

    # create vocabulary
    vocab = set([word for dataset in datasets for word in dataset])
    vocab = sorted(vocab)

    # create word to index and index to word mappings
    word_to_idx = {word: i for i, word in enumerate(vocab)}
    idx_to_word = {i: word for i, word in enumerate(vocab)}

    # convert dataset to indices
    datasets = [[word_to_idx[word] for word in dataset] for dataset in datasets]

    trainX = []
    trainY = []

    for dataset in datasets:
        if len(dataset) == 0:
            continue

        # [0, 0, ..., 0, 1] -> [2]
        # [0, 0, ..., 1, 2] -> [3]
        # [0, 0, ..., 2, 3] -> [4]
        # ...
        # [0, 1, 2, ..., k-1] -> [k]
        trainX += [[0] * (window_size - i - 1) + dataset[:i + 1] for i in range(window_size - 1)]
        trainY += dataset[1:window_size]

        # [1, 2, ..., k] -> [k+1]
        # [2, 3, ..., k+1] -> [k+2]
        # [3, 4, ..., k+2] -> [k+3]
        # ...
        # [n-k, n-k+1, ..., n-1] -> [n]
        trainX += [dataset[i - window_size:i] for i in range(window_size, len(dataset))]
        trainY += dataset[window_size:]

        # print zip
        # for i in range(len(trainX)):
        #     print(trainX[i], trainY[i])

    # convert to tensors
    trainX = torch.tensor(trainX, dtype=torch.long).to(device)
    trainY = torch.tensor(trainY, dtype=torch.long).to(device)

    # Parameters
    embed_size = 32
    hidden_size = 64
    vocab_size = len(vocab)
    output_size = vocab_size

    # Instantiate the model, loss function, and optimizer
    model = LSTMModel(vocab_size, embed_size, hidden_size, output_size).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=0.01)

    # Training loop
    epochs = 1000
    for epoch in range(epochs):
        model.train()
        optimizer.zero_grad()
        output = model(trainX)
        loss = criterion(output, trainY)
        loss.backward()
        optimizer.step()

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch + 1}/{epochs}, Loss: {loss.item()}")

    # move the model to cpu
    model = model.cpu()

    # remove /truejit/predictor/model.pt if it already exists
    model_path = '/truejit/predictor/model.pt'
    if os.path.exists(model_path):
        os.remove(model_path)
    # save the model using torchscript
    torch.jit.save(torch.jit.script(model.eval()), model_path)

    # remove /truejit/predictor/vocab.txt if it already exists
    vocab_path = '/truejit/predictor/vocab.txt'
    if os.path.exists(vocab_path):
        os.remove(vocab_path)
        # save the vocab
    with open(vocab_path, 'w') as f:
        for word in vocab:
            f.write(word + '\n')


if __name__ == '__main__':
    main()
