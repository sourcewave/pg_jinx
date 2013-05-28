
#include <pg_jinx.h>

extern jobject makeQual( jobject fld, char *opname, jobject value);
    
typedef struct {
	int NumberOfRows;
	int NumberOfColumns;
	jobject java_call;
    jmethodID planForeignScan, getForeignRelSize, getForeignPaths, getForeignPlan, 
        explainForeignScan, beginForeignScan, iterateForeignScan, reScanForeignScan, 
        endForeignScan, cancelForeignScan, addForeignUpdateTargets,
	 	planForeignModify, beginForeignModify, 
		execForeignInsert, execForeignUpdate, execForeignDelete,
		endForeignModify;
} javaFdwExecutionState;

static void SIGINTInterruptCheckProcess(javaFdwExecutionState *);
static jobject fromNode(PlannerInfo *root, Expr *node);

void check_exception(const char *MSG, const char *arg) {
    jthrowable xc = (*env)->ExceptionOccurred(env);
    jclass exClass;
    jmethodID mid;
    jboolean isCopy;
    const char *cmsg;
    jstring errMsg;
    
    if (xc != NULL) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	    exClass = (*env)->GetObjectClass( env, xc );
		errMsg = (jstring)(*env)->CallObjectMethod( env, xc, toString );
		isCopy = JNI_FALSE;
		cmsg = (*env)->GetStringUTFChars( env, errMsg, &isCopy );
        char buf[4096];
        strncpy(buf, cmsg, 4096);
		(*env)->ReleaseStringUTFChars( env, errMsg, cmsg );
        ereport(ERROR,(errcode(99), (errmsg(MSG, buf, arg))));
    }
}

int extensionDebug;

static void SIGINTInterruptCheckProcess( javaFdwExecutionState *state) {

	if (InterruptFlag == true) {
		// jstring 	cancel_result = NULL;
		// char 		*cancel_result_cstring = NULL;

		(*env)->CallObjectMethod(env, state->java_call, state->cancelForeignScan);

		// elog(ERROR, "%s", ConvertStringToCString((jobject)cancel_result));

		InterruptFlag = false;
		elog(ERROR, "Query has been cancelled");

		// (*env)->ReleaseStringUTFChars(env, cancel_result, cancel_result_cstring);
		// (*env)->DeleteLocalRef(env, cancel_result);
	}
}



