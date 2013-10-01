
#include <pg_jinx.h>

static jclass bridgeClass;
static jmethodID bridgeInit;

extern void bridge_initialize(void);

static void throwJavaException(const char* funcName) {
  jobject ex;
  PG_TRY(); {
    jarray jstrs;
    jintArray jints;
    ErrorData* errorData = CopyErrorData();
    int eints[] = {0,0,0,0,0,0,0,0,0};
        
    jints = (*env)->NewIntArray(env, 9);
    eints[0]=errorData->elevel;
    eints[1]=errorData->saved_errno;
    eints[2]=errorData->sqlerrcode;
    eints[3]=errorData->lineno;
    eints[4]=errorData->cursorpos;
    eints[5]=errorData->internalpos;
    eints[6]=errorData->output_to_server;
    eints[7]=errorData->output_to_client;
    eints[8]=errorData->show_funcname;
        
    (*env)->SetIntArrayRegion(env, jints, 0, 9, eints);

    jstrs = (*env)->NewObjectArray(env, 8, stringClass, NULL);
    (*env)->SetObjectArrayElement(env, jstrs, 0, makeJavaString(errorData->message) );
    (*env)->SetObjectArrayElement(env, jstrs, 1, makeJavaString(errorData->filename));
    (*env)->SetObjectArrayElement(env, jstrs, 2, makeJavaString(errorData->funcname));
    (*env)->SetObjectArrayElement(env, jstrs, 3, makeJavaString(errorData->detail));
    (*env)->SetObjectArrayElement(env, jstrs, 4, makeJavaString(errorData->hint));
    (*env)->SetObjectArrayElement(env, jstrs, 5, makeJavaString(errorData->context));
    (*env)->SetObjectArrayElement(env, jstrs, 6, makeJavaString(errorData->internalquery));
    (*env)->SetObjectArrayElement(env, jstrs, 7, makeJavaString(funcName));

    ex = (*env)->NewObject(env,serverExceptionClass, serverExceptionConstructor, jints, jstrs );
    elog(DEBUG1, "Exception in function %s", funcName);

    FlushErrorState();
    FreeErrorData(errorData);
        
    (*env)->Throw(env,ex);
  }
  PG_CATCH(); { elog(WARNING, "Exception while generating exception"); }
  PG_END_TRY();
}

static void Exception_throw(int errCode, const char* errMessage, ...) {
  char buf[1024];
  va_list args;
  jstring message;
  jstring sqlState;
  jobject ex;
  int idx;

  va_start(args, errMessage);
  vsnprintf(buf, sizeof(buf), errMessage, args);
  ereport(DEBUG3, (errcode(errCode), errmsg("%s", buf)));

  PG_TRY(); {
    message = makeJavaString(buf);

    // unpack MAKE_SQLSTATE code
    for (idx = 0; idx < 5; ++idx) {
      buf[idx] = PGUNSIXBIT(errCode);
      errCode >>= 6;
    }
    buf[idx] = 0;
    sqlState = makeJavaString(buf);

    ex = (*env)->NewObject(env, sqlException, sqlExceptionConstructor, message, sqlState);
    (*env)->DeleteLocalRef(env,message);
    (*env)->DeleteLocalRef(env,sqlState);
    (*env)->Throw(env,ex);
  }
  PG_CATCH(); { ereport(WARNING, (errcode(errCode), errmsg("Exception while generating exception: %s", buf))); }
  PG_END_TRY();
  va_end(args);
}

static void Exception_throwSPI(const char* function, int errCode) {
	Exception_throw(ERRCODE_INTERNAL_ERROR,
		"SPI function SPI_%s failed with error %s", function,
			SPI_result_code_string(errCode));
}

static JNIEXPORT void JNICALL pglog(JNIEnv* env, jclass cls, jint logLevel, jstring jstr) {
	char* str = fromJavaString(jstr);
	if(str != 0) {
		// elog uses printf formatting but the logger does not so we must escape all '%' in the string.
		char c;
		const char* cp;
		int percentCount = 0;
		for(cp = str; (c = *cp) != 0; ++cp) {
			if(c == '%') ++percentCount;
		}
	
		if(percentCount > 0) {
			// Make room to expand all "%" to "%%"
			char* str2 = palloc((cp - str) + percentCount + 1);
			char* cp2 = str2;
	
			/* Expand... */
			for(cp = str; (c = *cp) != 0; ++cp) {
				if(c == '%') *cp2++ = c;
				*cp2++ = c;
			}
			*cp2 = 0;
			pfree(str);
			str = str2;
		}
	
		PG_TRY(); { elog(logLevel, "%s", str); pfree(str); }
		PG_CATCH(); { throwJavaException("ereport"); }
		PG_END_TRY();
	}
}

