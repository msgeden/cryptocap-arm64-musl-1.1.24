cp -r /root/musl-custom/ /workspace/qemu-shared/
rm /workspace/qemu-shared/musl-custom/syslib/ld-musl-aarch64.so.1
cp /root/musl-custom/lib/libc.so /workspace/qemu-shared/musl-custom/syslib/ld-musl-aarch64.so.1
