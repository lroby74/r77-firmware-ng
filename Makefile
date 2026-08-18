include build/main.mk
include ./app/busybox/Makefile
include ./app/uboot/Makefile
include ./lib/alsa/Makefile
include ./lib/libpng/Makefile
include ./lib/zlib/Makefile
include ./app/stella/Makefile
include ./kernel/armbian-linux/Makefile
include ./app/dumper/Makefile
include ./app/expandfs/Makefile
include ./app/fbmsg/Makefile
include ./app/dropbear/Makefile
include ./app/sftpserver/Makefile
include ./lib/libump/Makefile
include ./lib/sunxi-mali/Makefile
include ./lib/sdl2/Makefile

all:
	@echo "TOP Makefile"
	make app/busybox/compile
	make app/uboot/compile
	make lib/alsa/compile
	make lib/zlib/compile
	make lib/png/compile
	make lib/libump/compile
	make lib/sunxi-mali/compile
	make lib/sdl2/compile
	make app/dropbear/compile
	make app/sftpserver/compile
	make app/stella/compile
	make app/dumper/compile
	make app/expandfs/compile
	make lima-memtester
	make cpio
	make kernel/armbian-linux/compile
	make sdcard
clean:
	@echo "TOP Makefile clean"
	make app/busybox/clean
	make app/uboot/clean
	make lib/alsa/clean
	make lib/zlib/clean
	make lib/png/clean
	make lib/libump/clean
	make lib/sunxi-mali/clean
	make lib/sdl2/clean
	make app/dropbear/clean
	make app/sftpserver/distclean
	make app/stella/clean
	make app/dumper/clean
	make app/expandfs/clean
	make kernel/armbian-linux/clean

distclean:
	@echo "TOP Makefile distclean"
	make app/busybox/distclean
	make app/uboot/distclean
	make lib/alsa/distclean
	make lib/zlib/distclean
	make lib/png/distclean
	make lib/libump/distclean
	make lib/sunxi-mali/distclean
	make lib/sdl2/distclean
	make app/stella/distclean
	make app/dumper/distclean
	make app/expandfs/distclean
	make app/dropbear/distclean
	make app/sftpserver/distclean
	make kernel/armbian-linux/distclean
	-rm -rf $(OUTDIR)

install:
	make app/busybox/install
	make lib/alsa/install
	make lib/png/install
	make lib/zlib/install
	make lib/libump/install
	make lib/sunxi-mali/install
	make lib/sdl2/install
	make app/dropbear/install
	make app/sftpserver/install
	make app/stella/install
	make app/dumper/install
	make app/expandfs/install
	make kernel/armbian-linux/install

cpio:
	rm -rf $(OUTDIR)/rootfs.cpio.lzma
	cd $(ROOTFSDIR) && \
	find . | cpio -H newc -o --owner root:root -F ../rootfs.cpio && \
	cd .. && \
	lzma rootfs.cpio

lima-memtester:
	install -m 0755 bin/lima-memtester $(ROOTFSDIR)/bin

# Dimensione dell'immagine in MB. Deve solo contenere il payload: al primo
# avvio rcS estende la partizione a tutta la microSD (vedi app/expandfs).
SDCARD_SIZE_MB ?= 48

# La partizione parte a 1 MiB (LBA 2048): sotto ci stanno SPL e U-Boot, che
# vengono scritti a 8 KiB. Cambiare questo valore rompe expandfs.
SDCARD_PART_OFFSET := 1M

# Niente losetup/mount/sudo: la FAT si costruisce con mtools, che lavora
# sul file. Cosi' l'immagine si crea anche senza root e dentro WSL1.
MTOOLS := MTOOLS_SKIP_CHECK=1
IMG := $(OUTDIR)/sdcard.img

sdcard:
	rm -rf $(IMG)
	dd if=/dev/zero of=$(IMG) bs=1M count=$(SDCARD_SIZE_MB)
	echo '2048,,c,*' | sfdisk --label dos --no-reread --no-tell-kernel $(IMG)
	dd if=$(OUTDIR)/u-boot-sunxi-with-spl.bin of=$(IMG) bs=1k seek=8 conv=notrunc
	$(MTOOLS) mformat -i $(IMG)@@$(SDCARD_PART_OFFSET) -F -v RETRON77 ::
	# mformat lascia a zero il campo "settori nascosti" del BPB, che invece
	# deve valere il settore d'inizio della partizione (2048). Con zero li'
	# dentro Windows considera il volume da correggere e ci scrive sopra
	# appena lo monta: la scrittura resta buona, ma la verifica fallisce.
	# Si corregge il settore di avvio e la sua copia di riserva (settore 6).
	printf '\000\010\000\000' | dd of=$(IMG) bs=1 seek=1048604 count=4 conv=notrunc status=none
	printf '\000\010\000\000' | dd of=$(IMG) bs=1 seek=1051676 count=4 conv=notrunc status=none
	$(MTOOLS) mcopy -i $(IMG)@@$(SDCARD_PART_OFFSET) -s -Q $(TOPDIR)/rom/* ::
	$(MTOOLS) mcopy -i $(IMG)@@$(SDCARD_PART_OFFSET) -Q \
		$(OUTDIR)/boot.scr $(OUTDIR)/script.bin $(OUTDIR)/uImage ::
	# Gli SPL per la scelta del clock DRAM dal menu "OC settings" di Stella.
	# Il 624 e' quello appena compilato, cioe' l'impostazione normale.
	cp $(OUTDIR)/u-boot-sunxi-with-spl.bin $(OUTDIR)/uboot-624mhz.bin
	# ::/sys esiste gia': arriva da rom/sys/settings copiato qui sopra
	$(MTOOLS) mmd -i $(IMG)@@$(SDCARD_PART_OFFSET) ::/sys/uboot
	$(MTOOLS) mcopy -i $(IMG)@@$(SDCARD_PART_OFFSET) -Q \
		$(OUTDIR)/uboot-624mhz.bin bin/uboot-408mhz.bin \
		bin/uboot-480mhz.bin bin/uboot-504mhz.bin \
		bin/uboot-648mhz.bin bin/uboot-672mhz.bin \
		bin/uboot-696mhz.bin bin/uboot-720mhz.bin ::/sys/uboot/
	$(MTOOLS) mdir -i $(IMG)@@$(SDCARD_PART_OFFSET) ::
	sync
	for i in 408 480 504; do \
		cp $(IMG) $(OUTDIR)/sdcard-$${i}mhz.bin; \
		dd if=bin/uboot-$${i}mhz.bin of=$(OUTDIR)/sdcard-$${i}mhz.bin bs=1k seek=8 conv=notrunc; \
	done