static JNIEXPORT jlong JNICALL _modifyTuple(JNIEnv* env, jclass clazz, jlong _this, jlong _tuple, jintArray _indexes, jobjectArray _values) {
	Relation self = (Relation)(_this);
	jlong result = 0;
	if(self != 0 && _tuple != 0) {
        HeapTuple tuple = (HeapTuple)_tuple;
		PG_TRY(); {
			jint idx;
			TupleDesc tupleDesc = self->rd_att;

			jint   count  = (*env)->GetArrayLength(env, _indexes);
			Datum* values = (Datum*)palloc(count * sizeof(Datum));
			char*  nulls  = 0;
		
			jint* javaIdxs = (*env)->GetIntArrayElements(env, _indexes, 0);
		
			int* indexes;
			if(sizeof(int) == sizeof(jint))	/* compiler will optimize this */
				indexes = (int*)javaIdxs;
			else indexes = (int*)palloc(count * sizeof(int));

			for(idx = 0; idx < count; ++idx) {
				int attIndex;
				Oid typeId;
				jobject value;
	
				if(sizeof(int) == sizeof(jint))	/* compiler will optimize this */
					attIndex = indexes[idx];
				else {
					attIndex = (int)javaIdxs[idx];
					indexes[idx] = attIndex;
				}
		
				typeId = SPI_gettypeid(tupleDesc, attIndex);
				if(!OidIsValid(typeId)) {
					Exception_throw(ERRCODE_INVALID_DESCRIPTOR_INDEX,
						"Invalid attribute index \"%d\"", attIndex);
					return 0L;	/* Exception */
				}
		
				value = (*env)->GetObjectArrayElement(env,_values, idx);
				if(value != 0) values[idx] = objectToDatum(typeId, value);
				else {
					if(nulls == 0) {
						nulls = (char*)palloc(count+1);
						memset(nulls, ' ', count);	/* all values non-null initially */
						nulls[count] = 0;
					}
					nulls[idx] = 'n';
					values[idx] = 0;
				}
			}
	
			tuple = SPI_modifytuple(self, tuple, count, indexes, values, nulls);
			if(tuple == 0)
				Exception_throwSPI("modifytuple", SPI_result);
	
			(*env)->ReleaseIntArrayElements(env, _indexes, javaIdxs, JNI_ABORT);
		
			if(sizeof(int) != sizeof(jint))	/* compiler will optimize this */
				pfree(indexes);
		
			pfree(values);
			if(nulls != 0)
				pfree(nulls);	
		}
		PG_CATCH();
		{
			tuple = 0;
			throwJavaException("SPI_gettypeid");
		}
		PG_END_TRY();
		if(tuple != 0)
			result = (jlong)tuple;
	}
	return result;
}

static JNIEXPORT jarray JNICALL getUser(JNIEnv *env, jclass clazz) {
	jarray result = (*env)->NewObjectArray(env, 2, stringClass, NULL);
    CHECK_EXCEPTION("%s\nallocating string array","unused");
	char * u = CStringGetDatum(GetUserNameFromId(GetUserId()));
	(*env)->SetObjectArrayElement(env, result, 0, makeJavaString(u));
    CHECK_EXCEPTION("%s\nstoring user", NULL);
	u = CStringGetDatum(GetUserNameFromId(GetSessionUserId()));
	(*env)->SetObjectArrayElement(env, result, 1, makeJavaString(u));
    CHECK_EXCEPTION("%s\nstoring current user",NULL );
	return result;
}


