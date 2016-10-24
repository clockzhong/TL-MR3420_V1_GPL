两个toolchain没有本质上的区别，build0.9.29.tar.bz2新一些，含有最新的Makefile，但无法在.29服务器上使用，mips-linux-gcc本身会提示cpu type r4000 error。因此目前的正式软件都是用较老一些的build0.9.29.pb42.tar.bz2编译。

gcc-4.3.3.tar.bz2是For新平台的toolchain，即2.6.31平台。