/** @file dlo_mode.c
 *
 *  @brief Implementation of the screen mode-related functions.
 *
 *  See dlo_mode.h for more information.
 *
 *  DisplayLink Open Source Software (libdlo)
 *  Copyright (C) 2009, DisplayLink
 *  www.displaylink.com
 *
 *  This library is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU Library General Public License as published by the Free
 *  Software Foundation; LGPL version 2, dated June 1991.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE. See the GNU Library General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <string.h>
#include "dlo_defs.h"
#include "dlo_mode.h"
#include "dlo_data.h"
#include "dlo_usb.h"


/* File-scope defines ------------------------------------------------------------------*/


/** Pre-defined EDID header used to check that data read from a device is valid.
 */
const uint8_t header[8] =
{
  0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

/** Constant used for deciding if a host is big- or little endian.
 */
const uint32_t endian = 1;

/** Unlock video registers command.
 */
#define WRITE_VIDREG_LOCK "\xAF\x20\xFF\x00"

/** Lock video registers command.
 */
#define WRITE_VIDREG_UNLOCK "\xAF\x20\xFF\xFF\xAF\xA0"

/** Return non-zero if the host is big endian or zero if the host is little endian.
 */
#define IS_BIGENDIAN() ((*(char*)&endian) == '\0')

/** Convert a 32 bit little endian value into a host byte-order value.
 *
 *  @param  val  Little-endian 32 bit value to convert.
 *
 *  @return  Value in host byte order.
 */
#define LETOHL(val) (IS_BIGENDIAN() ? swap_endian_l(val) : val)

/** Convert a 16 bit little endian value into a host byte-order value.
 *
 *  @param  val  Little-endian 16 bit value to convert.
 *
 *  @return  Value in host byte order.
 */
#define LETOHS(val) (IS_BIGENDIAN() ? swap_endian_s(val) : val)

/** Initialise a @a dlo_mode_data array entry.
 *
 *  @param  idx       Array index.
 *  @param  mwidth    Mode width (pixels).
 *  @param  mheight   Mode height (pixels).
 *  @param  mrefresh  Mode refresh rate (Hz).
 *  @param  mbpp      Mode colour depth (bits per pixel).
 */
#define init_mode(idx, mwidth, mheight, mrefresh, mbpp)                                                    \
{                                                                                                          \
  dlo_mode_data[idx].width      = mwidth;                                                                  \
  dlo_mode_data[idx].height     = mheight;                                                                 \
  dlo_mode_data[idx].refresh    = mrefresh;                                                                \
  dlo_mode_data[idx].bpp        = mbpp;                                                                    \
  dlo_mode_data[idx].data       = DLO_MODE_DATA_##mwidth##_##mheight##_##mrefresh##_##mbpp##_0;            \
  dlo_mode_data[idx].data_sz    = DSIZEOF(DLO_MODE_DATA_##mwidth##_##mheight##_##mrefresh##_##mbpp##_0);   \
  dlo_mode_data[idx].mode_en    = DLO_MODE_ENABLE_##mwidth##_##mheight##_##mrefresh##_##mbpp##_0;          \
  dlo_mode_data[idx].mode_en_sz = DSIZEOF(DLO_MODE_ENABLE_##mwidth##_##mheight##_##mrefresh##_##mbpp##_0); \
  dlo_mode_data[idx].low_blank  = false;                                                                   \
}


/* External-scope inline functions -----------------------------------------------------*/


/** Swap the endianness of a long (four byte) integer.
 *
 *  @param  val  Integer to alter.
 *
 *  @return  Value with swapped endianness.
 */
inline uint32_t swap_endian_l(uint32_t val)
{
  return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | (( val >> 24) & 0xFF);
}


/** Swap the endianness of a short (two byte) integer.
 *
 *  @param  val  Integer to alter.
 *
 *  @return  Value with swapped endianness.
 */
inline uint16_t swap_endian_s(uint16_t val)
{
  return ((val >> 8) & 0xFF) | ((val << 8) & 0xFF00);
}


extern uint32_t swap_endian_l(uint32_t val);
extern uint16_t swap_endian_s(uint16_t val);


/* File-scope types --------------------------------------------------------------------*/


/** Structure to hold information about a specific screen mode.
 */
typedef struct dlo_mode_data_s
{
  uint16_t width;        /**< Width (pixels). */
  uint16_t height;       /**< Height (pixels). */
  uint8_t  refresh;      /**< Refresh rate (Hz). */
  uint8_t  bpp;          /**< Colour depth (bits per pixel). */
  char    *data;         /**< Block of mode data. */
  size_t   data_sz;      /**< Size of mode data block (bytes). */
  char    *mode_en;      /**< Block of mode enable data. */
  size_t   mode_en_sz;   /**< Size of mode enable data block (bytes). */
  bool     low_blank;    /**< Screen mode has reduced blanking. */
} dlo_mode_data_t;       /**< A struct @a dlo_mode_data_s. */


/** Established timing information, derived from EDID.
 */
typedef struct est_timing_s
{
  uint16_t width;        /**< Width of mode (pixels). */
  uint16_t height;       /**< Height of mode (pixels). */
  uint8_t  refresh;      /**< Mode refresh rate (Hz). */
} est_timing_t;          /**< A struct @a est_timings_s. */


/** Vendor/product information.
 */
typedef struct edid_prod_id_s
{
  uint16_t manuf_name;   /**< ID manufacturer code. */
  uint16_t prod_code;    /**< ID product code. */
  uint32_t serial_num;   /**< ID serial number. */
  uint8_t  manuf_wk;     /**< Week of manufacture. */
  uint8_t  manuf_yr;     /**< Year of manufacture. */
} edid_prod_id_t;        /**< A struct @a edid_prod_id_s. */


/** EDID structure information.
 */
typedef struct edid_struct_vsn_s
{
  uint8_t number;        /**< Version number. */
  uint8_t revision;      /**< Revision number. */
} edid_struct_vsn_t;     /**< A struct @a edid_struct_vsn_s. */


/** Basic dislpay parameters/features.
 */
typedef struct edid_basic_params_s
{
  uint8_t input_def;     /**< Video input definition. */
  uint8_t max_horiz_sz;  /**< Maximum horizontal image size (cm). */
  uint8_t max_vert_sz;   /**< Maximum vertical image size (cm). */
  float   gamma;         /**< Display transfer characteristic (gamma). */
  uint8_t features;      /**< Feature support. */
} edid_basic_params_t;   /**< A struct @a edid_basic_params_s. */


/** Colour characteristics.
 */
typedef struct edid_colours_s
{
  uint16_t red_grn_low;  /**< Red/green low bits. */
  uint16_t blu_wht_low;  /**< Blue/white low bits. */
  uint16_t red_x;        /**< Red-x (bits 9-2). */
  uint16_t red_y;        /**< Red-y (bits 9-2). */
  uint16_t grn_x;        /**< Green-x (bits 9-2). */
  uint16_t grn_y;        /**< Green-y (bits 9-2). */
  uint16_t blu_x;        /**< Blue-x (bits 9-2). */
  uint16_t blu_y;        /**< Blue-y (bits 9-2). */
  uint16_t wht_x;        /**< White-x (bits 9-2). */
  uint16_t wht_y;        /**< White-y (bits 9-2). */
} edid_colours_t;        /**< A struct @a edid_colours_s. */


/** Established timings.
 */
typedef struct edid_est_timings_s
{
  uint8_t timings[2];    /**< Bitfields of established timings. */
  uint8_t resvd;         /**< Manufacturer's reserved timings. */
} edid_est_timings_t;    /**< A struct @a edid_est_timings_s. */


/** Standard timing identification.
 */
typedef struct edid_std_timing_s
{
  uint16_t timing_id[8];  /**< Standard timing identification. */
} edid_std_timing_t;      /**< A struct @a edid_std_timing_s. */


/** Detailed timing description.
 */
typedef struct edid_detail_s
{
  bool     is_detail;        /**< Flag is set to true - the structure is an @a edid_detail_t. */
  float    pixclk;           /**< Timing parameter. */
  uint32_t hactl;            /**< Timing parameter. */
  uint32_t hblankl;          /**< Timing parameter. */
  uint32_t hactblankh;       /**< Timing parameter. */
  uint32_t vactl;            /**< Timing parameter. */
  uint32_t vblankl;          /**< Timing parameter. */
  uint32_t vactblankh;       /**< Timing parameter. */
  uint32_t hsyncoffl;        /**< Timing parameter. */
  uint32_t hsyncwl;          /**< Timing parameter. */
  uint32_t vsyncoffvsyncwl;  /**< Timing parameter. */
  uint32_t synch;            /**< Timing parameter. */
  uint32_t hsizel;           /**< Timing parameter. */
  uint32_t vsizel;           /**< Timing parameter. */
  uint32_t hvsizeh;          /**< Timing parameter. */
  uint8_t  hbord;            /**< Timing parameter. */
  uint8_t  vbord;            /**< Timing parameter. */
  uint8_t  flags;            /**< Timing parameter. */
} edid_detail_t;             /**< A struct @a edid_detail_s. */


/** Monitor descriptor.
 */
typedef struct edid_monitor_desc_s
{
  bool    is_detail;     /**< Flag is set to false - the structure is not an @a edid_detail_t. */
  uint8_t unknown[18];   /**< Contents of block are unknown. */
} edid_monitor_desc_t;   /**< A struct @a edid_monitor_desc_s. */


/** A timing description block may be either a @a edid_detail_t or a @a edid_monitor_desc_t.
 */
typedef union edid_timing_desc_u
{
  edid_detail_t       detail;  /**< Either a detailed timing description. */
  edid_monitor_desc_t desc;    /**< Or a monitor descriptor. */
} edid_timing_desc_t;          /**< A struct @a edid_timing_desc_s. */


/** An EDID extension block.
 */
typedef struct edid_ext_block_s
{
  uint8_t unknown[128];  /**< Contents of block are unknown. */
} edid_ext_block_t;      /**< A struct @a edid_ext_block_s. */


/** An EDID structure.
 */
typedef struct edid_format_s
{
  edid_prod_id_t      product;      /**< Vendor/product information. */
  edid_struct_vsn_t   version;      /**< EDID structure information. */
  edid_basic_params_t basic;        /**< Basic dislpay parameters/features. */
  edid_colours_t      colours;      /**< Colour characteristics. */
  edid_est_timings_t  est_timings;  /**< Established timings. */
  edid_std_timing_t   std_timings;  /**< Standard timing identification. */
  edid_timing_desc_t  timings[4];   /**< Timing descriptions. */
  uint8_t             ext_blocks;   /**< Number of extension blocks. */
} edid_format_t;                    /**< A struct @a edid_format_s. */


/* External scope variables ------------------------------------------------------------*/


/* File-scope variables ----------------------------------------------------------------*/


/** Array of hard-wired screen mode definitions.
 */
static dlo_mode_data_t dlo_mode_data[DLO_MODE_DATA_NUM];


/** Mode information corresponding with flag bits in EDID establisted timings bytes.
 */
const est_timing_t est_timings[24] =
{
  { 800, 600, 60 },        /* bit 0 */
  { 800, 600, 56 },        /* bit 1 */
  { 640, 480, 75 },        /* bit 2 */
  { 640, 480, 72 },        /* bit 3 */
  { 640, 480, 67 },        /* bit 4 */
  { 640, 480, 60 },        /* bit 5 */
  { 720, 400, 88 },        /* bit 6 */
  { 720, 400, 70 },        /* bit 7 */
  { 1280, 1024, 75 },      /* bit 8 */
  { 1024, 768, 75 },       /* bit 9 */
  { 1024, 768, 70 },       /* bit 10 */
  { 1024, 768, 60 },       /* bit 11 */
  { 1024, 768, 87 },       /* bit 12 */
  { 832, 624, 75 },        /* bit 13 */
  { 800, 600, 75 },        /* bit 14 */
  { 800, 600, 72 },        /* bit 15 */
  { 0, 0, 0 },             /* bit 16 */
  { 0, 0, 0 },             /* bit 17 */
  { 0, 0, 0 },             /* bit 18 */
  { 0, 0, 0 },             /* bit 19 */
  { 0, 0, 0 },             /* bit 20 */
  { 0, 0, 0 },             /* bit 21 */
  { 0, 0, 0 },             /* bit 22 */
  { 1152, 870, 75 }        /* bit 23 */
};


/* File-scope function declarations ----------------------------------------------------*/


/** Append a video register setting command onto the specified char block.
 *
 *  @param  dev  Pointer to @a dlo_device_t structure.
 *  @param  reg  Register number (0..255).
 *  @param  val  Value to set (byte).
 *
 *  @return  Return code, zero for no error.
 */
static dlo_retcode_t vreg(dlo_device_t * const dev, uint8_t reg, uint8_t val);


/** Append a video register setting command onto the specified char block.
 *
 *  @param  dev  Pointer to @a dlo_device_t structure.
 *  @param  reg  Register number (0..255).
 *  @param  val  Value to set (byte).
 *
 *  @return  Return code, zero for no error.
 */
static dlo_retcode_t vbuf(dlo_device_t * const dev, const char * const buf, size_t len);


/** Check an EDID checksum to see if it is invalid.
 *
 *  @param  ptr   Pointer to base of EDID structure data.
 *  @param  size  Size of data (bytes).
 *
 *  @return  true if checksum is incorrect, false if OK.
 */
static bool bad_edid_checksum(const uint8_t * const ptr, const size_t size);


/** Parse an EDID detailed timing descriptor/mode descriptor.
 *
 *  @param  desc  Pointer to descriptor structure to initialise.
 *  @param  ptr   Pointer to base of descriptor in EDID data to parse.
 */
static void parse_edid_detail_desc(edid_timing_desc_t * const desc, const uint8_t * const ptr);


/** Parse EDID colour characteristics.
 *
 *  @param  cols  Pointer to colour structure to initialise.
 *  @param  ptr   Pointer to base of colour characteristics block to parse from EDID data.
 */
static void parse_edid_colours(edid_colours_t * const cols, const uint8_t * const ptr);


/** Look for a mode definition amongst the list of known modes.
 *
 *  @param  dev      Pointer to @a dlo_device_t structure.
 *  @param  width    Width of desired display (pixels).
 *  @param  height   Hieght of desired display (pixels) - zero to select first available.
 *  @param  refresh  Desired refresh rate (Hz) - zero to select first available.
 *  @param  bpp      Colour depth (bits per pixel) - zero to select first available.
 *
 *  @return  Mode number of the best matching mode definition (or @a DLO_INVALID_MODE if none found).
 */
static dlo_modenum_t get_mode_number(dlo_device_t * const dev, const uint16_t width, const uint16_t height, const uint8_t refresh, const uint8_t bpp);


/** Given a bitmap and a mode number, set the current screen mode.
 *
 *  @param  bmp   Bitmap to set as the current rastered display mode.
 *  @param  mode  Mode number for best match to the supplied bitmap.
 *
 *  @return  Return code, zero for no error.
 */
static dlo_retcode_t mode_select(dlo_device_t * const dev, const dlo_mode_t * const desc, const dlo_modenum_t mode);


/** Look up the specified mode and add to the supported list if found.
 *
 *  @param  dev      Pointer to @a dlo_device_t structure.
 *  @param  idx      Index - incrememnted if the mode was found.
 *  @param  width    Width of the mode to look for (pixels).
 *  @param  height   Height of the mode to look for (pixels).
 *  @param  refresh  Refresh rate of the mode to look for (Hz).
 *
 *  @return  Updated index (as @a idx if mode not found, else incremented by one).
 */
static uint16_t add_supported(dlo_device_t * const dev, uint16_t idx, const uint16_t width, const uint16_t height, const uint8_t refresh);


/** Build a list of supported modes, based upon the supplied EDID information.
 *
 *  @param  dev   Device to update.
 *  @param  edid  Pointer to structure holding parsed EDID information.
 *
 *  @return  Return code, zero for no error.
 */
static dlo_retcode_t lookup_edid_modes(dlo_device_t * const dev, const edid_format_t * const edid);


/** Program the base addresses of the video display in the device.
 *
 *  @param  dev    Pointer to @a dlo_device_t structure.
 *  @param  base   Address of the base of the 16 bpp segment.
 *  @param  base8  Address of the base of the 8 bpp segment.
 *
 *  @return  Return code, zero for no error.
 *
 *  Note: this call first will cause any buffered commands to be sent to the device then
 *  the set base commands are sent. The buffer to that device is thus flushed.
 */
static dlo_retcode_t set_base(dlo_device_t * const dev, const dlo_ptr_t base, const dlo_ptr_t base8);


/* Public function definitions ---------------------------------------------------------*/


dlo_retcode_t dlo_mode_init(const dlo_init_t flags)
{
  init_mode(0,  1920, 1080, 60, 24);
  init_mode(1,  1600, 1200, 60, 24);
  init_mode(2,  1400, 1050, 85, 24);
  init_mode(3,  1400, 1050, 75, 24);
  init_mode(4,  1400, 1050, 60, 24);
  init_mode(5,  1400, 1050, 60, 24);
  init_mode(6,  1366, 768,  60, 24);
  init_mode(7,  1360, 768,  60, 24);
  init_mode(8,  1280, 960,  85, 24);
  init_mode(9,  1280, 960,  60, 24);
  init_mode(10, 1280, 800,  60, 24);
  init_mode(11, 1280, 768,  85, 24);
  init_mode(12, 1280, 768,  75, 24);
  init_mode(13, 1280, 1024, 85, 24);
  init_mode(14, 1280, 1024, 75, 24);
  init_mode(15, 1280, 1024, 60, 24);
  init_mode(16, 1280, 768,  60, 24);
  init_mode(17, 1152, 864,  75, 24);
  init_mode(18, 1024, 768,  85, 24);
  init_mode(19, 1024, 768,  75, 24);
  init_mode(20, 1024, 768,  70, 24);
  init_mode(21, 1024, 768,  60, 24);
  init_mode(22, 848,  480,  60, 24);
  init_mode(23, 800,  600,  85, 24);
  init_mode(24, 800,  600,  75, 24);
  init_mode(25, 800,  600,  72, 24);
  init_mode(26, 800,  600,  60, 24);
  init_mode(27, 800,  600,  56, 24);
  init_mode(28, 800,  480,  60, 24);
  init_mode(29, 720,  400,  85, 24);
  init_mode(30, 720,  400,  70, 24);
  init_mode(31, 640,  480,  85, 24);
  init_mode(32, 640,  480,  75, 24);
  init_mode(33, 640,  480,  73, 24);
  init_mode(34, 640,  480,  60, 24);

  return dlo_ok;
}


dlo_retcode_t dlo_mode_final(const dlo_final_t flags)
{
  return dlo_ok;
}


dlo_mode_t *dlo_mode_from_number(const dlo_modenum_t num)
{
  static dlo_mode_t mode;

  if (num < DLO_MODE_DATA_NUM)
  {
    mode.view.width  = dlo_mode_data[num].width;
    mode.view.height = dlo_mode_data[num].height;
    mode.view.bpp    = dlo_mode_data[num].bpp;
    mode.view.base   = 0;
    mode.refresh     = dlo_mode_data[num].refresh;
    return &mode;
  }
  return NULL;
}


dlo_modenum_t dlo_mode_lookup(dlo_device_t * const dev, const uint16_t width, const uint16_t height, const uint8_t refresh, uint8_t bpp)
{
  /* Check that the requested screen mode is supported */
  //DPRINTF("mode: lookup: %ux%u @ %u Hz, %u bpp\n", width, height, refresh, bpp);
  return bpp != 24 ? DLO_INVALID_MODE : get_mode_number(dev, width, height, refresh, bpp);
}


dlo_retcode_t dlo_mode_change(dlo_device_t * const dev, const dlo_mode_t * const desc, dlo_modenum_t mode)
{
  /* If no mode number was specified on entry, try looking one up for the supplied bitmap */
  if (mode == DLO_INVALID_MODE)
    mode = get_mode_number(dev, desc->view.width, desc->view.height, 0, desc->view.bpp);

  /* Change mode or return an error */
  return mode_select(dev, desc, mode);
}


dlo_retcode_t dlo_mode_parse_edid(dlo_device_t * const dev, const uint8_t * const ptr, const size_t size)
{
  static edid_format_t edid;
  uint32_t             i;

  /* Copy the header bytes straight into our structure (assumes the structure is correct size!) */
  ASSERT(EDID_STRUCT_SZ == size);
  if (memcmp(header, ptr, sizeof(header)))
    return dlo_err_edid_fail;

  if (bad_edid_checksum(ptr, size))
    return dlo_err_edid_fail;

#if (defined(DEBUG) && 0) /* If you really want this verbose debug output, change the 0 to a 1 */
  uint32_t j;
  /* Parse the vendor/product information */
  for (j = 0; j < 128; j += 8)
  {
    DPRINTF("mode: edid: raw: ");
    for (i = 0; i < 8; i++)
      DPRINTF("%02X=%02X ", i + j, RD_B(ptr + i + j));
    DPRINTF("\n");
  }
#endif

  edid.product.manuf_name = LETOHS(RD_S(ptr + 0x08));
  edid.product.prod_code  = LETOHS(RD_S(ptr + 0x0A));
  edid.product.serial_num = LETOHL(RD_L(ptr + 0x0C));
  edid.product.manuf_wk   = RD_B(ptr + 0x10);
  edid.product.manuf_yr   = RD_B(ptr + 0x11);
  //DPRINTF("mode: edid: manuf &%X prod &%X serial &%X wk %u yr %u\n",
  //        edid.product.manuf_name, edid.product.prod_code, edid.product.serial_num,
  //        edid.product.manuf_wk, edid.product.manuf_yr + 1990);

  /* Parse the EDID structure information */
  edid.version.number   = RD_B(ptr + 0x12);
  edid.version.revision = RD_B(ptr + 0x13);
  //DPRINTF("edid: version &%02X revision &%02X\n", edid.version.number, edid.version.revision);

  /* Parse the basic dislpay parameters/features */
  edid.basic.input_def    = RD_B(ptr + 0x14);
  edid.basic.max_horiz_sz = RD_B(ptr + 0x15);
  edid.basic.max_vert_sz  = RD_B(ptr + 0x16);
  edid.basic.gamma        = (100.0 + RD_B(ptr + 0x17)) / 100.0;
  edid.basic.features     = RD_B(ptr + 0x18);

  /* Parse the colour characteristics */
  parse_edid_colours(&(edid.colours), ptr + 0x19);

  /* Parse the established timings */
  edid.est_timings.timings[0] = RD_B(ptr + 0x23);
  edid.est_timings.timings[1] = RD_B(ptr + 0x24);
  edid.est_timings.resvd      = RD_B(ptr + 0x25);

  /* Parse the bits at the end of the EDID structure */
  edid.ext_blocks = RD_B(ptr + 0x7E);

  /* Parse all of the standard timing identification */
  for (i = 0; i < sizeof(edid.std_timings.timing_id); i++)
    edid.std_timings.timing_id[i] = RD_B(ptr + 0x26 + i);

  /* Parse all of the detailed timing descriptions (or monitor descriptors) */
  for (i = 0; i < 4; i++)
    parse_edid_detail_desc(&(edid.timings[i]), ptr + 0x36 + (i * 0x12));

  return lookup_edid_modes(dev, &edid);
}


void use_default_modes(dlo_device_t * const dev)
{
  uint32_t i;

  for (i = 0; i < DLO_MODE_DATA_NUM; i++)
    dev->supported[i] = (dlo_modenum_t)i;
}


/* File-scope function definitions -----------------------------------------------------*/


static dlo_retcode_t vreg(dlo_device_t * const dev, uint8_t reg, uint8_t val)
{
  if (dev->bufend - dev->bufptr < 4)
    return dlo_err_buf_full;

  *(dev->bufptr)++ = '\xAF';
  *(dev->bufptr)++ = '\x20';
  *(dev->bufptr)++ = reg;
  *(dev->bufptr)++ = val;

  return dlo_ok;
}


static dlo_retcode_t vbuf(dlo_device_t * const dev, const char * buf, size_t len)
{
  if (dev->bufend - dev->bufptr < len)
    return dlo_err_buf_full;

  while (len--)
    *(dev->bufptr)++ = *buf++;

  return dlo_ok;
}


static dlo_modenum_t get_mode_number(dlo_device_t * const dev, const uint16_t width, const uint16_t height, const uint8_t refresh, const uint8_t bpp)
{
  dlo_modenum_t idx;
  uint32_t       num;

  /* Look for all matching modes in our array of video timing structures.
   *
   * Note: if we don't have EDID data for the monitor attached to the device
   * we simply look through all the modes we have, rather than only looking
   * at the supported modes list.
   */
  for (idx = 0; idx < DLO_MODE_DATA_NUM; idx++)
  {
    /* Read the mode number from the device's supported modes array */
    num = dev->supported[idx];
    if (num == DLO_INVALID_MODE)
      break;

    /* This sequence of if statements looks odd, take care if you decide to 'optimise' it! */
    if (dlo_mode_data[num].width != width)
      continue;
    if (bpp && dlo_mode_data[num].bpp != bpp)
      continue;
    if (height && dlo_mode_data[num].height != height)
      continue;
    if (refresh && dlo_mode_data[num].refresh != refresh)
      continue;

    /* If we're satisfied with the tests above, then return the mode number */
    return num;
  }

  /* No matches found, return an invalid mode number */
  return DLO_INVALID_MODE;
}


static dlo_retcode_t mode_select(dlo_device_t * const dev, const dlo_mode_t * const desc, const dlo_modenum_t mode)
{
  if (mode >= DLO_MODE_DATA_NUM || mode == DLO_INVALID_MODE)
    return dlo_err_bad_mode;

  /* Base address must be aligned to a two byte boundary */
  if (desc->view.base & 1)
    return dlo_err_bad_mode;

  /* Flush the command buffer */
  if (dlo_ok != dlo_usb_write(dev))
    return DLO_INVALID_MODE;

  dev->mode.view.base = desc->view.base;
  dev->base8          = desc->view.base + (BYTES_PER_16BPP * desc->view.width * desc->view.height);
  ERR(set_base(dev, dev->mode.view.base, dev->base8));

  /* Only change mode if the new raster bitmap's characteristics differ from the current.
   *
   * Note: don't compare reduced blanking flag because if the rest is the same, we can use the
   * same blanking type. However, there's an outside chance that the low_blank hint was changed
   * since entering the current mode in which case you may well want a mode change to happen but
   * we'll just hope that never happens (seems like a very unlikely scenario).
   */
  if (desc->view.width  != dev->mode.view.width  ||
      desc->view.height != dev->mode.view.height ||
      desc->view.bpp    != dev->mode.view.bpp)
  {
    ERR(dlo_usb_chan_sel(dev, dlo_mode_data[mode].mode_en, dlo_mode_data[mode].mode_en_sz));
    ERR(dlo_usb_write_buf(dev, dlo_mode_data[mode].data, dlo_mode_data[mode].data_sz));
    ERR(dlo_usb_chan_sel(dev, DLO_MODE_POSTAMBLE, DSIZEOF(DLO_MODE_POSTAMBLE)));
  }

  /* Update the device with the new mode details */
  dev->mode         = *desc;
  dev->mode.refresh = dlo_mode_data[mode].refresh;
  dev->low_blank    = dlo_mode_data[mode].low_blank;

  //DPRINTF("mode: select: %ux%u @ %u Hz %u bpp (base &%X base8 &%X low? %s)\n",
  //        dev->mode.view.width, dev->mode.view.height, dev->mode.refresh, dev->mode.view.bpp,
  //        dev->mode.view.base, dev->base8, dev->low_blank ? "yes" : "no");

  /* Flush the command buffer */
  ERR(dlo_usb_write(dev));

  /* Return a warning for DL160 modes */
  return (mode < DLO_DL120_MODES) ? dlo_warn_dl160_mode : dlo_ok;
}


static uint16_t add_supported(dlo_device_t * const dev, uint16_t idx, const uint16_t width, const uint16_t height, const uint8_t refresh)
{
  dlo_modenum_t num;

  num = get_mode_number(dev, width, height, refresh, 24);
  if (num != DLO_INVALID_MODE)
    dev->supported[idx++] = num;

//  DPRINTF("mode: add_supt: %ux%u @ %u Hz, %u bpp = num %u\n", width, height, refresh, 24, (int)num);

  return idx;
}


static dlo_retcode_t lookup_edid_modes(dlo_device_t * const dev, const edid_format_t * const edid)
{
  uint32_t timings, bit;
  uint32_t idx    = 0;

  /* Clear the native mode information for the device */
  (void) dlo_memset(&dev->native, 0, sizeof(dlo_mode_t));

  /* Add mode numbers for any supported established timing modes */
  timings = edid->est_timings.timings[0] | (edid->est_timings.timings[1] << 8) | (edid->est_timings.resvd << 16);
  for (bit = 0; bit < 24; bit++)
  {
    if (est_timings[bit].width)
      idx = add_supported(dev, idx, est_timings[bit].width, est_timings[bit].height, est_timings[bit].refresh);
  }

  /* Add further support from the detailed timing descriptions */
  for (timings = 0; timings < 4; timings++)
  {
    const edid_detail_t * const detail = &(edid->timings[timings].detail);

    if (detail->is_detail)
    {
      uint8_t hz;

      for (hz = 50; hz < 100; hz++)
      {
        uint32_t prev   = idx;
        uint32_t width  = detail->hactl + ((detail->hactblankh & 0xF0) << 4);
        uint32_t height = detail->vactl + ((detail->vactblankh & 0xF0) << 4);

        idx = add_supported(dev, idx, width, height, hz);

        /* Have we added a mode definition for the native resolution reported by the display? */
        if (idx != prev)
        {
          dlo_modenum_t num = dev->supported[prev];

          dev->native.view.base   = 0;
          dev->native.view.width  = dlo_mode_data[num].width;
          dev->native.view.height = dlo_mode_data[num].height;
          dev->native.view.bpp    = dlo_mode_data[num].bpp;
          dev->native.refresh     = dlo_mode_data[num].refresh;
          DPRINTF("mode: lookup: native mode %u (%ux%u @ %u bpp, %uHz base &%X)\n",
                  num,
                  dev->native.view.width,
                  dev->native.view.height,
                  dev->native.view.bpp,
                  dev->native.refresh,
                  (int)dev->native.view.base);
        }
      }
    }
  }

  /* Fill any remaining array entries with the invalid mode constant */
  while (idx < DLO_MODE_DATA_NUM - 1)
    dev->supported[idx++] = DLO_INVALID_MODE;

  return dlo_ok;
}


static dlo_retcode_t set_base(dlo_device_t * const dev, const dlo_ptr_t base, const dlo_ptr_t base8)
{
  //DPRINTF("mode: set_base: base=&%X base8=&%X\n", base, base8);
  ERR(vbuf(dev, WRITE_VIDREG_LOCK, DSIZEOF(WRITE_VIDREG_LOCK)));
  ERR(vreg(dev, 0x20, base >> 16));
  ERR(vreg(dev, 0x21, base >> 8));
  ERR(vreg(dev, 0x22, base));
  ERR(vreg(dev, 0x26, base8 >> 16));
  ERR(vreg(dev, 0x27, base8 >> 8));
  ERR(vreg(dev, 0x28, base8));
  ERR(vbuf(dev, WRITE_VIDREG_UNLOCK, DSIZEOF(WRITE_VIDREG_UNLOCK)));
  ERR(dlo_usb_write(dev));

  return dlo_ok;
}


static bool bad_edid_checksum(const uint8_t * const ptr, const size_t size)
{
  uint32_t i;
  uint8_t  csum = 0;

  for (i = 0; i < size; i++)
    csum += ptr[i];

  return 0 != csum;
}


static void parse_edid_detail_desc(edid_timing_desc_t * const desc, const uint8_t * const ptr)
{
  if (RD_B(ptr) || RD_B(ptr + 1) || RD_B(ptr + 2))
  {
    //DPRINTF("edid: found timing detail (&%04X)\n", LETOHS(RD_S(ptr)));
    desc->detail.is_detail       = true;
    desc->detail.pixclk          = (float)LETOHS(RD_S(ptr)) / 100.0;
    desc->detail.hactl           = RD_B(ptr + 0x02);
    desc->detail.hblankl         = RD_B(ptr + 0x03);
    desc->detail.hactblankh      = RD_B(ptr + 0x04);
    desc->detail.vactl           = RD_B(ptr + 0x05);
    desc->detail.vblankl         = RD_B(ptr + 0x06);
    desc->detail.vactblankh      = RD_B(ptr + 0x07);
    desc->detail.hsyncoffl       = RD_B(ptr + 0x08);
    desc->detail.hsyncwl         = RD_B(ptr + 0x09);
    desc->detail.vsyncoffvsyncwl = RD_B(ptr + 0x0A);
    desc->detail.synch           = RD_B(ptr + 0x0B);
    desc->detail.hsizel          = RD_B(ptr + 0x0C);
    desc->detail.vsizel          = RD_B(ptr + 0x0D);
    desc->detail.hvsizeh         = RD_B(ptr + 0x0E);
    desc->detail.hbord           = RD_B(ptr + 0x0F);
    desc->detail.vbord           = RD_B(ptr + 0x10);
    desc->detail.flags           = RD_B(ptr + 0x11);

    //DPRINTF("edid: Pixel Clock:     %f MHz\n", (float)desc->detail.pixclk);
    //DPRINTF("edid: H Active pixels: %d\n",     desc->detail.hactl + ((desc->detail.hactblankh & 0xF0) << 4));
    //DPRINTF("edid: H Blank pixels:  %d\n",     desc->detail.hblankl + ((desc->detail.hactblankh & 0x0F) << 8));
    //DPRINTF("edid: V Active pixels: %d\n",     desc->detail.vactl + ((desc->detail.vactblankh & 0xF0) << 4));
    //DPRINTF("edid: V Blank pixels:  %d\n",     desc->detail.vblankl + ((desc->detail.vactblankh & 0x0F) << 8));
    //DPRINTF("edid: H Sync Off:      %d\n",     desc->detail.hsyncoffl + ((desc->detail.synch & 0xC0) << 2));
    //DPRINTF("edid: H Sync Width:    %d\n",     desc->detail.hsyncwl + ((desc->detail.synch & 0x30) << 4));
    //DPRINTF("edid: V Sync Off:      %d\n",     (desc->detail.vsyncoffvsyncwl >> 4) + ((desc->detail.synch & 0x0C) << 6));
    //DPRINTF("edid: V Sync Width:    %d\n",     (desc->detail.vsyncoffvsyncwl & 0xF) + ((desc->detail.synch & 0x03) << 8));
    //DPRINTF("edid: H size:          %d mm\n",  desc->detail.hsizel + ((desc->detail.hvsizeh & 0xF0) << 4));
    //DPRINTF("edid: V size:          %d mm\n",  desc->detail.vsizel + ((desc->detail.hvsizeh & 0x0F) << 8));
  }
  else
  {
    /* It's a mode descriptor - ignore for now */
    //DPRINTF("edid: found a mode descriptor type &%02X\n", RD_B(ptr+3));
    desc->desc.is_detail = false;
    switch (RD_B(ptr+3))
    {
      case 0xFC:
      case 0xFE:
      case 0xFF:
      {
        char  buf[14];
        char *chr;

        snprintf(buf, 13, "%s", ptr + 5);
        buf[13] = '\0';
        chr = strchr(buf, '\n');
        if (chr)
          *chr = '\0';
        //DPRINTF("edid: monitor string '%s'\n", buf);
        break;
      }
      default:
      {
        //uint32_t i;
        //DPRINTF("edid: ");
        //for (i = 0; i < 0x12; i++)
        //  DPRINTF("%02X ", RD_B(ptr+i));
        //DPRINTF("\n");
      }
    }
  }
}

static void parse_edid_colours(edid_colours_t * const cols, const uint8_t * const ptr)
{
  /* Read the raw data */
  cols->red_grn_low = RD_B(ptr + 0x19);
  cols->blu_wht_low = RD_B(ptr + 0x1A);
  cols->red_x       = RD_B(ptr + 0x1B);
  cols->red_y       = RD_B(ptr + 0x1C);
  cols->grn_x       = RD_B(ptr + 0x1D);
  cols->grn_y       = RD_B(ptr + 0x1E);
  cols->blu_x       = RD_B(ptr + 0x1F);
  cols->blu_y       = RD_B(ptr + 0x20);
  cols->wht_x       = RD_B(ptr + 0x21);
  cols->wht_y       = RD_B(ptr + 0x22);

  /* Do the expansion */
  cols->red_x = ((cols->red_grn_low & 0xC0) >> 6) + (cols->red_x << 2);
  cols->red_y = ((cols->red_grn_low & 0x30) >> 4) + (cols->red_y << 2);
  cols->grn_x = ((cols->red_grn_low & 0x0C) >> 2) + (cols->grn_x << 2);
  cols->grn_y = ((cols->red_grn_low & 0x03) >> 0) + (cols->grn_y << 2);
  cols->blu_x = ((cols->blu_wht_low & 0xC0) >> 6) + (cols->blu_x << 2);
  cols->blu_y = ((cols->blu_wht_low & 0x40) >> 4) + (cols->blu_y << 2);
  cols->wht_x = ((cols->blu_wht_low & 0x0C) >> 2) + (cols->wht_y << 2);
  cols->wht_y = ((cols->blu_wht_low & 0x03) >> 0) + (cols->wht_y << 2);
}


/* End of file -------------------------------------------------------------------------*/
