# Notes: see EOF

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

qemu_mon() {
    # ${QEMU} -M raspi3b \
    # -kernel ${KERNEL} -serial null -serial mon:stdio -nographic \
    # -d int -D qemu.log 

    #### qemuv8 monitor
    ${QEMU} -M raspi3b \
    -kernel ${KERNEL} -monitor stdio -serial null \
    -d int -D qemu.log \
    -nographic \
    -usb -device usb-kbd
}    

#### qemu v8, console only
qemu_min () {
    ${QEMU} -M raspi3b \
    -kernel ${KERNEL} -serial null -serial mon:stdio -nographic \
    -d int -D qemu.log 
}    

### qemu v8, no grahpics, no kb, with sd
qemu_small () {
    ${QEMU} -M raspi3b \
    -kernel ${KERNEL} -serial null -serial mon:stdio \
    -d int -D qemu.log \
    -smp 4 \
    -nographic \
    -drive file=smallfat.bin,if=sd,format=raw
}    

### qemu v8, + grahpics, no kb, + sdls, no sd (until "rich user")
qemu_full () {
    ${QEMU} -M raspi3b \
    -kernel ${KERNEL} -serial null -serial mon:stdio \
    -d int -D qemu.log
    # -usb -device usb-kbd \
    # -drive file=smallfat.bin,if=sd,format=raw    # SD 
}

### qemu v8, no gfx, no kb, virtual fat
# cannot make it work as sd driver expects certain
# disk size. cannot fig out how to speicfy 
# ${QEMU8} -M raspi3b \
# -kernel ./kernel8-rpi3qemu.img -serial null -serial mon:stdio \
# -d int -D qemu.log \
# -nographic \
# -drive file=fat:rw:/tmp/testdir,if=sd,format=raw

if [ "$1" = "min" ]
then
    qemu_min
elif [ "$1" = "full" ]
then
    qemu_full
elif [ "$1" = "mon" ]    
then
    qemu_mon
else    # default ...     
    # qemu_small      
    qemu_full
fi

#############################

# cf for rpi3 qemu: 
#  https://github.com/RT-Thread/rt-thread/blob/master/bsp/raspberry-pi/raspi3-64/qemu-64.sh

# hw devices implemented
# https://www.qemu.org/docs/master/system/arm/raspi.html