CREATE TABLE IF NOT EXISTS customers (
id      INTEGER PRIMARY KEY,
name    TEXT    NOT NULL,
country TEXT    NOT NULL
);
CREATE TABLE IF NOT EXISTS orders (
id          INTEGER PRIMARY KEY,
customer_id INTEGER NOT NULL REFERENCES customers(id),
amount      REAL    NOT NULL
);
INSERT INTO customers (name, country)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 1000)
SELECT 'Customer_' || i, CASE (i%3) WHEN 0 THEN 'US' WHEN 1 THEN 'UK' ELSE 'DE' END
FROM seq;
INSERT INTO orders (customer_id, amount)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 10000)
SELECT (i % 1000) + 1, ROUND((i % 1000) + 0.5, 2) FROM seq;
SELECT c.name, SUM(o.amount) AS total
FROM customers c
JOIN orders o ON o.customer_id = c.id
WHERE c.country = 'US'
GROUP BY c.id
ORDER BY total DESC
LIMIT 20;
