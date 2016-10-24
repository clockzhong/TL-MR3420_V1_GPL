DRIVER_PATH = drivers/2.6.15
APP_PATH	= src

usbip_server: stub_driver usbipd

stub_driver:
	make clean 
	make -C $(DRIVER_PATH) DEBUG=n
	cp $(DRIVER_PATH)/usbip.ko .

usbipd:
	cd $(APP_PATH) && aclocal && autoheader && automake-1.9 -a -c -f && autoconf && \
	./configure CC=mips-linux-gcc --host=mips-linux --enable-shared=no --enable-static=yes --without-PACKAGE && \
	make && cp cmd/usbipd .. && cd ..

clean:
	-rm -f usbipd usbip.ko
	make -C $(DRIVER_PATH) clean
	if test -e $(APP_PATH)/Makefile; then make -C $(APP_PATH) clean; fi