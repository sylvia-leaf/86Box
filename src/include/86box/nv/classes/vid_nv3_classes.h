/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Defines graphics objects for Nvidia NV3 architecture-based GPU (RIVA 128/RIVA 128 ZX),
 *          as well as for later GPUs if they use the same objects.     
 *          Note: These uint32_ts are basically object methods that are being submitted
 *          They have different names so the user can use them more easily but different versions of the same class can be distinguished
 *          ALL of these structures HAVE(?) to be a size of exactly 0x2000 bytes because that's what the hashtable expects and they need to actually map into the vram address space
 *          (they are converted to pointers). In the case of NV3, these map directly to the PHYSICAL PGRAPH REGISTERS while sitting in RAMHT!!!!.
 *
 *          Also, these class IDs don't relate to the internal architecture of the GPU.
 *          Effectively, the NVIDIA drivers are faking shit. There are only 22 classes but the drivers recognise many more and have a different naming scheme. See nv3_object_classes_driver.txt for the list of  
 *          classes recognised by the driver.
 *          This is why the Class IDs you see here are not the same as you may see in other places.
 *
 *          Todo: Is reserved* actually needed?
 *
 *
 * Authors: Connor Hyde <mario64crashed@gmail.com>
 *
 *          Copyright 2024-2025 Connor Hyde
 */

#pragma once 
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// CLass names for debugging
extern const char* nv3_class_names[];

/* Defines valid classes. */
typedef enum nv3_pgraph_class_e
{
    nv3_pgraph_class01_beta_factor = 0x01,
    nv3_pgraph_class02_rop = 0x02,
    nv3_pgraph_class03_chroma_key = 0x03,
    nv3_pgraph_class04_plane_mask = 0x04,
    nv3_pgraph_class05_clipping_rectangle = 0x05,
    nv3_pgraph_class06_pattern = 0x06,
    nv3_pgraph_class07_rectangle = 0x07,
    nv3_pgraph_class08_point = 0x08,
    nv3_pgraph_class09_line = 0x09,
    nv3_pgraph_class0a_lin = 0x0a,
    nv3_pgraph_class0b_triangle = 0x0b,
    nv3_pgraph_class0c_w95txt = 0x0c,
    nv3_pgraph_class0d_m2mf = 0x0d,
    nv3_pgraph_class0e_scaled_image_from_memory = 0x0e,
    nv3_pgraph_class10_blit = 0x10,
    nv3_pgraph_class11_image = 0x11,
    nv3_pgraph_class12_bitmap = 0x12,
    nv3_pgraph_class14_transfer2memory = 0x14,
    nv3_pgraph_class15_stretched_image_from_cpu = 0x15,
    nv3_pgraph_class17_d3d5tri_zeta_buffer = 0x17,
    nv3_pgraph_class18_point_zeta_buffer = 0x18, 
    nv3_pgraph_class1c_image_in_memory = 0x1c,
} nv3_pgraph_class;

/* 
    OBJECT METHODS
*/

// Global stuff
#define NV3_ROOT_HI_IM_OBJECT_MCOBJECTYFACE             0x0     // I'm going insane at 00:48 14/02/2025
#define NV3_SET_NOTIFY_CONTEXT_FOR_DMA                  0x0100  // Set object ctx for dma...see nv3_dma_context_t structure
#define NV3_SET_NOTIFY                                  0x0104

// Crap e.g. "OS name", that sometimes gets submitted, for some reason. So we just suppress the warning messages for them
#define NV3_NVCLASS_CRAP_START                          0x0310
#define NV3_NVCLASS_CRAP_END                            0x0324

// Render OPeration
#define NV3_ROP_SET_ROP                                 0x0300  // Set GDI standard rop

// Beta Factor
#define NV3_BETA_FACTOR                                 0x0300

// Chroma Key
// Can't figure out what this is, used in 9x but certainly software, can't find anywhere...
#define NV3_CHROMA_UNKNOWN_0200                         0x0200
#define NV3_CHROMA_KEY                                  0x0304

// Clip
#define NV3_CLIP_POSITION                               0x0300  // S16:S16, 0=topleft
#define NV3_CLIP_SIZE                                   0x0304  // U16:U16

// Blit Pattern
#define NV3_PATTERN_FORMAT                              0x0304
#define NV3_PATTERN_SHAPE                               0x0308
#define NV3_PATTERN_SHAPE_8X8                           0
#define NV3_PATTERN_SHAPE_64X1                          1
#define NV3_PATTERN_SHAPE_1X64                          2
#define NV3_PATTERN_SHAPE_LAST_VALID                    NV3_PATTERN_SHAPE_1X64

#define NV3_PATTERN_UNUSED_DRIVER_BUG                   0x030C
#define NV3_PATTERN_COLOR0                              0x0310
#define NV3_PATTERN_COLOR1                              0x0314
#define NV3_PATTERN_BITMAP_HIGH                         0x0318
#define NV3_PATTERN_BITMAP_LOW                          0x031C

// Rect

#define NV3_RECTANGLE_COLOR                             0x0304

// 16 possible rectangles. 8 byte structure, first 4 bytes = position, second 2 = size.
#define NV3_RECTANGLE_START                             0x0400

#define NV3_RECTANGLE_MAX                               16
#define NV3_RECTANGLE_END                               0x0480

