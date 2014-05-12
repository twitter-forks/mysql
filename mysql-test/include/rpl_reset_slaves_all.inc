--let $include_filename= rpl_reset_slaves_all.inc
--source include/begin_include_file.inc

--let $rpl_source_file= include/reset_slave_all.inc
--source include/rpl_for_each_slave.inc

--let $include_filename= rpl_reset_slaves_all.inc
--source include/end_include_file.inc
