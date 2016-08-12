content goes here



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
				    