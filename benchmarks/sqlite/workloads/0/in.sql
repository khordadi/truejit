-- Create a table to store user information
CREATE TABLE IF NOT EXISTS users
(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL,
    email TEXT NOT NULL,
    age INTEGER
);

-- Insert 10,000 users
BEGIN TRANSACTION;
WITH RECURSIVE cnt(x) AS (
    SELECT 1
    UNION ALL
    SELECT x+1 FROM cnt WHERE x < 100000
)
INSERT INTO users (username, email, age)
SELECT
    'user_' || x,
    'user_' || x || '@example.com',
    20 + (x % 30)
FROM cnt;
COMMIT;

-- Update ages for first 5,000 users
UPDATE users
SET age = age + 1
WHERE id <= 50000;

-- Query all users
SELECT * FROM users;
-- Query users older than 25
SELECT * FROM users WHERE age > 25;
-- Query users with username starting with 'user_1'
SELECT * FROM users WHERE username LIKE 'user_1%';
-- Query users with email ending with 'example.com'
SELECT * FROM users WHERE email LIKE '%@example.com';
-- Query users aged between 30 and 40
SELECT * FROM users WHERE age BETWEEN 30 AND 40;
-- Query users ordered by age descending
SELECT * FROM users ORDER BY age DESC;
-- Query users with pagination (limit 10, offset 20)
SELECT * FROM users LIMIT 10 OFFSET 20;