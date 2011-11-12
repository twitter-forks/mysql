# Avoid any side effects, recreate test database.
--disable_query_log
DROP DATABASE test;
CREATE DATABASE test;
--enable_query_log
