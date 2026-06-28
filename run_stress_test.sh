#!/bin/bash
# Run qemu for extended time to complete STRESS test
cd /mnt/d/leos/leos

pkill -9 qemu-system-i386 2>/dev/null
sleep 2
rm -f /tmp/qemu-leos.sock serial.log

qemu-system-i386 \
    -drive format=raw,file=build/output/leos_hdd.img,if=ide \
    -boot c -m 32 \
    -serial file:/mnt/d/leos/leos/serial.log \
    -display none \
    -monitor unix:/tmp/qemu-leos.sock,server,nowait \
    -daemonize 2>/dev/null

echo "qemu started, waiting 300s for STRESS test..."
sleep 300

# Dump VGA
echo 'pmemsave 0xb8000 0xfa0 "/tmp/vga_dump.bin"' | socat - UNIX-CONNECT:/tmp/qemu-leos.sock 2>/dev/null
sleep 1
echo "quit" | socat - UNIX-CONNECT:/tmp/qemu-leos.sock 2>/dev/null

echo "=== VGA dump ==="
for row in $(seq 0 24); do
    offset=$((row * 160))
    line=""
    has_content=false
    for col in $(seq 0 79); do
        pos=$((offset + col * 2))
        ch=$(dd if=/tmp/vga_dump.bin bs=1 skip=$pos count=1 2>/dev/null | od -A n -t x1 | tr -d ' \n')
        if [ "$ch" != "00" ] && [ "$ch" != "20" ]; then
            char=$(printf "\\x$ch" 2>/dev/null)
            line="${line}${char}"
            has_content=true
        elif [ "$ch" = "20" ]; then
            line="${line} "
        else
            line="${line} "
        fi
    done
    if $has_content; then
        echo "Row $row: $line"
    fi
done

pkill -9 qemu-system-i386 2>/dev/null
echo "=== done ==="
