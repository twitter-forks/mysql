disable_query_log;
disable_result_log;
disable_abort_on_error;
exec innodb_space --help;
enable_abort_on_error;
if ($exec_exit_status != 0)
{
  skip Test requires innodb_ruby;
}
enable_result_log;
enable_query_log;
