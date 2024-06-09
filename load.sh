dd if=./build/output/os_mbr.bin of=/os_i386/bochs/hd60M.img bs=512 count=1 conv=notrunc
dd if=./build/output/os_loader.bin of=/os_i386/bochs/hd60M.img bs=512 count=8 seek=2 conv=notrunc
dd if=./build/output/kernel.bin of=/os_i386/bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc