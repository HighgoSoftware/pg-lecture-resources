# checkout repo
git checkout pg15.3


# apply patch, compile and install
git apply --check  guc_demo.diff
git apply  guc_demo.diff
make clean 
make -j
make install


# test
initdb -D pgdata
pg_ctl -D pgdata -l logfile start
...
cat logfile |grep "guc demo"
#expect: hello, guc demo disabled


edit pgdata/postgresql.conf
guc_demo = true

pg_ctl -D pgdata -l logfile stop
pg_ctl -D pgdata -l logfile start
...
cat logfile |grep "guc demo"
#expect: hello, guc demo enabled

