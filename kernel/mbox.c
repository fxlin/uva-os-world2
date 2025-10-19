// #define K2_DEBUG_WARN
#define K2_DEBUG_INFO

/* 
    the mailbox driver (&framebuffer, display) for rpi3.
    use mailbox "property" interface
    cf: https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface

    CREDITS, more ref: see the end of this file
*/

#include "plat.h"
#include "utils.h"
#include "spinlock.h"

struct spinlock mboxlock = {.locked=0, .cpu=0, .name="mbox_lock"};

/* mailbox message buffer */
volatile unsigned int  __attribute__((aligned(16))) mbox[36];

#define MMIO_BASE       0x3F000000UL
#define VIDEOCORE_MBOX  (MMIO_BASE+0x0000B880)
#define MBOX_READ       ((volatile unsigned int*)((VIDEOCORE_MBOX)+0x0))
#define MBOX_POLL       ((volatile unsigned int*)((VIDEOCORE_MBOX)+0x10))
#define MBOX_SENDER     ((volatile unsigned int*)((VIDEOCORE_MBOX)+0x14))
#define MBOX_STATUS     ((volatile unsigned int*)((VIDEOCORE_MBOX)+0x18))
#define MBOX_CONFIG     ((volatile unsigned int*)((VIDEOCORE_MBOX)+0x1C))
#define MBOX_WRITE      ((volatile unsigned int*)((VIDEOCORE_MBOX)+0x20))
#define MBOX_RESPONSE   0x80000000
#define MBOX_FULL       0x80000000
#define MBOX_EMPTY      0x40000000

/**
 * Make a mailbox call. Use the "mbox" buffer for both request and response.
 * response overwrites request
 * Spin wait for mailbox hw.  
 * Returns 0 on failure, non-zero on success
 * 
 * caller must hold mboxlock
 */
int mbox_call(unsigned char ch)
{
    // the buf addr (pa) w/ ch (chan id) in LSB 
    unsigned int r = (((unsigned int)((unsigned long)&mbox)&~0xF) | (ch&0xF));
    r = BUS_ADDRESS(r); 
    /* wait until we can write to the mailbox */
    do{asm volatile("nop");}while(*MBOX_STATUS & MBOX_FULL);
    __asm__ volatile ("dmb sy" ::: "memory");    // mem barrier, ensuring msg in mem
    __asm_flush_dcache_range((void *)mbox, (char *)mbox + sizeof(mbox)); 

    /* write the address of our message to the mailbox with channel identifier */
    *MBOX_WRITE = r; 
    /* now wait for the response */
    while(1) {
        /* is there a response? */
        do{asm volatile("nop");}while(*MBOX_STATUS & MBOX_EMPTY);
        /* is it a response to our message? */
        if(r == *MBOX_READ) {
            V("r is 0x%x", r); 
            __asm_invalidate_dcache_range((void *)mbox, (char *)mbox + sizeof(mbox)); 
            /* is it a valid successful response? (strange it's benign) */
            if (mbox[1]!=MBOX_RESPONSE) I("mbox[1] is %08x", mbox[1]);            
            return mbox[1]==MBOX_RESPONSE;
        } else {
            W("got an irrelvant msg. bug?"); 
        }
    }
    return 0;
}

///////////////////////////////////////////////////
// property interfaces via mbox
#define MBOX_REQUEST    0
#define CODE_RESPONSE_SUCCESS	0x80000000
#define CODE_RESPONSE_FAILURE	0x80000001
	
/* channels */
#define MBOX_CH_POWER   0
#define MBOX_CH_FB      1
#define MBOX_CH_VUART   2
#define MBOX_CH_VCHIQ   3
#define MBOX_CH_LEDS    4
#define MBOX_CH_BTNS    5
#define MBOX_CH_TOUCH   6
#define MBOX_CH_COUNT   7
#define MBOX_CH_PROP    8

/* tags */
#define MBOX_TAG_LAST           0

// in a successful resp, b31 is set; b30-0 is "value length in bytes"
#define VALUE_LENGTH_RESPONSE	(1 << 31)

