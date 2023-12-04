
### step-1, compile and install postgres 15.3
git clone https://github.com/postgres/postgres.git
git checkout REL_15_3 -b wal-redo
cd postgres
cp pg-lecture-resources/ch5/section2/0001-wal-redo.patch .
git apply --check 0001-wal-redo.patch
git apply  0001-wal-redo.patch
./configure --prefix=/tmp/pg153 --enable-tap-tests --enable-debug CFLAGS="-g3 -O0" CC="gcc -std=gnu99"
make clean && make -j && make install


### step-2, setup Primary node
initdb -D /tmp/pgdata
pg_ctl -D /tmp/pgdata -l logfile start


### step-3, setup Standby 
pg_basebackup -D /tmp/backup
touch /tmp/backup/standby.signal

tail -n 3 /tmp/backup/postgresql.conf 
port = 5433
primary_conninfo = 'dbname=postgres user=david host=127.0.0.1 port=5432'
my_standby = true

pg_ctl -D /tmp/backup -l logfile-standby start 


### step-4, monitor logfile for Primary and Standby
##### console-1
tail -f /tmp/logfile
##### console-2
tail -f /tmp/logfile-standby


### step-5, insert records from Primary 
##### console-3, Primary
psql -d postgres -p 5432
create table t(a int, b text);

insert into t values(generate_series(1, 1));

insert into t values(generate_series(1, 200));

## notes, there are some delay
select relpages from pg_class where relname = 't';


### step-6, console-4 make sure Standby can replicate from Primary
psql -d postgres -p 5433
select count(*) from t;


### step-7, expected wal redo log from Primary and Standby logs
##### console-1, expect "Add PAGE_NEW xlog record" on Primary, when inserting 1st record, or whenever a new 8KB page is created.
2023-12-04 12:30:56.789 PST [494077] WARNING:  Add PAGE_NEW xlog record: spcNode 1663, dbNode 5, relNode 16392, blocknum 0, forknum 0


##### console-2, expect "REDO PAGE_NEW" on Standby, when receiving WAL record contains customized WAL record from Primary
2023-12-04 12:30:56.794 PST [494069] WARNING:  REDO PAGE_NEW: spcNode 1663, dbNode 5, relNode 16392, blocknum 0, forknum 0 
2023-12-04 12:30:56.794 PST [494069] CONTEXT:  WAL redo at 0/3056C18 for Storage/PAGENEW: base/5/16392 to buftag: 1663/5/16392/0/0

##### check the customized WAL record from WAL segment file
pg_waldump -p /tmp/pgdata/pg_wal 000000010000000000000003 |grep buftag
