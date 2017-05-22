
##### Building in Windows (incomplete)

git clone depot tools as above. - $ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

Put the new directory in your path - http://www.computerhope.com/issues/ch000549.htm

install python (2) from python.org - I used 2.7.11 -  https://www.python.org/downloads/

put python in your permanent path - it must be in your path environment variable for visual studio to use later or you will get errors in the output window in visual studio

start the bash included with depot_tools:  depot_tools\git-2.7.4-64_bin\bin/bash.exe

This next line shouldn't be needed and I don't know what it does, but if you don't use it and get errors in landmines.py, use it
> export DEPOT_TOOLS_WIN_TOOLCHAIN=0 

type >fetch v8

in v8/build there is all.sln file and that can be loaded into visual studio 2015 (I was using Update 2 when I wrote this).   It will tell you it's an older version and you need to convert it.  Just hit "ok" with all the options selected.   After it converts, build the d8 javascript shell by opening clicking "build", "build" (in the drop down), "d8".  

`NOTE: This will build with the 2013 toolchain by default.  This means you CANNOT link it with code compiled with the 2015 toolchain.  To change this, go to the Project menu, then `Properties`, `Configuration Properties`, `General`, and go to the `Platform Toolset` option and select `Visual Studio 2015 (v140)` or whatever version you want to use.  However, it probably isn't guaranteed to work in any other version of the toolset.

Also note this will build with the statically linked runtime.  As far as I understand, the runtimes of what you link to it must match.  I don't know if building with the dynamically linked runtime works or not.

In `v8/build/Debug` you should now have d8.exe.  If it isn't there, make sure you have python in your permanent PATH environment variable (following the directions in the URL above) and didn't just set it on the command line.   Visual Studio has to know where to find it.  
