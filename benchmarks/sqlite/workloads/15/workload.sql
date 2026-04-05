CREATE TABLE IF NOT EXISTS scratch (
id    INTEGER PRIMARY KEY,
value TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_scratch_value ON scratch(value);
-- insert a large dataset to fill pages...
INSERT INTO scratch (value)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 50000)
SELECT 'value_' || i FROM seq;
-- ...then delete half to fragment the file
DELETE FROM scratch WHERE id % 2 = 0;
-- ANALYZE updates internal statistics used by the query planner
ANALYZE;
-- VACUUM rewrites the entire database file, reclaiming freed pages
VACUUM;
SELECT COUNT(*) FROM scratch;
