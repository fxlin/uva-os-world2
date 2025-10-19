/*
    Draw a rotating donut on text console or screen.
    dependency:
        delay
        fb (uart may work? depending on terminal program)
    CREDITS: see end of the file

    lab2: a multitasking version
    - support multi donuts (via xoff,yoff arguments)
    - global buffer -> multiple smaller buffers

*/

#include "debug.h"
#include "plat.h"
#include "utils.h"

#define PIXELSIZE 4 /*ARGB, expected by /dev/fb*/
typedef unsigned int PIXEL;

#include "fb.h"
static inline void setpixel(unsigned char *buf, int x, int y, int pit, PIXEL p) {
    // W("buf %lx xx %d yy %d pitch %d", (unsigned long)buf, x, y, pit);
    assert(x >= 0 && y >= 0); // important guard
    *(PIXEL *)(buf + y * pit + x * PIXELSIZE) = p;
}

// canvas layout, 2x2
static const int xoff[] = {0,NN/2,0,NN/2};
static const int yoff[] = {0,0,NN/2,NN/2};
_Static_assert(N_DONUTS <= NELEM(xoff));

enum {K=4}; // donut scale factor, see code below
_Static_assert(80*K  <= NN/2); // columns
_Static_assert(22*K*2  <= NN/2); // rows

static char b[N_DONUTS][1760];        // text buffer (W 80 H 22?
static signed char z[N_DONUTS][1760]; // z buffer

void donut_canvas_init(void) {
    fb_fini();
    // acquire(&mboxlock);      //it's a test. so no lock

    the_fb.width = NN;
    the_fb.height = NN;

    the_fb.vwidth = NN;
    the_fb.vheight = NN;

    if (fb_init() != 0)
        BUG();
}

// actually not needed
// static void screen_clear(int idx) {
//     PIXEL blk = 0x0;
//     int x, y;
//     int offsetx = xoff[idx], offsety = yoff[idx]; 
//     int pitch = the_fb.pitch;
//     for (y = offsety; y < offsety + NN/2; y++)
//         for (x = offsetx; x < offsetx + NN/2; x++) {
//             setpixel(the_fb.fb, x, y, pitch, blk);
//         }
// }


static PIXEL int2rgb (int value); 

/////////////////////////////////////
#define R(mul, shift, x, y)              \
    _ = x;                               \
    x -= mul * y >> shift;               \
    y += mul * _ >> shift;               \
    _ = (3145728 - x * x - y * y) >> 11; \
    x = x * _ >> 10;                     \
    y = y * _ >> 10;

