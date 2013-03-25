
OBJS = pg_jinx.o pg_jinx_fdw.o init_jvm.o jni_utils.o jbridge.o datumToObject.o objectToDatum.o 
EXTENSION = pg_jinx.so
DATA = pg_jinx--1.0.sql

UNAME = $(shell uname)

SHLIB_LINK = -ljvm
ifeq ($(UNAME), Darwin)
	SHLIB_LINK = -L$(JAVA_HOME)/jre/lib/server -ljvm 
endif

TRGTS = $(OBJS) $(EXTENSION)

all:$(TRGTS)


JDEBUG = -g 
JAR_DIR=lib
JARS = $(shell echo $(JAR_DIR)/*.jar)
EMPTY = 
SPACE = $(EMPTY) $(EMPTY)
SEP = :

LOCAL_JARTMP     = $(patsubst %,$(JAR_DIR)/%,$(JARS))
LOCAL_JARLIST    = $(subst $(SPACE),$(SEP),$(JARS))


.PHONY: clean JAVAFILES

clean:
	rm -f *.o */*.o *.so

# the build for com has the samples
JAVAFILES:
	echo JAR_DIR: $(JAR_DIR)
	echo JARS: $(JARS)
	echo LOCAL_JARTMP: $(LOCAL_JARTMP)
	echo LOCAL_JARLIST: $(LOCAL_JARLIST)
	mkdir -p build
	# /bin/zsh -c 'javac $(JDEBUG) -classpath $(LOCAL_JARLIST) -d build classes/**/*.java'
	/bin/zsh -c 'javac $(JDEBUG) -d build classes/**/*.java'
	jar cf pg_jinx.jar -C build org
	rm -f pg_combined.jar
	ant jar

pg_include_dir = ../Vendor/postgres/include/server
pg_exe =         ../Vendor/postgres/bin/postgres

CC = gcc
CPP = gcc -E
CPPFLAGS =  -I. -I/usr/include/libxml2 -I$(pg_include_dir) -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/darwin

# CFLAGS = -g -O2 -Wall -Wmissing-prototypes -Wpointer-arith -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv
CFLAGS = -g -std=gnu99 -Wall -Wmissing-prototypes -Wpointer-arith -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -std=gnu99

COMPILE.c = $(CC) $(CFLAGS) $(CPPFLAGS) -c

# LIBS = -lxslt -lxml2 -lssl -lcrypto -lgssapi_krb5 -lz -lreadline -lm 

LDFLAGS +=   -headerpad_max_install_names -Wl,-dead_strip_dylibs
LDFLAGS_SL = -bundle -multiply_defined suppress
BE_DLLLIBS = -bundle_loader $(pg_exe)

%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $< 

$(EXTENSION): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDFLAGS_SL) -o $@ $(OBJS) $(SHLIB_LINK) -bundle $(BE_DLLLIBS) 
	install_name_tool -add_rpath @loader_path/../../PlugIns/jdk1.7.jdk/Contents/Home/jre/lib/server $@

# install_name_tool -change "@rpath/libjvm.dylib" "${JAVA_HOME}/jre/lib/server/libjvm.dylib" $@

# this is just for zapping Transgres during development
PGDIR = /Users/r0ml/Desktop/Transgres.app/Contents/MacOS

install: all JAVAFILES
	cp pg_jinx.control pg_jinx--*.sql $(PGDIR)/share/extension/
	cp $(EXTENSION) $(PGDIR)/lib
	cp pg_combined.jar $(PGDIR)/lib/pg_jinx.jar # pg_combined -> pg_jinx