// #define PROPTAG_GET_FIRMWARE_REVISION	0x00000001
// #define PROPTAG_GET_BOARD_MODEL		0x00010001
// #define PROPTAG_GET_BOARD_REVISION	0x00010002
// #define PROPTAG_GET_MAC_ADDRESS		0x00010003
// #define PROPTAG_GET_BOARD_SERIAL	0x00010004
// #define PROPTAG_GET_ARM_MEMORY		0x00010005
// #define PROPTAG_GET_VC_MEMORY		0x00010006
// #define PROPTAG_SET_POWER_STATE		0x00028001
//     #define DEVICE_ID_SD_CARD	0   // FL: SDHCI, not SDHOST
// 	#define DEVICE_ID_USB_HCD	3
//     #define POWER_STATE_OFF		(0 << 0)
// 	#define POWER_STATE_ON		(1 << 0)
// 	#define POWER_STATE_WAIT	(1 << 1)
// 	#define POWER_STATE_NO_DEVICE	(1 << 1)	// in response
// #define PROPTAG_GET_CLOCK_RATE		0x00030002
// #define PROPTAG_GET_TEMPERATURE		0x00030006
// #define PROPTAG_GET_EDID_BLOCK		0x00030020
// #define PROPTAG_GET_DISPLAY_DIMENSIONS	0x00040003
// #define PROPTAG_GET_COMMAND_LINE	0x00050001
// // undocumented. cf https://github.com/raspberrypi/firmware/issues/719
// // also sound/sample/env.c EnableVCHIQ
// #define PROPTAG_VCHIQ_INIT  	    0x48010 

///////////////////////////////////////////////////
//  framebuffer driver (via mbox)
#include "fb.h"

/* PC Screen Font as used by Linux Console */
typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int headersize;
    unsigned int flags;
    unsigned int numglyph;
    unsigned int bytesperglyph;
    unsigned int height;
    unsigned int width;
    unsigned char glyphs;
} __attribute__((packed)) psf_t;
// cf: Makefile font build rules
extern volatile unsigned char _binary_font_psf_start;  

// /* Scalable Screen Font (https://gitlab.com/bztsrc/scalable-font2) */
// typedef struct {
//     unsigned char  magic[4];
//     unsigned int   size;
//     unsigned char  type;
//     unsigned char  features;
//     unsigned char  width;
//     unsigned char  height;
//     unsigned char  baseline;
//     unsigned char  underline;
//     unsigned short fragments_offs;
//     unsigned int   characters_offs;
//     unsigned int   ligature_offs;
//     unsigned int   kerning_offs;
//     unsigned int   cmap_offs;
// } __attribute__((packed)) sfn_t;
// extern volatile unsigned char _binary_font_sfn_start; // cf: linker script

// default (upon boot): 1024x768, phys WH = virt WH, offset (0,0)
// said to support up to 1920x1080
struct fb_struct the_fb = {
    .fb = 0,
#ifdef PLAT_RPI3QEMU
    // these are just initial fb sizes; app will ask for diff
    // sizes based on their logic. so we keep them small for qemu
    // to avoid a big blank screen upon boot
    .width = 320,
    .height = 240, 
    .vwidth = 320, 
    .vheight = 240,
#else // rpi3 hw
    // =0 same as the detected scr dim, see below
    .width  = 0, // 1024,  
    .height = 0, // 768, 
    .vwidth = 0, // 1024, 
    .vheight = 0, // 768,
#endif
    .scr_width = 0,
    .scr_height = 0, 
    .depth = 32, 
    .isrgb = 0,     // see below 
    .offsetx = 0,
    .offsety = 0,
    .size = 0, 
}; 
// isrgb: whatever the doc says, 0 seems rgb; 1 seems bgr (per my test)
// rpi3 hw will return "0" even if we asks for "1"
// qemu will do whatever we ask ("0" or "1"); if "1", channel order is bgr