static JNIEXPORT jint JNICALL exec(JNIEnv* env, jclass cls, jlong threadId, jstring cmd, jint count) {
	jint result = 0;

	char* command = fromJavaString(cmd);
	if(command != 0) {
		PG_TRY();
		{
		    // TODO: can I make sure I'm connected?
		// Invocation_assertConnect();
			result = (jint)SPI_exec(command, (int)count);
			if(result < 0)
				Exception_throwSPI("exec", result);
	
		}
		PG_CATCH();
		{
			throwJavaException("SPI_exec");
		}
		PG_END_TRY();
		pfree(command);
	}
	return result;
}


static JNIEXPORT jobject JNICALL formTuple(JNIEnv* env, jclass cls, jlong _this, jobjectArray jvalues) {
	jobject result = 0;
	PG_TRY(); {
		jint   idx;
		HeapTuple tuple;
		MemoryContext curr;
		TupleDesc self = (TupleDesc)_this;
		int    count   = self->natts;
		Datum* values  = (Datum*)palloc(count * sizeof(Datum));
		char*  nulls   = palloc(count);
		
		// TODO: this is totally broken

		memset(values, 0,  count * sizeof(Datum));
		memset(nulls, 'n', count);	/* all values null initially */
	
		for(idx = 0; idx < count; ++idx) {
			jobject value = (*env)->GetObjectArrayElement(env,jvalues, idx);
			if(value != 0) {
				values[idx] = objectToDatum(SPI_gettypeid(self, idx+1), value);
				nulls[idx] = ' ';
			}
		}

		tuple = heap_formtuple(self, values, nulls);
		// FIXME: 
//		result = Tuple_internalCreate(tuple, false);
		pfree(values);
		pfree(nulls);
	}
	PG_CATCH(); { throwJavaException("heap_formtuple"); }
	PG_END_TRY();
	return result;
}

// ExecutionPlan

#define ERRCODE_PARAMETER_COUNT_MISMATCH	MAKE_SQLSTATE('0','7', '0','0','1')

static bool coerceObjects(void* ePlan, jobjectArray jvalues, Datum** valuesPtr, char** nullsPtr) {
	char*  nulls = 0;
	Datum* values = 0;

	int count = SPI_getargcount(ePlan);
	if((jvalues == 0 && count != 0)
	|| (jvalues != 0 && count != (*env)->GetArrayLength(env,jvalues)))
	{
		Exception_throw(ERRCODE_PARAMETER_COUNT_MISMATCH,
			"Number of values does not match number of arguments for prepared plan");
		return false;
	}

	if(count > 0)
	{
		int idx;
		values = (Datum*)palloc(count * sizeof(Datum));
		for(idx = 0; idx < count; ++idx)
		{
			Oid typeId = SPI_getargtypeid(ePlan, idx);
			jobject value = (*env)->GetObjectArrayElement(env,jvalues, idx);
			if(value != 0) {
				values[idx] = objectToDatum(typeId, value);
				(*env)->DeleteLocalRef(env,value);
			}
			else
			{
				values[idx] = 0;
				if(nulls == 0)
				{
					nulls = (char*)palloc(count+1);
					memset(nulls, ' ', count);	/* all values non-null initially */
					nulls[count] = 0;
					*nullsPtr = nulls;
				}
				nulls[idx] = 'n';
			}
		}
	}
	*valuesPtr = values;
	*nullsPtr = nulls;
	return true;
}


static JNIEXPORT jlong JNICALL _cursorOpen(JNIEnv* env, jclass clazz, jlong _this, jlong threadId, jstring cursorName, jobjectArray jvalues) {
	jlong jportal = 0;
	if(_this != 0) {
		PG_TRY(); {
			Datum*  values  = 0;
			char*   nulls   = 0;
			if(coerceObjects( (void *)_this, jvalues, &values, &nulls)) {
				Portal portal;
				char* name = 0;
				if(cursorName != 0) name = fromJavaString(cursorName);

                // TODO:  make sure the SPI is open
                // false -- was currentReadOnly 
				// Invocation_assertConnect();
				portal = SPI_cursor_open( name, (void *)_this, values, nulls, false);
				if(name != 0) pfree(name);
				if(values != 0) pfree(values);
				if(nulls != 0) pfree(nulls);
				jportal = (jlong)portal;
			}
		}
		PG_CATCH(); { throwJavaException("SPI_cursor_open"); }
		PG_END_TRY();
	}
	return jportal;
}


