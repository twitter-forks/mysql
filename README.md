# Twitter MySQL 5.5 #

This is [Twitter](http://twitter.com/)'s MySQL development branch, which is based on MySQL 5.5 as published by Oracle on [MySQL on Launchpad](https://launchpad.net/mysql-server). 

This repository is published in order to share code and information and is *not intended to be used directly outside of Twitter*. We provide no guarantees of bug fixes, ongoing maintenance, compatibility, or suitability for any user outside of Twitter.

The original README file provided with the upstream MySQL release can be found at [README-MySQL](https://github.com/twitter/mysql/blob/master/README-MySQL).

# Features in Twitter MySQL #

## Additional status variables ##

Additional status variables have been added, particularly from the internals of InnoDB. This allows us to monitor our systems more effectively and understand their behavior better when handling production workloads. The variables added are:

* The number of InnoDB files and tablespace files opened, closed, and currently open. This information was previously not exposed by InnoDB.
* The number of deadlocks encountered. This information was previously not exposed by InnoDB.
* The current log sequence number (LSN) as well as the LSN flushed up to and checkpointed up to. This information has previously been available in `SHOW ENGINE INNODB STATUS`.

## Optimization of memory allocation under NUMA ##

On most recent multi-processor systems, a [non-uniform memory access NUMA](http://en.wikipedia.org/wiki/Non-Uniform_Memory_Access) (NUMA) architecture is in use, which divides the total system memory across multiple NUMA "nodes". When allocating large amounts of memory to InnoDB's buffer pool, as is typical, some inefficiencies as well as serious problems can be encountered. More details about the problems typical in NUMA systems running MySQL can be found in [a blog post by Jeremy Cole](http://blog.jcole.us/2010/09/28/mysql-swap-insanity-and-the-numa-architecture/). The following changes have been made to optimize and improve this:

* An option has been added to forcibly pre-allocate the entire buffer pool during startup. This is primarily intended to force the system to decide which pages to allocate, and on which NUMA node to allocate them. If the buffer pool can't be fully allocated for any reason, InnoDB will abort during startup.
* An option has been added to `mysqld_safe` to wrap the start of `mysqld` with `numactl --interleave=all` to interleave memory allocation between all NUMA nodes available. This ensures that no NUMA node is favored for any allocation, so that memory usage will remain even over time between multiple NUMA nodes.
* An option has been added to `mysqld_safe` to flush the operating system buffer caches before startup (on Linux only). Linux will normally not evict optional caches to make new allocations unless the system is under memory pressure, which can cause supposedly evenly interleaved memory allocations to still be done unevenly, favoring the node with less data cached before startup. Flushing the caches before startup ensures that no large cache allocations are present in the system before InnoDB allocates its buffer pool.

## Server-side query timeout ##

* Reduced unnecessary work through improved server-side statement timeout support. This allows the server to proactively cancel queries that run longer than a millisecond-granularity timeout.

## Buffer pool export and restore by prefetch ##

* Export and restore InnoDB buffer pool in using a safe and lightweight method. This enables us to build tools to support rolling restarts of our services with minimal pain.

## Optimization for solid-state drives (SSDs) ##

* Optimize MySQL for SSD-based machines, including page-flushing behavior and reduction in writes to disk to improve lifespan.
## What is MySQL ##
* Basically, MySQL is a Relational DataBase Management System(RDBMS). All DBMS work as an interface between DataBase and Client. It uses Structured Query Language(SQL) which is mostly used in other DBMS softwares also. MySQL is written in C and C++. Its SQL parser is written in yacc, but it uses a home-brewed lexical analyzer. MySQL works on many system platforms, including AIX, BSDi, FreeBSD, HP-UX, eComStation, i5/OS, IRIX, Linux, OS X, Microsoft Windows, NetBSD, Novell NetWare, OpenBSD, OpenSolaris, OS/2 Warp, QNX, Oracle Solaris, Symbian, SunOS, SCO OpenServer, SCO UnixWare, Sanos and Tru64. A port of MySQL to OpenVMS also exists. It is not a Case Sensitive software. We can fire queries in any of the cases.
