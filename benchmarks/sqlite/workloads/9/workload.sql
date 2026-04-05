DROP TABLE IF EXISTS docs;
CREATE TABLE docs (
    id       INTEGER PRIMARY KEY,
    title    TEXT    NOT NULL,
    category TEXT    NOT NULL,
    body     TEXT    NOT NULL,
    score    REAL    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_docs_cat_score ON docs(category, score DESC);
INSERT INTO docs (title, category, body, score)
    WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 5000)
    SELECT
        'Document title number ' || i,
        CASE (i%5)
            WHEN 0 THEN 'database'
            WHEN 1 THEN 'network'
            WHEN 2 THEN 'compiler'
            WHEN 3 THEN 'systems'
            ELSE        'security'
        END,
        CASE (i%5)
            WHEN 0 THEN 'SQLite query planning index scan benchmark workload ' || i
            WHEN 1 THEN 'TCP UDP socket performance throughput latency test ' || i
            WHEN 2 THEN 'LLVM IR bytecode optimisation pass pipeline stage '  || i
            WHEN 3 THEN 'kernel memory page fault scheduling preemption '     || i
            ELSE        'buffer overflow exploit mitigation sandbox policy '  || i
        END,
        ROUND(1.0 + (i % 999) + (i % 7) * 0.1, 3)
    FROM seq;
SELECT id, title, score
FROM docs
WHERE category = 'database'
ORDER BY score DESC
LIMIT 25;
SELECT COUNT(*), AVG(score)
FROM docs
WHERE body LIKE '%benchmark%' AND score > 500.0;
