patch can be applied to PostgreSQL version 15.3 by

```
patch -p1 < libpq-copy-wrapper-test.patch
```

it adds a utility in standby to send a request to primary and ask if it has a buffer block in its memory. If no, primary returns "1", if yes, the primary returns the whole block of data using COPY protocol.

how to test:
1) apply the patch and build PostgreSQL
2) follow the slides to set up streaming replication between primary and standby
3) build and install pg_buffercache extension

```
cd contrib/pg_buffercache
make
make install
```

### on primary:
```
Create table mytable(a int, b char(20));
insert into mytable values(12, 'hello');
insert into mytable values(12, 'hello');
insert into mytable values(12, 'hello');
insert into mytable values(12, 'hello');
```

### on standby:
make sure the same data is visible

### back on primary:
find out the information about mytable's buffer block inforamtion
```
select * from pg_buffercache where relfilenode=16388;
-[ RECORD 1 ]----+------
bufferid         | 317
relfilenode      | 16388
reltablespace    | 1663
reldatabase      | 5
relforknumber    | 0
relblocknumber   | 0
isdirty          | t
usagecount       | 4
pinning_backends | 0
```

for example, we have:
- tablespace oid: 1663
- database oid: 5
- relation oid: 16388
- fork number: 0
- block number: 0

### back on standby 
use test_wrapper() function to send a on demand query to primary asking about the same block:

```
select test_wrapper(1663, 5, 16388, 0, 0);
                          test_wrapper
-----------------------------------------------------------------
 standby: receive 8192 bytes for buffer block (1663 5 16388 0 0)
(1 row)

```

if you go to primary and issue
```
checkpoint;
```
which makes primary flushes this block to disk, then test_wrapper will return:
```
select test_wrapper(1663, 5, 16388, 0, 0);
                         test_wrapper
--------------------------------------------------------------
 primary does not have this block in buffer(1663 5 16388 0 0)
(1 row)

```

because the block is no longer in primary's memory.


