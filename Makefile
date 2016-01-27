
all:
	$(info )
	$(info )
	$(info THIS MAKEFILE IS FOR MY DEVELOPMENT WORK ONLY, IT IS NOT A GENERAL PURPOSE MAKEFILE)
	$(info You can look at this for an example of what to do, but you'll have to customize it)
	$(info )
	$(info )
	clang++ -g -c v8toolkit.cpp javascript.cpp -std=c++14 -I/usr/local/include -I/users/xaxxon/v8 
	ar cr libv8toolkit.a v8toolkit.o javascript.o
	clang++ -std=c++14 -I/usr/local/include -I/users/xaxxon/v8  thread_sample.cpp -o thread_sample /users/xaxxon/v8/out/x64.release/{libv8_base.a,libv8_libbase.a,libicudata.a,libicuuc.a,libicui18n.a,libv8_base.a,libv8_external_snapshot.a,libv8_libplatform.a} javascript.o v8toolkit.o
	clang++ -std=c++14 -I/usr/local/include -I/users/xaxxon/v8  javascript_sample.cpp -o javascript /users/xaxxon/v8/out/x64.release/{libv8_base.a,libv8_libbase.a,libicudata.a,libicuuc.a,libicui18n.a,libv8_base.a,libv8_external_snapshot.a,libv8_libplatform.a} javascript.o v8toolkit.o
	clang++ -std=c++14 -I/usr/local/include -I/users/xaxxon/v8  sample.cpp -o sample /users/xaxxon/v8/out/x64.release/{libv8_base.a,libv8_libbase.a,libicudata.a,libicuuc.a,libicui18n.a,libv8_base.a,libv8_external_snapshot.a,libv8_libplatform.a} v8toolkit.o
	clang++ -std=c++14 -I/usr/local/include -I/users/xaxxon/v8  toolbox_sample.cpp -o toolbox /users/xaxxon/v8/out/x64.release/{libv8_base.a,libv8_libbase.a,libicudata.a,libicuuc.a,libicui18n.a,libv8_base.a,libv8_external_snapshot.a,libv8_libplatform.a} v8toolkit.o


run:
	./thread_sample
	./javascript
	./sample
	./toolbox

clean_docs:
	rm -rf doc	

docs: clean_docs
	doxygen doxygen.cfg
	