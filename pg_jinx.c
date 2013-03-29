
#include <pg_jinx.h>

// extern void destroyJVM(int status, Datum dummy);
extern void initializeJVM(bool);

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(pg_jinx_fdw_validator);
PG_FUNCTION_INFO_V1(pg_jinx_fdw_handler);

extern Datum pg_jinx_fdw_validator(PG_FUNCTION_ARGS);
// FOREIGN DATA WRAPPER, SERVER, USER MAPPING or FOREIGN TABLE options.
Datum pg_jinx_fdw_validator(PG_FUNCTION_ARGS) {
//	List		*options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid		catalog = PG_GETARG_OID(1);
//	char *jarfile = NULL;
//	ListCell	*cell;
//    char *classname = NULL;
    
    if (catalog == ForeignDataWrapperRelationId) {
        PG_RETURN_VOID();
    };
    
/*	foreach(cell, options_list) {
		DefElem *def = (DefElem *) lfirst(cell);
        if (strcmp(def->defname, "javaclass") == 0) {
            if (catalog == ForeignTableRelationId) {
                ereport(ERROR, (errmsg("%s", "can only set the javaclass on the server")));
            } else {
                classname = (char *) defGetString(def);
            }
        } else if (strcmp(def->defname, "jarfile") == 0) {
            if (catalog == ForeignTableRelationId) {
                ereport(ERROR, (errmsg("%s", "can only set the jarfile on the server")));
            } else {
                jarfile = (char *) defGetString(def);
            }
        }
    }
    if (catalog == ForeignServerRelationId) {
        if (classname == NULL || jarfile == NULL) {
            ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), (errmsg("%s", "jarfile and javaclass parameters are mandatory"))));
        }
        
    }
    */
    PG_RETURN_VOID();
    
/*    if (jarfile == NULL) {
        ereport(ERROR, (errmsg("%s", "jarfile not set")));
    }
    */
}

// boilerplate

Datum pg_jinx_fdw_handler(PG_FUNCTION_ARGS) {
    FdwRoutine *fdwroutine = makeNode(FdwRoutine);
	
	#if (PG_VERSION_NUM < 90200)
	fdwroutine->PlanForeignScan = javaPlanForeignScan;
	#endif

	#if (PG_VERSION_NUM >= 90200)
	fdwroutine->GetForeignRelSize = javaGetForeignRelSize;
	fdwroutine->GetForeignPaths = javaGetForeignPaths;
	fdwroutine->GetForeignPlan = javaGetForeignPlan;
	#endif

	fdwroutine->ExplainForeignScan = javaExplainForeignScan;
	fdwroutine->BeginForeignScan = javaBeginForeignScan;
	fdwroutine->IterateForeignScan = javaIterateForeignScan;
	fdwroutine->ReScanForeignScan = javaReScanForeignScan;
	fdwroutine->EndForeignScan = javaEndForeignScan;

	PG_RETURN_POINTER(fdwroutine);
}
// end boilerplate for FDW


/*static Datum internalCallHandler(bool trusted, PG_FUNCTION_ARGS) {
  Function function;
  initializeJavaVM(trusted);
  function = Function_getFunction(fcinfo);
  return (CALLED_AS_TRIGGER(fcinfo)) ? Function_invokeTrigger(function, fcinfo) // Called as a trigger
    : Function_invoke(function, fcinfo);
}
*/

extern JavaVM *s_javaVM;
extern Datum internalCallHandler(bool trusted, PG_FUNCTION_ARGS);

