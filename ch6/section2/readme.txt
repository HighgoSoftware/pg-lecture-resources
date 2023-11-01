
### step-1, compile and install postgres 15.3
git clone https://github.com/postgres/postgres.git
git checkout REL_15_3 -b pg153
cd postgres
./configure --prefix=/tmp/pg153 --enable-tap-tests --enable-debug CFLAGS="-g3 -O0" CC="gcc -std=gnu99"
make -j && make install

### step-2, compile and install pg_rman
git clone https://github.com/ossc-db/pg_rman.git
git checkout e412f9729808ef427d5cd27950cf47d4e5597282 -b table-restore
git apply --check 0001-to-restore-a-specific-table-using-oid.patch
cd pg_rman
make && make install

### step-3 run below two commands to have pg153 binaries and libraries in our console path
export LD_LIBRARY_PATH=/tmp/pg153/lib
export PATH=/tmp/pg153/bin:$PATH


### step-4, pg_rman backup and restore 
mkdir -p /tmp/rman/archive /tmp/rman/pglog
initdb  -D /tmp/rman/pgdata
echo "archive_mode = on" >> /tmp/rman/pgdata/postgresql.conf
echo "archive_command = 'cp %p /tmp/rman/archive/%f'" >> /tmp/rman/pgdata/postgresql.conf
echo "log_directory = '/tmp/rman/pglog'" >> /tmp/rman/pgdata/postgresql.conf
pg_ctl -D /tmp/rman/pgdata -l logfile start

# init backup
pg_rman init -B /tmp/rman/backup -D /tmp/rman/pgdata 
# create table and insert data
psql -d postgres -c "CREATE TABLE abc (ID INT);"
psql -d postgres -c "INSERT INTO abc VALUES (1);"

# backup and validate
pg_rman backup --backup-mode=full --with-serverlog -B /tmp/rman/backup -D /tmp/rman/pgdata -A /tmp/rman/archive -S /tmp/rman/pglog -p 5432 -d postgres
pg_rman validate -B /tmp/rman/backup

# update database (assume a wrong update happens here)
psql -d postgres -c "INSERT INTO abc VALUES (2);"

# restore database
pg_ctl -D /tmp/rman/pgdata -l logfile stop
pg_rman show -B /tmp/rman/backup
pg_rman restore -B /tmp/rman/backup -D /tmp/rman/pgdata --recovery-target-time="2023-10-31 21:51:00"
pg_ctl -D /tmp/rman/pgdata -l logfile start
psql -d postgres -c "SELECT count(*) from abc;"


### step-5, CASE 1, specific table off-line restore
# Init PG15.3
mkdir -p /tmp/rman/pglog/ /tmp/rman/archive 
initdb  -D /tmp/rman/pgdata 
echo "archive_mode = on" >> /tmp/rman/pgdata/postgresql.conf
echo "archive_command = 'cp %p /tmp/rman/archive/%f'" >> /tmp/rman/pgdata/postgresql.conf
echo "log_directory = '/tmp/rman/pglog'" >> /tmp/rman/pgdata/postgresql.conf
pg_ctl -D /tmp/rman/pgdata -l /tmp/rman/pglog/logfile start

# Initialize backup catalog and create tables
pg_rman init -B /tmp/rman/backup -D /tmp/rman/pgdata 
psql -d postgres -c "CREATE TABLE abc (ID INT);"
psql -d postgres -c "CREATE TABLE xyz (ID INT);"
psql -d postgres -c "INSERT INTO abc VALUES (1);"
psql -d postgres -c "INSERT INTO xyz VALUES (1);"
psql -d postgres -c "SELECT count(*) from abc;"
psql -d postgres -c "SELECT count(*) from xyz;"


# Do backup
pg_rman backup --backup-mode=full --with-serverlog -B /tmp/rman/backup -D /tmp/rman/pgdata -A /tmp/rman/archive -S /tmp/rman/pglog -p 5432 -d postgres

# Validate backup
pg_rman validate -B /tmp/rman/backup
pg_rman show -B /tmp/rman/backup


