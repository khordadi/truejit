CREATE TABLE IF NOT EXISTS accounts (
id      INTEGER PRIMARY KEY,
owner   TEXT NOT NULL,
balance REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS transactions (
id         INTEGER PRIMARY KEY,
account_id INTEGER NOT NULL REFERENCES accounts(id),
amount     REAL    NOT NULL
);
INSERT INTO accounts (owner, balance)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 500)
SELECT 'Owner_'||i, ROUND(1000 + (i%9000), 2) FROM seq;
INSERT INTO transactions (account_id, amount)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 10000)
SELECT (i%500)+1, ROUND(((i%200)-100) + 0.5, 2) FROM seq;
-- correlated: for each account, compute its transaction sum inline
SELECT a.id, a.owner, a.balance,
    (SELECT SUM(t.amount) FROM transactions t WHERE t.account_id = a.id) AS tx_total
FROM accounts a
WHERE a.balance > (SELECT AVG(balance) FROM accounts)
ORDER BY tx_total DESC;