static javaFdwExecutionState* javaGetOptions(Oid foreigntableid) {
	ForeignTable	*f_table;
	ForeignServer	*f_server;
	UserMapping	*f_mapping;
	List		*options;
	ListCell	*cell;
    jobject obj, res, serverClass;
    jmethodID serverConstructor;
    jclass cl;
    javaFdwExecutionState *state;
    char *classname = NULL;
    
	options = NIL;
	f_table = GetForeignTable(foreigntableid);
	options = list_concat(options, f_table->options);

	f_server = GetForeignServer(f_table->serverid);	
	options = list_concat(options, f_server->options);

	PG_TRY();
	{
		f_mapping = GetUserMapping(GetUserId(), f_table->serverid);
		options = list_concat(options, f_mapping->options);
	}
	PG_CATCH();
	{
		FlushErrorState();
		/* DO NOTHING HERE */
	}
	PG_END_TRY();
	

	foreach(cell, options) {
		DefElem *def = (DefElem *) lfirst(cell);
        if (strcmp(def->defname, "class") == 0) {
            classname = (char *) defGetString(def);
        }
    }

    if (classname == NULL) {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), (errmsg("%s", "the 'class' option was not specified"))));
    }    

    initializeJVM(false);  // trusted or untrusted?

    obj = (*env)->NewObject(env, mapClass, mapConstructor);
    CHECK_EXCEPTION("%s\nexecuting map constructor","unused");
    foreach(cell, options) {
        DefElem *def = (DefElem *) lfirst(cell);
        jobject jone = (*env)->NewStringUTF(env, def->defname);
        
        // This could be other types besides string (numeric)
        jobject jtwo = (*env)->NewStringUTF(env, defGetString(def));
        (*env)->CallObjectMethod(env, obj, mapPut, jone, jtwo);
        CHECK_EXCEPTION("%s\nerror putting option entry in map","unused");
    }

    CACHE_CLASS(selectionClass, "org.sourcewave.jinx.fdw.Selection");
    CACHE_METHOD(selectionConstructor, selectionClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)V")
    CACHE_CLASS(serverClass, classname );
    CACHE_METHOD(serverConstructor, serverClass, "<init>", "(Ljava/util/Map;)V");
    res = (*env)->NewObject(env, serverClass, serverConstructor, obj);
    CHECK_EXCEPTION("%s\ninstantiating java scan object","unused");
    
    cl = (*env)->GetObjectClass(env, res);

    state = (javaFdwExecutionState *) palloc(sizeof(javaFdwExecutionState));
    state->java_call = (*env)->NewGlobalRef(env, res);
    
    CACHE_CLASS(columnClass, "org.sourcewave.jinx.fdw.Column");
    
    CACHE_CLASS(colrefClass, "org.sourcewave.jinx.fdw.ColumnReference");
    CACHE_METHOD(colrefConstructor, colrefClass, "<init>", "(Ljava/lang/String;I)V");

    CACHE_CLASS(pgfuncClass, "org.sourcewave.jinx.fdw.PgFunction");
    CACHE_METHOD(pgfunc0Cons, pgfuncClass, "<init>", "(I)V");
    CACHE_METHOD(pgfunc1Cons, pgfuncClass, "<init>", "(ILjava/lang/Object;)V");
    CACHE_METHOD(pgfunc2Cons, pgfuncClass, "<init>", "(ILjava/lang/Object;Ljava/lang/Object;)V");
    CACHE_METHOD(pgfunc3Cons, pgfuncClass, "<init>", "(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V");
    CACHE_METHOD(pgfuncnCons, pgfuncClass, "<init>", "(I[Ljava/lang/Object;)V");
    
    CACHE_METHOD(columnConstructor, columnClass, "<init>", "(Ljava/lang/String;IIZ)V");
    CACHE_METHOD(state->planForeignScan, cl, "planForeignScan", "()V" );
    CACHE_METHOD(state->getForeignRelSize, cl, "getForeignRelSize", "([Lorg/sourcewave/jinx/fdw/ColumnReference;[Lorg/sourcewave/jinx/fdw/ColumnReference;[Lorg/sourcewave/jinx/fdw/PgFunction;)[I" );
    CACHE_METHOD(state->getForeignPaths, cl, "getForeignPaths", "()V" );
    CACHE_METHOD(state->getForeignPlan, cl, "getForeignPlan", "()V" );
    CACHE_METHOD(state->explainForeignScan, cl, "explainForeignScan", "()V" );
    CACHE_METHOD(state->beginForeignScan, cl, "beginForeignScan", "([Lorg/sourcewave/jinx/fdw/Column;)V" );
    CACHE_METHOD(state->iterateForeignScan, cl, "iterateForeignScan", "()[Ljava/lang/Object;" );
    CACHE_METHOD(state->endForeignScan, cl, "endForeignScan", "()V" );
    CACHE_METHOD(state->cancelForeignScan, cl, "cancelForeignScan", "()V" );

	CACHE_METHOD(state->addForeignUpdateTargets, cl, "addForeignUpdateTargets", "()V" );
	// (Query *parsetree, RangeTblEntry *target_rte, Relation target_relation) 
	
	CACHE_METHOD(state->planForeignModify, cl, "planForeignModify", "()V");
	// (PlannerInfo *root, ModifyTable *plan, Index resultRelation, int subplan_index);

	CACHE_METHOD(state->beginForeignModify, cl, "beginForeignModify", "()V");
	// (ModifyTableState *mtstate, ResultRelInfo *rinfo, List *fdw_private, int subplan_index, int eflags)

	CACHE_METHOD(state->execForeignInsert, cl, "execForeignInsert", "()V");
	// (EState *estate, ResultRelInfo *rinfo, TupleTableSlot *slot, TupleTableSlot *planSlot) 

	CACHE_METHOD(state->execForeignUpdate, cl, "execForeignUpdate", "()V");
	// (EState *estate, ResultRelInfo *rinfo, TupleTableSlot *slot, TupleTableSlot *planSlot)

	CACHE_METHOD(state->execForeignDelete, cl, "execForeignDelete", "()V");
	// (EState *estate, ResultRelInfo *rinfo, TupleTableSlot *slot, TupleTableSlot *planSlot) 

	CACHE_METHOD(state->endForeignModify, cl, "endForeignModify", "()V");
	// (EState *estate, ResultRelInfo *rinfo) 

    return state;
}

