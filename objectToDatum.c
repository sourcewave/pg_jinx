#include <pg_jinx.h>
#include <access/tuptoaster.h>

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


    case BYTEAOID: {
        bytea *bytes = NULL;
        jsize length = (*env)->GetArrayLength(env, javaObject);
        int byteaSize = length+VARHDRSZ;
        bytes = (bytea *)palloc(byteaSize);
        SET_VARSIZE(bytes, byteaSize);
        (*env)->GetByteArrayRegion(env, javaObject, 0, length, (jbyte *)VARDATA(bytes));
        return PointerGetDatum(bytes);
    }
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
    	if ((*env)->GetObjectClass(env, javaObject) != stringClass) {
            javaObject = (*env)->CallObjectMethod(env, javaObject, toString);
            check_exception("%s\nconverting to a String","unused");
    	}
    	
        utf8 = (*env)->GetStringUTFChars(env, javaObject, 0);
		str = (char*)pg_do_encoding_conversion( (unsigned char*)utf8, strlen(utf8), PG_UTF8, GetDatabaseEncoding());
        ret = DirectFunctionCall1( textin, CStringGetDatum(str));
        (*env)->ReleaseStringUTFChars(env, javaObject, utf8);
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

int objectArrayToDatumArray(jarray jary, Oid**oids, Datum**dats, char **nulls) {
    int parmcount = jary == NULL ? 0 : (*env)->GetArrayLength(env, jary);
    CHECK_EXCEPTION("%s\ngetting array length", "unused");

    *dats = (Datum *) palloc(parmcount * sizeof(Datum));
    *nulls = (char *) palloc(parmcount * sizeof(char));
    *oids = (Oid *) palloc(parmcount * sizeof(Oid));

    // This is very similar to code in pg_jinx (around 120), pg_jinx_fdw, and objectToDatum
    for(int i=0;i<parmcount;i++) {
        jobject fe = (*env)->GetObjectArrayElement(env, jary, i); // this arg
        (*nulls)[i]= fe == NULL;
        Oid *toid = &(*oids)[i];
        if ((*nulls)[i]) continue;
        
        if ( (*env)->IsInstanceOf(env, fe, intClass)) *toid=INT4OID;
        else if ( (*env)->IsInstanceOf(env, fe, dblClass) ) *toid=FLOAT8OID;
        else if ( (*env)->IsInstanceOf(env, fe, floatClass) ) *toid=FLOAT4OID;
        else if ( (*env)->IsInstanceOf(env, fe, shortClass) ) *toid=INT2OID;
        else if ( (*env)->IsInstanceOf(env, fe, longClass) ) *toid=INT8OID;
        else if ( (*env)->IsInstanceOf(env, fe, stringClass)) *toid=TEXTOID; // could be VARCHAROID -- but they are all the same
        else if ( (*env)->IsInstanceOf(env, fe, booleanClass)) *toid=BOOLOID;
        else if ( (*env)->IsInstanceOf(env, fe, bigDecimalClass)) *toid=NUMERICOID;
        else if ( (*env)->IsInstanceOf(env, fe, dateClass)) *toid=TIMESTAMPOID;
        else if ( (*env)->IsInstanceOf(env, fe, byteArrayClass)) *toid=BYTEAOID;
        else {
            ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_DATA_TYPE), (errmsg("unknown conversion for tuple"))));
        }
        (*dats)[i] = objectToDatum(*toid, fe);
    }
    return parmcount;
}