// 			result = (jboolean)SPI_is_cursor_plan( (void *)_this );

//				result = (jint)SPI_execute_plan( (void *)_this, values, nulls, false , (int)count);

static JNIEXPORT jlong JNICALL _prepare(JNIEnv* env, jclass clazz, jlong threadId, jstring jcmd, jlongArray paramTypes) {
	jlong result = 0;
	PG_TRY(); {
		char* cmd;
		void* ePlan;
		int paramCount = (*env)->GetArrayLength(env,paramTypes);
		jOid* paramOids = (jOid *)(*env)->GetIntArrayElements(env,paramTypes, false);

		cmd   = fromJavaString(jcmd);
		// Invocation_assertConnect();
		ePlan = SPI_prepare(cmd, paramCount, (Oid *)paramOids);
		pfree(cmd);

		if(ePlan == 0) Exception_throwSPI("prepare", SPI_result);
		else {
			result = (jlong) SPI_saveplan(ePlan);
			SPI_freeplan(ePlan);	/* Get rid of the original, nobody can see it anymore */
		}
	}
    PG_CATCH(); { throwJavaException("SPI_prepare"); }
	PG_END_TRY();
	return result;
}

static JNIEXPORT jint JNICALL _fetch(JNIEnv* env, jclass clazz, jlong portal, jlong threadId, jboolean forward, jint count) {
	jint result = 0;
	if(portal != 0) {
		PG_TRY(); {
			SPI_cursor_fetch((Portal)portal, forward == JNI_TRUE, (int)count);
			result = (jint)SPI_processed;
		} PG_CATCH(); { throwJavaException("SPI_cursor_fetch"); }
		PG_END_TRY();
	}
	return result;
}

static JNIEXPORT jint JNICALL _move(JNIEnv* env, jclass clazz, jlong portal, jlong threadId, jboolean forward, jint count) {
	jint result = 0;
	if(portal != 0) {
		PG_TRY(); {
			SPI_cursor_move((Portal)portal, forward == JNI_TRUE, (int)count);
			result = (jint)SPI_processed;
		}
		PG_CATCH(); { throwJavaException("SPI_cursor_move"); }
		PG_END_TRY();
	}
	return result;
}

static JNIEXPORT jint JNICALL sp_start(JNIEnv* env, jclass cls) {
    PG_TRY(); { BeginInternalSubTransaction(NULL); }
	PG_CATCH(); { throwJavaException("BeginInternalSubTransaction"); }
	PG_END_TRY();
    return GetCurrentSubTransactionId();
}

static JNIEXPORT void JNICALL sp_commit(JNIEnv* env, jclass clazz) {
    PG_TRY(); { ReleaseCurrentSubTransaction(); }
	PG_CATCH(); { throwJavaException("ReleaseCurrentSubTransaction"); }
    PG_END_TRY();
    SPI_restore_connection();
}

static JNIEXPORT void JNICALL sp_rollback(JNIEnv* env, jclass clazz) {
	PG_TRY(); { RollbackAndReleaseCurrentSubTransaction(); }
	PG_CATCH(); { throwJavaException("RollbackAndReleaseCurrentSubTransaction"); }
	PG_END_TRY();
    SPI_restore_connection();
}

#define MAX_PARMS 20
static JNIEXPORT jarray JNICALL _execute(JNIEnv *env, jclass clazz, jint num, jobject cmd, jarray args) {
 
    char *strcmd = fromJavaString(cmd);
    Oid *argtyps;
    Datum *values;
    char *nulls;
    
    int parmcount = objectArrayToDatumArray(args, &argtyps, &values, &nulls );

    int ret;
    if ((ret = SPI_connect()) < 0) {
      throwJavaException("SPI_connect");
      return NULL;
    }
    PG_TRY(); { ret = SPI_execute_with_args(strcmd, parmcount, argtyps, values, nulls, false, num); }
    PG_CATCH(); { throwJavaException("SPI_execute_args"); }
    PG_END_TRY();
    
    pfree(argtyps);
    pfree(values);
    pfree(nulls);
    pfree(strcmd);
    
    int rows = SPI_processed;

    if (ret == SPI_OK_SELECT || (ret == SPI_OK_UTILITY  && rows > 0 && SPI_tuptable != NULL) );
    else {
      SPI_finish();
      // I should probably contrive to return a number here?
      return (*env)->NewObject(env, intClass, intConstructor, rows);
      
    }

    SPITupleTable *spi_tuptable = SPI_tuptable;
    TupleDesc spi_tupdesc = spi_tuptable->tupdesc;

    AttInMetadata *attinmeta = TupleDescGetAttInMetadata(spi_tupdesc);

    jarray result = (*env)->NewObjectArray(env, rows, objClass, NULL);
    CHECK_EXCEPTION("%s\nallocating resultset array","unused");

    for (int cntr = 0; cntr < rows; cntr++) {
      HeapTuple spi_tuple = spi_tuptable->vals[cntr];
      jobject row = tupleToObject(spi_tupdesc, spi_tuple);
      (*env)->SetObjectArrayElement(env, result, cntr, row);
      CHECK_EXCEPTION("%s\nstoring row %d ", cntr);
    }

    SPI_finish();
    return result;
    
}