static HeapTuple handleRecordD(jarray result, TupleDesc tupdesc) {
    AttInMetadata *attinmeta = TupleDescGetAttInMetadata(tupdesc);



    int natts = tupdesc->natts;
    Datum *dvalues = (Datum *) palloc(natts * sizeof(Datum));
    bool *nulls = (bool *) palloc(natts * sizeof(bool));
    int i;

    //	SIGINTInterruptCheckProcess(state);
    //	java_rowarray = (*env)->CallObjectMethod(env, state -> java_call, state-> iterateForeignScan);
    //    CHECK_EXCEPTION("%s\ncalling iterateForeignScan","unused");
    if (result == NULL) return 0;

    int colcount = (*env)->GetArrayLength(env, result);
    CHECK_EXCEPTION("%s\ngetting array length", "unused");
	if (colcount != natts) {
        ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), (errmsg("returned array had %d elements, should have been %d",colcount, natts))));
	}

	for (i = 0; i < natts; i++)  {
	  jobject fe;
      dvalues[i] = (Datum)0;
      nulls[i] = true;
      if (tupdesc->attrs[i]->attisdropped) continue;
      fe = (*env)->GetObjectArrayElement(env, result, i);
      CHECK_EXCEPTION("%s\ngetting object array element from iterate","unused");
      if (fe == NULL) continue;
      nulls[i] = false;
      if ( (*env)->IsInstanceOf(env, fe, intClass)) {
          if (tupdesc->attrs[i]->atttypid == INT4OID) {
              dvalues[i] = Int32GetDatum( (*env)->CallIntMethod(env, fe, intValue));
          } else ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_DATA_TYPE), (errmsg("java integer value returned for column not INT4OID"))));
      } else if ( (*env)->IsInstanceOf(env, fe, dblClass) ) {
          if (tupdesc->attrs[i]->atttypid == FLOAT8OID) {
              dvalues[i] = Float8GetDatum( (*env)->CallDoubleMethod(env, fe, doubleValue));
          } else ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_DATA_TYPE), (errmsg("java double value returned for column not FLOAT8OID"))));
      } else if ( (*env)->IsInstanceOf(env, fe, floatClass) ) {
          if (tupdesc->attrs[i]->atttypid == FLOAT4OID) {
              dvalues[i] = Float4GetDatum( (*env)->CallFloatMethod(env, fe, floatValue));
          } else ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_DATA_TYPE), (errmsg("java float value returned for column not FLOAT4OID"))));
      } else if ( (*env)->IsInstanceOf(env, fe, shortClass) ) {
          if (tupdesc->attrs[i]->atttypid == INT2OID) {
              dvalues[i] = Int16GetDatum( (*env)->CallShortMethod(env, fe, shortValue));
          } else ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_DATA_TYPE), (errmsg("java short value returned for column not INT2OID"))));
      } else if ( (*env)->IsInstanceOf(env, fe, longClass) ) {
          if (tupdesc->attrs[i]->atttypid == INT8OID) {
              dvalues[i] = Int64GetDatum( (*env)->CallLongMethod(env, fe, shortValue));
          } else ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_DATA_TYPE), (errmsg("java long value returned for column not INT8OID"))));
      } else if ( (*env)->IsInstanceOf(env, fe,  stringClass)) {
          char *js = (char *)(*env)->GetStringUTFChars(env, fe, NULL);
          dvalues[i] = InputFunctionCall(&attinmeta->attinfuncs[i], js, attinmeta->attioparams[i], attinmeta->atttypmods[i]);
      } else {
          ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_DATA_TYPE), (errmsg("unknown conversion for tuple"))));
      }

    }
    HeapTuple tuple = heap_form_tuple(tupdesc, dvalues, nulls);
    pfree(nulls);

//        	pfree(dvalues);   ??

    //	ExecStoreTuple(tuple, slot, InvalidBuffer, false);
    return tuple;
}

static Datum handleRecord(jarray result, PG_FUNCTION_ARGS) {
    TupleDesc tupdesc;
        
        /// should I check that   rsinfo->returnMode == SFRM_ValuePerCall
        // should I test the "first call"  SRF_IS_FIRSTCALL()   -->  fcinfo->flinfo->fn_extra == NULL
        // caller with procStruct->proretset== true     means multi-call
        
    ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
    TypeFuncClass z = get_call_result_type(fcinfo, NULL, &tupdesc);
	switch(z) {
        case TYPEFUNC_OTHER: break; // is this for trigger?
        case TYPEFUNC_COMPOSITE: break;
        case TYPEFUNC_RECORD:
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("looking for a TYPEFUNC_COMPOSITE")));
            break;
        default:
            ereport(ERROR, (errmsg("unknown record type")));
            break;
	}
	
    HeapTuple ht = handleRecordD(result, tupdesc);
    PG_RETURN_DATUM (HeapTupleGetDatum(ht));
}
    