// M2MF
#define NV3_M2MF_IN_CTXDMA_OFFSET                       0x030C
#define NV3_M2MF_OUT_CTXDMA_OFFSET                      0x0310
#define NV3_M2MF_IN_PITCH                               0x0314
#define NV3_M2MF_OUT_PITCH                              0x0318
#define NV3_M2MF_SCANLINE_LENGTH_IN_BYTES               0x031C
#define NV3_M2MF_NUM_SCANLINES                          0x0320
#define NV3_M2MF_FORMAT                                 0x0324

// M2MF formats (IN and OUT ORed together)
#define NV3_M2MF_FORMAT_INPUT                           0
#define NV3_M2MF_FORMAT_OUTPUT                          8

#define NV3_M2MF_NOTIFY                                 0x0328

// blit
#define NV3_BLIT_POSITION_IN                            0x0300
#define NV3_BLIT_POSITION_OUT                           0x0304
#define NV3_BLIT_SIZE                                   0x0308

// image_from_cpu
#define NV3_IMAGE_START_POSITION                        0x0304  // starting position of image from cpu
#define NV3_IMAGE_SIZE                                  0x0308
#define NV3_IMAGE_SIZE_IN                               0x030C
#define NV3_IMAGE_COLOR_START                           0x0400
#define NV3_IMAGE_COLOR_MAX                             32
#define NV3_IMAGE_COLOR_END                             0x0480

#define NV3_IMAGE_IN_MEMORY_COLOR_FORMAT                0x0300
#define NV3_IMAGE_IN_MEMORY_IN_MEMORY_DMA_CTX_TYPE      0x0304
#define NV3_IMAGE_IN_MEMORY_PITCH                       0x0308
#define NV3_IMAGE_IN_MEMORY_TOP_LEFT_OFFSET             0x030C
#define NV3_IMAGE_IN_MEMORY_TOP_LEFT_OFFSET_END         22  

// scaled_image_from_cpu
#define NV3_SCALED_IMAGE_SIZE_IN                               0x0304
#define NV3_SCALED_IMAGE_START_POSITION                        0x0310
#define NV3_SCALED_IMAGE_SIZE                                  0x0314
#define NV3_SCALED_IMAGE_COLOR_START                           0x0400
#define NV3_SCALED_IMAGE_COLOR_MAX                             32
#define NV3_SCALED_IMAGE_COLOR_END                             0x0480

/* GDI */

/* Type A: Unclipped Rectangle */
#define NV3_W95TXT_A_COLOR                              0x03FC  // It's the colour of the text. This is used to submit a dummy object so the notifier can be used to sync in Win2000 DDraw6 drivers.
#define NV3_W95TXT_A_RECT_START                         0x0400
#define NV3_W95TXT_A_RECT_SIZE                          64      // Number of rects
#define NV3_W95TXT_A_RECT_END                           0x05FF

/* Type B: Clipped Rectangle */
#define NV3_W95TXT_B_CLIP_TOPLEFT                       0x07F4
#define NV3_W95TXT_B_CLIP_BOTTOMRIGHT                   0x07F8
#define NV3_W95TXT_B_COLOR                              0x07FC
#define NV3_W95TXT_B_CLIP_CLIPRECT_START                0x0800
#define NV3_W95TXT_B_CLIP_CLIPRECT_SIZE                 128     // Number of rects
#define NV3_W95TXT_B_CLIP_CLIPRECT_END                  0x09FF

/* Type C: Unscaled Single-Colour Text (filled in from mono bitmap) */
#define NV3_W95TXT_C_CLIP_TOPLEFT                       0x0BEC
#define NV3_W95TXT_C_CLIP_BOTTOMRIGHT                   0x0BF0
#define NV3_W95TXT_C_CLIP_COLOR                         0x0BF4
#define NV3_W95TXT_C_CLIP_SIZE                          0x0BF8
#define NV3_W95TXT_C_CLIP_POSITION                      0x0BFC  /* TOP LEFT */
#define NV3_W95TXT_C_CLIP_CLIPRECT_START                0x0C00
#define NV3_W95TXT_C_CLIP_CLIPRECT_SIZE                 64      // Number of rects
#define NV3_W95TXT_C_CLIP_CLIPRECT_END                  0x0DFF

/* Type D: Scaled Single-Colour Text (filled in from mono bitmap) */
#define NV3_W95TXT_D_CLIP_TOPLEFT                       0x0FE8
#define NV3_W95TXT_D_CLIP_BOTTOMRIGHT                   0x0FEC
#define NV3_W95TXT_D_CLIP_COLOR                         0x0FF0
#define NV3_W95TXT_D_CLIP_SIZE_IN                       0x0FF4
#define NV3_W95TXT_D_CLIP_SIZE_OUT                      0x0FF8
#define NV3_W95TXT_D_CLIP_POSITION                      0x0FFC  /* TOP LEFT */
#define NV3_W95TXT_D_CLIP_CLIPRECT_START                0x1000
#define NV3_W95TXT_D_CLIP_CLIPRECT_SIZE                 128     // Number of rects
#define NV3_W95TXT_D_CLIP_CLIPRECT_END                  0x11FF

/* Type E: Scaled Pattern-type Bitmap Text (filled) */
#define NV3_W95TXT_E_CLIP_TOPLEFT                       0x13E4
#define NV3_W95TXT_E_CLIP_BOTTOMRIGHT                   0x13E8
#define NV3_W95TXT_E_CLIP_COLOR_0                       0x13EC
#define NV3_W95TXT_E_CLIP_COLOR_1                       0x13F0
#define NV3_W95TXT_E_CLIP_SIZE_IN                       0x13F4
#define NV3_W95TXT_E_CLIP_SIZE_OUT                      0x13F8
#define NV3_W95TXT_E_CLIP_POSITION                      0x13FC  /* TOP LEFT */
#define NV3_W95TXT_E_CLIP_CLIPRECT_START                0x1400
#define NV3_W95TXT_E_CLIP_CLIPRECT_SIZE                 128     // Number of rects
#define NV3_W95TXT_E_CLIP_CLIPRECT_END                  0x15FF