/* 
    detect phys display optimal x/y, if unconfigured
    caller must hold mboxlock
    return: 0 on success 

    FL's 720p monitor: 1360 768
    qemu 640 480 (initial? subject to reconfig for larger fb)
*/
int fb_detect_scr_dim(uint *w, uint *h) {
    mbox[0] = 8*4;     // size of the whole buf that follows
    mbox[1] = MBOX_REQUEST; // cpu->gpu request
        mbox[2] = 0x40003;     // rls framebuffer
        mbox[3] = 8;           // total buf size
        mbox[4] = 0;           // req para size
        mbox[5] = 0;           // resp: width
        mbox[6] = 0;           // resp: height
    mbox[7] = MBOX_TAG_LAST;

    if(!mbox_call(MBOX_CH_PROP)) {
        E("failed to get screen dim");
        return -1;
    } 

    *w=mbox[5];*h=mbox[6]; I("detected screen dim %d %d", *w, *h);

    if (*w == 1184 || *h == 624) {
        W("detected screen 1184x624. assume a Waveshare HAT. force 480 320");
        *w = 480; *h = 320;
    }    
    return 0; 
}

// set virt offset
// caller must hold mboxlock
// 0 on success
int fb_set_voffsets(int offsetx, int offsety) {

    mbox[0] = 8*4;
    mbox[1] = MBOX_REQUEST;
    
    mbox[2] = 0x48009; 
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] =  offsetx;           //FrameBufferInfo.x_offset
    mbox[6] =  offsety;           //FrameBufferInfo.y.offset    

    mbox[7] = MBOX_TAG_LAST;

    if(!mbox_call(MBOX_CH_PROP)) {
        E("failed to set virt offsets, requested x=%d y=%d", offsetx, offsety);
        return -1;
    }     
     if (mbox[5] != offsetx || mbox[6] != offsety) {
        E("failed set: offsetx %u offsety %u res: offsetx %u offsety %u", 
            offsetx, offsety, mbox[5], mbox[6]);
        return -1;     
     }
     V("set OK: offsetx %u offsety %u res: offsetx %u offsety %u", 
            offsetx, offsety, mbox[5], mbox[6]);
     return 0; 
}

/* 
    init the actual fb hw, by invoking the mbox interface
    return 0 if succeeds. 
 */
static int do_fb_init(struct fb_struct *fbs)
{    
    if (!fbs) return -1; 

    acquire(&mboxlock); 

#ifdef PLAT_RPI3
    // if (v)width/(v)height is 0, set them = the scr size
    if (fb_detect_scr_dim(&fbs->scr_width,&fbs->scr_height)==0) {
        fbs->vwidth = fbs->vwidth ? fbs->vwidth:fbs->scr_width;
        fbs->vheight = fbs->vheight ? fbs->vheight:fbs->scr_height;
        fbs->width  = fbs->width ? fbs->width:fbs->scr_width;
        fbs->height  = fbs->height ? fbs->height:fbs->scr_height;        
    }
#endif

    mbox[0] = 35*4;     // size of the whole buf that follows
    mbox[1] = MBOX_REQUEST; // cpu->gpu request

    // a sequence of tags below 
    mbox[2] = 0x48003;  //set phy width & height
    mbox[3] = 8;        // total buf size of this tag
    mbox[4] = 8;        // req val size (needed?), to be overwritten as resp val size
    mbox[5] = fbs->width;           //(val) FrameBufferInfo.width
    mbox[6] = fbs->height;          //(val) FrameBufferInfo.height

    mbox[7] = 0x48004;  //set virt width & height
    mbox[8] = 8;
    mbox[9] = 8;
    mbox[10] = fbs->vwidth;        //FrameBufferInfo.virtual_width
    mbox[11] = fbs->vheight;         //FrameBufferInfo.virtual_height

    mbox[12] = 0x48009; //set virt offset
    mbox[13] = 8;
    mbox[14] = 8;
    mbox[15] = fbs->offsetx;           
    mbox[16] = fbs->offsety;           

    mbox[17] = 0x48005; //set depth
    mbox[18] = 4;
    mbox[19] = 4;
    mbox[20] = fbs->depth;       

    mbox[21] = 0x48006;     //set pixel order
    mbox[22] = 4;
    mbox[23] = 4;
    mbox[24] = fbs->isrgb;           //RGB, not BGR preferably

    mbox[25] = 0x40001;     //get framebuffer, gets alignment on request
    mbox[26] = 8;
    mbox[27] = 8;           // fxl: should be 4?? (req para size)
    mbox[28] = 4096;        //req: alignment; resp: FrameBufferInfo.pointer
    mbox[29] = 0;           //resp: FrameBufferInfo.size

    mbox[30] = 0x40008;     //get pitch
    mbox[31] = 4;
    mbox[32] = 4;
    mbox[33] = 0;           //FrameBufferInfo.pitch

    mbox[34] = MBOX_TAG_LAST;   // the end of tag seq

    // make call, then check some response vals that may fail
    if(mbox_call(MBOX_CH_PROP) 
        && mbox[20]==fbs->depth /*depth*/ 
        && mbox[28]!=0 /*framebuf*/) {
        // extract framebuf info from resp...
        mbox[28]&=0x3FFFFFFF;  
        fbs->fb = (unsigned char *)((unsigned long)mbox[28]);   // save framebuf ptr
        fbs->width=mbox[5];
        fbs->height=mbox[6];
        fbs->vwidth=mbox[10];
        fbs->vheight=mbox[11];        
        fbs->depth=mbox[20]; 
        fbs->isrgb=mbox[24];         // channel order        
        fbs->pitch=mbox[33];
        if(fbs->pitch * fbs->vheight > mbox[29])  // possible that pitch*vheight < actual allocation
            {W("pitch %d x vheight %d!= mbox[29] %u", fbs->pitch, fbs->vheight, mbox[29]);BUG();}
        fbs->size = PGROUNDUP(fbs->pitch * fbs->vheight);  // roundup b/c we'll reserve pages for it
        I("From GPU: fb pa: 0x%08x w %u h %u vw %u vh %u pitch %u isrgb %u", 
            mbox[28], fbs->width, fbs->height, fbs->vwidth, fbs->vheight, 
                fbs->pitch, fbs->isrgb); 
    } else {
        E("Unable to set scr res to %d x %d\n", fbs->width, fbs->height);
        return -2; 
    }
    release(&mboxlock); 

    if (reserve_phys_region(mbox[28], fbs->size)) {
        E("failed to reserve fb mem. pa 0x%x size 0x%x already in use.",
            mbox[28], fbs->size); BUG(); 
        return -1; 
    } else 
        return 0; 
}