static jobject callJavaMethod(jobject fn, HeapTuple procTup, PG_FUNCTION_ARGS) {
  Oid *types;
  char **names, *modes;

  // look at how this is done in plpy_procedure.c  (plpython)
  int total = get_func_arg_info(procTup, &types, &names, &modes);
  char **onames = names;
  if (onames == NULL)  names = palloc( total * sizeof(char *));
  
  jarray paramNames = (*env)->NewObjectArray(env, total, stringClass, NULL);
  char buf[100];
  for(int i=0;i<total;i++) {
      if (onames == NULL || names[i] == NULL) {
          sprintf(buf, "$%d", i);
          names[i]=buf;
      }
    (*env)->SetObjectArrayElement(env, paramNames, i, makeJavaString(names[i]));
  }

  bool isNull = false;
  char *cname;
  if (onames == NULL) pfree(names);
  
  Datum tmp = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_prosrc, &isNull);
  if(isNull) { ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("'AS' clause of Java function cannot be NULL"))); }
  jobject name = (*env)->NewStringUTF(env, cname = DatumGetCString(DirectFunctionCall1(textout, tmp)));

  Form_pg_proc procStruct = (Form_pg_proc)GETSTRUCT(procTup);
  jint top = procStruct->pronargs;
  int rettype = procStruct->prorettype;
  Oid *paramOids;
  
  jarray argt = (*env)->NewIntArray(env, top);
  if(top > 0) {
    int idx;
    jint *at; 
    paramOids = procStruct->proargtypes.values;
    at = (*env)->GetIntArrayElements(env, argt, NULL);
    for(idx = 0; idx < top; ++idx) at[idx] = paramOids[idx];
    (*env)->ReleaseIntArrayElements(env,argt,at,0);
  }

  jarray args  = (*env)->NewObjectArray(env, top, objClass, NULL); // 
  if(top > 0) {
    int idx;
    jobject value;
    for(idx = 0; idx < top; ++idx) {
      if(PG_ARGISNULL(idx)) continue;
      value = datumToObject(paramOids[idx], PG_GETARG_DATUM(idx));
      (*env)->SetObjectArrayElement(env, args, idx, value); 
    }                
  }

  jobject result = (*env)->CallObjectMethod(env, fn, functionInvoke, name, paramNames, argt, rettype, args);
  check_exception("%s\ncalling method %s", cname);

  // return (CALLED_AS_TRIGGER(fcinfo)) ? Function_invokeTrigger(function, fcinfo) : Function_invoke(function, fcinfo);
  return result;
}

static Datum startSRF(jobject fn, HeapTuple procTup, PG_FUNCTION_ARGS) {
	FuncCallContext *context = SRF_FIRSTCALL_INIT();
    jobject result = callJavaMethod(fn, procTup, fcinfo);
	if(result == NULL) {
	    // Invocation_assertDisconnect();
		fcinfo->isnull = true;
		SRF_RETURN_DONE(context);
	}
    context->user_fctx = (*env)->NewGlobalRef(env, result);
//	RegisterExprContextCallback(((ReturnSetInfo*)fcinfo->resultinfo)->econtext, _endOfSetCB, PointerGetDatum(NULL));
    return 0;
}

static Datum doSRF(Oid rettype, PG_FUNCTION_ARGS) {
    FuncCallContext *context = SRF_PERCALL_SETUP();
    jobject resultIter = (jobject)context->user_fctx;
    jboolean hn = (*env)->CallBooleanMethod(env, resultIter, hasNext);
    check_exception("%s\nchecking for next row", "unused");
    if (!hn) {
        (*env)->DeleteGlobalRef(env, resultIter);
        SRF_RETURN_DONE(context);
    }
    jobject result = (*env)->CallObjectMethod(env, resultIter, next);
    check_exception("%s\ngetting next row", "unused");
    
    Datum row;
    if (rettype == RECORDOID) { row = handleRecord(result, fcinfo); }
    else row = objectToDatum(rettype, result);
    SRF_RETURN_NEXT(context, row);
}