/* Class context switch method */
typedef struct nv3_class_ctx_switch_method_s
{
    union 
    {
        uint32_t data;

        uint16_t instance;
        uint8_t channel : 6;
        uint16_t reserved : 9;
        bool reset_if_volatile; // ????
    } set_notify_ctx_dma;           // Set notifier context for DMA (context switch)

} nv3_class_ctx_switch_method_t;

/* 
enumerates color formats
there are some other colour formats that are only used in certain areas which are defined below
*/
typedef enum nv3_pgraph_pixel_format_e
{
    nv3_pgraph_pixel_format_r5g5b5 = 0,
    nv3_pgraph_pixel_format_r8g8b8 = 1,
    nv3_pgraph_pixel_format_r10g10b10 = 2,
    nv3_pgraph_pixel_format_y8 = 3,
    nv3_pgraph_pixel_format_y16 = 4,
    nv3_pgraph_pixel_format_v8y8u8y18 = 5, // "18"? NV wtf?
    nv3_pgraph_pixel_format_y18v8y8u8 = 6, // "18"? NV wtf?
    nv3_pgraph_pixel_format_y420 = 7, // YUV 420
} nv3_pgraph_pixel_format;

/* Main color format */
typedef struct nv3_color_expanded_s
{
    uint8_t a;

    /* WARNING: The internal format is 10-bit RGB! */
    uint16_t r;
    uint16_t g;
    uint16_t b;

    // YUV stuff
    float y;
    float u;
    float v; 

    // Indexed colour
    union 
    {
        uint16_t i16;

        uint8_t i16_high;
        uint8_t i8;
    };
    
    // the pixel format
    nv3_pgraph_pixel_format pixel_format; 
} nv3_color_expanded_t;

/* A simple ARGB format colour */
typedef struct nv3_color_argb_s
{
    uint8_t a;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} nv3_color_argb_t;

/* Generic 16-bit coordinate*/
typedef struct nv3_coord_16_s
{
    uint16_t x;
    uint16_t y;
} nv3_coord_16_t;

/* A big position format with 30:16 = y, 15:11 = nothing, 10:0 = x */
typedef struct nv3_coord_16_bigy_s
{
    // WHOSE IDEA WAS THIS?
    uint16_t x : 11; 
    uint8_t reserved : 5;
    uint16_t y : 15;
    bool reserved2 : 1;
} nv3_coord_16_bigy_t;

/* Generic 32-bit colour + 16-bit position */
typedef struct nv3_color_and_coord_16_s
{
    nv3_color_expanded_t color;
    nv3_coord_16_t points;
} nv3_color_and_coord_16_t;

/* "UTRI" type triangle */
typedef struct nv3_utri_s 
{
    uint32_t color;                             // use nv3_color_expanded_t but changed for alignment reasons
    nv3_coord_16_t point0;
    nv3_coord_16_t point1;
    nv3_coord_16_t point2;
} nv3_utri_t;

/* Generic 16-bit clip region */
typedef struct nv3_clip_16_s
{
    // The bounds of the clipping area.
    uint16_t left;
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
} nv3_clip_16_t; 

/* In case your positions weren't HIGH PRECISION enough */
typedef struct nv3_coord_32_s
{
    uint32_t x;
    uint32_t y;
} nv3_coord_32_t;

// COLOUR FORMATS

/* 
    Object Class 0x01 (real hardware, also 0x41) 
    0x12 (drivers)
    Beta factor
*/
typedef struct nv3_object_class_001
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;    // Set notifier context for DMA (context switch)
    uint32_t set_notify;
    uint32_t set_beta_factor_1d31;                       // 31:31 (?) value, 30:21 fraction
    // Put the rest of it here
} nv3_beta_factor_t;

/* 
    Object class 0x02 (real hardware)
    0x14/0x43 (drivers)
    Also 0x42 in context IDs
    Render operation used for things like blending. Appears to be 8-bit i.e. a ROP3 with 256 possible operations.
*/
typedef struct nv3_object_class_002
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;   // Set notifier context for DMA (context switch)
    uint32_t set_notify;                                // Set notifier
    uint8_t rop;                                        // ROP3 (ID = ????????)
} nv3_render_operation_t;

/* 
    Object class 0x03 (real hardware)
    0x15 (drivers)
    Also 0x43 in context IDs
    A chroma/color key, like in video editing
*/
typedef struct nv3_object_class_003
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;   // Set notifier context for DMA (context switch)
    uint32_t set_notify;                                // Set notifier
    uint32_t color;                                     // ROP3 (ID = ????????)
} nv3_chroma_key_t;

/* 
    Object class 0x04 (real hardware)
    0x15 (drivers)
    Also 0x44 in context IDs
    Plane mask
*/
typedef struct nv3_object_class_004
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;   // Set notifier context for DMA (context switch)
    uint32_t set_notify;                                // Set notifier
    uint32_t color;                                     // Plane mask
} nv3_plane_mask_t;

/* 
    Object class 0x05 (real hardware)
    0x19/0x1E/0x47 (drivers)
    Also 0x45 in context IDs
    Clipping rectangle used for various blitting operations
*/
typedef struct nv3_object_class_005
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;    // Set notifier context for DMA (context switch)
    uint32_t set_notify;            // Set notifier

    /* 16-bit precision */
    nv3_coord_16_t position;
    nv3_coord_16_t size;

} nv3_clipping_rectangle_t;