// draw dots on canvas, closer to the original js version (see comment at the end)
//quest: "two donuts". understand code below
//quest: "donuts in sync"
void donut_pixel(int idx) {
    int sA = 1024, cA = 0, sB = 1024, cB = 0, _;
    
    while (1) {
        memset(b[idx], 0, 1760);  // text buffer 0: black bkgnd
        memset(z[idx], 127, 1760); // z buffer
        int sj = 0, cj = 1024;
        for (int j = 0; j < 90; j++) {
            int si = 0, ci = 1024; // sine and cosine of angle i
            for (int i = 0; i < 324; i++) {
                int R1 = 1, R2 = 2048, K2 = 5120 * 1024;

                int x0 = R1 * cj + R2,
                    x1 = ci * x0 >> 10,
                    x2 = cA * sj >> 10,
                    x3 = si * x0 >> 10,
                    x4 = R1 * x2 - (sA * x3 >> 10),
                    x5 = sA * sj >> 10,
                    x6 = K2 + R1 * 1024 * x5 + cA * x3,
                    x7 = cj * si >> 10,
                    x = 25 + 30 * (cB * x1 - sB * x4) / x6,
                    y = 12 + 15 * (cB * x4 + sB * x1) / x6,
                    lumince = (((-cA * x7 - cB * ((-sA * x7 >> 10) + x2) - ci * (cj * sB >> 10)) >> 10) - x5); 
                    // fxl: range likely: <0..~1408, scale to 0..255
                    lumince = lumince<0? 0 : lumince/5; 
                    lumince = lumince<255? lumince : 255; 

                int o = x + 80 * y; // fxl: 80 chars per row
                signed char zz = (x6 - K2) >> 15;
                if (22 > y && y > 0 && x > 0 && 80 > x && zz < z[idx][o]) { // fxl: z depth will control visibility
                    z[idx][o] = zz;
                    // luminance_index is now in the range 0..11 (8*sqrt(2) = 11.3)
                    // now we lookup the character corresponding to the
                    // luminance and plot it in our output:
                    b[idx][o] = lumince;
                }
                R(5, 8, ci, si) // rotate i
            }
            R(9, 7, cj, sj) // rotate j
        }
        R(5, 7, cA, sA);
        R(5, 8, cB, sB);

        // screen_clear(idx);   // not needed
        int offsetx = xoff[idx], offsety = yoff[idx]; 
        
        int y = 0, x = 0;
        for (int k = 0; 1761 > k; k++) {
            if (k % 80) {
                if (x < 50) {
                    // scale x by K, y by 2K (so we have a round donut)
                    //   then offset by (offsetx,offsety)
                    int xx=x*K+offsetx, yy=y*K*2+offsety;
                    // PIXEL clr = b[k]; // blue only
                    PIXEL clr = int2rgb(b[idx][k]); // to a color spectrum
                    // W("fb %lx idx %d xx %d yy %d pitch %d",
                    //     (unsigned long)the_fb.fb, idx, xx, yy, the_fb.pitch);
                    // expand to a neighborhood of 4 pixels
                    setpixel(the_fb.fb, xx, yy, the_fb.pitch, clr);
                    setpixel(the_fb.fb, xx+1, yy, the_fb.pitch, clr);
                    setpixel(the_fb.fb, xx, yy+1, the_fb.pitch, clr);
                    setpixel(the_fb.fb, xx+1, yy+1, the_fb.pitch, clr);
                }
                x++;
            } else { 
                y++;
                x = 1;
            }
        }
        /* TODO: your code here */
    }
}

// map luminance [0..255] to rgb color
// value: 0..255, PIXEL: argb
static PIXEL int2rgb (int value) {
    int r,g,b;     
    if (value >= 0 && value <= 85) {
        // Black to Yellow (R stays 0, G increases, B stays 0)
        r = 0;
        g = (value * 3);
        b = 0;
    } else if (value > 85 && value <= 170) {
        // Yellow to Cyan (G stays 255, R decreases, B increases)
        r = 255 - ((value - 85) * 3);
        g = 255;
        b = (value - 85) * 3;
    } else if (value > 170 && value <= 255) {
        // Cyan to Blue (G decreases, B stays 255, R stays 0)
        r = 0;
        g = 255 - ((value - 170) * 3);
        b = 255;
    } else {
        // Value out of range
        r=g=b=0;
    }    
    return (r<<16)|(g<<8)|b; 
}

// idx: region in the canvas
// 
void donut(int idx) {
    donut_pixel(idx);
    // donut_uart();
    // donut_char_canvas();
}

/**
 * Original author:
 * https://twitter.com/a1k0n
 * https://www.a1k0n.net/2021/01/13/optimizing-donut.html
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-09-15     Andy Sloane  First version
 * 2011-07-20     Andy Sloane  Second version
 * 2021-01-13     Andy Sloane  Third version
 * 2021-03-25     Meco Man     Port to RT-Thread RTOS
 *
 *
 *  js code for both canvas & text version
 *  https://www.a1k0n.net/js/donut.js
 *
 *  ported by FL
 * From the NJU OS project:
 * https://github.com/NJU-ProjectN/am-kernels/blob/master/kernels/demo/src/donut/donut.c
 *
 */
