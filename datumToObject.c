#include <pg_jinx.h>

#define dtoArray(typ,Typ) { \
  ArrayType* v        = DatumGetArrayTypeP(d); \
  jsize      nElems   = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v)); \
  j##typ##Array result  = (*env)->New##Typ##Array(env, nElems); \
  if(ARR_HASNULL(v)) { \
    jsize idx; \
    jboolean isCopy = JNI_FALSE; \
    bits8* nullBitMap = ARR_NULLBITMAP(v); \
    j##typ * values = (j##typ *)ARR_DATA_PTR(v); \
    j##typ * elems  = (*env)->Get##Typ##ArrayElements(env, result, &isCopy); \
    for(idx = 0; idx < nElems; ++idx) { \
        if(arrayIsNull(nullBitMap, idx)) elems[idx] = 0; \
        else elems[idx] = *values++; \
    } \
    (*env) -> Release##Typ##ArrayElements(env, result, elems, JNI_COMMIT); \
  } \
  else (*env)->Set##Typ##ArrayRegion(env, result, 0, nElems, (j##typ *)ARR_DATA_PTR(v)); \
  return result; \
}

static bool arrayIsNull(const bits8* bitmap, int offset) {
  return bitmap == 0 ? false : !(bitmap[offset / 8] & (1 << (offset % 8)));
}


jobject tupleToObject(TupleDesc tupdesc, HeapTuple tuple) {
  // AttInMetadata *attinmeta = TupleDescGetAttInMetadata(tupdesc);
  
  if (tuple == NULL) return NULL;
  jarray row = (*env)->NewObjectArray(env, tupdesc->natts, objClass, NULL);
  CHECK_EXCEPTION("%s\nallocating row for tuple", "unused");

  for (int j = 1; j <= tupdesc->natts; j++) {
    (*env)->SetObjectArrayElement(env, row, j-1, makeJavaString( SPI_getvalue(tuple, tupdesc, j) ));
    CHECK_EXCEPTION("%s\ncreating field %d for a row", j);
  }
  return row;
}

jobject datumToObject(Oid paramtype, Datum d) {
    jobject result;
    
    switch(paramtype) {
        case 1000: dtoArray(boolean,Boolean); break;
        case 1005: dtoArray(short,Short); break;
        case 1007: dtoArray(int,Int); break;
        case 1016: dtoArray(long,Long); break;
        case 1021: dtoArray(float,Float); break;
        case 1022: dtoArray(double,Double); break;
        case 1001: dtoArray(byte,Byte); break;
        
        
/*        case 1001: 
        { 
          ArrayType* v        = DatumGetArrayTypeP(d); 
          jsize      nElems   = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v)); 
          jintArray result  = (*env)->NewIntArray(env, nElems); 
          if(ARR_HASNULL(v)) { 
            jsize idx; 
            jboolean isCopy = JNI_FALSE; 
            bits8* nullBitMap = ARR_NULLBITMAP(v); 
            jint * values = (jint *)ARR_DATA_PTR(v); 
            jint * elems  = (*env)->GetIntArrayElements(env, result, &isCopy); 
            for(idx = 0; idx < nElems; ++idx) { 
                if(arrayIsNull(nullBitMap, idx)) elems[idx] = 0; 
                else elems[idx] = *values++; 
            } 
            (*env) -> ReleaseIntArrayElements(env, result, elems, JNI_COMMIT); 
          } 
          else (*env)->SetIntArrayRegion(env, result, 0, nElems, (jint *)ARR_DATA_PTR(v)); 
          return result; 
        }
        */
        
    /*
    1001: _bytea
    1002: _char
    1003: _name
    1005: _int2
    1006: _int2vector
    1007: _int4 (INT4ARRAYOID)
    1008: _regproc
    1009: _text
    1010: _tid
    1011: _xid
    1012: _cid
    1013: _oidvector
    1014: _bpchar
    1015: _varchar
    1016: _int8
    1017: _point
    1018: _lseg
    1019: _path
    1020: _box
    
    1021: _float4
    1022: _float8
    1023: _abstime
    1024: _reltime
    1025: _tinterval
    1026:
    1027: _polygon
    1028: _oid
    */
    
    
    case BOOLOID: return (*env)->NewObject(env, booleanClass, boolConstructor, DatumGetBool(d)); 
    case INT4OID: return (*env)->NewObject(env, intClass, intConstructor, DatumGetInt32(d));
    case FLOAT8OID: return (*env)->NewObject(env, dblClass, dblConstructor, DatumGetFloat8(d));
    case INT2OID: return (*env)->NewObject(env, shortClass, shortConstructor, DatumGetInt16(d));
    case FLOAT4OID: return (*env)->NewObject(env, floatClass, floatConstructor, DatumGetFloat4(d));
    case TEXTOID: 
    case VARCHAROID: {
        char *str = DatumGetCString(DirectFunctionCall1(textout, d));
        char* utf8 = (char*)pg_do_encoding_conversion((unsigned char*)str, strlen(str), GetDatabaseEncoding(), PG_UTF8);
        result = (*env)->NewStringUTF(env, utf8);
        if(utf8 != str) pfree(utf8);
        pfree(str);
        return result;
        }
    case NUMERICOID: {
        jobject result;
//        char* tmp = DatumGetCString(DirectFunctionCall1(textout, d));
//        char* tmp = DatumGetCString(DirectFunctionCall3(textout, d, ObjectIdGetDatum(NUMERICOID), Int32GetDatum(-1)));
        ssize_t numvalue = (ssize_t) DatumGetNumeric(d);
        char *tmp = (char *)DirectFunctionCall1(numeric_out, numvalue);
        jstring arg = (*env)->NewStringUTF(env, tmp);
    	pfree(tmp);
    	result = (*env)-> NewObject(env, bigDecimalClass, bigDecimalConstructor, arg);
        check_exception("%s\ntrying to convert datum to BigDecimal", "unused");
        return result;
        
        
    	
    }
    
    default: ereport(ERROR, (errmsg("unable to convert Datum to java object")));
    }
    return NULL;
}