/* 
    Object Class 0x06 (real hardware)
    0x?? (drivers)
    Also 0x46 in context IDs
    A pattern used for blits. Wahey!
*/
typedef struct nv3_object_class_006
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;    // Set notifier context for DMA (context switch)
    uint32_t set_notify;            // Set notifier
    uint32_t shape;                 // 0 = 8x8, 1 = 64x1, 2 = 1x64
    uint32_t color0;                // Some 32-bit format (argb?)
    uint32_t color1;                // bit0=color0, bit1=color1
    uint32_t pattern[2];            // bit0=color0, bit1=color1
} nv3_pattern_t;

/* 
    Object Class 0x07 (real hardware)
    0x1E (drivers)
    Also 0x47 in context IDs
    A rectangle. Wahey!
*/
typedef struct nv3_object_class_007
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;       // Set notifier context for DMA (context switch)
    uint32_t set_notify;                                    // Set notifier         
    uint32_t color;                                         // The colour of the object.
    nv3_coord_16_t position[16];                         // The positions of up to 16 rectangles.
    nv3_coord_16_t size[16];                                 // The sizes of up to 16 rectangles
} nv3_rectangle_t;


/* In case your points weren't colourful enough */
typedef struct nv3_object_class_008_cpoint_s
{
    nv3_color_expanded_t color;                             // argb-format 32-bit color
    nv3_coord_16_t position;                             // position
} nv3_object_class_008_cpoint_t;

/* 
    Object Class 0x08 (real hardware)
    0x1A (drivers)
    Also 0x48 in context IDs
    A point: the revolutionary 3d graphics technique...
*/
typedef struct nv3_object_class_008
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;                    // Set notifier context for DMA (context switch)
    uint32_t set_notify;                            // Set notifier
    nv3_color_expanded_t color;                                 // argb?
    nv3_coord_16_t point[16];                    // Boring points 
    nv3_coord_32_t point32[16];                  // Allows you to have points with full 32-bit precision 
    nv3_object_class_008_cpoint_t cpoint[16];       // Allows you to have c o l o r f u l points! 
} nv3_point_t; 

/* Normal line... */
typedef struct nv3_object_class_009_line_s
{
    nv3_coord_16_t start; // presumably unless it's in reverse order...TODO: check the order
    nv3_coord_16_t end; 

} nv3_object_class_009_line_t;

/* THIRTY TWO BIT PRECISION line */
typedef struct nv3_object_class_009_line32_s
{
    uint32_t x0;
    uint32_t x1;
    uint32_t y0;
    uint32_t y1;
} nv3_object_class_009_line32_t;

/* nv3_object_class_009_polyline_t not implemented because it's just a duplicate of nv3_object_class_009_line */
/* nv3_object_class_009_polyline32_t not implemented because it's just a duplicate of nv3_object_class_009_line32 */


/* 
    Object Class 0x09 (real hardware)
    0x1B (drivers)
    Also 0x49 in context IDs
    It's a line, but also a polygon...
*/
typedef struct nv3_object_class_009
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;                    // Set notifier context for DMA (context switch)
    uint32_t set_notify;                            // Set notifier
    nv3_color_expanded_t color;                                 // argb?
    nv3_object_class_009_line_t line[16];           // List of line points (...)
    nv3_object_class_009_line32_t line32[8];
    nv3_object_class_009_line_t polyline[32];
    nv3_object_class_009_line32_t polyline32[16];
    nv3_color_and_coord_16_t cpolyline[16]; // List of line points and colours.
} nv3_line_t;

/* 
    Object Class 0x0A (real hardware)
    0x1c (drivers)
    Also 0x4a in context IDs
    
    This one is where nvidia reinvents the line, but without the starting or ending pixel.
    Seriously.
*/
typedef struct nv3_object_class_00A
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;                    // Set notifier context for DMA (context switch)

    uint32_t set_notify;                            // Set notifier

    nv3_color_expanded_t color;                                 // argb?
    nv3_object_class_009_line_t line[16];           // List of line points (...)
    nv3_object_class_009_line32_t line32[8];
    nv3_object_class_009_line_t polyline[32];
    nv3_object_class_009_line32_t polyline32[16];
    nv3_color_and_coord_16_t cpolyline[16]; // List of line points and colours.

} nv3_lin_t;

/* 
    Object Class 0x0B (real hardware)
    0x?? (drivers)
    Also 0x4b in context IDs.
    
    This is a triangle but seems to be obsolete. It's replaced with UD3D0Z / D3D5 Accelerated Triangle with Zeta Buffer. Does it even exist?
*/
typedef struct nv3_object_class_00B
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;               // Set notifier context for DMA (context switch)
    uint32_t set_notify;                                            // Set notifier
    nv3_color_expanded_t color;                                     // argb?
    // The points of the triangle.
    nv3_coord_16_t points[3];

    // Another way of filling out the points of the triangle
    uint32_t x0;
    uint32_t y0;
    uint32_t x1;
    uint32_t y1;
    uint32_t x2;
    uint32_t y2; 

    nv3_coord_16_t mesh[32];                                        // Some kind of mesh format. I guess a list of vertex positions?
    nv3_coord_32_t mesh32[16];                                      // Mesh with 32-bit format
    nv3_utri_t ctriangle[8];                                        // Triangles with colour
    nv3_color_and_coord_16_t ctrimesh[16];                          // Some kind of mesh format. I guess a list of vertex positions? with colours
    
} nv3_triangle_t;

