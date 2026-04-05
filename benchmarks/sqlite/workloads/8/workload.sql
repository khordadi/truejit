CREATE TABLE IF NOT EXISTS nodes (id INTEGER PRIMARY KEY, label TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS edges (src INTEGER NOT NULL, dst INTEGER NOT NULL);
INSERT INTO nodes (label)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 200)
SELECT 'node_'||i FROM seq;
INSERT INTO edges (src, dst)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 200)
SELECT i, (i%200)+1 FROM seq
UNION ALL
SELECT i, ((i+7)%200)+1 FROM seq WHERE (i%5)=0;
-- reachability from node 1 via recursive CTE
WITH RECURSIVE reachable(id, depth) AS (
SELECT 1, 0
UNION ALL
SELECT e.dst, r.depth+1
FROM edges e
JOIN reachable r ON e.src = r.id
WHERE r.depth < 10
)
SELECT n.label, MIN(r.depth) AS min_hops
FROM reachable r
JOIN nodes n ON n.id = r.id
GROUP BY r.id
ORDER BY min_hops, n.label;
