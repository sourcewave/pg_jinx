
DROP SCHEMA IF EXISTS jinx CASCADE;
CREATE SCHEMA jinx;


CREATE FUNCTION jinx.fdw_handler() RETURNS fdw_handler AS 'MODULE_PATHNAME' LANGUAGE C STRICT;
CREATE FUNCTION jinx.fdw_validator(text[], oid) RETURNS void AS 'MODULE_PATHNAME' LANGUAGE C STRICT;


CREATE FOREIGN DATA WRAPPER pg_jinx HANDLER jinx.fdw_handler VALIDATOR jinx.fdw_validator;

CREATE OR REPLACE FUNCTION jinx.javax_call_handler() RETURNS language_handler AS 'pg_jinx' LANGUAGE C;
CREATE OR REPLACE FUNCTION jinx.inline_handler(internal) RETURNS void AS 'pg_jinx' LANGUAGE C;

CREATE /* TRUSTED */ LANGUAGE javax HANDLER jinx.javax_call_handler INLINE jinx.inline_handler;

/* Use as follows from psql:
\set ss `base64 jarfile`
select jinx.install_jar( decode(:'ss', 'base64'),'jarname');
*/

CREATE FUNCTION jinx.install_jar(bytea, varchar) RETURNS setof varchar AS 'org.sourcewave.jinx.PostgresBridge.install_jar' LANGUAGE JAVAX;
CREATE FUNCTION jinx.remove_jar(varchar) RETURNS setof varchar AS 'org.sourcewave.jinx.PostgresBridge.remove_jar' LANGUAGE JAVAX;
CREATE FUNCTION jinx.jarlist() RETURNS setof varchar AS 'org.sourcewave.jinx.PostgresBridge.jarList' LANGUAGE JAVAX;

CREATE SERVER jar_server VERSION '1.2' FOREIGN DATA WRAPPER pg_jinx;
CREATE FOREIGN TABLE jinx.jars(name varchar, data bytea) SERVER jar_server OPTIONS (class 'org.sourcewave.jinx.FdwJars');

create view jinx.jar_view as select * from jinx.jars; 
create or replace function jinx.install_jar_trigger() returns trigger language plpgsql as $$
  begin
    PERFORM jinx.install_jar(NEW.data, NEW.name);
    RETURN NEW;
  end;
$$;

create or replace function jinx.remove_jar_trigger() returns trigger language plpgsql as $$
    begin
        PERFORM jinx.remove_jar(OLD.name);
        RETURN OLD;
    end;
$$;

CREATE TRIGGER insert_jar_trigger INSTEAD OF INSERT ON jinx.jar_view FOR EACH ROW EXECUTE PROCEDURE jinx.install_jar_trigger();
CREATE TRIGGER remove_jar_trigger INSTEAD OF DELETE ON jinx.jar_view FOR EACH ROW EXECUTE PROCEDURE jinx.remove_jar_trigger();
/* then one can:
\set ss `base64 jarfile`
insert into jar_view values('clem',decode(:'ss','base64')); 
*/
    
-- set up a sample schema


create type jinx.propertyType as (k varchar, v varchar);
create function jinx.example_rs(int) returns varchar as 'org.sourcewave.jinx.example.Example.randomString' language javax;
create function jinx.property(varchar) returns varchar as 'java.lang.System.getProperty' language javax;
create function jinx.properties() returns setof jinx.propertyType as 'java.lang.System.properties' language javax;
    
CREATE SERVER sample FOREIGN DATA WRAPPER pg_jinx;

CREATE FOREIGN TABLE jinx.example_def(a int, b varchar, c float, d timestamp, e date, f boolean, g decimal, h real, i bigint, j smallint,
    k bytea, l inet ) SERVER sample;
    
CREATE FOREIGN TABLE jinx.example_property(k varchar, v varchar) SERVER sample OPTIONS (class 'org.sourcewave.jinx.example.FdwProperty');

create function jinx.countRows(varchar) returns int as 'org.sourcewave.jinx.example.Example.countRows' language javax;
    
/*
    select * from getOptions() as (key varchar, val varchar) order by key;
    */
create or replace function jinx.getOptions() returns setof record as 'org.sourcewave.jinx.example.Example.getOptions' volatile language javax;

create or replace function jinx.getGravatar() returns trigger as 'org.sourcewave.jinx.example.GravatarTrigger.xxx' volatile language javax;

create table jinx.TestTrigger(email varchar, gravatar varchar, primary key (email));

create trigger getGravatar BEFORE INSERT OR UPDATE ON jinx.TestTrigger FOR EACH ROW EXECUTE PROCEDURE jinx.getGravatar();
