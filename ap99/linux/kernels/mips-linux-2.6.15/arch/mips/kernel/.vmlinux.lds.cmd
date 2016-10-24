cmd_arch/mips/kernel/vmlinux.lds := mips-linux-uclibc-gcc -E -Wp,-MD,arch/mips/kernel/.vmlinux.lds.d  -nostdinc -isystem /home/project/svn/TL-MR3420_V1_GPL/build/gcc-3.4.4-2.16.1/build_mips/bin/../lib/gcc/mips-linux-uclibc/3.4.4/include -D__KERNEL__ -Iinclude  -include include/linux/autoconf.h  -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -Os     -fomit-frame-pointer  -I /home/project/svn/TL-MR3420_V1_GPL/ap99/linux/kernels/mips-linux-2.6.15/include/asm/gcc -G 0 -mno-abicalls -fno-pic -pipe  -mabi=32 -march=mips32r2 -Wa,-32 -Wa,-march=mips32r2 -Wa,-mips32r2 -Wa,--trap -Iinclude/asm-mips/mach-ar7240 -Iinclude/asm-mips/mach-generic -D"LOADADDR=0xffffffff80002000" -D"JIFFIES=jiffies_64 + 4" -D"DATAOFFSET=0" -P -C -Umips -D__ASSEMBLY__ -o arch/mips/kernel/vmlinux.lds arch/mips/kernel/vmlinux.lds.S

deps_arch/mips/kernel/vmlinux.lds := \
  arch/mips/kernel/vmlinux.lds.S \
    $(wildcard include/config/boot/elf64.h) \
    $(wildcard include/config/mapped/kernel.h) \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/asm-generic/vmlinux.lds.h \

arch/mips/kernel/vmlinux.lds: $(deps_arch/mips/kernel/vmlinux.lds)

$(deps_arch/mips/kernel/vmlinux.lds):
