CREATE TABLE IF NOT EXISTS orders2 (
id         INTEGER PRIMARY KEY,
product    TEXT    NOT NULL,
qty        INTEGER NOT NULL,
unit_price REAL    NOT NULL,
total      REAL
);
CREATE TABLE IF NOT EXISTS audit_log (
id        INTEGER PRIMARY KEY,
order_id  INTEGER NOT NULL,
action    TEXT    NOT NULL,
ts        INTEGER NOT NULL DEFAULT (strftime('%s','now'))
);
CREATE TRIGGER IF NOT EXISTS trg_orders_insert
AFTER INSERT ON orders2
BEGIN
UPDATE orders2 SET total = NEW.qty * NEW.unit_price WHERE id = NEW.id;
INSERT INTO audit_log (order_id, action) VALUES (NEW.id, 'INSERT');
END;
INSERT INTO orders2 (product, qty, unit_price)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 5000)
SELECT 'prod_'||(i%50), (i%20)+1, ROUND(1.99+(i%98), 2) FROM seq;
SELECT COUNT(*) AS orders, SUM(total) AS revenue FROM orders2;
SELECT COUNT(*) AS audit_rows                    FROM audit_log;
