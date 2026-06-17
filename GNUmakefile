.SUFFIXES:
QEMUFLAGS := -m 2G
override IMAGE_NAME := OSecond
INITRD_PATH := kernel/src/_initrd


EXTRA_QEMU_DEPS :=

AHCI_DISK1 := ahci_1.img

QEMUFLAGS += -device ich9-ahci,id=ahci

QEMUFLAGS += -drive file=$(AHCI_DISK1),format=raw,if=none,id=disk1
QEMUFLAGS += -device ide-hd,drive=disk1,bus=ahci.0

EXTRA_QEMU_DEPS += $(AHCI_DISK1)


HOST_CC := cc
HOST_CFLAGS := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=



# GDB port for remote debugging
GDB_PORT := 1234

# Common QEMU GDB flags: open a GDB stub and pause until GDB connects
QEMU_GDB_FLAGS := -s -S

.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: all-hdd
all-hdd: $(IMAGE_NAME).hdd



.PHONY: run
run: edk2-ovmf-bins $(IMAGE_NAME).iso $(EXTRA_QEMU_DEPS)
	qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf-bins/ovmf-code-x86_64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		-serial stdio \
		$(QEMUFLAGS)

.PHONY: run-hdd
run-hdd: edk2-ovmf-bins $(IMAGE_NAME).hdd $(EXTRA_QEMU_DEPS)
	qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf-bins/ovmf-code-x86_64.fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		-serial stdio \
		$(QEMUFLAGS)

.PHONY: run-bios
run-bios: $(IMAGE_NAME).iso $(EXTRA_QEMU_DEPS)
	qemu-system-x86_64 \
		-M q35 \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		-serial stdio \
		$(QEMUFLAGS)

.PHONY: run-hdd-bios
run-hdd-bios: $(IMAGE_NAME).hdd $(EXTRA_QEMU_DEPS)
	qemu-system-x86_64 \
		-M q35 \
		-hda $(IMAGE_NAME).hdd \
		-serial stdio \
		$(QEMUFLAGS)


.PHONY: run-gdb
run-gdb: edk2-ovmf-bins $(IMAGE_NAME).iso $(EXTRA_QEMU_DEPS)
	qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf-bins/ovmf-code-x86_64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		-serial stdio \
		$(QEMUFLAGS) \
		$(QEMU_GDB_FLAGS)

.PHONY: run-hdd-gdb
run-hdd-gdb: edk2-ovmf-bins $(IMAGE_NAME).hdd $(EXTRA_QEMU_DEPS)
	qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf-bins/ovmf-code-x86_64.fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		-serial stdio \
		$(QEMUFLAGS) \
		$(QEMU_GDB_FLAGS)

.PHONY: run-bios-gdb
run-bios-gdb: $(IMAGE_NAME).iso $(EXTRA_QEMU_DEPS)
	qemu-system-x86_64 \
		-M q35 \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		-serial stdio \
		$(QEMUFLAGS) \
		$(QEMU_GDB_FLAGS)

.PHONY: run-hdd-bios-gdb
run-hdd-bios-gdb: $(IMAGE_NAME).hdd $(EXTRA_QEMU_DEPS)
	qemu-system-x86_64 \
		-M q35 \
		-hda $(IMAGE_NAME).hdd \
		-serial stdio \
		$(QEMUFLAGS) \
		$(QEMU_GDB_FLAGS)

.PHONY: gdb
gdb: kernel/bin/kernel 
	gdb \
		-ex "set architecture i386:x86-64" \
		-ex "file kernel/bin/kernel" \
		-ex "target remote localhost:$(GDB_PORT)" \
		-ex "layout src"



edk2-ovmf-bins:
	curl -L https://github.com/osdev0/edk2-ovmf-stable-bins/releases/latest/download/edk2-ovmf-bins.tar.gz | gunzip | tar -xf -

limine-binary/limine:
	rm -rf limine-binary
	curl -L https://github.com/Limine-Bootloader/Limine/releases/latest/download/limine-binary.tar.gz | gunzip | tar -xf -
	$(MAKE) -C limine-binary \
		CC="$(HOST_CC)" \
		CFLAGS="$(HOST_CFLAGS)" \
		CPPFLAGS="$(HOST_CPPFLAGS)" \
		LDFLAGS="$(HOST_LDFLAGS)" \
		LIBS="$(HOST_LIBS)"

kernel/.deps-obtained:
	./kernel/get-deps

.PHONY: kernel
kernel: kernel/.deps-obtained
	$(MAKE) -C kernel



$(IMAGE_NAME).iso: limine-binary/limine kernel initrd.tar
	rm -rf iso_root/*
	mkdir -p iso_root/boot
	cp -v kernel/bin/kernel iso_root/boot/
	cp -v initrd.tar iso_root/boot/initrd.img
	mkdir -p iso_root/boot/limine
	cp -v limine.conf iso_root/boot/limine/
	mkdir -p iso_root/EFI/BOOT
	cp -v limine-binary/limine-bios.sys limine-binary/limine-bios-cd.bin limine-binary/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine-binary/BOOTX64.EFI iso_root/EFI/BOOT/
	cp -v limine-binary/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./limine-binary/limine bios-install $(IMAGE_NAME).iso
	rm -rf iso_root/*

initrd.tar: $(INITRD_PATH)
	initrd.tar: $(INITRD_PATH)
	cd $(INITRD_PATH) && tar -cf ../../../initrd.tar *

$(AHCI_DISK1):
	fallocate -l 1G $(AHCI_DISK1)



$(IMAGE_NAME).hdd: limine-binary/limine kernel
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
	PATH=$$PATH:/usr/sbin:/sbin gdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00 -m 1
	./limine-binary/limine bios-install $(IMAGE_NAME).hdd
	mformat -i $(IMAGE_NAME).hdd@@1M
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel/bin/kernel ::/boot
	mcopy -i $(IMAGE_NAME).hdd@@1M limine.conf ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/limine-bios.sys ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-binary/BOOTIA32.EFI ::/EFI/BOOT



.PHONY: clean
clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root $(IMAGE_NAME).iso $(IMAGE_NAME).hdd

.PHONY: distclean
distclean:
	$(MAKE) -C kernel distclean
	rm -rf iso_root *.iso *.hdd limine-binary edk2-ovmf-bins