/* 
    Object Class 0x0C (real hardware)
    0x0C (drivers)
    Also 0x4C in context IDs.
    
    GDI text acceleration for Windows 95.
*/
typedef struct nv3_object_class_00C
{
    /* Unclipped rect (basically class 0x07 )*/
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;                    // Set notifier context for DMA (context switch)
    uint32_t set_notify;                            // Set notifier
    uint32_t color_a;                               // Color for Clip A
    nv3_coord_16_t rect_a_position[64];
    nv3_coord_16_t rect_a_size[64];  
    /* Clipped rect */
    nv3_clip_16_t clip_b;
    uint32_t color_b;                               // Color for Clip B
    nv3_clip_16_t clipped_rect[64];
    /* Unclipped transparent bitmap */
    nv3_clip_16_t clip_c;
    uint32_t color1_c;
    nv3_coord_16_t size_c;
    nv3_coord_16_t point_c;
    uint32_t bitmap_c[128];
    /* Clipped transparent bitmap */
    nv3_clip_16_t clip_d;
    uint32_t color1_d;
    nv3_coord_16_t size_in_d;
    nv3_coord_16_t size_out_d;
    nv3_coord_16_t point_d;
    uint32_t bitmap_d[128];
    /* Clipped 1bpp bitmap */
    nv3_clip_16_t clip_e;
    uint32_t color0_e;
    uint32_t color1_e;
    nv3_coord_16_t size_in_e;
    nv3_coord_16_t size_out_e;
    nv3_coord_16_t point_e;
    uint32_t bitmap_e[128];
} nv3_win95_text_t;


/* 
    Object Class 0x0D (real hardware)
    0x?? (drivers)
    Also 0x4D in context IDs.
    
    Represents reformatting of an image in memory.
*/
typedef struct nv3_object_class_00D
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;                    // Set notifier context for DMA (context switch)
    uint32_t set_notify;                            // Set notifier
    uint32_t offset_in;
    uint32_t offset_out;
    uint32_t pitch_in;
    uint32_t pitch_out;
    uint32_t scanline_length;                        // Stride?
    uint32_t num_scanlines;
    uint8_t format;                                 // input increment 1 2 or 4, output increment 1 2 or 4 (represented by << 8)
    uint32_t buffer_notify;                         // Notify the Buffedr
} nv3_memory_to_memory_format_t;

/* 
    Object Class 0x0E (real hardware)
    0x?? (drivers)
    Also 0x4E in context IDs.
    
    Represents a scaled image coming from memory.
*/
typedef struct nv3_object_class_00E
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;
    uint32_t set_notify;
    nv3_coord_16_t clip_0;
    nv3_coord_16_t clip_1;
    nv3_coord_16_t rectangle_out_0;
    nv3_coord_16_t rectangle_out_1;
    // Calculus in a graphics card
    uint32_t delta_du_dx;
    uint32_t delta_dv_dy;
    nv3_coord_16_t size; // can be size_y if YUV420
    uint32_t pitch;
    uint32_t offset;
    uint32_t point;
    // YUV420 stuff
    uint32_t pitch_yuv420;
    uint32_t offset_y;
    uint32_t offset_u;
    uint32_t offset_v;
    uint32_t point_yuv420;
} nv3_scaled_image_from_memory_t;

// (0x0F does not exist)

/* 
    Object Class 0x10 (real hardware)
    0x?? (drivers)
    Also 0x50 in context IDs.
    
    Represents a screen to screen blit. I'm still figuring it out. 
*/

typedef struct nv3_object_class_010
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;
    uint32_t set_notify;
    nv3_coord_16_t point_in;
    nv3_coord_16_t point_out;
    nv3_coord_16_t size;
} nv3_blit_t;

/* 
    Object Class 0x11 (real hardware)
    0x?? (drivers)
    Also 0x51 in context IDs.
    
    Represents an image from the cpu (i guess from system memory)
*/
typedef struct nv3_object_class_011
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;
    uint32_t set_notify;
    nv3_coord_16_t point;
    nv3_coord_16_t size;
    nv3_coord_16_t size_in;
    nv3_color_expanded_t color[32];                           // The colour to use
} nv3_image_t;

/* 
    Object Class 0x12 (real hardware)
    0x?? (drivers)
    Also 0x52 in context IDs.
    
    Bitmap from CPU.
    It seems the difference is that an image is colour but a 
*/
typedef struct nv3_object_class_012
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;
    uint32_t set_notify;
    nv3_color_expanded_t color_0;
    nv3_color_expanded_t color_1;
    nv3_coord_16_t point;                        // Top left(?) of the bitmap
    nv3_coord_16_t size;
    nv3_coord_16_t size_in;
    uint32_t monochrome_bitmap[32];
} nv3_bitmap_t;

// 0x13 doesn't exist

/* 
    Object Class 0x14 (real hardware)
    0x?? (drivers)
    Also 0x54 in context IDs.
    
    Image Transfer to Memory
    It seems the difference is that an image is colour but a bitmap is b&w
*/
typedef struct nv3_object_class_014
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;
    uint32_t set_notify;
    nv3_coord_16_t point;
    nv3_coord_16_t size;
    uint32_t image_pitch;               // bytes per row
    uint32_t image_start;
} nv3_image_to_memory_t;

