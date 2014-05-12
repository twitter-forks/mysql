--let $include_filename= reset_slave_all.inc
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}

RESET SLAVE ALL;

--let $include_filename= reset_slave_all.inc
--source include/end_include_file.inc
