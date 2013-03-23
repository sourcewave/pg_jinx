#include <pg_jinx.h>

#define otdArray(typ, oid, Typ) { \
    ArrayType* v; \
	jsize nElems; \
	if (javaObject == 0) return 0; \
	if (! (*env)->IsInstanceOf(env, javaObject, typ##ArrayClass) ) { \
        ereport(ERROR, (errmsg("expecting result of class ##typ##[]"))); \
	} \
    nElems = (*env)->GetArrayLength(env, javaObject); \
	v = createArrayType(nElems, sizeof(j##typ), oid, false); \
	(*env)->Get##Typ##ArrayRegion(env, (j##typ##Array)javaObject, 0, nElems, (j##typ *)ARR_DATA_PTR(v)); \
	PG_RETURN_ARRAYTYPE_P(v); \
}

static ArrayType* createArrayType(jsize nElems, size_t elemSize, Oid elemType, bool withNulls) {
	ArrayType* v;
	int nBytes = elemSize * nElems;
	
	// TODO: why the context switch?
	// MemoryContext currCtx = Invocation_switchToUpperContext();

	int dataoffset;
	if(withNulls) {
		dataoffset = ARR_OVERHEAD_WITHNULLS(1, nElems);
		nBytes += dataoffset;
	} else {
		dataoffset = 0;			/* marker for no null bitmap */
		nBytes += ARR_OVERHEAD_NONULLS(1);
	}
	v = (ArrayType*)palloc0(nBytes);
	v->dataoffset = dataoffset;
	// MemoryContextSwitchTo(currCtx);

	SET_VARSIZE(v, nBytes);
	ARR_NDIM(v) = 1;
	ARR_ELEMTYPE(v) = elemType;
	*((int*)ARR_DIMS(v)) = nElems;
	*((int*)ARR_LBOUND(v)) = 1;
	return v;
}

Datum objectToDatum(Oid paramtype, jobject javaObject) {
    Datum ret;
    switch(paramtype) {
        case 1000: otdArray(boolean, BOOLOID, Boolean); break;
        case 1005: otdArray(short, INT2OID, Short); break;
        case 1007: otdArray(int, INT4OID, Int); break;
        case 1016: otdArray(long, INT8OID, Long); break;
        case 1021: otdArray(float, FLOAT4OID, Float); break;
        case 1022: otdArray(double, FLOAT8OID, Double); break;
        case 1001: otdArray(byte, CHAROID, Byte); break;

    case BOOLOID: {
        jboolean jb = (*env)->CallBooleanMethod(env, javaObject, booleanValue);
        return BoolGetDatum(jb);
    }
    case INT4OID: {
        jint ji = (*env)->CallIntMethod(env, javaObject, intValue);
        return Int32GetDatum(ji);
    }
    case FLOAT8OID: {
        jdouble jd = (*env)->CallDoubleMethod(env, javaObject, doubleValue);
        return Float8GetDatum(jd);
    }
    case INT2OID: {
        jshort js = (*env)->CallShortMethod(env, javaObject, shortValue);
        return Int16GetDatum(js);
    }
    case FLOAT4OID: {
        jfloat jf = (*env)->CallFloatMethod(env, javaObject, floatValue);
        return Float4GetDatum(jf);
    }
    case TEXTOID: 
    case VARCHAROID: {
        const char *utf8;
        char *str;
    	if(javaObject == NULL) return 0;
        utf8 = (*env)->GetStringUTFChars(env, javaObject, 0);
		str = (char*)pg_do_encoding_conversion( (unsigned char*)utf8, strlen(utf8), PG_UTF8, GetDatabaseEncoding());
        ret = DirectFunctionCall1( textin, CStringGetDatum(str));
        (*env)->ReleaseStringUTFChars(env, javaObject, utf8);
    	(*env)->DeleteLocalRef(env,javaObject);
    	if (utf8 != str) pfree(str);
    	return ret;
    }
    case NUMERICOID: {
/*        value = InputFunctionCall(cinfo->attinfunc,
								  buffer->data,
								  cinfo->attioparam,
								  cinfo->atttypmod);
								  */
      	char* tmp;
    	jstring jstr = (jstring)(*env)->CallObjectMethod(env, javaObject, toString);
        check_exception("%s\nconverting a BigDecimal to a string","unused");

    	tmp = fromJavaString(jstr);
    	(*env)->DeleteLocalRef(env, jstr);
        ret = DirectFunctionCall1(numeric_in, CStringGetDatum(tmp) );
    	pfree(tmp);
    	return ret;
    }
    default: ereport(ERROR, (errmsg("unable to convert java object to Datum")));
    }
    return 0;
}
