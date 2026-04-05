CREATE TABLE IF NOT EXISTS inventory (
id       INTEGER PRIMARY KEY,
item     TEXT    NOT NULL,
stock    INTEGER NOT NULL,
reserved INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_inventory_stock ON inventory(stock);
INSERT INTO inventory (item, stock)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 5000)
SELECT 'item_'||i, (i%200)+1 FROM seq;
-- batch update: reserve low-stock items
UPDATE inventory SET reserved = stock WHERE stock < 20;
-- targeted update with expression
UPDATE inventory SET stock = stock - 1 WHERE stock > 100 AND (id % 3) = 0;
-- delete cleared items
DELETE FROM inventory WHERE stock = 0;
SELECT COUNT(*), SUM(stock), SUM(reserved) FROM inventory;
