CREATE TABLE IF NOT EXISTS payloads (
id   INTEGER PRIMARY KEY,
data TEXT NOT NULL          -- JSON blob
);
INSERT INTO payloads (data)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 10000)
SELECT json_object(
    'id',       i,
    'user',     'user_' || (i%500),
    'action',   CASE (i%4) WHEN 0 THEN 'buy' WHEN 1 THEN 'sell'
                            WHEN 2 THEN 'view' ELSE 'refund' END,
    'amount',   ROUND((i%1000)+0.99, 2),
    'meta',     json_object('ip', '10.0.' || (i%256) || '.1', 'ua', 'agent_' || (i%10))
)
FROM seq;
SELECT
json_extract(data, '$.user')           AS usr,
json_extract(data, '$.action')         AS action,
COUNT(*)                               AS cnt,
SUM(json_extract(data, '$.amount'))    AS total,
AVG(json_extract(data, '$.amount'))    AS avg_amount
FROM payloads
GROUP BY json_extract(data, '$.user'), json_extract(data, '$.action')
ORDER BY total DESC
LIMIT 30;
