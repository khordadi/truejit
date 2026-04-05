CREATE TABLE IF NOT EXISTS products (
id       INTEGER PRIMARY KEY,
sku      TEXT    NOT NULL,
category TEXT    NOT NULL,
price    REAL    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_products_category ON products(category);
INSERT INTO products (sku, category, price)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 20000)
SELECT
    'SKU-' || i,
    CASE (i % 5)
        WHEN 0 THEN 'electronics'
        WHEN 1 THEN 'clothing'
        WHEN 2 THEN 'food'
        WHEN 3 THEN 'books'
        ELSE        'toys'
    END,
    ROUND((i % 500) + 0.99, 2)
FROM seq;
-- indexed lookup
SELECT id, sku, price FROM products WHERE category = 'electronics' AND price < 100.0;
-- full scan (no index on price alone)
SELECT COUNT(*) FROM products WHERE price BETWEEN 50.0 AND 150.0;
