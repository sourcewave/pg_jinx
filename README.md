PG_JINX
=======
The *P* ost *G* res *J* ava *IN* terface E *X* tension.

PG_JINX creates an interface that allows foreign data wrappers and stored procedures for PostgreSQL to be written in Java by bridging the C API to Java.

This project is a submodule of TransGres.app -- the drop-in OSX application for running a local Postgres server.  Building TransGres.app will include PG_JINX as an available extension.   This project can also be built for installation into a Linux instance of Postgres.  Instructions for that will be included below.

PG_JINX is similar to [PL/Java](https://github.com/tada/pljava/wiki) with the following
differences:

1. Packaged as a Postgres extension.
2. Support for foreign data wrappers.
3. Support for specifying Java stored procedures as Java source code (as well as method reference).
4. Does not use JDBC for access to Postgres internals.
5. Installation of jar files into the file system.
6. Support for Transgres.

INSTALLING
==========

1) Install the extension in a database.  If you install it into the _template1_ database, newly created databases will have the extension pre-installed.

```sql
CREATE EXTENSION pg_jinx;
```

## Installing the foreign data wrapper support:

2) set up server:
```sql 
CREATE SERVER pg_jinx_server FOREIGN DATA WRAPPER pg_jinx;
```

jarfile : The jarfile is the path (visible from the server) which will be added to the classpath containing the code required to implement this server.

3) Create a foreign table on the server.
```sql
CREATE FOREIGN TABLE jproperties(k text, v text) server pg_jinx_server
   options( class 'org.sourcewave.jinx.example.FdwProperty');
```

Now, you can access the foreign table test_table
```sql
SELECT * FROM jproperties;
```

## Installing a stored procedure:

4) create a stored procedure with Java code.  The java names used in the method body are the same as the stored procedure parameter names.
```sql
CREATE FUNCTION math_test(a int, b float) returns float as 'return a * Math.exp(b); ' LANGUAGE JAVAX;
```

Access it like so:
```sql
SELECT math_test(3,5);
```

or
```sql
SELECT * from math_text(3,5);
```