#if (PG_VERSION_NUM < 90200)
FdwPlan*javaPlanForeignScan(Oid foreigntableid, PlannerInfo *root, RelOptInfo *baserel) {
	FdwPlan 	*fdwplan = NULL;
	SIGINTInterruptCheckProcess();

	fdwplan = makeNode(FdwPlan);

	initializeJVM(false); // is this trusted or not?

	/* Fetch options */
    jobject jobj = javaGetOptions( foreigntableid );
    {
		size_t len = strlen(svr_table) + 23;
		query = (char *) palloc(len);
		snprintf(query, len, "EXPLAIN SELECT * FROM %s", svr_table);
	}
	return (fdwplan);
}
#endif



void javaExplainForeignScan(ForeignScanState *node, ExplainState *es) {
	javaFdwExecutionState   *state = (javaFdwExecutionState *)node->fdw_state;
	SIGINTInterruptCheckProcess(state);

    (*env)->CallObjectMethod(env, state->java_call, state->explainForeignScan);
    CHECK_EXCEPTION("%s\ncalling explainForeignScan","unused");
}

void javaBeginForeignScan(ForeignScanState *node, int eflags) {
	javaFdwExecutionState *state;

// there is no way to get the private data from the GetForeignXXX calls
    node->fdw_state = ((ForeignScan *)node->ss.ps.plan)->fdw_private;
    state = (javaFdwExecutionState *)node->fdw_state;

	SIGINTInterruptCheckProcess(state);

    {
        int i=0;
    AttInMetadata *md = TupleDescGetAttInMetadata(node->ss.ss_currentRelation->rd_att);

    jobject mdx = (*env)->NewObjectArray(env, md->tupdesc->natts, columnClass, NULL);
    CHECK_EXCEPTION("%s\nunable to create metadata array","unused");
    // TupleDescGetAttInMetadata(node->ss.ss_ScanTupleSlot->tts_tupleDescriptor);
    for(i=0;i<md->tupdesc->natts;i++) {
        jobject nam, ct;
        Form_pg_attribute fa = md->tupdesc->attrs[i];
        nam = (*env)->NewStringUTF(env, NameStr(fa->attname));
        CHECK_EXCEPTION("%s\ncreating column name","unused");
        ct = (*env)->NewObject(env, columnClass, columnConstructor, nam, (jint)fa->atttypid, (jint)fa->atttypmod, (jboolean) fa-> attnotnull);
        CHECK_EXCEPTION("%s\ncreating column type","unused");
        (*env)->DeleteLocalRef(env, nam);
        (*env)->SetObjectArrayElement(env, mdx, i, ct);
        CHECK_EXCEPTION("%s\nsetting column type","unused");
	}
    
    (*env)->CallObjectMethod(env, state->java_call, state->beginForeignScan, mdx);
    CHECK_EXCEPTION("%s\ncalling beginForeignScan","unused");
    }
    // id_numberofcolumns = (*env)->GetFieldID(env, JDBCUtilsClass, "NumberOfColumns" , "I");
	// if (id_numberofcolumns == NULL) { elog(ERROR, "id_numberofcolumns is NULL"); }
}

