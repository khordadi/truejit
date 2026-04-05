CREATE TABLE IF NOT EXISTS logs (
id       INTEGER PRIMARY KEY,
level    TEXT    NOT NULL,
message  TEXT    NOT NULL,
ts       INTEGER NOT NULL
);
INSERT INTO logs (level, message, ts)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 100000)
SELECT
    CASE (i%3) WHEN 0 THEN 'INFO' WHEN 1 THEN 'WARN' ELSE 'ERROR' END,
    'Log message number ' || i,
    1700000000 + (i * 7)
FROM seq;
-- top-N: planner can short-circuit the full sort
SELECT id, level, message, ts FROM logs ORDER BY ts DESC LIMIT 100;
SELECT id, message, ts        FROM logs WHERE level = 'ERROR' ORDER BY ts DESC LIMIT 50;
