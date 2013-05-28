
#ifdef __APPLE__
#include <libproc.h>
#endif

#include <errno.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pg_config.h>
#include <postgres.h>

/*#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
*/

#include <miscadmin.h>
#ifndef WIN32
#include <libpq/pqsignal.h>
#endif

#include <fmgr.h>
#include <funcapi.h>

#include <jni.h>

#include <access/heapam.h>
#include <access/htup.h>
#include <access/htup_details.h>
#include <access/reloptions.h>
#include <access/tupdesc.h>
#include <access/xact.h>

#include <catalog/catalog.h>
#include <catalog/pg_foreign_data_wrapper.h>
#include <catalog/pg_foreign_server.h>
#include <catalog/pg_foreign_table.h>
#include <catalog/pg_operator.h>
#include <catalog/pg_proc.h>
#include <catalog/pg_type.h>
#include <catalog/pg_user_mapping.h>

#include <commands/defrem.h>
#include <commands/explain.h>
#include <commands/trigger.h>

#include <executor/tuptable.h>
#include <executor/spi.h>

#include <foreign/fdwapi.h>
#include <foreign/foreign.h>

#include <lib/stringinfo.h>

#include <mb/pg_wchar.h>

#include <nodes/nodes.h>
#include <nodes/relation.h>
#include <nodes/execnodes.h>

#include <optimizer/cost.h>
#include <optimizer/clauses.h>
#include <optimizer/subselect.h>

#if (PG_VERSION_NUM >= 90200)
#include <optimizer/pathnode.h>
#include <optimizer/restrictinfo.h>
#include <optimizer/planmain.h>
#include <optimizer/var.h>
#endif

#include <storage/fd.h>
#include <storage/ipc.h>
#include <storage/large_object.h>
#include <storage/proc.h>

#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/date.h>
#include <utils/elog.h>
#include <utils/guc.h>
#include <utils/lsyscache.h>
#include <utils/numeric.h>
#include <utils/rel.h>
#include <utils/syscache.h>
#include <utils/timestamp.h>

// typedef unsigned int Oid;
typedef unsigned int jOid;


#define CHECK_EXCEPTION(MSG, ARG) check_exception(MSG, ARG)

#define CACHE_METHOD(y,cc,mm,rr) { jmethodID jmx = (*env)->GetMethodID(env, cc, mm, rr); \
	CHECK_EXCEPTION("%s\nError finding method "mm" in class %s", "complicated"); \
	y = (jmethodID)(*env)->NewGlobalRef(env, (jobject)jmx); \
	CHECK_EXCEPTION("%s\nCreating global reference to method "mm" in class %s", "complicated"); \
}

#define CACHE_STATIC_METHOD(y,cc,mm,rr) { jmethodID jmx = (*env)->GetStaticMethodID(env, cc, mm, rr); \
	CHECK_EXCEPTION("%s\nError finding static method "mm" in class %s", "complicated"); \
	y = (jmethodID)(*env)->NewGlobalRef(env, (jobject)jmx); \
	CHECK_EXCEPTION("%s\nCreating global reference to static method "mm" in class %s", "complicated"); \
}

#define CACHE_CLASS(y,x) { \
    jclass jc; \
    char *p; \
    char buf[1024]; \
    snprintf(buf, 1024, "L%s;", x); \
    while( NULL != (p = strchr(buf,'.'))) *p='/'; \
    (*env)->ExceptionClear(env); \
    jc = (*env)->FindClass(env, buf); \
    CHECK_EXCEPTION("%s\nError finding class %s", x); \
    y = (*env)->NewGlobalRef(env,jc); \
    CHECK_EXCEPTION("%s\nCreating global reference to class %s",x); \
}



extern JNIEnv *env;
extern JavaVM *jvm;

extern void initializeJVM(bool);

extern void check_exception(const char *MSG, const char *arg);
extern Datum pg_jinx_fdw_handler(PG_FUNCTION_ARGS);
extern bool InterruptFlag, TerminationFlag;


