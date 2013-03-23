
#include <pg_jinx.h>

JNIEnv *env;
JavaVM* s_javaVM = NULL;

bool integerDateTimes = false;

extern void destroyJVM(int, Datum);

static bool s_firstTimeInit = true;

typedef struct {
	JavaVMOption* options;
	unsigned int  size;
	unsigned int  capacity;
} JVMOptList;

jclass mapClass, classClass, throwableClass, sqlException, stringClass, objClass;
jclass intClass, dblClass, shortClass, floatClass, byteClass, byteArrayClass, booleanClass,
    longClass, charClass;
jclass selectionClass, columnClass, colrefClass, pgfuncClass;

jclass serverExceptionClass, functionClass, byteArrayClass, booleanArrayClass, intArrayClass,
    shortArrayClass, longArrayClass, floatArrayClass, doubleArrayClass, bigDecimalClass, iterator;

jmethodID mapConstructor, mapPut, classGetName, getMessage, getSQLState, printStackTrace, getClass;
jmethodID selectionConstructor, columnConstructor, colrefConstructor, sqlExceptionConstructor,
    pgfunc0Cons, pgfunc1Cons, pgfunc2Cons, pgfunc3Cons, pgfuncnCons;

jmethodID intValue, doubleValue, longValue, shortValue, floatValue, booleanValue, byteValue, charValue;
jmethodID intConstructor, dblConstructor, shortConstructor, floatConstructor, longConstructor, boolConstructor,
    byteConstructor, charConstructor, bigDecimalConstructor, toString, hasNext, next;

jmethodID serverExceptionConstructor, functionConstructor, functionInvoke, triggerInvoke;

static void JVMOptList_init(JVMOptList* jol) {
	jol->options  = (JavaVMOption*)palloc(10 * sizeof(JavaVMOption));
	jol->size     = 0;
	jol->capacity = 10;
}
static void JVMOptList_delete(JVMOptList* jol) {
	JavaVMOption* opt = jol->options;
	JavaVMOption* top = opt + jol->size;
	while(opt < top) {
		pfree(opt->optionString);
		opt++;
	}
	pfree(jol->options);
}

static void JVMOptList_add(JVMOptList* jol, const char* optString) {
	int newPos = jol->size;
	if(newPos >= jol->capacity) {
		int newCap = jol->capacity * 2;
		JavaVMOption* newOpts = (JavaVMOption*)palloc(newCap * sizeof(JavaVMOption));
		memcpy(newOpts, jol->options, newPos * sizeof(JavaVMOption));
		pfree(jol->options);
		jol->options = newOpts;
		jol->capacity = newCap;
	}
//	added = jol->options + newPos;
	jol->options[newPos].optionString = pstrdup(optString);
	jol->size++;
}

static char *getPGdir() {
    // WARNING: this code will only work on linux or OS X
#ifdef __APPLE__
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    int pid = getpid();
    int ret = proc_pidpath(pid, pathbuf, PROC_PIDPATHINFO_MAXSIZE);
    if ( ret <= 0 ) {
        syslog(LOG_ERR, "PID %d: proc_pidpath ();\n", pid);
        syslog(LOG_ERR, "    %s\n", strerror(errno));
    } else {
        syslog(LOG_ERR, "proc %d: %s\n", pid, pathbuf);
    }

#else
    char pathbuf[PATH_MAX];
    const char* exe_sym_path = "/proc/self/exe";
        // when BSD: /proc/curproc/file
        // when Solaris: /proc/self/path/a.out
        // Unix: getexecname(3)
    ssize_t ret = readlink(exe_sym_path, pathbuf, PATH_MAX);
    if (ret == PATH_MAX || ret == -1) {
      syslog(LOG_ERR, "Getting executable path: %s\n",strerror(errno));
    } else {
      pathbuf[ret] = '\0';
    }
#endif
    return pstrdup(pathbuf);
}

