#include <pg_jinx.h>

#include <utils/typcache.h>

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
      bool isNull;
      Datum dd = SPI_getbinval(tuple, tupdesc, j, &isNull);
      jobject jo = isNull ? NULL : datumToObject( SPI_gettypeid(tupdesc, j), dd);
    (*env)->SetObjectArrayElement(env, row, j-1, jo );
    CHECK_EXCEPTION("%s\ncreating field %d for a row", j);
  }
  return row;
}

static int getTimezone(int64 dt) {
  pg_time_t time = (dt / INT64CONST(1000000) + (POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * 86400);
  struct pg_tm* tx = pg_localtime(&time, session_timezone);
  return -tx->tm_gmtoff;
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
        case RECORDARRAYOID: {
            ArrayType* v        = DatumGetArrayTypeP( PointerGetDatum(d) ); 
            jsize      nElems   = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v)); 
            jobjectArray result  = (*env)->NewObjectArray(env, nElems, objClass, NULL); 

            jsize idx; 
            bits8* nullBitMap = ARR_NULLBITMAP(v); 
            char *values = (char *)ARR_DATA_PTR(v);
            Oid eltyp = ARR_ELEMTYPE(v);
            bool tbv;
            char ta;
            short tl;
            
            get_typlenbyvalalign( eltyp, &tl, &tbv, &ta);

            // toast_flatten_tuple_attribute  ??

            // jobject * elems  = (*env)->Get##Typ##ArrayElements(env, result, &isCopy); 
            for(idx = 0; idx < nElems; ++idx) { 
                Datum value = fetch_att(values, tbv, tl);
                jobject jo = NULL;
                if( ! arrayIsNull(nullBitMap, idx)) {
                    
                    jo = datumToObject(eltyp, value); 
                    values = att_addlength_datum(values, tl, PointerGetDatum(values));
                    values = (char *) att_align_nominal(values, ta);
                }
                (*env)->SetObjectArrayElement(env, result, idx, jo);
            } 
            return result; 
        }
        
        
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
    
    case BYTEAOID: {
        bytea *bytes = DatumGetByteaP(d);
        jsize length = VARSIZE(bytes) - VARHDRSZ;
        jbyteArray ba = (*env)->NewByteArray(env, length);
        (*env)->SetByteArrayRegion(env, ba, 0, length, (jbyte*)VARDATA(bytes));
        return ba;
    }
    case BOOLOID: return (*env)->NewObject(env, booleanClass, boolConstructor, DatumGetBool(d)); 
    case INT4OID: return (*env)->NewObject(env, intClass, intConstructor, DatumGetInt32(d));
    case FLOAT8OID: return (*env)->NewObject(env, dblClass, dblConstructor, DatumGetFloat8(d));
    case INT2OID: return (*env)->NewObject(env, shortClass, shortConstructor, DatumGetInt16(d));
    case FLOAT4OID: return (*env)->NewObject(env, floatClass, floatConstructor, DatumGetFloat4(d));
    case UNKNOWNOID: {
        char *str = DatumGetCString(DirectFunctionCall1(unknownout, d));
        char* utf8 = (char*)pg_do_encoding_conversion((unsigned char*)str, strlen(str), GetDatabaseEncoding(), PG_UTF8);
        result = (*env)->NewStringUTF(env, utf8);
        if(utf8 != str) pfree(utf8);
        pfree(str);
        return result;
        }
    case TEXTOID: 
    case VARCHAROID: {
//        char *str = DatumGetCString(DirectFunctionCall1(textout, d ));
        char *str = TextDatumGetCString(d);
        char* utf8 = (char*)pg_do_encoding_conversion((unsigned char*)str, strlen(str), GetDatabaseEncoding(), PG_UTF8);
        result = (*env)->NewStringUTF(env, utf8);
        if(utf8 != str) pfree(utf8);
        pfree(str);
        return result;
        }

#define EPOCH_DIFF (POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE)

    case DATEOID: {
        DateADT pgDate = DatumGetDateADT(d);
        int tz = getTimezone((int64)pgDate * INT64CONST(86400000000));

        jlong date = (jlong)(pgDate + EPOCH_DIFF);
        date *= 86400L;
        date += tz;
        return (*env)->NewObject(env, dateClass, dateConstructor, date * 1000);
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
    case RECORDOID: {
        HeapTupleHeader hth = DatumGetHeapTupleHeader(d);
        if (hth == 0) return NULL;
        int a = HeapTupleHeaderGetTypeId(hth);
        int b = HeapTupleHeaderGetTypMod(hth);
        TupleDesc td = lookup_rowtype_tupdesc(a,b);
        
        HeapTupleData htd;
        htd.t_len = HeapTupleHeaderGetDatumLength(td);
        htd.t_data = hth;
        
        // d = toast_flatten_tuple(d, td );
        
        jobject rr = tupleToObject(td, &htd);
        ReleaseTupleDesc(td);
        return rr;
    }
    
    default: ereport(ERROR, (errmsg("unable to convert Datum to java object")));
    }
    return NULL;
}
