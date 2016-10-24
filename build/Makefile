ifndef BOARD_TYPE
$(error "You have to define Board Type to use this Makefile")
#export BOARD_TYPE=ap81fus
#export BOARD_TYPE=ap93fus
endif

#
# board related targets
#
include Makefile.$(BOARD_TYPE)

export WEBDIR=$(TOPDIR)/webs/web_pages/

######## build fakeroot host tool ########
fakeroot_build:
	cd $(TOPDIR)/util/fakeroot && make -f make.fakeroot fakeroot
	
######## prepare toolchain ########
toolchain_prep:
	@echo Uncompressing toolchain, please wait for a few minutes ...
	cd $(TOPDIR)/toolchain && \
	tar -jxf build0.9.29.pb42.tar.bz2 && \
	tar -jxf gcc-4.3.3.tar.bz2 && \
	mv ./build/gcc-3.4.4-2.16.1 $(TOPDIR)/build && \
	rm -rf $(TOPDIR)/toolchain/build && \
	mv gcc-4.3.3 $(TOPDIR)/build

#
#  CONFIG setting
#

# modules used by all boards
export BOARD_KERNEL_MODULES = stats netfilter wlan_warn netsched br_filter ts_kmp pppol2tp pptp_module

# modules specified by special board type
ifeq ($(BOARD_TYPE),ap99)
BOARD_KERNEL_MODULES += usb_serial
endif

ifeq ($(BOARD_TYPE),ap99tpl)
BOARD_KERNEL_MODULES += usb_serial
endif

ifeq ($(CONFIG_USB_SERIAL_MODULE),y)
BOARD_KERNEL_MODULES += usb_serial
endif

# applications used by all board types 
BOARD_APPLICATIONS = busybox pppoe l2tp bpa iptables tc_build lltd net_ioctl arp libssl

# applications specified by special board
ifeq ($(BOARD_TYPE),ap83fus)
BOARD_APPLICATIONS += nas ftpserver ushare
endif
ifeq ($(BOARD_TYPE),pb92)
BOARD_APPLICATIONS += nas
endif 

ifeq ($(BOARD_TYPE),ap99)
#BOARD_APPLICATIONS += nas ftpserver ushare
endif

#ifeq ($(BOARD_TYPE),ap99tpl)
#CONFIG_3G_TOOLS=y
#BOARD_APPLICATIONS += 3g_tools
#endif

#
# common build
#

fs_prep:install_root_clean util_chmod check_tftp libs_copy etc_prep empty_dir_prep