/*
static JNIEXPORT jarray JNICALL _execute(JNIEnv *env, jclass clazz, jint num, jobject cmd, jarray args) {
  int ret;
  if ((ret = SPI_connect()) < 0) {
    throwJavaException("SPI_connect");
    return NULL;
  }
// the true/false (read-only) setting and the #-of-rows setting should be parameters
  PG_TRY(); { ret = SPI_execute(fromJavaString(cmd), false, 0); }
  PG_CATCH(); { throwJavaException("SPI_execute"); }
  PG_END_TRY();
  
  int rows = SPI_processed;

  if (ret == SPI_OK_SELECT || (ret == SPI_OK_UTILITY  && rows > 0 && SPI_tuptable != NULL) );
  else {
    SPI_finish();
    return NULL;
  }

  SPITupleTable *spi_tuptable = SPI_tuptable;
  TupleDesc spi_tupdesc = spi_tuptable->tupdesc;

  AttInMetadata *attinmeta = TupleDescGetAttInMetadata(spi_tupdesc);

  jarray result = (*env)->NewObjectArray(env, rows, objClass, NULL);
  CHECK_EXCEPTION("%s\nallocating resultset array","unused");
  
  for (int cntr = 0; cntr < rows; cntr++) {
    HeapTuple spi_tuple = spi_tuptable->vals[cntr];
    jobject row = tupleToObject(spi_tupdesc, spi_tuple);
    (*env)->SetObjectArrayElement(env, result, cntr, row);
    CHECK_EXCEPTION("%s\nstoring row %d ", cntr);
  }

  SPI_finish();
  return result;
}
*/


void bridge_initialize(void) {
    char *cls = "org.sourcewave.jinx.PostgresBridge";
	jint nMethods = 0;
    
	JNINativeMethod methods[] = {
        { "log", "(ILjava/lang/String;)V", pglog },

		{ "getUser", "()[Ljava/lang/String;", getUser },
		
        { "_modifyTuple", "(JJ[I[Ljava/lang/Object;)J", _modifyTuple },

        // TupleDesc
		{ "_formTuple", "(J[Ljava/lang/Object;)J", formTuple },
		
		// ExecutionPlan
	    { "_prepare", "(JLjava/lang/String;[I)J", _prepare },

	    // portal
	    { "_fetch", "(JJZI)I", _fetch },
		{ "_move", "(JJZI)I", _move },
		
		// savepoint
		{ "sp_start", "()I", sp_start },
		{ "sp_commit", "()V", sp_commit },
		{ "sp_rollback", "()V", sp_rollback },

        // execute
        { "_execute", "(ILjava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;", _execute },
        
        { NULL, NULL, NULL }};

    CACHE_CLASS(bridgeClass, cls);
    CACHE_STATIC_METHOD(bridgeInit, bridgeClass, "init", "()V");
    (*env)->CallStaticObjectMethod(env, bridgeClass, bridgeInit);
    CHECK_EXCEPTION("%s\nbridge init","unused");

    for(nMethods = 0; methods[nMethods].name != NULL;  nMethods++);

	if((*env)->RegisterNatives(env, bridgeClass, methods, nMethods) != JNI_OK) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		ereport(ERROR, ( errmsg("Unable to register native methods")));
	}
}
