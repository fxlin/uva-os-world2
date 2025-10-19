# UVA-OS Lab2 "Embedded" 

This is part of the UVA-OS class (CS4414/CS6456). 

[OVERVIEW](https://github.com/fxlin/cs4414-main) |
[LAB1](https://github.com/fxlin/uva-os-world1) |
[LAB2](https://github.com/fxlin/uva-os-world2) |
[LAB3](https://github.com/fxlin/uva-os-world3) |
[LAB4](https://github.com/fxlin/uva-os-world4) |
[LAB5](https://github.com/fxlin/uva-os-world5) 

Students: see [quests-lab2.md](quests-lab2.md)

## Changelog
- 1/30/25: improved code comments

## GALLERY

<img src="2donuts-sync.gif" alt="description" width="300">

<video controls src="gamehat 2donuts.mp4" title="2 donuts on rpi3(gamehat)" width="300"></video>

<img src="wordsworth.gif" alt="description" width="300">

## DESIGNS

This OS resembles what you would see in an "embedded systems" course. In addition to World 1 features, it can run multiple tasks and preempt their execution. However, everything still runs at EL1.

<img src="image.png" alt="description" width="500">

✅ Scheduler: cooperative & preemptive

✅ Memory allocator (simple)

✅ Task management (sleep, wait, exit, kill)

⛔ EL1 only
⛔ No virtual memory 


## QUICKSTART

### For rpi3 (QEMU)

```
export PLAT=rpi3qemu
```

| Action                      | Command                   |
|-----------------------------|---------------------------|
| To clean up                 | `./cleanall.sh`           |
| To build everything         | `./makeall.sh`            |
| To run on qemu              | `./run-rpi3qemu.sh`       |
| Launch qemu for debugging   | `./dbg-rpi3qemu.sh`       |

### For rpi3 (hardware)
```
export PLAT=rpi3
```

| Action              | Command             |
|---------------------|---------------------|
| To clean up         | `./cleanall.sh`     |
| To build everything | `./makeall.sh`      |

<!-- get a blank SD card, burn the provided image with Win32DiskImager, 
balenaEtcher, or Raspberry Pi Imager.  -->

(One time): Prepare the SD card

https://github.com/fxlin/uva-os-main/tree/main/make-sd


Copy the kernel image `kernel8.img` to the partition named `bootfs` and boot. 