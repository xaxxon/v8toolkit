Next, run `fetch v8`. This will put the source for V8 in a directory called v8.  The `fetch` command is in the `depot_tools` package you cloned earlier.

You'll need to tell V8 to build with libc++ (instead of libstdc++).  To do this, set the following environment variables.
They can either be set on the command line or put in your `~/.bash_profile`.  If you put them in your `.bash_profile`, you must either start a new
shell or `source` your .bash_rc like `. ~/.bash_rc` to get the environment variables in your current shell

    export CXX="`which clang++` -std=c++11 -stdlib=libc++"
    export CC="`which clang`"
    export CPP="`which clang` -E"
    export LINK="`which clang++` -std=c++11 -stdlib=libc++"
    export CXX_host="`which clang++`"
    export CC_host="`which clang`"
    export CPP_host="`which clang` -E"
    export LINK_host="`which clang++`"
    export GYP_DEFINES="clang=1 mac_deployment_target=10.7"

If V8 is not built with libc++ in OS X, the code we write later in this guide will spew errors about `std::string` being an undefined symbol.

`Note: You can decrease the build time by around 50% by editing build/all.gyp and commenting out (with #'s) the lines about cctest.gyp and unittests.gyp`

Start the build by going into the v8 directory and running `make x64.debug` (add a `-j <num CPU cores>` to speed things up).   This will build V8 static 64-bit debug libraries.  (x64.release is also available, but the debugging messages only present in a debug build are very nice to have while developing)

To build shared libraries, add `library=shared snapshot=off` to the make command.  Snapshots aren't necessary and can get confusing while doing initial development.

Detailed build instructions are here: https://github.com/v8/v8/wiki/Building%20with%20Gyp

Once this finishes, from the v8 directory, type `cd out/x64.debug` and then run `./d8` (a javascript shell) to verify success. (ctrl-d to quit d8)

You libraries will be in `v8/out/x64.debug/*.dylib`.