install_root_clean:
	-rm -rf $(INSTALL_ROOT)/*
	
util_chmod:
	-cd $(TOPDIR)/util && chmod 777 cplibs md5_des mkdev mksquashfs pad
	-chmod 777 $(TOPDIR)/webs/web_pages/utils/getini/gather
	-chmod 777 $(TOPDIR)/webs/web_pages/utils/mkrc/mkrc
	-chmod 777 $(TOPDIR)/webs/web_pages/utils/mkrc/rmtag
	-chmod 777 $(TOPDIR)/webs/web_pages/utils/web_collector/webc
	
check_tftp:
	mkdir -p $(IMAGEPATH)
	if test -d $(TFTPPATH); then echo $(TFTPPATH) exists; else mkdir -p $(TFTPPATH); fi;

libs_copy:
	$(TOPDIR)/util/cplibs
	
dev_create:
	rm -rf $(INSTALL_ROOT)/dev/*
	$(TOPDIR)/util/mkdev

etc_prep:
	rm -rf $(INSTALL_ROOT)/etc
	cp -r $(TOPDIR)/filesystem/ap93fus/etc $(INSTALL_ROOT)/

empty_dir_prep:
	if test -d $(INSTALL_ROOT)/mnt; then echo $(INSTALL_ROOT)/mnt exists; else mkdir $(INSTALL_ROOT)/mnt; fi;
	if test -d $(INSTALL_ROOT)/proc; then echo $(INSTALL_ROOT)/proc exists; else mkdir $(INSTALL_ROOT)/proc; fi;
	if test -d $(INSTALL_ROOT)/root; then echo $(INSTALL_ROOT)/root exists; else mkdir $(INSTALL_ROOT)/root; fi;
	if test -d $(INSTALL_ROOT)/tmp; then echo $(INSTALL_ROOT)/mnt exists; else mkdir $(INSTALL_ROOT)/tmp; fi;
	if test -d $(INSTALL_ROOT)/var; then echo $(INSTALL_ROOT)/var exists; else mkdir -p $(INSTALL_ROOT)/var/run; fi;
	if test -d $(INSTALL_ROOT)/sbin; then echo $(INSTALL_ROOT)/sbin exists; else mkdir -p $(INSTALL_ROOT)/sbin; fi;
	
#
# kernel modules
#

KERNEL_MODULE_PATH=$(TOPDIR)/images/$(BOARD_TYPE)/kernel_modules/

kernel_modules: kernel_modules_prep $(BOARD_KERNEL_MODULES) 

kernel_modules_prep:
	test -d $(KERNEL_MODULE_PATH) || mkdir -p $(KERNEL_MODULE_PATH)

kernel_modules_install:
	mkdir -p $(INSTALL_ROOT_FOR_BOARD)/lib/modules/$(KERNELVER)/kernel/
	cp -Rf $(KERNEL_MODULE_PATH)/*.ko $(INSTALL_ROOT_FOR_BOARD)/lib/modules/$(KERNELVER)/kernel/
	
stats:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/stats modules
	cp $(TOPDIR)/kernel_modules/stats/statistics.ko $(KERNEL_MODULE_PATH)

netfilter:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=net/ipv4/netfilter modules	
	cp $(KERNELPATH)/net/ipv4/netfilter/*.ko $(KERNEL_MODULE_PATH)

wlan_warn:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/wlan_warn modules
	cp $(TOPDIR)/kernel_modules/wlan_warn/wlan_warn.ko $(KERNEL_MODULE_PATH)

netsched:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=net/sched modules	
	cp $(KERNELPATH)/net/sched/*.ko $(KERNEL_MODULE_PATH)

ts_kmp:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=lib modules	
	cp $(KERNELPATH)/lib/ts_kmp.ko $(KERNEL_MODULE_PATH)
	
br_filter:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/br_hook modules
	cp $(TOPDIR)/kernel_modules/br_hook/br_filter.ko $(KERNEL_MODULE_PATH)
	
flashid:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/flash_id modules
	cp $(TOPDIR)/kernel_modules/flash_id/flashid.ko $(KERNEL_MODULE_PATH)
	
antind:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/anti_nat_detect modules
	#cp $(TOPDIR)/kernel_modules/anti_nat_detect/ipt_ANTIND.ko $(KERNEL_MODULE_PATH)
	cp $(TOPDIR)/kernel_modules/anti_nat_detect/ipt_ANTIND.ko /tftpboot/tftp/
	
gpio:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/gpio modules
	cp $(TOPDIR)/kernel_modules/gpio/gpio.ko $(KERNEL_MODULE_PATH)
	
cone_udp:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/cone_udp modules
	cp $(TOPDIR)/kernel_modules/cone_udp/cone_udp.ko $(KERNEL_MODULE_PATH) 
	cp $(TOPDIR)/kernel_modules/cone_udp/cone_udp.ko /tftpboot/tftp/
	
starcraft_helper:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/starcraft_helper modules
	cp $(TOPDIR)/kernel_modules/starcraft_helper/ip_conntrack_helper_starcraft.ko /tftpboot/tftp/

harmony:
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/harmony modules
	
pppol2tp:	
	$(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/pppol2tp-kmod-0.17 modules
	cp $(TOPDIR)/kernel_modules/pppol2tp-kmod-0.17/*.ko $(KERNEL_MODULE_PATH)

pptp_module:
	 $(MAKEARCH) -C $(KERNELPATH) SUBDIRS=$(TOPDIR)/kernel_modules/pptp_driver-0.8.3 modules
	cp $(TOPDIR)/kernel_modules/pptp_driver-0.8.3/*.ko $(KERNEL_MODULE_PATH)
#
# applications
#

apps_build: $(BOARD_APPLICATIONS)


libffmpeg: # easy build for libffmpeg 
	cd $(TOPDIR)/apps/nas/media_server/ffmpeg-0.6 && \
	if test -e config.mak; then make distclean; fi ;
	cd $(TOPDIR)/apps/nas/media_server/ffmpeg-0.6 && \
	./configure --prefix=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE) \
				--libdir=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE)/lib \
				--enable-static --disable-ffmpeg --disable-ffserver \
				--disable-ffplay --disable-ffprobe \
				--disable-zlib --disable-bzlib \
				--enable-cross-compile --target-os=linux --arch=mips;
	cd $(TOPDIR)/apps/nas/media_server/ffmpeg-0.6 &&\
	$(MAKE) CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar RANLIB=$(TOOLPREFIX)ranlib &&\
	$(MAKE) CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar RANLIB=$(TOOLPREFIX)ranlib install

libdlna: libffmpeg # easy build for libdlna
	cd $(TOPDIR)/apps/nas/media_server/libdlna-0.2.3 && \
	if test -e config.mak; then make distclean; fi ;
	cp -rf $(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE)/include/* $(TOPDIR)/build/$(TOOLCHAIN)/build_mips/include/ ;	
	cd $(TOPDIR)/apps/nas/media_server/libdlna-0.2.3 &&\
	export CC=$(TOOLPREFIX)gcc &&\
	export LD=$(TOOLPREFIX)ld &&\
	export AR=$(TOOLPREFIX)ar &&\
	export RANLIB=$(TOOLPREFIX)ranlib && \
	./configure --libdir=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE)/lib \
				--prefix=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE) \
				--includedir=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE)/include \
				--disable-shared --cross-compile && \
	make CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar RANLIB=$(TOOLPREFIX)ranlib install
	
libupnp: libdlna # easy build for libupnp
	cd $(TOPDIR)/apps/nas/media_server/libupnp-1.6.6 &&\
	if test -e Makefile; then make distclean; fi &&\
	./configure --prefix=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE) \
				--libexecdir=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE) \
				--libdir=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE)/lib \
				--build=i386 --host=mips-linux && \
	make CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar RANLIB=$(TOOLPREFIX)ranlib install

ushare: libupnp # easy build for ushare 
	cp -rf $(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE)/include/* $(TOPDIR)/build/$(TOOLCHAIN)/build_mips/include/ ;	
	cd $(TOPDIR)/apps/nas/media_server/ushare-1.1a &&\
	if test -e config.mak; then make distclean; fi ;
	cd $(TOPDIR)/apps/nas/media_server/ushare-1.1a &&\
	export CC=$(TOOLPREFIX)gcc &&\
	export LD=$(TOOLPREFIX)ld &&\
	export AR=$(TOOLPREFIX)ar &&\
	export RANLIB=$(TOOLPREFIX)ranlib && \
	export PKG_CONFIG_PATH=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE)/lib/pkgconfig &&\
	./configure --prefix=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE) \
				--with-libupnp-dir=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE)/lib \
				--with-libdlna-dir=$(TOPDIR)/apps/nas/media_server/$(BOARD_TYPE)/lib \
				--cross-compile --enable-dlna --disable-nls && \
	make CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar RANLIB=$(TOOLPREFIX)ranlib install;

busybox: # easy build for busybox, no clean
	cd $(TOPDIR)/apps/$(BUSYBOX); $(MAKE) PREFIX=$(INSTALL_ROOT) uninstall; \
	find . -name "*.o" | xargs rm -rf; \
	$(MAKE); \
	$(MAKE) PREFIX=$(INSTALL_ROOT) install
	
pppoe:
	cd $(TOPDIR)/apps/ppp-2.4.3/pppd && make clean && make CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar && \
	cp pppd $(INSTALL_ROOT)/usr/sbin

pptp:
	cd $(TOPDIR)/apps/pptp-1.7.1 && make clean && make CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar && \
	cp pptp $(INSTALL_ROOT)/usr/sbin

l2tp:
	cd $(TOPDIR)/apps/xl2tpd-1.1.12 && make clean &&  make CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar && \
	cp xl2tpd $(INSTALL_ROOT)/usr/sbin

bpa:
	cd $(TOPDIR)/apps/bpalogin-2.0.2 && make clean &&  make CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar && \
	cp bpalogin $(INSTALL_ROOT)/usr/sbin
	
iptables:
	@echo $(KERNELVER)
	if [ $(KERNELVER) == 2.6.15 ] ; \
	then \
	cd $(TOPDIR)/apps/iptables-1.3.7/ && \
	make clean && \
	$(MAKE) CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld DO_IPV6=0 KERNEL_DIR=$(KERNELPATH) LIBDIR=/lib && \
	cp iptables $(INSTALL_ROOT)/sbin/ && \
	cd extensions && \
	test -d $(INSTALL_ROOT)/lib/iptables/ || mkdir -p $(INSTALL_ROOT)/lib/iptables/ && \
	mv -f libipt_DNAT.so libipt_MASQUERADE.so libipt_SNAT.so libipt_TCPMSS.so libipt_TRIGGER.so \
	libipt_comment.so libipt_icmp.so libipt_iprange.so libipt_mac.so \
	libipt_multiport.so libipt_standard.so libipt_state.so libipt_string.so \
	libipt_tcp.so libipt_time.so libipt_udp.so libipt_MARK.so libipt_multiurl.so libipt_wlanwarn.so $(INSTALL_ROOT)/lib/iptables/ ; \
	elif [ $(KERNELVER) == 2.6.31 ] ; \
	then \
	cd $(TOPDIR)/apps/iptables-1.4.5 && \
	if test -e Makefile ; then make distclean ; fi && \
	./configure --prefix=$(INSTALL_ROOT) \
	--libexecdir=$(INSTALL_ROOT)/lib \
	--build=i386 --host=mips-linux  \
	--with-ksource=$(KERNELPATH) \
	--disable-devel --disable-libipq \
	--disable-ipv6 && \
	make CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar RANLIB=$(TOOLPREFIX)ranlib && \
	make CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar RANLIB=$(TOOLPREFIX)ranlib install ; fi
	

tc_build:
	cd $(TOPDIR)/apps/iproute2-$(KERNELVER) && make CC=$(TOOLPREFIX)gcc && \
	cp $(TOPDIR)/apps/iproute2-$(KERNELVER)/tc/tc $(INSTALL_ROOT)/sbin
	
lltd:
	@echo ********** making WinRally ************
	cd $(TOPDIR)/apps/lltd/wr841n-linux && \
	make clean && $(MAKE) CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)gcc && \
	cp lld2d $(INSTALL_ROOT)/usr/bin/

net_ioctl:
	cd $(TOPDIR)/debug_tools/net_ioctl && \
	cp net_ioctl $(INSTALL_ROOT)/usr/
	
arp:
	cd $(TOPDIR)/apps/arp && \
	$(TOOLPREFIX)gcc -o arp arp.c && cp arp $(INSTALL_ROOT)/usr/
	
ftpserver:
	cd $(TOPDIR)/apps/vsftpd-2.3.2 && \
	$(MAKE) clean && $(MAKE) vsftpd CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld AR=$(TOOLPREFIX)ar && \
	cp vsftpd $(IMAGEPATH)

libssl:
	cd $(TOPDIR)/apps/openssl-0.9.8a && \
	make clean && make && \
	cp libcrypto.so.0.9.8 $(INSTALL_ROOT)/lib && \
	cp libssl.so.0.9.8 $(INSTALL_ROOT)/lib

web_server:
	echo -n "BUILD_TIME = " > $(TOPDIR)/web_server/sys/buildtime.h;
	expr `date +%H` \* 3600 + `date +%M` \* 60 + `date +%S` >> $(TOPDIR)/web_server/sys/buildtime.h;
	echo -n "BUILD_DATE = " >> $(TOPDIR)/web_server/sys/buildtime.h;
	@date +%D >> $(TOPDIR)/web_server/sys/buildtime.h;

	if test $(clean) -eq 1; then \
	cd $(TOPDIR)/web_server && 	make clean; \
	fi;
	
	cd $(TOPDIR)/web_server && $(MAKE) CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld && \
	ls -l httpd && $(STRIP) httpd && ls -l httpd && \
	cp httpd $(IMAGEPATH) && cp httpd $(TFTPPATH)
	
httpd_build:
	cd $(TOPDIR)/web_server && cp httpd $(TOPDIR)/images/ap99/

	
httpd:
	cd $(TOPDIR)/WR841N && $(MAKE) CC=$(TOOLPREFIX)gcc LD=$(TOOLPREFIX)ld
	
#
# utils
#

make_flash:
	cd $(TOPDIR)/util/make_flash_src/ && make && \
	cp make_flash $(TOPDIR)/util/

md5_des:
	cd $(TOPDIR)/util/md5_des_src/ && gcc main.c md5_des.c -o md5_des && \
	cp md5_des $(TOPDIR)/util/
		
#
# fs and image
#
strip_bin:
	$(STRIP) $(INSTALL_ROOT_FOR_BOARD)/bin/*
	-$(STRIP) $(INSTALL_ROOT_FOR_BOARD)/sbin/*
	$(STRIP) $(INSTALL_ROOT_FOR_BOARD)/usr/bin/*
	$(STRIP) $(INSTALL_ROOT_FOR_BOARD)/usr/sbin/*
	
strip_all:
	cd $(INSTALL_ROOT_FOR_BOARD)/bin/ && \
	ls -l busybox && $(TOOLPREFIX)strip busybox && ls -l busybox 
	
	cd $(INSTALL_ROOT_FOR_BOARD)/sbin/ && \
	ls -l hostapd && $(TOOLPREFIX)strip hostapd && ls -l hostapd && \
	ls -l tc && $(TOOLPREFIX)strip tc && ls -l tc && \
	ls -l iptables && $(TOOLPREFIX)strip iptables && ls -l iptables && \
	ls -l wlanconfig && $(TOOLPREFIX)strip wlanconfig && ls -l wlanconfig && \
	ls -l iwlist && $(TOOLPREFIX)strip iwlist && ls -l iwlist && \
	ls -l wpatalk && $(TOOLPREFIX)strip wpatalk && ls -l wpatalk && \
	ls -l iwconfig && $(TOOLPREFIX)strip iwconfig && ls -l iwconfig && \
	ls -l apstart && $(TOOLPREFIX)strip apstart && ls -l apstart && \
	ls -l iwevent && $(TOOLPREFIX)strip iwevent && ls -l iwevent && \
	ls -l iwpriv && $(TOOLPREFIX)strip iwpriv && ls -l iwpriv && \
	ls -l iwgetid && $(TOOLPREFIX)strip iwgetid && ls -l iwgetid && \
	ls -l iwspy && $(TOOLPREFIX)strip iwspy && ls -l iwspy 
	
	cd $(INSTALL_ROOT)/usr/bin/ && \
	ls -l httpd && $(TOOLPREFIX)strip httpd && ls -l httpd && \
	ls -l lld2d && $(TOOLPREFIX)strip lld2d && ls -l lld2d
	
	cd $(INSTALL_ROOT)/usr/sbin/ && \
	ls -l bpalogin && $(TOOLPREFIX)strip bpalogin && ls -l bpalogin && \
	ls -l pppd && $(TOOLPREFIX)strip pppd && ls -l pppd && \
	ls -l pptp && $(TOOLPREFIX)strip pptp && ls -l pptp && \
	ls -l xl2tpd && $(TOOLPREFIX)strip xl2tpd && ls -l xl2tpd
	
ifndef lzma
encode = -nolzma
endif

apxxfus_fs_prep:
	rm -Rf $(INSTALL_ROOT_FOR_BOARD)
	cp -Rf $(INSTALL_ROOT) $(INSTALL_ROOT_FOR_BOARD)
	make kernel_modules_install	
	make wireless_install
ifeq ($(CONFIG_3G_TOOLS),y)
	#make 3g_tools_install
endif
	cp $(IMAGEPATH)/httpd $(INSTALL_ROOT_FOR_BOARD)/usr/bin/
	make strip_bin
	
ifeq ($(CONFIG_3G_TOOLS),y)
	cd $(WEBDIR)/utils/web_collector/ && ./webc -s $(WEBDIR)/$(lang)/common:$(WEBDIR)/$(lang)/common_3g:$(WEBDIR)/$(lang)/oem/$(oem_dir) -d $(INSTALL_ROOT_FOR_BOARD)/web
else
	cd $(WEBDIR)/utils/web_collector/ && ./webc -s $(WEBDIR)/$(lang)/common:$(WEBDIR)/$(lang)/oem/$(oem_dir) -d $(INSTALL_ROOT_FOR_BOARD)/web
endif
	test -d $(INSTALL_ROOT_FOR_BOARD)/web/oem/ || mkdir -p $(INSTALL_ROOT_FOR_BOARD)/web/oem/
	cp $(TOPDIR)/webs/models/$(conf_file) $(TOPDIR)/webs/models/model_tmp.conf
	
mk_squashfs_build: fakeroot_build
	echo "" >> $(TOPDIR)/webs/models/model_tmp.conf
	cat $(TOPDIR)/webs/models/version.conf >> $(TOPDIR)/webs/models/model_tmp.conf
	echo "" >> $(TOPDIR)/webs/models/model_tmp.conf
	cat $(TOPDIR)/web_server/sys/buildtime.h >> $(TOPDIR)/webs/models/model_tmp.conf
	$(TOPDIR)/util/md5_des $(TOPDIR)/webs/models/model_tmp.conf $(INSTALL_ROOT_FOR_BOARD)/web/oem/model.conf enc
	find $(INSTALL_ROOT_FOR_BOARD) -name ".svn" | xargs rm -rf
	#if [[ $(BOARD_TYPE) == ap91fus || $(BOARD_TYPE) == ap93fus ]]; then $(TOPDIR)/util/fakeroot/fakeroot/fakeroot $(TOPDIR)/util/buildFS_LZ; else $(TOPDIR)/util/fakeroot/fakeroot/fakeroot $(TOPDIR)/util/buildFS; fi;
	$(TOPDIR)/util/fakeroot/fakeroot/fakeroot $(TOPDIR)/util/buildFS_LZ
	#$(TOPDIR)/util/mksquashfs $(INSTALL_ROOT_FOR_BOARD) $(IMAGEPATH)/$(BOARD_TYPE)-squashfs -noappend -be -always-use-fragments -nolzma
	ls -l $(IMAGEPATH)/$(BOARD_TYPE)-squashfs	
	$(TOPDIR)/util/pad -i $(IMAGEPATH)/$(BOARD_TYPE)-squashfs -p $(fs_size)
	cp $(IMAGEPATH)/$(BOARD_TYPE)-squashfs $(TFTPPATH)
	ls -l $(TFTPPATH)/$(BOARD_TYPE)-squashfs
	
squashfs_build:	
	$(TOPDIR)/util/mksquashfs $(INSTALL_ROOT) $(IMAGEPATH)/$(BOARD_TYPE)-squashfs -noappend -be -always-use-fragments $(encode)
	ls -l $(IMAGEPATH)/$(BOARD_TYPE)-squashfs
	$(TOPDIR)/util/pad -i $(IMAGEPATH)/$(BOARD_TYPE)-squashfs -p 0x2C0000
	cp $(IMAGEPATH)/$(BOARD_TYPE)-squashfs $(TFTPPATH)
	cp $(IMAGEPATH)/$(BOARD_TYPE)-squashfs $(TOPDIR)/make_flash
	ls -l $(TFTPPATH)/$(BOARD_TYPE)-squashfs
	
image_build:
	cd $(IMAGEPATH)/ && \
	$(TOPDIR)/util/make_flash -b u-boot.bin -k vmlinux.bin.gz -f $(BOARD_TYPE)-squashfs \
	-o $(prefix) -c $(TOPDIR)/webs/models/model_tmp.conf -t $(TOPDIR)/web_server/sys/buildtime.h && \
	chmod 777 *

linux_build_all: uboot kernel_build kernel_modules wireless apps_build
