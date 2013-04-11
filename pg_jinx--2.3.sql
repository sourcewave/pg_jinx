
CREATE FUNCTION pg_jinx_fdw_handler() RETURNS fdw_handler AS 'MODULE_PATHNAME' LANGUAGE C STRICT;
CREATE FUNCTION pg_jinx_fdw_validator(text[], oid) RETURNS void AS 'MODULE_PATHNAME' LANGUAGE C STRICT;


CREATE FOREIGN DATA WRAPPER pg_jinx HANDLER pg_jinx_fdw_handler VALIDATOR pg_jinx_fdw_validator;

CREATE OR REPLACE FUNCTION javax_call_handler() RETURNS language_handler AS 'pg_jinx' LANGUAGE C;
CREATE /* TRUSTED */ LANGUAGE javax HANDLER javax_call_handler;

/* Use as follows from psql:
\set ss `base64 jarfile`
select pg_jinx_install_jar( decode(:'ss', 'base64'),'jarname');
*/

DROP SCHEMA IF EXISTS example;
CREATE SCHEMA example;
set search_path to example, public;


CREATE FUNCTION pg_jinx_install_jar(bytea, varchar) RETURNS setof varchar AS 'org.sourcewave.jinx.PostgresBridge.install_jar' LANGUAGE JAVAX;
CREATE FUNCTION pg_jinx_remove_jar(varchar) RETURNS setof varchar AS 'org.sourcewave.jinx.PostgresBridge.remove_jar' LANGUAGE JAVAX;
CREATE FUNCTION pg_jinx_jarlist() RETURNS setof varchar AS 'org.sourcewave.jinx.PostgresBridge.jarList' LANGUAGE JAVAX;

CREATE SERVER jar_server VERSION '1.2' FOREIGN DATA WRAPPER pg_jinx;
CREATE FOREIGN TABLE example.jars(name varchar, data bytea) SERVER jar_server OPTIONS (class 'org.sourcewave.jinx.FdwJars');

create view example.jar_view as select * from example.jars; 
create or replace function pg_jinx_install_jar_trigger() returns trigger language plpgsql as $$
  begin
    PERFORM pg_jinx_install_jar(NEW.data, NEW.name);
    RETURN NEW;
  end;
$$;

create or replace function pg_jinx_remove_jar_trigger() returns trigger language plpgsql as $$
    begin
        PERFORM pg_jinx_remove_jar(OLD.name);
        RETURN OLD;
    end;
$$;

CREATE TRIGGER insert_jar_trigger INSTEAD OF INSERT ON example.jar_view FOR EACH ROW EXECUTE PROCEDURE pg_jinx_install_jar_trigger();
CREATE TRIGGER remove_jar_trigger INSTEAD OF DELETE ON example.jar_view FOR EACH ROW EXECUTE PROCEDURE pg_jinx_remove_jar_trigger();
/* then one can:
\set ss `base64 jarfile`
insert into jar_view values('clem',decode(:'ss','base64')); 
*/
    
-- set up a sample schema


create type example.propertyType as (k varchar, v varchar);
create function example.rs(int) returns varchar as 'org.sourcewave.jinx.example.Example.randomString' language javax;
create function example.property(varchar) returns varchar as 'java.lang.System.getProperty' language javax;
create function example.properties() returns setof example.propertyType as 'java.lang.System.properties' language javax;
    
CREATE SERVER pg_jinx_sample FOREIGN DATA WRAPPER pg_jinx;

CREATE FOREIGN TABLE example.def(a int, b varchar, c float, d timestamp, e date, f boolean, g decimal, h real, i bigint, j smallint,
    k bytea, l inet ) SERVER pg_jinx_sample;
    
CREATE FOREIGN TABLE example.property(k varchar, v varchar) SERVER pg_jinx_sample OPTIONS (class 'org.sourcewave.jinx.example.FdwProperty');

create function example.countRows(varchar) returns int as 'org.sourcewave.jinx.example.Example.countRows' language javax;
    
/*
    select * from getOptions() as (key varchar, val varchar) order by key;
    */
create or replace function example.getOptions() returns setof record as 'org.sourcewave.jinx.example.Example.getOptions' volatile language javax;

create or replace function example.getGravatar() returns trigger as 'org.sourcewave.jinx.example.GravatarTrigger.xxx' volatile language javax;

create table example.TestTrigger(email varchar, gravatar varchar, primary key (email));

create trigger getGravatar BEFORE INSERT OR UPDATE ON example.TestTrigger FOR EACH ROW EXECUTE PROCEDURE example.getGravatar();
