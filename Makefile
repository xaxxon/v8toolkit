# DEBUG = -g
V8_DIR = /users/xaxxon/v8
V8_INCLUDE_DIR = ${V8_DIR}
V8_TARGET = native
#V8_LIB_DIR = ${V8_DIR}/out/native

# OS X put libs in first location, linux puts in second two
V8_LIB_DIR = ${V8_DIR}/out/${V8_TARGET}/
V8_LIB_DIR2 = ${V8_LIB_DIR}/obj.target/tools/gyp/
V8_LIB_DIR3 = ${V8_LIB_DIR}/obj.target/third_party/icu/

V8_LIB_DIR_FLAGS = -L${V8_LIB_DIR}
#V8_LIB_DIR_FLAGS = -L${V8_LIB_DIR2} -L${V8_LIB_DIR3}

#V8_LIB_DIR = ${V8_DIR}/out/x64.release
#V8_LIB_DIR = ${V8_DIR}/out/x64.debug

# Whether you want to use snapshot files, but easier not to use them.  I see a .05s decrease in startup speed by not using them
ifdef USE_SNAPSHOTS
DEFINES = -DUSE_SNAPSHOTS -DV8TOOLKIT_JAVASCRIPT_DEBUG 
V8_LIBS = -lv8_base -lv8_libbase -licudata -licuuc -licui18n -lv8_base -lv8_libplatform -lv8_external_snapshot
else
#DEFINES = -DV8TOOLKIT_JAVASCRIPT_DEBUG 
V8_LIBS = -lv8_base -lv8_libbase -licudata -licuuc -licui18n -lv8_base -lv8_libplatform -lv8_nosnapshot
endif

CPPFLAGS = -I${V8_INCLUDE_DIR} ${DEBUG} -std=c++14 -I/usr/local/include ${DEFINES} -Wall -Werror

# LIBS = -L/usr/local/lib -L${V8_LIB_DIR}  libv8toolkit.a ${V8_LIBS} -lboost_system -lboost_filesystem
LIBS = -L/usr/local/lib ${V8_LIB_DIR_FLAGS}  libv8toolkit.a ${V8_LIBS}



all: warning thread_sample javascript sample toolbox_sample exception_sample

SRCS=v8toolkit.cpp javascript.cpp

OBJS=$(SRCS:.cpp=.o)


warning:
	$(info )
	$(info )
	$(info THIS MAKEFILE IS FOR MY DEVELOPMENT WORK ONLY; IT IS NOT A GENERAL PURPOSE MAKEFILE)
	$(info You can look at this for an example of what to do, but you'll have to customize it)
	$(info )
	$(info )


thread_sample: lib
	clang++ -std=c++14 ${DEBUG} -I./ ${CPPFLAGS}  samples/thread_sample.cpp -o samples/thread_sample  ${LIBS}

javascript: lib
	clang++ -std=c++14 ${DEBUG} -I./ ${CPPFLAGS}  samples/javascript_sample.cpp -o samples/javascript_sample ${LIBS}

sample: lib
	clang++ -std=c++14 ${DEBUG} -I./ ${CPPFLAGS}  samples/sample.cpp -o samples/sample ${LIBS}

toolbox_sample: lib
	clang++ -std=c++14 ${DEBUG} -I./ ${CPPFLAGS}  samples/toolbox_sample.cpp -o samples/toolbox_sample ${LIBS}

exception_sample: lib
	clang++ -std=c++14 ${DEBUG} -I./ ${CPPFLAGS}  samples/exception_sample.cpp -o samples/exception_sample ${LIBS}


lib: ${OBJS}
	ar cr libv8toolkit.a v8toolkit.o javascript.o

clean:
	rm -f *.o *.a samples/*sample

run:
	(cd samples && ./thread_sample && ./javascript_sample && ./sample && ./toolbox_sample)

clean_docs:
	rm -rf doc	

docs: clean_docs
	mkdir -p docs/html
	doxygen doxygen.cfg
	

lint:
	cpplint.py --linelength=200 --filter=-whitespace/end_of_line *.cpp *.hpp *.h