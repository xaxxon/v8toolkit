
Very similar to os x.

When building the libfmt dependency, make sure to build the shared object library:

    > cmake -DBUILD_SHARED_LIBS=TRUE



Depending on your version of V8, you may run into this:

https://bbs.archlinux.org/viewtopic.php?id=209871

Solution is to just symlink in your global ld.gold something like this:

~/v8$ ln -is `which ld.gold`  third_party/binutils/Linux_x64/Release/bin/ld.gold 

    export CLANG_HOME="/path/to/clang/root"
    export CXX="$CLANG_HOME/bin/clang++ -std=c++11 -stdlib=libc++"
    export CC="$CLANG_HOME/bin/clang"
    export CPP="$CLANG_HOME/bin/clang -E"
    export LINK="$CLANG_HOME/bin/clang++ -std=c++11 -stdlib=libc++"
    export CXX_host="$CLANG_HOME/bin/clang++"
    export CC_host="$CLANG_HOME/bin/clang"
    export CPP_host="$CLANG_HOME/bin/clang -E"
    export LINK_host="$CLANG_HOME/bin/clang++"
    export GYP_DEFINES="clang=1"
				    
make x64.debug library=shared snapshot=off 

new way:

in v8 directory:
`./tools/dev/v8gen.py is_component_build=true is_debug=true v8_use_snapshot=false`
\