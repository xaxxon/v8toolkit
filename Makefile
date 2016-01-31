# DEBUG = -g
V8DIR = /users/xaxxon/v8
V8LIBDIR = ${V8DIR}/out/native
#V8LIBDIR = ${V8DIR}/out/x64.release
#V8LIBDIR = ${V8DIR}/out/x64.debug
CPPFLAGS = -I${V8DIR} ${DEBUG} -std=c++14 -I/usr/local/include ${DEBUG} -DV8TOOLKIT_JAVASCRIPT_DEBUG

all: warning thread_sample javascript sample toolbox_sample


warning:
	$(info )
	$(info )
	$(info THIS MAKEFILE IS FOR MY DEVELOPMENT WORK ONLY, IT IS NOT A GENERAL PURPOSE MAKEFILE)
	$(info You can look at this for an example of what to do, but you'll have to customize it)
	$(info )
	$(info )


thread_sample: lib
	clang++ -std=c++14 ${DEBUG} -I/usr/local/include -I/users/xaxxon/v8  thread_sample.cpp -o thread_sample ${V8LIBDIR}/{libv8_base.a,libv8_libbase.a,libicudata.a,libicuuc.a,libicui18n.a,libv8_base.a,libv8_external_snapshot.a,libv8_libplatform.a} javascript.o v8toolkit.o

javascript: lib
	clang++ -std=c++14 ${DEBUG} -I/usr/local/include -I/users/xaxxon/v8  javascript_sample.cpp -o javascript ${V8LIBDIR}/{libv8_base.a,libv8_libbase.a,libicudata.a,libicuuc.a,libicui18n.a,libv8_base.a,libv8_external_snapshot.a,libv8_libplatform.a} javascript.o v8toolkit.o

sample: lib
	clang++ -std=c++14 ${DEBUG} -I/usr/local/include -I/users/xaxxon/v8  sample.cpp -o sample ${V8LIBDIR}/{libv8_base.a,libv8_libbase.a,libicudata.a,libicuuc.a,libicui18n.a,libv8_base.a,libv8_external_snapshot.a,libv8_libplatform.a} v8toolkit.o

toolbox_sample: lib
	clang++ -std=c++14 ${DEBUG} -I/usr/local/include -I/users/xaxxon/v8  toolbox_sample.cpp -o toolbox ${V8LIBDIR}/{libv8_base.a,libv8_libbase.a,libicudata.a,libicuuc.a,libicui18n.a,libv8_base.a,libv8_external_snapshot.a,libv8_libplatform.a} v8toolkit.o

lib: v8toolkit.o javascript.o
	ar cr libv8toolkit.a v8toolkit.o javascript.o

clean:
	rm -f *.o *.a *sample

run:
	./thread_sample
	./javascript
	./sample
	./toolbox

clean_docs:
	rm -rf doc	

docs: clean_docs
	mkdir -p docs/html
	doxygen doxygen.cfg
	

lint:
	cpplint.py --linelength=200 --filter=-whitespace/end_of_line *.cpp *.hpp *.h