Datum internalCallHandler(bool trusted, PG_FUNCTION_ARGS) {
    HeapTuple procTup;
    Form_pg_proc procStruct;
    Oid foid;
    jboolean multicall;
    
	if(s_javaVM == NULL) initializeJVM(trusted);


    foid = fcinfo->flinfo->fn_oid;
    jobject fn = (*env)->CallStaticObjectMethod(env, functionClass, functionConstructor, foid );

	procTup = SearchSysCache(PROCOID, ObjectIdGetDatum(foid), 0, 0, 0);
	if(!HeapTupleIsValid(procTup))
		ereport(ERROR, (errmsg("cache lookup failed for function (PROCOID) %u", foid)));

    procStruct = (Form_pg_proc)GETSTRUCT(procTup);
    multicall = procStruct->proretset;
    
    // rettype = get_fn_expr_rettype(fcinfo->flinfo);
    Oid rettype = procStruct->prorettype;
    ReleaseSysCache(procTup);
    
    
    if (CALLED_AS_TRIGGER(fcinfo)) {
        jbooleanArray triggerBits = (*env)->NewBooleanArray(env, 9);
        TriggerData* trigd = (TriggerData *) fcinfo->context;
        jboolean *temp = (jboolean *)(*env)->GetBooleanArrayElements(env, triggerBits, NULL);
        temp[0] = (jboolean)TRIGGER_FIRED_AFTER(trigd->tg_event);
        temp[1] = (jboolean)TRIGGER_FIRED_BEFORE(trigd->tg_event);
        temp[2] = (jboolean)TRIGGER_FIRED_INSTEAD(trigd->tg_event);
        temp[3] = (jboolean)TRIGGER_FIRED_FOR_ROW(trigd->tg_event);
        temp[4] = (jboolean)TRIGGER_FIRED_FOR_STATEMENT(trigd->tg_event);
        temp[5] = (jboolean)TRIGGER_FIRED_BY_INSERT(trigd->tg_event);    
        temp[6] = (jboolean)TRIGGER_FIRED_BY_DELETE(trigd->tg_event);
        temp[7] = (jboolean)TRIGGER_FIRED_BY_UPDATE(trigd->tg_event);
        temp[8] = (jboolean)TRIGGER_FIRED_BY_TRUNCATE(trigd->tg_event);
        (*env)->ReleaseBooleanArrayElements(env, triggerBits, temp, 0);

        // triggerBits is a boolean[] of bits
        
        TupleDesc td = trigd->tg_relation->rd_att;
        jobject newt = tupleToObject(td, trigd->tg_newtuple);
        jobject oldt = tupleToObject(td, trigd->tg_trigtuple);
        
        int n = trigd->tg_trigger->tgnargs;  // this should be zero
        
        bool isNull = false;
        char *cname;
        
        Datum tmp = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_prosrc, &isNull);
        if(isNull) { ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("'AS' clause of Java function cannot be NULL"))); }
        jobject name = (*env)->NewStringUTF(env, cname = DatumGetCString(DirectFunctionCall1(textout, tmp)));

        jstring tgname = makeJavaString(trigd->tg_trigger->tgname);

        jobject result = (*env)->CallObjectMethod(env, fn, triggerInvoke, name, tgname, triggerBits, oldt, newt);
        check_exception("%s\ncalling trigger", "unused");

        (*env)->DeleteLocalRef(env, name);
        
        return heap_copytuple(handleRecordD(result, td));
    }
    
    jobject result;
    if (multicall) {
        if (SRF_IS_FIRSTCALL()) {
            startSRF(fn, procTup, fcinfo);
        }
        return doSRF(rettype, fcinfo);
    }

    else {
        result = callJavaMethod(fn, procTup, fcinfo);
	    if (rettype == RECORDOID) { return handleRecord(result, fcinfo); }
        return objectToDatum(rettype, result);
    }
}

// Now the boilerplate for Stored Procedures
extern Datum javau_call_handler(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(javau_call_handler);

// This is the entry point for all untrusted calls.
Datum javau_call_handler(PG_FUNCTION_ARGS) {
	return internalCallHandler(false, fcinfo);
}

extern Datum javax_call_handler(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(javax_call_handler);

// This is the entry point for all trusted calls.
Datum javax_call_handler(PG_FUNCTION_ARGS) {
	return internalCallHandler(true, fcinfo);
}

// Set up interrupt handler
bool InterruptFlag = false;   // Used for checking for SIGINT interrupt 
bool TerminationFlag = false;
