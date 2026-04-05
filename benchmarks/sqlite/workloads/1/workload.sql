CREATE TABLE IF NOT EXISTS events (
id        INTEGER PRIMARY KEY,
type      TEXT    NOT NULL,
payload   TEXT,
ts        INTEGER NOT NULL
);
BEGIN;
INSERT INTO events (type, payload, ts)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 50000)
SELECT
    CASE (i % 4)
        WHEN 0 THEN 'click'
        WHEN 1 THEN 'view'
        WHEN 2 THEN 'purchase'
        ELSE        'logout'
    END,
    '{"item":' || i || '}',
    1700000000 + i
FROM seq;
COMMIT;
SELECT type, COUNT(*) FROM events GROUP BY type;