static char *getClasspath(const char *jarfile) {
    StringInfoData buf;
    char *pbuf = getPGdir();
    char *lst = strrchr(pbuf,'/'); // last slash in path
    int n = lst-pbuf;
    initStringInfo(&buf);
    appendStringInfo(&buf, "-Djava.class.path=");
    appendBinaryStringInfo(&buf, pbuf, n+1);
    appendStringInfo(&buf,"../lib/pg_jinx.jar:");
    appendBinaryStringInfo(&buf, pbuf, n+1);
    appendStringInfo(&buf,"../lib/%s.jar:",jarfile); // 
    appendBinaryStringInfo(&buf, pbuf, n+1);
    appendStringInfo(&buf,"../lib/classes/");
    pfree(pbuf);
    syslog(LOG_ERR, "classpath = %s\n", buf.data);
    return pstrdup(buf.data);
}

// only in elogExceptionMessage
static void appendJavaString(StringInfoData* buf, jstring javaString) {
  if(javaString != 0) {
      const char* utf8 = (*env)->GetStringUTFChars(env, javaString, 0);
      char* dbEnc = (char*)pg_do_encoding_conversion((unsigned char *)utf8, strlen(utf8), PG_UTF8, GetDatabaseEncoding());
      appendStringInfoString(buf, dbEnc);
      if(dbEnc != utf8) pfree(dbEnc);
      (*env)->ReleaseStringUTFChars(env,javaString, utf8);
  }
}

// only in doJVMinit
static void elogExceptionMessage(JNIEnv* env, jthrowable exh, int logLevel) {
	StringInfoData buf;
	int sqlState = ERRCODE_INTERNAL_ERROR;
	jclass exhClass = (*env)->GetObjectClass(env, exh);
	jstring jtmp = (jstring)(*env)->CallObjectMethod(env, exhClass, classGetName);

	initStringInfo(&buf);
	appendJavaString(&buf, jtmp);

	(*env)->DeleteLocalRef(env, exhClass);
	(*env)->DeleteLocalRef(env, jtmp);

	jtmp = (jstring)(*env)->CallObjectMethod(env, exh, getMessage);
	if(jtmp != 0) {
		appendStringInfoString(&buf, ": ");
		appendJavaString(&buf, jtmp);
		(*env)->DeleteLocalRef(env, jtmp);
	}

	if((*env)->IsInstanceOf(env, exh, sqlException)) {
		jtmp = (*env)->CallObjectMethod(env, exh, getSQLState);
		if(jtmp != 0) {
			char* s = fromJavaString(jtmp);
			(*env)->DeleteLocalRef(env, jtmp);
			if(strlen(s) >= 5) sqlState = MAKE_SQLSTATE(s[0], s[1], s[2], s[3], s[4]);
			pfree(s);
		}
	}
	ereport(logLevel, (errcode(sqlState), errmsg("%s", buf.data)));
}




static void doJVMinit() {
    jint jstat;
    int debug_port = 0;
    JVMOptList optList;
    JavaVMInitArgs vm_args;
    const char *dbp;
    const char *jarfile="swdb.jar";
    char *classpath = getClasspath(jarfile);
    
    JVMOptList_init(&optList);
    JVMOptList_add(&optList, classpath);
    pfree(classpath);
    
    dbp = GetConfigOption("pg_jinx.debug_port", true, false);
    if (dbp != NULL) debug_port = atoi(dbp);
 	if (debug_port > 0) {
        char buf[4096];
        sprintf(buf, "-Xrunjdwp:transport=dt_socket,server=y,suspend=n,address=localhost:%d", debug_port);
        JVMOptList_add(&optList, "-Xdebug");
        JVMOptList_add(&optList, buf);
    }
    JVMOptList_add(&optList, "-Xrs");
	JVMOptList_add(&optList, "-Dsqlj.defaultconnection=jdbc:default:connection");
	JVMOptList_add(&optList, "-Djava.awt.headless=true");
    { char buf[4096];
        char *pbuf = getPGdir();
        sprintf(buf, "-Djava.library.path=%s/../lib", pbuf );
        JVMOptList_add(&optList, buf);
        pfree(pbuf);
	}
    // JVMOptList_add(&optList, "-Xmx256m");
    
	vm_args.version = JNI_VERSION_1_6;
	vm_args.options = optList.options;
	vm_args.ignoreUnrecognized = JNI_FALSE;
    vm_args.nOptions=optList.size;
    
	jstat = JNI_CreateJavaVM(&s_javaVM, (void **) &env, &vm_args);
	if (jstat == JNI_OK && (*env)->ExceptionCheck(env)) {
        jthrowable jt = (*env)->ExceptionOccurred(env);
        if (jt != 0) {
            (*env)->ExceptionClear(env);
            (*env)->CallVoidMethod(env, jt, printStackTrace);
            elogExceptionMessage(env, jt, WARNING);
        }
        jstat = JNI_ERR;
    }
    JVMOptList_delete(&optList);
    if (jstat != JNI_OK) { ereport(ERROR, (errmsg("Failed to create Java VM"))); }
}

