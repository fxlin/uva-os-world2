# changelog: 1/24/25 fixed qemu_full/min/small, so that "small" will still enable graphics

p1-gen-hash-ports() {
    if [ "${USER}" = "student" ]; then
        export MYGDBPORT=53089  # special rule, for ease deployment of the VM image
    else
        export MYGDBPORT=$(echo -n ${USER} | md5sum | cut -c1-8 | while read hex; do
            decimal=$((0x$hex & 0xffffffff))  # Convert to decimal and ensure 32-bit
            echo $((50000 + (decimal % (65536 - 50000 + 1))))  # Map to range [50000, 65536]
        done)
    fi
    echo "set gdb port: ${MYGDBPORT}"
}

p1-gen-hash-ports

echo "Listen at port: ${MYGDBPORT}"
echo "**To terminate QEMU, press Ctrl-a then x"
echo 
echo "  Next: in a separate window, launch gdb, e.g. (replace the path with your own)"
echo "      gdb-multiarch kernel/build-rpi3qemu/kernel8.elf "
echo 
echo "  Example gdb commands -- "
echo "      (gdb) file kernel/build/kernel8.elf"
echo "      (gdb) target remote :${MYGDBPORT}"
echo "      (gdb) layout asm"
echo "  To avoid typing every time, have a custom ~/.gdbinit "
echo "	Details: https://fxlin.github.io/p1-kernel/gdb/"
echo " ------------------------------------------------"

# must do this for Linux + VSCode
# https://github.com/ros2/ros2/issues/1406
unset GTK_PATH

# qemu v5.2, used by cs4414 Sp24 --- too old
# QEMU5=/cs4414-shared/qemu/aarch64-softmmu/qemu-system-aarch64

# qemu6, default installed on Ubuntu 2204
QEMU6=qemu-system-aarch64

# qemu8, apr 2024 (incomplete build under wsl? no graphics?? to fix (Apr 2024)
#QEMU8=~/qemu-8.2-apr2024/build/qemu-system-aarch64   

# sp25, containing our own fix
QEMU9="/home/student/qemu-9.1.1/build/qemu-system-aarch64"

if [ -x "${QEMU9}" ]; then
    QEMU=${QEMU9}
else
    QEMU=${QEMU6}   # default 
fi

echo "Using QEMU: ${QEMU}"

#########################################

KERNEL=./kernel/kernel8-rpi3qemu.img

# has grahpics, has usb kb, has sd
qemu_full() {
    ${QEMU} -M raspi3b \
    -kernel ${KERNEL} -serial null -serial mon:stdio \
    -d int -D qemu.log \
    -nographic \
    -usb -device usb-kbd \
    -drive file=smallfat.bin,if=sd,format=raw \
    -gdb tcp::${MYGDBPORT} -S
}

# has graphics, no kb, no sd
qemu_small() {
    ${QEMU} -M raspi3b \
    -kernel ${KERNEL} -serial null -serial mon:stdio \
    -d int -D qemu.log \
    -gdb tcp::${MYGDBPORT} -S
}

# no graphics, no kb, no sd
qemu_min () {
    ${QEMU} -M raspi3b \
    -kernel ${KERNEL} -serial null -serial mon:stdio \
    -nographic \
    -d int -D qemu.log \
    -gdb tcp::${MYGDBPORT} -S
}

if [ "$1" = "min" ]
then
    qemu_min
elif [ "$1" = "full" ]
then
    qemu_full
else        # default
    qemu_small
fi