#if (PG_VERSION_NUM < 90200)
	extern FdwPlan *javaPlanForeignScan(Oid foreigntableid,PlannerInfo *root, RelOptInfo *baserel);
#endif

#if (PG_VERSION_NUM >= 90200)
	extern void javaGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
	extern void javaGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
	extern ForeignScan *javaGetForeignPlan(
	    PlannerInfo *root, 
	    RelOptInfo *baserel, 
	    Oid foreigntableid, 
	    ForeignPath *best_path, 
	    List *tlist, 
	    List *scan_clauses
	);
#endif

extern void javaExplainForeignScan(ForeignScanState *node, ExplainState *es);
extern void javaBeginForeignScan(ForeignScanState *node, int eflags);
extern TupleTableSlot *javaIterateForeignScan(ForeignScanState *node);
extern void javaReScanForeignScan(ForeignScanState *node);
extern void javaEndForeignScan(ForeignScanState *node);

extern void javaAddForeignUpdateTargets(Query *parsetree,
  RangeTblEntry *target_rte, Relation target_relation);
extern List *javaPlanForeignModify(PlannerInfo *root,
  ModifyTable *plan, Index resultRelation, int subplan_index);
extern void javaBeginForeignModify(ModifyTableState *mtstate,
  ResultRelInfo *rinfo, List *fdw_private, int subplan_index, int eflags);
extern TupleTableSlot *javaExecForeignInsert( EState *estate,
  ResultRelInfo *rinfo, TupleTableSlot *slot, TupleTableSlot *planSlot);
extern TupleTableSlot *javaExecForeignUpdate(EState *estate,
  ResultRelInfo *rinfo, TupleTableSlot *slot, TupleTableSlot *planSlot);
extern TupleTableSlot *javaExecForeignDelete(EState *estate,
  ResultRelInfo *rinfo, TupleTableSlot *slot, TupleTableSlot *planSlot);
extern void javaEndForeignModify(EState *estate, ResultRelInfo *rinfo);

extern jobject tupleToObject(TupleDesc tupdesc, HeapTuple tuple);

// -----------



extern jclass mapClass, classClass, throwableClass, sqlException, stringClass, objClass;
extern jclass intClass, dblClass, shortClass, floatClass, byteClass, byteArrayClass, booleanClass,
    longClass, charClass, dateClass;
extern jclass selectionClass, columnClass, colrefClass, pgfuncClass;
extern jclass serverExceptionClass, functionClass, byteArrayClass, booleanArrayClass, intArrayClass,
 shortArrayClass, longArrayClass, floatArrayClass, doubleArrayClass, bigDecimalClass, iterator;

extern jmethodID mapConstructor, mapPut, classGetName, getMessage, sqlExceptionConstructor, getSQLState, printStackTrace, getClass;
extern jmethodID selectionConstructor, columnConstructor, colrefConstructor, 
    pgfunc0Cons, pgfunc1Cons, pgfunc2Cons, pgfunc3Cons, pgfuncnCons;

extern jmethodID intValue, doubleValue, longValue, shortValue, floatValue, booleanValue, byteValue, charValue;
extern jmethodID intConstructor, dblConstructor, shortConstructor, floatConstructor, longConstructor, boolConstructor,
    dateConstructor,
    byteConstructor, charConstructor, bigDecimalConstructor, toString, hasNext, next;
extern jmethodID serverExceptionConstructor, functionConstructor, functionInvoke, triggerInvoke;

extern bool integerDateTimes;

typedef struct {
	SubTransactionId xid;
	int  nestingLevel;
	char name[1];
} Savepoint;

//--------------------

extern jobject datumToObject(Oid paramtype, Datum d);

extern Datum objectToDatum(Oid paramtype, jobject j);
extern int objectArrayToDatumArray(jarray jary, Oid**oids, Datum**dats, char **nulls);
extern HeapTuple handleRecordD(jarray result, TupleDesc tupdesc);

extern jstring makeJavaString(const char* cp);
extern char *fromJavaString(jstring s);

#define ERRCODE_INVALID_DESCRIPTOR_INDEX		MAKE_SQLSTATE('0','7', '0','0','9')

