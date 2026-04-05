CREATE TABLE IF NOT EXISTS departments (id INTEGER PRIMARY KEY, name TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS employees (
id            INTEGER PRIMARY KEY,
department_id INTEGER NOT NULL REFERENCES departments(id),
name          TEXT    NOT NULL,
salary        REAL    NOT NULL
);
CREATE TABLE IF NOT EXISTS projects (
id          INTEGER PRIMARY KEY,
employee_id INTEGER NOT NULL REFERENCES employees(id),
budget      REAL    NOT NULL,
status      TEXT    NOT NULL
);
INSERT INTO departments (name) VALUES ('Engineering'),('Sales'),('HR'),('Marketing'),('Finance');
INSERT INTO employees (department_id, name, salary)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 2000)
SELECT (i%5)+1, 'Emp_'||i, 40000 + (i%60000) FROM seq;
INSERT INTO projects (employee_id, budget, status)
WITH RECURSIVE seq(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM seq WHERE i < 8000)
SELECT (i%2000)+1, (i%100000)+1000,
        CASE (i%3) WHEN 0 THEN 'active' WHEN 1 THEN 'done' ELSE 'pending' END
FROM seq;
SELECT d.name, COUNT(DISTINCT e.id) AS headcount,
    SUM(p.budget) AS total_budget
FROM departments d
JOIN employees  e ON e.department_id = d.id
JOIN projects   p ON p.employee_id   = e.id
WHERE p.status = 'active'
GROUP BY d.id
ORDER BY total_budget DESC;