/* 
    Object Class 0x15 (real hardware)
    0x?? (drivers)
    Also 0x55 in context IDs.
    
    Stretched Image from CPU
    Seems to be, by the very large color array, the main class used in 2d acceleration.
*/
typedef struct nv3_object_class_015
{    
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;
    uint32_t set_notify;
    nv3_coord_16_t size_in;
    uint32_t delta_dx_du;
    uint32_t delta_dy_dv;
    nv3_coord_16_t point;
    nv3_coord_16_t size;
    uint32_t point12d4; /* todo: fraction struct */
    nv3_color_expanded_t color[32]; // The color to use
    // no reserve needed
} nv3_stretched_image_from_cpu_t; 

// 0x16 invalid

/* 
    Object Class 0x17 (real hardware)
    0x?? (drivers)
    Also 0x57 in context IDs.
    
    Direct3D 5.0 accelerated triangle with zeta buffer (combined z buffer and stencil buffer)
    This is the final boss of this GPU. True horror stands below.
*/

//
// NV3 D3D5: TEXTURING & PIXEL FORMATS
//

typedef enum nv3_d3d5_texture_pixel_format_e
{
    /*
    16-Bit Pixel Format
    5 bits red, 5 bits green, 5 bits alpha, "boolean" alpha
    */
    nv3_d3d5_pixel_format_le_a1r5g5b5 = 0,

    /*
    15-Bit Pixel Format (represented as 16)
    1 bit irrelevant, 5 bits red, 5 bits green, 5 bits alpha
    */
    nv3_d3d5_pixel_format_le_x1r5g5b5 = 1,

    /*
    16-Bit Pixel Format
    4 bits alpha, 4 bits red, 4 bits green, 4 bits blue
    */
    nv3_d3d5_pixel_format_le_a4r4g4b4 = 2,

    /*
    16-Bit Pixel Format
    5 bits red, 6 bits green, 5 bits blue 
    (Required nv3tweak...)
    */
    nv3_d3d5_pixel_format_le_r5g6b5 = 3,

} nv3_d3d5_texture_pixel_format;

/* Output format

   The output pixel format...I
*/
typedef enum nv3_d3d5_output_pixel_format_e
{
    /*
    32-Bit Pixel Format
    8 bits irrelevant, 8 bits red, 8 bits green, 8 bits blue
    */
    nv3_d3d5_output_pixel_format_le_x8r8g8b8 = 0,

    /*
    32-Bit Pixel Format
    8 bits irrelevant, 8 bits red, 8 bits green, 8 bits blue
    Is this even used?? The riva can't even do 32 bit colour in 3d...
    */
    nv3_d3d5_output_pixel_format_le_a8r8g8b8 = 1,


} nv3_d3d5_output_pixel_format;


/* Texture size 

   NOTE: ONLY 256X256 OR LOWER ARE SUPPORTED! 2048X2048X16 TAKES UP ENTIRE VRAM OF RIVA 128 ZX...
   I ASSUME THESE ARE INTERNALLY SCALED DOWN
*/
typedef enum nv3_d3d5_texture_size_e
{
    nv3_d3d5_texture_size_1x1 = 0,

    nv3_d3d5_texture_size_2x2 = 1,

    nv3_d3d5_texture_size_4x4 = 2,

    nv3_d3d5_texture_size_8x8 = 3,

    nv3_d3d5_texture_size_16x16 = 4,

    nv3_d3d5_texture_size_32x32 = 5,

    nv3_d3d5_texture_size_64x64 = 6,

    nv3_d3d5_texture_size_128x128 = 7,

    nv3_d3d5_texture_size_256x256 = 8,

    nv3_d3d5_texture_size_512x512 = 9,

    nv3_d3d5_texture_size_1024x1024 = 10,

    // Kind of infeasible considering hardware VRAM size limitations
    nv3_d3d5_texture_size_2048x2048 = 11,

    
} nv3_d3d5_texture_size;

/* Texture Wrapping Mode for U/V Coordinate Overflow 

*/
typedef enum nv3_d3d5_texture_wrap_mode_e
{
    // Map textures in a cylindrical fashion.
    nv3_d3d5_texture_wrap_mode_cylindrical = 0,

    // Simply wrap back to the start.
    nv3_d3d5_texture_wrap_mode_wrap = 1,

    // Mirror the texture.
    nv3_d3d5_texture_wrap_mode_mirror = 2,

    // Clamp to the last border pixel.
    nv3_d3d5_texture_wrap_mode_clamp = 3,
} nv3_d3d5_texture_wrap_mode; 


/* This is blending but isn't really considered to be it in the GPU for some reason
    What do we do with out input texel BEFORE processing it?
 */
typedef enum nv3_d3d5_dest_color_interpretation_e
{
    // Do nothing
    nv3_d3d5_source_color_normal = 0,

    // Invert Colour
    nv3_d3d5_source_color_inverse = 1,

    // Invert Alpha before Processing
    nv3_d3d5_source_color_alpha_inverse = 2,

    // Take Alpha as 1
    nv3_d3d5_source_color_alpha_one = 3,

} nv3_d3d5_dest_color_interpretation;

// The full texture format structure
typedef struct nv3_d3d5_texture_format_s
{
    uint16_t color_key_color_mask;
    bool color_key_enabled : 1;
    nv3_d3d5_texture_pixel_format color_format : 2;
    nv3_d3d5_texture_size size_min : 4;
    nv3_d3d5_texture_size size_max : 4;
} nv3_d3d5_texture_format_t;

//
// NV3 D3D5: INTERPOLATION
//

