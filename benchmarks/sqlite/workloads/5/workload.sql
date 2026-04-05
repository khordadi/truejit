CREATE TABLE IF NOT EXISTS sales (
id       INTEGER PRIMARY KEY,
region   TEXT    NOT NULL,
rep      TEXT    NOT NULL,
product  TEXT    NOT NULL,
qty      INTEGER NOT NULL,
revenue  REAL    NOT NULL
);
INSERT INTO sales (region, rep, product, qty, revenue)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 30000)
SELECT
    CASE (i%4) WHEN 0 THEN 'North' WHEN 1 THEN 'South' WHEN 2 THEN 'East' ELSE 'West' END,
    'Rep_' || (i%50),
    'Prod_' || (i%20),
    (i%100)+1,
    ROUND(((i%100)+1) * (9.99 + (i%90)), 2)
FROM seq;
SELECT region, product,
    COUNT(*)        AS num_sales,
    SUM(qty)        AS total_qty,
    SUM(revenue)    AS total_revenue,
    AVG(revenue)    AS avg_revenue,
    MAX(revenue)    AS max_revenue
FROM sales
GROUP BY region, product
ORDER BY total_revenue DESC;
