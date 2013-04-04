
#include <jni.h>
#include <postgres.h>
#ifdef __APPLE__
#undef false
#undef true
#include <libproc.h>
#endif
#include <limits.h>
#include <syslog.h>
#include <unistd.h>

#include <utils/guc.h>

JNIEnv *env;
JavaVM* s_javaVM = NULL;
JNIEnv* jniEnv;

JavaVMOption options[20];

// the postgres server datadir
extern char *DataDir;

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
    char *lst = strrchr(pathbuf,'/'); // last slash in path
    int n = lst-pathbuf;
    return pnstrdup(pathbuf, n);
}

static char *getClasspath() {
    char buf[10000];
    char *pbuf = getPGdir();
    /* get all the jar fles in the jar directory and append? */
    snprintf(buf, 10000, "-Djava.class.path=%s/../lib/pg_jinx.jar:%s/../lib/pljava.jar", pbuf,pbuf);
    syslog(LOG_ERR, "classpath = %s\n", buf);
    return pstrdup(buf);
}

void doJVMinit() {
    
    jsize nVMs;
    JavaVM *vmBuf[2];
    JNI_GetCreatedJavaVMs(vmBuf, 2, &nVMs);
    if (nVMs > 0) {
        s_javaVM = vmBuf[0];
        (*s_javaVM)->GetEnv(s_javaVM, (void **)&env, JNI_VERSION_1_6);
        jniEnv = env;
        return;
    }
    
    
    jint jstat;
    int debug_port = 0;

    JavaVMInitArgs vm_args;
    const char *dbp;
    
    int ox = 0;
    options[ox++].optionString = getClasspath();
    
    dbp = GetConfigOption("pg_jinx.debug_port", true, false);
    if (dbp != NULL) debug_port = atoi(dbp);
 	if (debug_port != 0) {
        char buf[4096];
        sprintf(buf, "-Xrunjdwp:transport=dt_socket,server=y,suspend=%s,address=localhost:%d", debug_port > 0 ? "n" : "y", abs(debug_port)) ;
        options[ox++].optionString = "-Xdebug";
        options[ox++].optionString = pstrdup(buf);
    }
    options[ox++].optionString = "-Xrs";
	options[ox++].optionString = "-Dsqlj.defaultconnection=jdbc:default:connection";
	options[ox++].optionString = "-Djava.awt.headless=true";
    { char buf[4096];
        char *pbuf = getPGdir();
        sprintf(buf, "-Djava.library.path=%s/../lib", pbuf );
        options[ox++].optionString = pstrdup(buf);
        pfree(pbuf);
	}
	
    { char buf[4096];
        char *pbuf = getPGdir();
        // Why do I need to include the class path in the ext dir list?
        snprintf(buf, 4096, "-Djava.ext.dirs=%s/../ext:%s/../lib",DataDir,pbuf);
        options[ox++].optionString = pstrdup(buf);
        pfree(pbuf);
    }
    
    { char buf[4096];
        snprintf(buf, 4096, "-Djinx.extDir=%s/../ext",DataDir);
        options[ox++].optionString=pstrdup(buf);
    }
    // JVMOptList_add(&optList, "-Xmx256m");
    //    JVMOptList_add(&optList, "-verbose:jni",0,true);
        
	vm_args.version = JNI_VERSION_1_6;
	vm_args.options = options;
	vm_args.ignoreUnrecognized = JNI_FALSE;
    vm_args.nOptions=ox;
        
	jstat = JNI_CreateJavaVM(&s_javaVM, (void **) &env, &vm_args);
    jniEnv = env;
	if (jstat == JNI_OK && (*env)->ExceptionCheck(env)) {
        // jthrowable jt = (*env)->ExceptionOccurred(env);
        ereport(ERROR, (errmsg("Java VM threw exception during startup")));
/*        if (jt != 0) {
            
            (*env)->ExceptionClear(env);
            (*env)->CallVoidMethod(env, jt, printStackTrace);
            elogExceptionMessage(env, jt, WARNING);
        }
        */
        jstat = JNI_ERR;
    }
    if (jstat != JNI_OK) { ereport(ERROR, (errmsg("Failed to create Java VM"))); }
}

// Since DestroyJavaVM actually doesn't work, there's no point in calling it, is there?
/*void destroyJVM(int status, Datum dummy) {
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
*/