void fb_showpicture();

/* init the fb, show a picture (OS logo) 
    return 0 on success (display will go black)
*/
int fb_init(void) {
    static int once = 1; 
    int ret = do_fb_init(&the_fb); 
    if (ret==0 && once)
        {fb_showpicture(); once=0;}
    return ret; 
}

/* finalize the fb, clean up. 
    return 0 on success (display will go blank)
*/
int fb_fini(void) {
    int ret = 0; 

    acquire(&mboxlock); 
    if (!the_fb.fb || !the_fb.size) {
        ret = -1; 
        goto out; 
    }

#ifdef PLAT_RPI3QEMU    // avoid artifacts: qemu does not clear old fb
    memset(the_fb.fb, 0, the_fb.size);     
#endif

    mbox[0] = 6*4;     // size of the whole buf that follows
    mbox[1] = MBOX_REQUEST; // cpu->gpu request

    mbox[2] = 0x48001;     // rls framebuffer
    mbox[3] = 0;           // total buf size
    mbox[4] = 0;           // req para size
        
    mbox[5] = MBOX_TAG_LAST;

    if(!mbox_call(MBOX_CH_PROP))
        I("failed to rls fb with GPU (could be benign)"); 
        // response code always 0x80000001 (failure). couldn't figure out why

    if (free_phys_region((unsigned long)the_fb.fb, the_fb.size)) {
        E("failed to free fb memory. bug?"); 
        ret = -2; 
    }
    the_fb.fb = 0; 
out:
    release(&mboxlock);          
    return ret; 
}

///////////////////////////////////////////////////
//  draw: picture/text on the fb display 

