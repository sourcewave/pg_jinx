drop schema if exists example cascade;

create schema example;

set search_path to example, public;

create type example.propertyType as (k varchar, v varchar);
create function example.rs(int) returns varchar as 'org.sourcewave.jinx.example.Example.randomString' language javax;
create function example.property(varchar) returns varchar as 'java.lang.System.getProperty' language javax;
create function example.properties() returns setof example.propertyType as 'java.lang.System.properties' language javax;
    
CREATE SERVER pg_jinx_server FOREIGN DATA WRAPPER pg_jinx;

CREATE FOREIGN TABLE example.def(a int, b varchar, c float, d timestamp, e date, f boolean, g decimal, h real, i bigint, j smallint,
    k bytea, l inet ) SERVER pg_jinx_server;
    
CREATE FOREIGN TABLE example.property(k varchar, v varchar) SERVER pg_jinx_server OPTIONS (class 'org.sourcewave.jinx.example.FdwProperty');

create function example.countRows(varchar) returns int as 'org.sourcewave.jinx.example.Example.countRows' language javax;
    
/*
    select * from getOptions() as (key varchar, val varchar) order by key;
    */
create or replace function example.getOptions() returns setof record as 'org.sourcewave.jinx.example.Example.getOptions' volatile language javax;

create or replace function example.getGravatar() returns trigger as 'org.sourcewave.jinx.example.GravatarTrigger.xxx' volatile language javax;

create table example.TestTrigger(email varchar, gravatar varchar, primary key (email));

create trigger getGravatar BEFORE INSERT OR UPDATE ON example.TestTrigger FOR EACH ROW EXECUTE PROCEDURE example.getGravatar();