TupleTableSlot* javaIterateForeignScan(ForeignScanState *node) {
	HeapTuple		tuple;
	jobjectArray 		java_rowarray; 
    jint colcount;
    
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
	javaFdwExecutionState   *state = (javaFdwExecutionState *)node->fdw_state;
    AttInMetadata *attinmeta = TupleDescGetAttInMetadata(node->ss.ss_currentRelation->rd_att);
    TupleDesc	tupdesc = attinmeta->tupdesc;
    int natts = tupdesc->natts;
    Datum *dvalues = (Datum *) palloc(natts * sizeof(Datum));
    bool *nulls = (bool *) palloc(natts * sizeof(bool));
    int i;
    
    ExecClearTuple(slot);
	SIGINTInterruptCheckProcess(state);

	java_rowarray = (*env)->CallObjectMethod(env, state -> java_call, state-> iterateForeignScan);
    CHECK_EXCEPTION("%s\ncalling iterateForeignScan","unused");
    if (java_rowarray == NULL) return slot;

    tuple  = handleRecordD(java_rowarray, tupdesc);
	ExecStoreTuple(tuple, slot, InvalidBuffer, false);
    return slot;
}

/*
 * jdbcEndForeignScan
 *		Finish scanning foreign table and dispose objects used for this scan
 */
void javaEndForeignScan(ForeignScanState *node) {
    javaFdwExecutionState *state = (javaFdwExecutionState *)node->fdw_state;
	SIGINTInterruptCheckProcess(state);
    
    (*env)->CallObjectMethod(env, state->java_call, state->endForeignScan);
    CHECK_EXCEPTION("%s\ncalling endForeignScan","unused");

	// (*env)->ReleaseStringUTFChars(env, close_result, close_result_cstring);
	// (*env)->DeleteLocalRef(env, close_result);
	(*env)->DeleteGlobalRef(env, state->java_call);
}

void javaReScanForeignScan(ForeignScanState *node) {
	SIGINTInterruptCheckProcess(node->fdw_state);
}

#if (PG_VERSION_NUM >= 90200)
void javaGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid) {
	Cost 		startup_cost = 0;
	Cost 		total_cost = 0;

	SIGINTInterruptCheckProcess(baserel->fdw_private);

	/* Create a ForeignPath node and add it as only possible path */
	add_path(baserel, (Path*)create_foreignscan_path(root, baserel, baserel->rows, startup_cost, total_cost, NIL, NULL, (void *)baserel->fdw_private)); 
}

ForeignScan *javaGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses) {
	Index 		scan_relid = baserel->relid;
    
	SIGINTInterruptCheckProcess(baserel->fdw_private);

	scan_clauses = extract_actual_clauses(scan_clauses, false);

	/* Create the ForeignScan node */
	return (make_foreignscan(tlist, scan_clauses, scan_relid, NIL, 
	    /*serializePlanState( */ baserel->fdw_private /* ) */ )); 
}

static jobject colRef(PlannerInfo *root, Var *var) {
    RangeTblEntry *rte;
    char *attname;
    jstring js;
    jobject res;
    
    if (var == NULL) return NULL;
    rte = root->simple_rte_array[var->varno];
    attname = get_attname(rte->relid, var->varattno);
    js = (*env)->NewStringUTF(env, attname);
    res = (*env)->NewObject(env, colrefClass, colrefConstructor, js, var->varattno);
    return res;
}

static jobject columnList(List *columns, PlannerInfo *root) {
    ListCell *lc;
    int i=0;
    int ll = list_length(columns);
    jobject ca = (*env)->NewObjectArray(env, ll, colrefClass, NULL);
    CHECK_EXCEPTION("%s\ncreating column array","unused");
    foreach(lc, columns) {
        jobject jj= colRef(root, (Var *) lfirst(lc));
        (*env)->SetObjectArrayElement(env, ca, (jint)i++, jj);
	}
    return ca;
}

