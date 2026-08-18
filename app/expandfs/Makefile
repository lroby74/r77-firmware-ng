expandfs:
	@echo "expandfs config"

app/expandfs/compile: app/expandfs/expandfs.c
	@echo "expandfs compile"
	cd app/expandfs && \
	$(GCC) -o expandfs expandfs.c $(CFLAGS)
	make app/expandfs/install

app/expandfs/install:
	@echo "expandfs install"
	@cp app/expandfs/expandfs $(ROOTFSDIR)/usr/bin
	@$(STRIP) $(ROOTFSDIR)/usr/bin/expandfs

app/expandfs/clean:
	@echo "expandfs clean"
	@rm -rf app/expandfs/*.o app/expandfs/expandfs

app/expandfs/distclean:
	@echo "expandfs distclean"
	-rm -rf app/expandfs/*.o app/expandfs/expandfs
