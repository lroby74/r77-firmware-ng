fbmsg:
	@echo "fbmsg config"

app/fbmsg/compile: app/fbmsg/fbmsg.c app/fbmsg/font8x8.h
	@echo "fbmsg compile"
	cd app/fbmsg && \
	$(GCC) -o fbmsg fbmsg.c $(CFLAGS)
	make app/fbmsg/install

app/fbmsg/install:
	@echo "fbmsg install"
	@cp app/fbmsg/fbmsg $(ROOTFSDIR)/usr/bin
	@$(STRIP) $(ROOTFSDIR)/usr/bin/fbmsg

app/fbmsg/clean:
	@echo "fbmsg clean"
	@rm -rf app/fbmsg/*.o app/fbmsg/fbmsg

app/fbmsg/distclean:
	@echo "fbmsg distclean"
	-rm -rf app/fbmsg/*.o app/fbmsg/fbmsg
