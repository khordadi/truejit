CREATE TABLE IF NOT EXISTS ledger (
id      INTEGER PRIMARY KEY,
account TEXT    NOT NULL,
delta   REAL    NOT NULL,
note    TEXT
);
BEGIN;
INSERT INTO ledger (account, delta, note) VALUES ('A', 1000.0, 'opening');
INSERT INTO ledger (account, delta, note) VALUES ('B', 2000.0, 'opening');

SAVEPOINT sp1;
INSERT INTO ledger (account, delta, note) VALUES ('A', -500.0, 'transfer out');
INSERT INTO ledger (account, delta, note) VALUES ('B',  500.0, 'transfer in');

SAVEPOINT sp2;
    INSERT INTO ledger (account, delta, note) VALUES ('A', -9999.0, 'bad txn');
ROLLBACK TO sp2;        -- undo only the bad transaction
RELEASE sp2;

INSERT INTO ledger (account, delta, note) VALUES ('A', -100.0, 'fee');
RELEASE sp1;

COMMIT;
SELECT account, SUM(delta) AS balance FROM ledger GROUP BY account;