/* 
    ??????
    Interpolating between mip levels? (or for texture blending)
*/
typedef enum nv3_d3d5_interpolator_algorithm_e
{
    // Zero-order hold interpolation? 
    nv3_d3d5_interpolator_zoh = 0,

    // Zero-order hold (microsoft variant)?
    nv3_d3d5_interpolator_zoh_ms = 1,

    // Full-order hold interpolation?   
    nv3_d3d5_interpolator_foh = 2,

} nv3_d3d5_interpolator_algorithm;

//
// NV3 D3D5: Z-BUFFER
//

/* Probably the sorting algorithm */
typedef enum nv3_d3d5_zbuffer_type_e
{
    // Sort based on linear distance
    nv3_d3d5_zbuffer_linear = 0,

    // Sort based on distance from view frustum
    nv3_d3d5_zbuffer_screen = 1,

} nv3_d3d5_zbuffer_type;

// NV3 D3D5: WRITE CONTROL (SHARED BETWEEN ZETA BUFFER AND ALPHA)
typedef enum nv3_d3d5_buffer_write_control_e
{
    // Never write.
    nv3_d3d5_buffer_write_control_never = 0,

    // Only write the alpha.
    nv3_d3d5_buffer_write_control_alpha = 1,

    // Write both alpha and the zeta-buffer.
    nv3_d3d5_buffer_write_control_alpha_zeta = 2,

    // Write only the zeta-buffer
    nv3_d3d5_buffer_write_control_zeta = 3,

    // Write everything (alpha+z+zeta?)
    nv3_d3d5_buffer_write_control_always = 4,

    
} nv3_d3d5_buffer_write_control; 

//
// NV3 D3D5: BUFFER COMPARISON (SHARED BETWEEN ZETA BUFFER AND ALPHA CONTROL)
//

// Todo: Which direction? (i.e. is less than p1 < p2 or p2 < p1!)
typedef enum nv3_d3d5_buffer_comparison_type_e
{
    // !!!ILLEGAL COMPARISON TYPE!!!!
    nv3_d3d5_buffer_comparison_illegal = 0,

    // The (depth?) test always fails.
    nv3_d3d5_buffer_comparison_always_false = 1,
    
    // True if less than fail.
    nv3_d3d5_buffer_comparison_less_than = 2,

    // The test succeeds if equal.
    nv3_d3d5_buffer_comparison_equal = 3,
    
    // The test succeeds if less or equal.
    nv3_d3d5_buffer_comparison_less_or_equal = 4,
    
    // The test succeeds if greater.
    nv3_d3d5_buffer_comparison_greater = 5,
    
    // The test succeeds if the two elements are equal.
    nv3_d3d5_buffer_comparison_not_equal = 6,
    
    // The test succeeds if greater or equal
    nv3_d3d5_buffer_comparison_greater_or_equal = 7,
    
    // The test always succeeds.
    nv3_d3d5_buffer_comparison_always_true = 8,
        
} nv3_d3d5_buffer_comparison_type;

//
// NV3 D3D5: BLENDING
//

/* Render Operation for Blending */
typedef enum nv3_d3d5_blend_render_operation_e
{
    nv3_d3d5_blend_render_operation_and = 0,

    nv3_d3d5_blend_add_with_saturation = 1,

} nv3_d3d5_blend_render_operation;

typedef enum nv3_d3d5_blend_beta_factor_e
{
    nv3_d3d5_blend_beta_factor_srcalpha = 0,

    nv3_d3d5_blend_beta_factor_zero = 1,

} nv3_d3d5_blend_beta_factor;

typedef enum nv3_d3d5_blend_input0_e
{
    nv3_d3d5_blend_input0_srcalpha = 0,

    nv3_d3d5_blend_input0_zero = 1,

} nv3_d3d5_blend_input0;

typedef enum nv3_d3d5_blend_input1_e
{
    nv3_d3d5_blend_input1_destalpha = 0,

    nv3_d3d5_blend_input1_zero = 1,

} nv3_d3d5_blend_input1;

//
// NV3 D3D5: CULLING
//

typedef enum nv3_d3d5_culling_algorithm_e
{
    // Don't cull
    nv3_d3d5_culling_algorithm_none = 0,

    // Cull Clockwise around view frustum?
    nv3_d3d5_culling_algorithm_clockwise = 1,

    // Cull counterclockwise around view frustum?
    nv3_d3d5_culling_algorithm_counterclockwise = 2,

} nv3_d3d5_culling_algorithm;

/* Specular reflection parameters */
typedef struct nv3_d3d5_specular_s
{
    uint8_t i0 : 4;
    uint8_t i1 : 4;
    uint8_t i2 : 4;
    uint8_t i3 : 4;
    uint8_t i4 : 4;
    uint8_t i5 : 4;
    uint8_t fog; //table fog emulation?
} nv3_d3d5_specular_t;

//
// NV3 D3D5: MISC
//
typedef struct nv3_d3d5_texture_filter_s
{
    uint8_t spread_x;
    uint8_t spread_y;
    uint8_t mipmap;
    uint8_t turbo;
} nv3_d3d5_texture_filter_t;

//
// NV3 D3D5: OUTPUT CONTROL STRUCTURE
//