// This is the first entry point to be called
void javaGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid) {
    javaFdwExecutionState *state = javaGetOptions( foreigntableid);
    ListCell   *lc;
    List	   *columns = NULL;
    int rl, ri;
    jobject rc, res, ca, cx;
    
    baserel -> fdw_private = (void *) state;
	SIGINTInterruptCheckProcess(state);

    foreach(lc, baserel->reltargetlist) {
		Node *node = (Node *) lfirst(lc);
		List *targetcolumns = pull_var_clause(node, PVC_RECURSE_AGGREGATES, PVC_RECURSE_PLACEHOLDERS);
        columns = list_union(columns, targetcolumns);
    }
    ca = columnList(columns, root);
    
    columns = NULL;
    foreach(lc, baserel->baserestrictinfo) {
		RestrictInfo *node = (RestrictInfo *) lfirst(lc);
		List	   *targetcolumns = pull_var_clause((Node *) node->clause, PVC_RECURSE_AGGREGATES, PVC_RECURSE_PLACEHOLDERS);
		columns = list_union(columns, targetcolumns);
	}
    cx = columnList(columns, root);
    
    rl = list_length(baserel->baserestrictinfo);
    
    // This class needs to be some superclass of selection/booleanexpression/nulltest?
    rc = (*env)->NewObjectArray(env, rl, pgfuncClass, NULL);
    CHECK_EXCEPTION("%s\ncreating column array","unused");
    ri = 0;
    foreach(lc, baserel->baserestrictinfo) {
        jobject rrr = fromNode(root, ((RestrictInfo *) lfirst(lc))->clause);
        (*env)->SetObjectArrayElement(env, rc, (jint)ri++, rrr);
        CHECK_EXCEPTION("%s\n storing selection element %d", ri);
    }
    
	// insert the results into   baserel->rows    and   baserel->width
    res = (*env)->CallObjectMethod(env, state->java_call, state->getForeignRelSize, ca, cx, rc);
    CHECK_EXCEPTION("%s\ncalling getForeignRelSize","unused");

    if (2 != (*env)->GetArrayLength(env, res)) {
        ereport(ERROR, (errcode(99), (errmsg("foreignRelSize must return 2 element integer array"))));
    }
    
    {
    jboolean jb = JNI_FALSE;
    
    jint* rx = (*env)->GetIntArrayElements(env, res, &jb);   // number of expected rows
    baserel -> rows = rx[0];
    baserel -> width = rx[1]; // number of bytes per row
    (*env)->ReleaseIntArrayElements(env, res, rx, JNI_ABORT);
    }
}
#endif

/*
static char *getOperatorString(Oid opoid) {
	HeapTuple	tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opoid));
	Form_pg_operator operator;

	if (!HeapTupleIsValid(tp)) elog(ERROR, "cache lookup failed for operator %u", opoid);
	operator = (Form_pg_operator) GETSTRUCT(tp);
	ReleaseSysCache(tp);
	return NameStr(operator->oprname);
}
*/

