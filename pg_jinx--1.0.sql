
CREATE FUNCTION pg_jinx_fdw_handler() RETURNS fdw_handler AS 'MODULE_PATHNAME' LANGUAGE C STRICT;
CREATE FUNCTION pg_jinx_fdw_validator(text[], oid) RETURNS void AS 'MODULE_PATHNAME' LANGUAGE C STRICT;
CREATE FOREIGN DATA WRAPPER pg_jinx HANDLER pg_jinx_fdw_handler VALIDATOR pg_jinx_fdw_validator;

CREATE OR REPLACE FUNCTION java_call_handler() RETURNS language_handler AS 'pg_jinx' LANGUAGE C;
CREATE TRUSTED LANGUAGE java HANDLER java_call_handler;

CREATE OR REPLACE FUNCTION javau_call_handler() RETURNS language_handler AS 'pg_jinx' LANGUAGE C;
CREATE LANGUAGE javaU HANDLER javau_call_handler;
