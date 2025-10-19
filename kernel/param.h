/* 
    Kernel configuration 
    included by: both the kernel (c and asm) and mkfs
*/

#define NOFILE          16  // open files per process
#define NCPU	        1   // # of cpu cores 
#define MAXPATH         128   // maximum file path name
#define NINODE          50  // maximum number of active i-nodes
// #define NDEV            10  // maximum major device number
#define MAXARG       32  // max exec arguments
#define NFILE       100  // open files per system
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes fxl:too small?
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define NR_TASKS				32   // 128     // 32 should be fine. usertests.c seems to expect > 100
#define NR_MMS              NR_TASKS

#define NN 640 // entire canvas dimension. NN by NN. cf donut.c
// #define N_DONUTS 2      // max # of donuts
#define N_DONUTS 4      // max # of donuts. slow w/o cache. project idea: compare the speed vs w/cache

#define DEV_RAMDISK     1       // ramdisk
#define DEV_VIRTDISK    2       // qemu's virtio device
// below is for sd card (phys or emulated)
#define DEV_SD0          3       // partition0
#define DEV_SD1          4       // partition1
#define DEV_SD2          5       // partition2
#define DEV_SD3          6       // partition3
#define is_sddev(dev) (dev>=DEV_SD0 && dev<=DEV_SD3)

// device number of file system root disk NB: this is block disk id, not major/minor
#define ROOTDEV       DEV_RAMDISK  
#if defined(PLAT_RPI3QEMU)
#define SECONDDEV     DEV_SD0       // secondary dev to mount under /d/
#elif defined(PLAT_RPI3)
#define SECONDDEV     DEV_SD1       // secondary dev to mount under /d/
#endif
    
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define FSSIZE       15000 // 2000  // size of file system in blocks
// (32 * 1024)  ok, but results in a very large ramdisk...

// the malloc()/free() region which is carved out from the paging area
// #define MALLOC_PAGES  (16*1024*1024 / PAGE_SIZE)  
#define MALLOC_PAGES  (8*1024*1024 / PAGE_SIZE)  

#define MAX_TASK_KER_PAGES      16       //max kernel pages per task. 

#ifndef __ASSEMBLER__
// below keeps xv6 code happy. TODO: separate them out
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;
#endif