static jobject fromNode(PlannerInfo *root, Expr *node) {
    if (IsA(node, Var)) return colRef(root, (Var *)node);
    if (IsA(node, Const)) {
        Const *con = (Const *)node;
        if (con->constisnull) return NULL;
        switch(con->consttype) {
            case BOOLOID: return (*env)->NewObject(env, booleanClass, boolConstructor, DatumGetBool(con->constvalue)); 
            case INT4OID: return (*env)->NewObject(env, intClass, intConstructor, DatumGetInt32(con->constvalue));
            case FLOAT8OID: return (*env)->NewObject(env, dblClass, dblConstructor, DatumGetFloat8(con->constvalue));
            case INT2OID: return (*env)->NewObject(env, shortClass, shortConstructor, DatumGetInt16(con->constvalue));
            case FLOAT4OID: return (*env)->NewObject(env, floatClass, floatConstructor, DatumGetFloat4(con->constvalue));
            case VARCHAROID:
            case TEXTOID: {
	
		        char *str = TextDatumGetCString(con->constvalue);
		        char* utf8 = (char*)pg_do_encoding_conversion((unsigned char*)str, strlen(str), GetDatabaseEncoding(), PG_UTF8);
		        jstring result = (*env)->NewStringUTF(env, utf8);
		        if(utf8 != str) pfree(utf8);
		        pfree(str);
		        return result;
	/*
				char *dsx = VARDATA(con->constvalue);
				int nn = VARSIZE(con->constvalue);
				jstring js = (*env)->NewString(env, dsx, nn);
				// do I need to do something to free memory from StringInfoData?
				return js;
				*/
			}
            default: break;
        }
    }
/*    if (IsA(node, Array)) {  // ArrayRef ?
    }
    */
    if (IsA(node, RelabelType)) {
        return fromNode(root, ((RelabelType *)node)->arg);
    }
    if (IsA(node, Param)) {
    }
    if (IsA(node, BoolExpr)) {
    }
    if (IsA(node, NullTest)) { // 
        NullTest *nt = (NullTest *)node;
//		char *opname = (nt->nulltesttype == IS_NULL) ? "=" : "<>";
// TODO: 101 and 102 are the wrong things here:  should be whatever the right thing is for IS_NULL and NOT_NULL
        return (*env)->NewObject(env, pgfuncClass, pgfunc1Cons, nt->nulltesttype == IS_NULL ? 101 : 102, fromNode(root, nt->arg));
    }
    if (IsA(node, OpExpr)) { // is this the same as a funcexpr?
        OpExpr *ox = (OpExpr *)node;
        if (list_length(ox->args) == 2) {
            Node *l = list_nth(ox->args, 0);
            Node *r = list_nth(ox->args, 1);
//            return makeQual( fromNode(root, (Expr *)l), getOperatorString(ox->opno), fromNode(root, (Expr *)r) );
            return (*env)->NewObject(env, pgfuncClass, pgfunc2Cons, ox->opno, fromNode(root, (Expr *)l), fromNode(root, (Expr *)r));
        }
        ereport(ERROR, (errmsg("opexpr with %d args for %d", list_length(ox->args), ox->opno)));
    }
    if (IsA(node, ArrayCoerceExpr)) {
        return fromNode(root,((ArrayCoerceExpr *) node)->arg);
    }
    if (IsA(node, DistinctExpr)) {
    }
    if (IsA(node, ScalarArrayOpExpr)) { // expressions such as 'ANY (...)", "ALL (...)"
    }
    if (IsA(node, FuncExpr)) {
        FuncExpr *fx = (FuncExpr *)node;
        int fi = fx->funcid;
        int ln = list_length(fx->args);
        switch(ln) {
            case 0: return (*env)->NewObject(env, pgfuncClass, pgfunc0Cons);
            case 1: return (*env)->NewObject(env, pgfuncClass, pgfunc1Cons, fi, fromNode(root,list_nth(fx->args, 0)));
            case 2: return (*env)->NewObject(env, pgfuncClass, pgfunc2Cons, fi, fromNode(root,list_nth(fx->args, 0)), fromNode(root,list_nth(fx->args,1)));
            case 3: return (*env)->NewObject(env, pgfuncClass, pgfunc3Cons, fi, fromNode(root,list_nth(fx->args, 0)), fromNode(root,list_nth(fx->args,1)), fromNode(root,list_nth(fx->args,2)));
            // here should go the handling for multiple (>3) arguments
        }
        ereport(ERROR, (errmsg("funcid %d, args = %d\n",fi, ln)));
    }    
    ereport(ERROR, (errmsg("converting node to Java object failed")));
    return NULL;
}

/*
static jobject extractClauseFromOpExpr(PlannerInfo *root, RelOptInfo *baserel, OpExpr *op) {

			case T_Var:
				// This job could/should? be done in the core by replace_nestloop_vars. Infortunately, its private. 
				if (bms_is_member(((Var *) right)->varno, root->curOuterRels)) {
					Param *param = assign_nestloop_param_var(root, (Var *) right);
                    return makeQual(attname, getOperatorString(op->opno), fromNode(root, (Expr *) param));
				}
				break;
				// Ignore other node types. 
			default:
				break;
		}
	}
	ereport(ERROR, (errmsg("extractClauseFromOpExpr arg is not handled")));
    return NULL;
}
*/

