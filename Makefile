
OBJS = pg_jinx.o pg_jinx_fdw.o init_jni.o init_jvm.o jni_utils.o jbridge.o datumToObject.o objectToDatum.o 
EXTENSION = pg_jinx.so
DATA = pg_jinx--2.3.sql
PG_CONFIG ?= pg_config
JSRCS := $(shell find ./classes -type f -name \*.java -print)

INSTALL_SHLIB = install -c -m 755
INSTALL_DATA = install -c

sharedir = $(shell ${PG_CONFIG} --sharedir)
pkglibdir = $(shell ${PG_CONFIG} --pkglibdir)

# CFLAGS = -g -O2 -Wall -Wmissing-prototypes -Wpointer-arith -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv
CFLAGS = -g -std=gnu99 -Wall -Wmissing-prototypes -Wpointer-arith -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -std=gnu99

UNAME = $(shell uname)

SHLIB_LINK = -ljvm
ifeq ($(UNAME), Darwin)
	SHLIB_LINK = -L$(JAVA_HOME)/jre/lib/server -ljvm 
        pg_include_dir = ../Vendor/postgres/include/server
        pg_exe =         ../Vendor/postgres/bin/postgres
        JOS = darwin
	LDFLAGS +=   -headerpad_max_install_names -Wl,-dead_strip_dylibs
	LDFLAGS_SL = -bundle -multiply_defined suppress
	BE_DLLLIBS = -bundle_loader $(pg_exe)
endif

ifeq ($(UNAME), Linux)
        JAVA_HOME = /usr/lib/jvm/java-7-oracle
	CFLAGS += -fpic 
	SHLIB_LINK = -L$(JAVA_HOME)/jre/lib/amd64/server -ljvm 
	LDFLAGS_SL = -fpic -shared
        pg_include_dir = $(shell ${PG_CONFIG} --includedir-server)
        pg_exe = $(shell ${PG_CONFIG} --bindir)/postgres
        JOS = linux
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
	javac $(JDEBUG) -d build $(JSRCS)
	jar cf pg_jinx.jar -C build org
	rm -f pg_combined.jar
	ant jar

CC = gcc
CPP = gcc -E
CPPFLAGS =  -I. -I/usr/include/libxml2 -I$(pg_include_dir) -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(JOS)

COMPILE.c = $(CC) $(CFLAGS) $(CPPFLAGS) -c

# LIBS = -lxslt -lxml2 -lssl -lcrypto -lgssapi_krb5 -lz -lreadline -lm 

%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $< 

$(EXTENSION): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDFLAGS_SL) -o $@ $(OBJS) $(SHLIB_LINK)$(BE_DLLLIBS) 

# this is just for zapping Transgres during development

PGDIR = /Users/r0ml/Library/Developer/Xcode/DerivedData/Transgres-coacovxfvhegpfexjtypwjlxnqsu/Build/Products/Debug/Transgres.app/Contents/MacOS

ifeq ($(UNAME), Darwin)
install: all JAVAFILES
	( install_name_tool -add_rpath @loader_path/../../PlugIns/jdk1.7.jdk/Contents/Home/jre/lib/server $(EXTENSION) || true)
	cp pg_jinx.control pg_jinx--*.sql $(PGDIR)/share/extension/
	cp $(EXTENSION) $(PGDIR)/lib
	cp pg_combined.jar $(PGDIR)/lib/pg_jinx.jar # pg_combined -> pg_jinx
endif

ifeq ($(UNAME), Linux)
install: all JAVAFILES
	$(INSTALL_SHLIB) $(EXTENSION) $(DESTDIR)$(pkglibdir)/$(EXTENSION)
	$(INSTALL_DATA) pg_jinx.control $(sharedir)/extension/
	$(INSTALL_DATA) pg_jinx--2.3.sql $(sharedir)/extension/
	$(INSTALL_DATA) pg_combined.jar $(DESTDIR)$(pkglibdir)/pg_jinx.jar
endif