static void checkIntTimeType(void) {
    const char* idt = GetConfigOption("integer_datetimes", true, false);
	integerDateTimes = (idt != NULL && strcmp(idt, "on") == 0);
}


static void jinxInterruptHandler(int signum) {
	if(!proc_exit_inprogress) InterruptFlag = true;
}

static void jinxTerminationHandler(int signum) {
	if(!proc_exit_inprogress) {
        TerminationFlag = true;
	}
}

static sigjmp_buf recoverBuf;
static void terminationTimeoutHandler(int signum) {
	kill(MyProcPid, SIGQUIT);
	pg_usleep(1);	// Some sleep to get the SIGQUIT a chance to generate the needed output.
	siglongjmp(recoverBuf, 1); // JavaVM did not die within the alloted time

}

extern void bridge_initialize(void);

static void initClasses(void) {
// I need these for error-handling
    CACHE_CLASS(classClass, "java.lang.Class");
    CACHE_CLASS(throwableClass,"java.lang.Throwable");
    CACHE_CLASS(sqlException, "java.sql.SQLException");
    CACHE_CLASS(objClass, "java.lang.Object");
    CACHE_CLASS(iterator, "java.util.Iterator");
    CACHE_METHOD(classGetName, classClass, "getName", "()Ljava/lang/String;");
    CACHE_METHOD(getMessage, throwableClass, "getMessage", "()Ljava/lang/String;");
    CACHE_METHOD(sqlExceptionConstructor, sqlException, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
    CACHE_METHOD(getSQLState, sqlException, "getSQLState", "()Ljava/lang/String;");
    CACHE_METHOD(printStackTrace, throwableClass, "printStackTrace", "()V");
    CACHE_METHOD(getClass, objClass, "getClass", "()Ljava/lang/Class;");
    
    CACHE_CLASS(serverExceptionClass, "org.sourcewave.jinx.ServerException");
    CACHE_METHOD(serverExceptionConstructor, serverExceptionClass, "<init>", "([I[Ljava/lang/String;)V");
    CACHE_METHOD(hasNext, iterator, "hasNext", "()Z");
    CACHE_METHOD(next, iterator, "next", "()Ljava/lang/Object;");

    bridge_initialize();
}


void destroyJVM(int status, Datum dummy) {
    pqsigfunc saveSigAlrm;

    if (s_javaVM == NULL) return;

	if(sigsetjmp(recoverBuf, 1) != 0) {
        elog(DEBUG1, "JavaVM destroyed with force");
        s_javaVM = NULL;
		return;
	}

	saveSigAlrm = pqsignal(SIGALRM, terminationTimeoutHandler);
	enable_sig_alarm(5000, false);

	elog(DEBUG1, "Destroying JavaVM...");
	(*s_javaVM)->DestroyJavaVM(s_javaVM);
    disable_sig_alarm(false);
	pqsignal(SIGALRM, saveSigAlrm);
	s_javaVM = NULL;
}

#define CACHE_ARRAY_CLASS(typ,sig) \
    typ##ArrayClass = (*env)->FindClass(env, sig); \
    CHECK_EXCEPTION("%s\nError finding ##type##array class", "unused"); \
    typ##ArrayClass = (*env)->NewGlobalRef(env, typ##ArrayClass); \
    CHECK_EXCEPTION("%s\nCreating global reference to ##type##array class","unused"); 


static void initMoreClasses() {
    CACHE_CLASS(mapClass, "java.util.HashMap");
    CACHE_CLASS(stringClass, "java.lang.String");
    
    CACHE_CLASS(intClass,"java.lang.Integer");
    CACHE_CLASS(dblClass, "java.lang.Double");
    CACHE_CLASS(shortClass, "java.lang.Short");
    CACHE_CLASS(floatClass, "java.lang.Float");
    CACHE_CLASS(byteClass, "java.lang.Byte");
    CACHE_CLASS(longClass, "java.lang.Long");
    CACHE_CLASS(booleanClass, "java.lang.Boolean");
    CACHE_CLASS(charClass, "java.lang.Character");
    CACHE_CLASS(bigDecimalClass, "java.math.BigDecimal");
    CACHE_METHOD(intValue, intClass, "intValue", "()I");
    CACHE_METHOD(doubleValue, dblClass, "doubleValue", "()D");
    CACHE_METHOD(longValue, longClass, "longValue", "()J");
    CACHE_METHOD(shortValue, shortClass, "shortValue", "()S");
    CACHE_METHOD(floatValue, dblClass, "floatValue", "()F");
    CACHE_METHOD(booleanValue, booleanClass, "booleanValue", "()Z");
    CACHE_METHOD(byteValue, byteClass, "byteValue", "()B");
    CACHE_METHOD(charValue, charClass, "charValue", "()C");

    CACHE_METHOD(intConstructor, intClass, "<init>", "(I)V");
    CACHE_METHOD(dblConstructor, dblClass, "<init>", "(D)V");
    CACHE_METHOD(longConstructor, longClass, "<init>", "(J)V");
    CACHE_METHOD(floatConstructor, floatClass, "<init>", "(F)V");
    CACHE_METHOD(shortConstructor, shortClass, "<init>", "(S)V");
    CACHE_METHOD(boolConstructor, booleanClass, "<init>", "(Z)V");
    CACHE_METHOD(byteConstructor, byteClass, "<init>", "(B)V");
    CACHE_METHOD(charConstructor, charClass, "<init>", "(C)V");
    CACHE_METHOD(bigDecimalConstructor, bigDecimalClass, "<init>", "(Ljava/lang/String;)V");
    CACHE_METHOD(toString, objClass, "toString", "()Ljava/lang/String;");
    
    CACHE_ARRAY_CLASS(byte,"[B");
    CACHE_ARRAY_CLASS(short,"[S");
    CACHE_ARRAY_CLASS(boolean,"[Z");
    CACHE_ARRAY_CLASS(int,"[I");
    CACHE_ARRAY_CLASS(long,"[J");
    CACHE_ARRAY_CLASS(float,"[F");
    CACHE_ARRAY_CLASS(double,"[D");

    CACHE_METHOD(mapConstructor, mapClass, "<init>", "()V");
    CACHE_METHOD(mapPut, mapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    
    CACHE_CLASS(functionClass, "org.sourcewave.jinx.Function");
    CACHE_STATIC_METHOD(functionConstructor, functionClass, "fromOID", "(I)Lorg/sourcewave/jinx/Function;");
    CACHE_METHOD(functionInvoke, functionClass, "invoke", "(Ljava/lang/String;[Ljava/lang/String;[II[Ljava/lang/Object;)Ljava/lang/Object;");
    CACHE_METHOD(triggerInvoke, functionClass, "triggerInvoke", "(Ljava/lang/String;Ljava/lang/String;[Z[Ljava/lang/Object;[Ljava/lang/Object;)[Ljava/lang/Object;");
}


void initializeJVM(bool trust) {
    bool c_debug = false;
    const char *cdb;
    
    if (!s_firstTimeInit) return;

    s_firstTimeInit = false;

	checkIntTimeType();
	
    cdb = GetConfigOption("pg_jinx.c_debug", true, false);
    if (cdb != NULL) {
        c_debug = *cdb == 't' || *cdb == '1' || *cdb == 'y';
    }
    if (c_debug) {
	  elog(INFO, "Backend pid = %d. Attach the debugger and set c_debug to false to continue", getpid());
	  while(c_debug) pg_usleep(1000000L);
    }
    
    doJVMinit();
    
	pqsignal(SIGINT,  jinxInterruptHandler);
	pqsignal(SIGTERM, jinxTerminationHandler);

	/* Register an on_proc_exit handler that destroys the VM
	 */
	on_proc_exit(destroyJVM, 0);
	initClasses();
    initMoreClasses();
}
