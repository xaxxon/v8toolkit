content goes here


<<<<<<< HEAD
Very similar to os x.


Depending on your version of V8, you may run into this:

https://bbs.archlinux.org/viewtopic.php?id=209871

Solution is to just symlink in your global ld.gold something like this:

~/v8$ ln -is `which ld.gold`  third_party/binutils/Linux_x64/Release/bin/ld.gold 


=======

export CLANG_DIR="/home/xaxxon/Downloads/clang+llvm-3.8.1-x86_64-linux-gnu-ubuntu-16.04"
export CXX="$CLANG_DIR/bin/clang++ -std=c++11 -stdlib=libc++"
export CC="$CLANG_DIR/bin/clang"
export CPP="$CLANG_DIR/bin/clang -E"
export LINK="$CLANG_DIR/bin/clang++ -std=c++11 -stdlib=libc++"
export CXX_host="$CLANG_DIR/bin/clang++"
export CC_host="$CLANG_DIR/bin/clang"
export CPP_host="$CLANG_DIR/bin/clang -E"
export LINK_host="$CLANG_DIR/bin/clang++"
export GYP_DEFINES="clang=1"
				    
>>>>>>> 34870959ea607a7b186ca6391011b5c84ca36f65
