#
# Setup RQG environment.
#

# RQG home directory.
if (`SELECT '$RQG_HOME' = ''`)
{
  --die Missing RQG_HOME variable.
}

# The MySQL engine to use.
if (`SELECT '$RQG_ENGINE' = ''`)
{
  let $RQG_ENGINE = `SELECT ENGINE FROM INFORMATION_SCHEMA.ENGINES WHERE SUPPORT = 'DEFAULT'`;
}

# RQG output is saved in the selected base directory.
if (`SELECT '$RQG_LOG_DIR' = ''`)
{
  let $RQG_LOG_DIR = $MYSQL_TMP_DIR;
}