/* Output Control for D3D5 Triangles */
typedef struct nv3_d3d5_control_out_s
{
    nv3_d3d5_interpolator_algorithm ctrl_out_interpolator : 2;
    uint8_t reserved : 2;
    nv3_d3d5_texture_wrap_mode wrap_u : 2;                              // Controls wrapping mode of U texture coordinate
    nv3_d3d5_texture_wrap_mode wrap_v : 2;                              // Controls wrapping move of V texture coordinate
    nv3_d3d5_output_pixel_format output_pixel_format : 1;
    bool reserved2 : 1;
    nv3_d3d5_dest_color_interpretation dest_color_interpretation : 2;
    nv3_d3d5_culling_algorithm culling_algorithm : 2;
    bool reserved3 : 1;
    nv3_d3d5_zbuffer_type zbuffer_type : 1;
    nv3_d3d5_buffer_comparison_type zeta_buffer_compare : 4;
    nv3_d3d5_buffer_write_control zeta_write : 3;
    bool reserved4 : 1;
    nv3_d3d5_buffer_write_control color_write : 3;
    bool reserved5 : 1;
    nv3_d3d5_blend_render_operation blend_rop : 1;
    nv3_d3d5_blend_input0 blend_input0 : 1;
    nv3_d3d5_blend_input1 blend_input1 : 1;
} nv3_d3d5_control_out_t;

typedef struct nv3_d3d5_alpha_control_s
{
    uint8_t alpha_key;
    nv3_d3d5_buffer_comparison_type zeta_buffer_compare : 4;
    uint32_t reserved : 20;
} nv3_d3d5_alpha_control_t;

//
// NV3 D3D5: Triangle Coordinates
//
typedef struct nv3_d3d5_coordinate_s
{
    nv3_d3d5_specular_t specular_reflection_parameters;     
    nv3_color_expanded_t color;                              // YOU HAVE TO FLIP THE ENDIANNESS. NVIDIA??? WHAT???

    // Seems more plausible for these specifically to be floats.
    // Also makes my life easier...
    float x;                     // X coordinate in 3d space of the triangle
    float y;                     // Y coordinate in 3d space of the triangle
    float z;                     // Z coordinate in 3d space of the triangle
    float m;                     // 1/W for projection
    float u;                     // U coordinate within texture for the (top left?) of the triangle where sampling starts.
    float v;                     // V coordinate within texture for the (top left?) of the triangle where sampling starts.
} nv3_d3d5_coordinate_t;

typedef struct nv3_object_class_017
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;       // Set notifier context for DMA (context switch)
    uint32_t set_notify;                                    // Set notifier         
    uint32_t texture_offset;
    nv3_d3d5_texture_format_t texture_format;
    nv3_d3d5_texture_filter_t texture_filter;
    nv3_color_expanded_t fog_color;                          // Alpha is ignored here!
    nv3_d3d5_control_out_t control_out;
    nv3_d3d5_alpha_control_t alpha_control;

    nv3_d3d5_coordinate_t coordinate_points[128];           // The points we are rendering.
    /* No placeholder needed, it really is that long. */
} nv3_d3d5_accelerated_triangle_with_zeta_buffer_t;


// Color and Zeta Buffer algorithm 
typedef struct nv3_zeta_buffer_s
{
    nv3_color_expanded_t color;
    uint32_t zeta;                                            // 16 bits z, 8 bits stenciul
} nv3_zeta_buffer_t;

typedef struct nv3_object_class_018
{  
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;
    uint32_t set_notify;
    nv3_d3d5_control_out_t control_out; 
    nv3_d3d5_alpha_control_t alpha_control;
    nv3_coord_16_t point;
    nv3_zeta_buffer_t zeta[8];
} nv3_point_with_zeta_buffer_t;

/* 0x19, 0x1A, 0x1B don't exist */

/* WHY IS THE FORMAT DIFFERENT TO THE REST OF THE GPU? 
   They are making it look like a bitfield but it's hex?

   THEY ARE ALL LITTLE ENDIAN
*/
typedef enum nv3_object_class_01C_pixel_format_e
{
    // Y8P4
    // 12-bits (Y8 - Planar YUV 8 bits (Y value only), 4 bits of indexed colour too?
    nv3_image_in_memory_pixel_format_le_y8_p4 = 0x1010000,

    // Y16P2
    // 16-bits (Y16) - Planar YUV 16 bits (Y value only), 2 bits of indexed colour too?
    nv3_image_in_memory_pixel_format_le_y16_p2 = 0x1010101,

    /* 1 unused bit, 555 15-bit format, p2(?) */
    nv3_image_in_memory_pixel_format_x1r5g5b5_p2 = 0x1000000,

    // X8G8B8R8, 24-bit colour (or 24-bit colour with alpha)
    nv3_image_in_memory_pixel_format_x8g8b8r8 = 0x1,
} nv3_object_class_01C_pixel_format; 

typedef struct nv3_object_class_01C
{
    nv3_class_ctx_switch_method_t set_notify_ctx_dma;   // Set notifier context for DMA (context switch)
    uint32_t set_notify;                                // Set notifier
    nv3_object_class_01C_pixel_format format;           // Completely different from everything else
    uint32_t pitch;                                     // 16-bit
    uint32_t linear_address;                            // 22-bit: Linear address in vram.
} nv3_image_in_memory_t;

// See envytools. This is where we finally end up after this mess, it allows parameters to be passed to the methods.
typedef struct nv3_grobj_s
{
    uint32_t grobj_0;
    uint32_t grobj_1;
    uint32_t grobj_2;
    uint32_t grobj_3;
} nv3_grobj_t;
// TODO: PATCHCORDS!!!! TO LINK ALL OF THIS TOGETHER!!!

// PIO Subchannel info
#define NV3_SUBCHANNEL_PIO_IS_PFIFO_FREE                0x0010
#define NV3_SUBCHANNEL_PIO_ALWAYS_ZERO_START            0x0012
#define NV3_SUBCHANNEL_PIO_ALWAYS_ZERO_END              0x0017