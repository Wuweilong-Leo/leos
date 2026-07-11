#!/bin/bash
pkill -9 qemu-system-i386 2>/dev/null
sleep 2
cd /mnt/d/leos/leos/build/output
rm -f /tmp/qemu-leos.sock /tmp/vga_dump.bin

qemu-system-i386 -drive format=raw,file=leos_hdd.img,if=ide -boot c -m 32 \
  -display none \
  -monitor unix:/tmp/qemu-leos.sock,server,nowait \
  -daemonize 2>/dev/null

sleep 8

echo 'pmemsave 0xb8000 0xfa0 /tmp/vga_dump.bin' | socat - UNIX-CONNECT:/tmp/qemu-leos.sock 2>/dev/null
sleep 2
echo 'quit' | socat - UNIX-CONNECT:/tmp/qemu-leos.sock 2>/dev/null
sleep 1

if [ -f /tmp/vga_dump.bin ]; then
  echo "=== VGA hex dump (first 5 rows) ==="
  xxd /tmp/vga_dump.bin | head -60
  echo ""
  echo "=== Non-empty rows as text ==="
  for row in $(seq 0 24); do
    offset=$((row * 160))
    line=""
    has_content=false
    for col in $(seq 0 79); do
      pos=$((offset + col * 2))
      ch=$(dd if=/tmp/vga_dump.bin bs=1 skip=$pos count=1 2>/dev/null | xxd -p | tr -d ' \n')
      if [ "$ch" != "00" ] && [ "$ch" != "20" ] && [ -n "$ch" ]; then
        line="${line}[${ch}]"
        has_content=true
      elif [ "$ch" = "20" ]; then
        line="${line} "
      else
        line="${line}."
      fi
    done
    if $has_content; then
      echo "Row $row: $line"
    fi
  done
else
  echo "VGA dump file not found"
fi

pkill -9 qemu-system-i386 2>/dev/null
