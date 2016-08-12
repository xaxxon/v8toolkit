content goes here


Very similar to os x.


Depending on your version of V8, you may run into this:

https://bbs.archlinux.org/viewtopic.php?id=209871

Solution is to just symlink in your global ld.gold something like this:

~/v8$ ln -is `which ld.gold`  third_party/binutils/Linux_x64/Release/bin/ld.gold 


