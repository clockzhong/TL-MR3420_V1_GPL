# AM_CFLAGS = -Wall -std=gnu99
lib_LTLIBRARIES = libusbip.la
libusbip_la_SOURCES = names.c names.h stub_driver.c stub_driver.h usbip.h usbip_common.c usbip_common.h vhci_driver.c vhci_driver.h
AM_CPPFLAGS = -I../include -Wall -std=gnu99 -I../../../../web_server/msglogd/
libusbip_la_LDFLAGS = -version-info 0:1:0 
libusbip_la_LDFLAGS += -L../../../../web_server/msglogd/ -lmsglog -lsysfs
EXTRA_CFLAGS = @EXTRA_CLFAGS@
libusbip_la_CFLAGS = -Wall -W -Wstrict-prototypes $(EXTRA_CLFAGS)