/* 
    Display a string using fixed size PSF update x,y screen coordinates
    x/y (IN|OUT): the postion before/after the screen output 
    NB these are pixel coordinates (not character locations)
*/
void fb_print(int *x, int *y, char *s)
{
    unsigned pitch = the_fb.pitch; 
    unsigned char *fb = the_fb.fb; 

    // get our font
    psf_t *font = (psf_t*)&_binary_font_psf_start;
    // draw next character if it's not zero
    while(*s) {
        /* get offset of the glyph. Need to adjust this to support unicode table */
        unsigned char *glyph = (unsigned char*)&_binary_font_psf_start +
         font->headersize + (*((unsigned char*)s)<font->numglyph?*s:0)*font->bytesperglyph;
        // calculate the offset on screen
        int offs = (*y * pitch) + (*x * 4);
        // variables
        int i,j, line,mask, bytesperline=(font->width+7)/8;
        // handle carrige return
        if(*s == '\r') {
            *x = 0;
        } else
        // new line
        if(*s == '\n') {
            *x = 0; *y += font->height;
        } else {
            // display a character
            for(j=0;j<font->height;j++){
                // display one row
                line=offs;
                mask=1<<(font->width-1);
                for(i=0;i<font->width;i++){
                    // if bit set, we use white color, otherwise black
                    *((unsigned int*)(fb + line))=((int)*glyph) & mask?0xFFFFFF:0;
                    mask>>=1;
                    line+=4;
                }
                // adjust to next line
                glyph+=bytesperline;
                offs+=pitch;
            }
            *x += (font->width+1);
        }
        // next character
        s++;
    }
}

#include "uvalogo.h"
#define IMG_DATA header_data      
#define IMG_HEIGHT height
#define IMG_WIDTH width

void fb_showpicture()
{
    int x,y;
    unsigned char *ptr=the_fb.fb;
    char *data=IMG_DATA, pixel[4];
    // fill framebuf. crop img data per the framebuf size
    unsigned int img_fb_height = the_fb.vheight < IMG_HEIGHT ? the_fb.vheight : IMG_HEIGHT; 
    unsigned int img_fb_width = the_fb.vwidth < IMG_WIDTH ? the_fb.vwidth : IMG_WIDTH; 

    // copy the image pixels to the start (top) of framebuf    
    //ptr += (vheight-img_fb_height)/2*pitch + (vwidth-img_fb_width)*2;  
    ptr += (the_fb.vwidth-img_fb_width)/2*PIXELSIZE;  // top center
    ptr += (the_fb.vheight-img_fb_height)/2*the_fb.pitch; 
    
    for(y=0;y<img_fb_height;y++) {
        for(x=0;x<img_fb_width;x++) {
            HEADER_PIXEL(data, pixel);
            /* the image is in RGB. So if we have an RGB framebuffer, we copy
            the pixels directly, but for BGR we must swap R (pixel[0]) and B
            (pixel[2]) channels. */
            *((unsigned int*)ptr)=the_fb.isrgb ? *((unsigned int *)&pixel) 
                : (unsigned int)(pixel[0]<<16 | pixel[1]<<8 | pixel[2]);
            // *((unsigned int*)ptr)=(!the_fb.isrgb) ? *((unsigned int *)&pixel) : (unsigned int)(pixel[0]<<16 | pixel[1]<<8 | pixel[2]);
            ptr+=4;
        }
        ptr+=the_fb.pitch-img_fb_width*4;
    }

    // show text strings
    x = (the_fb.vwidth-img_fb_width)/2;
    y = the_fb.vheight/2 + img_fb_height/2;
    fb_print(&x, &y, "UVA OS");
    char res[16]; 
    sprintf(res, " %dx%d", the_fb.width, the_fb.height); // debug info 
    fb_print(&x, &y, res);
    // __asm_flush_dcache_range(the_fb.fb, the_fb.fb + the_fb.size); 
}

/*
 * Copyright (C) 2018 bzt (bztsrc@github)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */


/*  Ref: 
    https://github.com/raspberrypi/firmware/wiki/Mailboxes
    ref: https://github.com/RT-Thread/rt-thread/blob/master/bsp/raspberry-pi/raspi3-64/driver/mbox.c 
    ref: https://www.valvers.com/open-software/raspberry-pi/bare-metal-programming-in-c-part-5/#part-5armc-016
    ref: uboot arch/arm/mach-bcm283x/msg.c  
    
    for do_fb_init():

     * more undocumented cf: 
    * https://github.com/raspberrypi/firmware/issues/719
    * 
    * below uses mbox "property channel". another way is to use the "fb" channel 
    * directly. cf: https://github.com/rsta2/circle/blob/master/lib/bcmframebuffer.cpp
    * 
    * code ex: 
    * https://github.com/RT-Thread/rt-thread/blob/master/bsp/raspberry-pi/raspi3-64/driver/mbox.c
    *     
*/