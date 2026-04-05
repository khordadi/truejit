CREATE TABLE IF NOT EXISTS prices (
id       INTEGER PRIMARY KEY,
symbol   TEXT    NOT NULL,
day      INTEGER NOT NULL,
close    REAL    NOT NULL
);
INSERT INTO prices (symbol, day, close)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 10000)
SELECT
    CASE (i%5) WHEN 0 THEN 'AAPL' WHEN 1 THEN 'GOOG' WHEN 2 THEN 'MSFT'
                WHEN 3 THEN 'AMZN' ELSE 'META' END,
    i,
    ROUND(100 + (i%200) + (i%7)*0.5, 2)
FROM seq;
SELECT symbol, day, close,
    ROW_NUMBER()  OVER w                          AS rn,
    RANK()        OVER w                          AS rnk,
    LAG(close, 1) OVER w                          AS prev_close,
    close - LAG(close,1) OVER w                   AS day_delta,
    AVG(close)    OVER (PARTITION BY symbol
                        ORDER BY day
                        ROWS BETWEEN 6 PRECEDING AND CURRENT ROW) AS ma7
FROM prices
WINDOW w AS (PARTITION BY symbol ORDER BY day)
ORDER BY symbol, day
LIMIT 500;