jobject makeQual( jobject fld, char *opname, jobject value) {
    jobject op = (*env)->NewStringUTF(env, opname);
    jobject res = (*env)->NewObject(env, selectionClass, selectionConstructor, fld, op, value);
    CHECK_EXCEPTION("%s\ncreating a Selection","unused");
//    (*env)->DeleteLocalRef(env, fld);
//    (*env)->DeleteLocalRef(env, op);
    return res;
}


void javaAddForeignUpdateTargets(Query *parsetree, RangeTblEntry *target_rte, Relation target_relation) {
/*    javaFdwExecutionState *state = (javaFdwExecutionState *)node->fdw_state;
	SIGINTInterruptCheckProcess(state);  
    (*env)->CallObjectMethod(env, state->java_call, state->addForeignUpdateTargets);
    CHECK_EXCEPTION("%s\ncalling addForeignUpdateTargers","unused");
*/
	printf("add foreign update targets\n");
}
List *javaPlanForeignModify(PlannerInfo *root, ModifyTable *plan, Index resultRelation, int subplan_index) {
/*    javaFdwExecutionState *state = (javaFdwExecutionState *)node->fdw_state;
	SIGINTInterruptCheckProcess(state);
    (*env)->CallObjectMethod(env, state->java_call, state->planForeignModify);
    CHECK_EXCEPTION("%s\ncalling planForeignModify","unused");
*/
	printf("plan foreign modify\n");
	return NULL;
}
void javaBeginForeignModify(ModifyTableState *mtstate, ResultRelInfo *rinfo, List *fdw_private, int subplan_index, int eflags) {
/*    javaFdwExecutionState *state = (javaFdwExecutionState *)node->fdw_state;
	SIGINTInterruptCheckProcess(state);
    (*env)->CallObjectMethod(env, state->java_call, state->beginForeignModify);
    CHECK_EXCEPTION("%s\ncalling beginForeignModify","unused");
*/
	printf("begin foreign modify\n");
}
TupleTableSlot *javaExecForeignInsert(EState *estate, ResultRelInfo *rinfo, TupleTableSlot *slot, TupleTableSlot *planSlot) {
/*    javaFdwExecutionState *state = (javaFdwExecutionState *)node->fdw_state;
	SIGINTInterruptCheckProcess(state);
    (*env)->CallObjectMethod(env, state->java_call, state->execForeignInsert);
    CHECK_EXCEPTION("%s\ncalling execForeignInsert","unused");
*/
	printf("exec foreign insert\n");
	return slot;
}
TupleTableSlot *javaExecForeignUpdate(EState *estate, ResultRelInfo *rinfo, TupleTableSlot *slot, TupleTableSlot *planSlot) {
/*    javaFdwExecutionState *state = (javaFdwExecutionState *)node->fdw_state;
	SIGINTInterruptCheckProcess(state);
    (*env)->CallObjectMethod(env, state->java_call, state->execForeignUpdate);
    CHECK_EXCEPTION("%s\ncalling execForeignUpdate","unused");
*/
	printf("exec foreign update\n");
	return slot;
}
TupleTableSlot *javaExecForeignDelete(EState *estate, ResultRelInfo *rinfo, TupleTableSlot *slot, TupleTableSlot *planSlot) {
/*    javaFdwExecutionState *state = (javaFdwExecutionState *)node->fdw_state;
	SIGINTInterruptCheckProcess(state);
    (*env)->CallObjectMethod(env, state->java_call, state->execForeignDelete);
    CHECK_EXCEPTION("%s\ncalling execForeignDelete","unused");	
*/
	printf("exec foreign delete\n");
	return slot;
}
void javaEndForeignModify(EState *estate, ResultRelInfo *rinfo) {
/*    javaFdwExecutionState *state = (javaFdwExecutionState *)node->fdw_state;
	SIGINTInterruptCheckProcess(state);
    (*env)->CallObjectMethod(env, state->java_call, state->endForeignModify);
    CHECK_EXCEPTION("%s\ncalling endForeignModify","unused");

	//	(*env)->DeleteGlobalRef(env, state->java_call);	
	*/
	printf("end foreign modify\n");
}

