#
# 3G tools Makefile
# this file is wrote based on yangxv's 3G tools
# Attention:
#     chat-2.4.3 need link libmsglog.so
#     handle_card and usb-modeswitch-1.1.3 depend on libusb-0.1.12 because they need libusb.so and usb.h     
# created by tiger 20091202

CURRENTDIR = $(3G_TOOLS_PATH)
LIBUSBMIPS = install_libusb_mips

.PHONY: all
all: chat_build usb_modeswitch handlecard_build 3g_conifg_build modemscan_build

3g_tools_prep:
	test -d $(CURRENTDIR)/$(LIBUSBMIPS) || mkdir -p $(CURRENTDIR)/$(LIBUSBMIPS) 

libusb_build: 3g_tools_prep
	cd $(CURRENTDIR)/libusb-0.1.12 && \
	if [ ! \( \( -e Makefile \) -a \( -e $(CURRENTDIR)/$(LIBUSBMIPS)/lib/libusb.so \) \) ]; then \
	./configure --prefix=$(CURRENTDIR)/$(LIBUSBMIPS) --build=i686-linux --host=mips-linux CC=$(TOOLPREFIX)gcc CXX=$(TOOLPREFIX)g++ AR=$(TOOLPREFIX)ar && \
	$(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld && \
	$(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld install ; \
	fi 
	cp $(CURRENTDIR)/$(LIBUSBMIPS)/lib/libusb-0.1.so.4.4.4 $(3G_PACKAGE_PATH)

chat_build:
	cd $(CURRENTDIR)/chat-2.4.3 && \
	$(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld TP_WEB_LOG=y CFLAGS+="-I$(TOPDIR)/web_server/msglogd" LDFLAGS+="-L$(INSTALL_ROOT)/lib" && \
	cp -f chat $(3G_PACKAGE_PATH)/

usb_modeswitch:libusb_build
	cd $(CURRENTDIR)/usb_modeswitch-1.1.3 && \
	$(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld DSTDIR=$(3G_PACKAGE_PATH) install

handlecard_build:libusb_build
	cd $(CURRENTDIR)/handle_card && \
	#$(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld && \
	cp -f handle_card $(3G_PACKAGE_PATH)/

modemscan_build:
	cd $(CURRENTDIR)/modem_scan && \
	#$(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld && \
	cp -f modem_scan $(3G_PACKAGE_PATH)/

3g_conifg_build:
	-rm -rf $(3G_PACKAGE_PATH)/etc/ppp
	cd $(CURRENTDIR) && \
	cp -rf $(CURRENTDIR)/ppp $(3G_PACKAGE_PATH)/etc && \
	cp mode_switch.conf $(3G_PACKAGE_PATH)/ && \
	cp hotplug $(3G_PACKAGE_PATH)/
	
3g_tools_clean:
	-rm -rf $(CURRENTDIR)/$(LIBUSBMIPS) 
	cd $(CURRENTDIR)/chat-2.4.3 && $(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld clean
	cd $(CURRENTDIR)/libusb-0.1.12 &&  \
	if [ -e Makefile ]; then \
	$(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld distclean; \
	fi  
	cd $(CURRENTDIR)/usb_modeswitch-1.1.3 && $(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld clean
	cd $(CURRENTDIR)/handle_card && $(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld clean
	cd $(CURRENTDIR)/modem_scan && $(MAKE) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld clean