# Insert more data and retrieve oid for targeted table
psql -d postgres -c "INSERT INTO abc VALUES (2);"
psql -d postgres -c "INSERT INTO xyz VALUES (2);"
psql -d postgres -c "SELECT count(*) from abc;"
psql -d postgres -c "SELECT count(*) from xyz;"
psql -d postgres -c "SELECT oid FROM pg_class WHERE relname = 'abc' AND relkind = 'r';"

# Stop PG (offline restore)
pg_ctl -D /tmp/rman/pgdata -l /tmp/rman/pglog/logfile stop

# Restore targeted table to a specific backup 
pg_rman restore -B /tmp/rman/backup -D /tmp/rman/pgdata --recovery-target-time="2023-10-31 21:51:00" --target-table-oid="16384"

# Start PG and verify targeted table restored
pg_ctl -D /tmp/rman/pgdata -l /tmp/rman/pglog/logfile start
psql -d postgres -c "SELECT count(*) from abc;"
psql -d postgres -c "SELECT count(*) from xyz;"


### step-6, CASE 2, specific table with schema changed on-line restore
# Generate a big chunk of data
psql -d postgres -c "INSERT INTO abc SELECT generate_series(1,5000);"
psql -d postgres -c "INSERT INTO xyz SELECT generate_series(1,5000);"
psql -d postgres -c "SELECT count(*) from abc;"
psql -d postgres -c "SELECT count(*) from xyz;"

# Do backup
pg_rman backup --backup-mode=full --with-serverlog -B /tmp/rman/backup -D /tmp/rman/pgdata -A /tmp/rman/archive -S /tmp/rman/pglog -p 5432 -d postgres

# Validate backup and save targeted table’s schema
pg_rman validate -B /tmp/rman/backup
pg_dump -d postgres -t abc --schema-only > abc-table.sql
psql -d postgres -c "SELECT oid FROM pg_class WHERE relname = 'abc' AND relkind = 'r';"

# Show backups
pg_rman show -B /tmp/rman/backup
=====================================================================
 StartTime           EndTime              Mode    Size   TLI  Status 
=====================================================================
2023-10-31 21:56:51  2023-10-31 21:56:53  FULL    51MB     1  OK
2023-10-31 21:50:58  2023-10-31 21:51:00  FULL    48MB     1  OK


# Insert more data
psql -d postgres -c "INSERT INTO abc SELECT generate_series(1,1000);"
psql -d postgres -c "INSERT INTO xyz SELECT generate_series(1,1000);"
psql -d postgres -c "SELECT count(*) from abc;"
psql -d postgres -c "SELECT count(*) from xyz;"

# Add a new column to table abc and insert data
psql -d postgres -c "ALTER TABLE abc ADD COLUMN name text;"
psql -d postgres -c "INSERT INTO abc SELECT generate_series(1,2000), 'hello world';"
psql -d postgres -c "SELECT count(*) from abc where name !='';" 
psql -d postgres -c "SELECT count(*) from abc;"
psql -d postgres -c "SELECT count(*) from xyz;"

# Drop and recreate table abc using table abc’s schema in the backup for targeted table restore
psql -d postgres -c "SELECT oid FROM pg_class WHERE relname = 'abc' AND relkind = 'r';"
psql -d postgres -c "drop table abc;"
psql -d postgres -f abc-table.sql
psql -d postgres -c "SELECT oid FROM pg_class WHERE relname = 'abc' AND relkind = 'r';"


# Lock table in exclusive mode in console-1
psql -d postgres
BEGIN;
lock TABLE abc in exclusive mode ;

# Restore targeted table with new oid in console-2
pg_rman restore -B /tmp/rman/backup -D /tmp/rman/pgdata --recovery-target-time="2023-10-31 17:14:10" --target-table-oid="16384" --target-table-oid-new="16396" --online-restore

# Abort, rollback or commit in console-1 after targeted table restore is done
abort;

# Keep monitoring PG log and perform some operations on the restored table
psql -d postgres -c "SELECT count(*) from abc;"
psql -d postgres -c "SELECT count(*) from xyz;"
