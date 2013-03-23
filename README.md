PG_JINX
=======
The *P*ost*G*res *J*ava *IN*terface E*X*tension.

PG_JINX creates an interface that allows foreign data wrappers and stored procedures for PostgreSQL to be written in Java by bridging the C API to Java.


This project is a submodule of TransGres.app -- the drop-in OSX application for running a local Postgres server.  Building TransGres.app will include PG_JINX as an available extension.   This project can also be built for installation into a Linux instance of Postgres.  Instructions for that will be included below.

INSTALLING
==========

1) Install the extension in a database.  If you install it into the _template1_ database, newly created databases will have the extension pre-installed.

    CREATE EXTENSION pg_jinx;

=== Installing the foreign data wrapper support:

2) set up server:
 
     CREATE SERVER pg_jinx_server FOREIGN DATA WRAPPER pg_jinx OPTIONS(
jarfile '/home/atri/Downloads/sqlite-jdbc-3.7.2.jar',
another 'any value'
);

jarfile : The jarfile is the path (visible from the server) which will be added to the classpath containing the code required to implement this server.

2a) (Optional) Create a user mapping for the server.

    CREATE USER MAPPING FOR userid SERVER pg_jinx_server OPTIONS(key 'test',secret 'test');

2b) Create a foreign table on the server.

    CREATE FOREIGN TABLE test_table(a int, b varchar, c double, d timestamp) SERVER pg_jinx_server OPTIONS (generator 'com.your.TableClass' );

Now, you can access the foreign table test_table
    SELECT * FROM test_table where a * 2 > c;

=== Installing a stored procedure:

3) create a stored procedure with Java code.  The java names used in the method body are the same as the stored procedure parameter names.

    CREATE FUNCTION math_test(a int, b float) returns float as 'return a * Math.exp(b); ' LANGUAGE JAVA;
