/** @file dlo_grfx.c
 *
 *  @brief This file implements the graphics primitive operations.
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

#include <string.h>
#include "dlo_defs.h"
#include "dlo_grfx.h"
#include "dlo_usb.h"


/* File-scope defines ------------------------------------------------------------------*/


/** Maximum number of pixels that can be supplied to a raw write command. */
#define RAW_MAX_PIXELS (256)

/** Maximum number of pixels that will fit into the scrape buffer. */
#define SCRAPE_MAX_PIXELS (2048)

/** Simple wrapper around the @c dlo_free() call to first check the pointer and zero it after the free. */
#define FREE(ptr) { if (ptr) { dlo_free(ptr); ptr = NULL; } }

/** Return red/green component of a 16 bpp colour number. */
#define DLO_RG16(red, grn) (uint8_t)((((red) & 0xF8) | ((grn) >> 5)) & 0xFF)

/** Return green/blue component of a 16 bpp colour number. */
#define DLO_GB16(grn, blu) (uint8_t)(((((grn) & 0x1C) << 3) | ((blu) >> 3)) & 0xFF)

/** Return 8 bpp colour number from red, green and blue components. */
#define DLO_RGB8(red, grn, blu) ((((red) << 5) | (((grn) & 3) << 3) | ((blu) & 7)) & 0xFF)


/* File-scope inline functions ---------------------------------------------------------*/


/** Build a "plot horizontal line" command in a buffer.
 *
 *  @param  dev     Pointer to @a dlo_device_t structure.
 *  @param  base    Destination address in device for start of line.
 *  @param  remain  Line length.
 *  @param  col     Colour of point to plot.
 *
 *  @return  Return code, zero for no error.
 */
inline dlo_retcode_t cmd_hline16(dlo_device_t * const dev, const dlo_ptr_t base, const uint32_t remain, const dlo_col16_t col)
{
  if (dev->bufend - dev->bufptr < 9)
    return dlo_err_buf_full;

  *(dev->bufptr)++ = WRITE_RL16[0];
  *(dev->bufptr)++ = WRITE_RL16[1];
  *(dev->bufptr)++ = (char)(base >> 16);
  *(dev->bufptr)++ = (char)(base >> 8);
  *(dev->bufptr)++ = (char)base;
  *(dev->bufptr)++ = (char)remain;
  *(dev->bufptr)++ = (char)remain;
  *(dev->bufptr)++ = (char)(col >> 8);
  *(dev->bufptr)++ = (char)col;

  return dlo_ok;
}



/** Build a "plot horizontal line" command in a buffer for fine detail colour space.
 *
 *  @param  dev     Pointer to @a dlo_device_t structure.
 *  @param  base    Destination address in device for start of line.
 *  @param  remain  Line length.
 *  @param  col     Colour of point to plot.
 *
 *  @return  Return code, zero for no error.
 */
inline dlo_retcode_t cmd_hline8(dlo_device_t * const dev, const dlo_ptr_t base, const uint32_t remain, const dlo_col8_t col)
{
  if (dev->bufend - dev->bufptr < 8)
    return dlo_err_buf_full;

  *(dev->bufptr)++ = WRITE_RL8[0];
  *(dev->bufptr)++ = WRITE_RL8[1];
  *(dev->bufptr)++ = (char)(base >> 16);
  *(dev->bufptr)++ = (char)(base >> 8);
  *(dev->bufptr)++ = (char)base;
  *(dev->bufptr)++ = (char)remain;
  *(dev->bufptr)++ = (char)remain;
  *(dev->bufptr)++ = (char)col;

  return dlo_ok;
}


/** Build a "copy horizontal line" command in a buffer.
 *
 *  @param  dev   Pointer to @a dlo_device_t structure.
 *  @param  src   Source address in device for start of copy.
 *  @param  len   Copy length.
 *  @param  dest  Destination address in device for copy.
 *
 *  @return  Return code, zero for no error.
 */
inline dlo_retcode_t cmd_copy16(dlo_device_t * const dev, const dlo_ptr_t src, const uint32_t len, const dlo_ptr_t dest)
{
  if (dev->bufend - dev->bufptr < 9)
    return dlo_err_buf_full;

  *(dev->bufptr)++ = WRITE_COPY16[0];
  *(dev->bufptr)++ = WRITE_COPY16[1];
  *(dev->bufptr)++ = (char)(dest >> 16);
  *(dev->bufptr)++ = (char)(dest >> 8);
  *(dev->bufptr)++ = (char)dest;
  *(dev->bufptr)++ = len == 256 ? '\0' : (char)len;
  *(dev->bufptr)++ = (char)(src >> 16);
  *(dev->bufptr)++ = (char)(src >> 8);
  *(dev->bufptr)++ = (char)src;

  return dlo_ok;
}


/** Build a "copy horizontal line" command in a buffer for fine detail colour space.
 *
 *  @param  dev   Pointer to @a dlo_device_t structure.
 *  @param  src   Source address in device for start of copy.
 *  @param  len   Copy length.
 *  @param  dest  Destination address in device for copy.
 *
 *  @return  Return code, zero for no error.
 */
inline dlo_retcode_t cmd_copy8(dlo_device_t * const dev, const dlo_ptr_t src, const uint32_t len, const dlo_ptr_t dest)
{
  if (dev->bufend - dev->bufptr < 9)
    return dlo_err_buf_full;

  *(dev->bufptr)++ = WRITE_COPY8[0];
  *(dev->bufptr)++ = WRITE_COPY8[1];
  *(dev->bufptr)++ = (char)(dest >> 16);
  *(dev->bufptr)++ = (char)(dest >> 8);
  *(dev->bufptr)++ = (char)dest;
  *(dev->bufptr)++ = len == 256 ? '\0' : (char)len;
  *(dev->bufptr)++ = (char)(src >> 16);
  *(dev->bufptr)++ = (char)(src >> 8);
  *(dev->bufptr)++ = (char)src;

  return dlo_ok;
}


/* Ensure the inline functions get compiled somewhere in case they cannot be inlined */
extern dlo_retcode_t cmd_hline16(dlo_device_t * const dev, const dlo_ptr_t base, const uint32_t remain, const dlo_col16_t col);
extern dlo_retcode_t cmd_hline8( dlo_device_t * const dev, const dlo_ptr_t base, const uint32_t remain, const dlo_col8_t  col);
extern dlo_retcode_t cmd_copy16( dlo_device_t * const dev, const dlo_ptr_t src,  const uint32_t len,    const dlo_ptr_t   dest);
extern dlo_retcode_t cmd_copy8(  dlo_device_t * const dev, const dlo_ptr_t src,  const uint32_t len,    const dlo_ptr_t   dest);


/* File-scope types --------------------------------------------------------------------*/


/** Function pointer for functions to convert a pixel into a colour number.
 *
 *  @param  ptr   Pointer to pixel.
 *  @param  swap  Flag to indicate red and blue components need to be swapped.
 *
 *  @return  Colour number for read pixel.
 */
typedef dlo_col32_t (*read_pixel_t) (const uint8_t * const ptr, const bool swap);


/* File-scope variables ----------------------------------------------------------------*/


/** Look-up table of function pointers for a specified pixel format.
 */
static read_pixel_t fmt_to_fn[DLO_PIXFMT_MAX];


/** Standard look-up table for converting 8 bpp pixels in bgr323 format into colour numbers.
 */
static dlo_col32_t lut8bpp[256];


/** Look-up table to use when reading 8 bpp pixels - required by @c read_pixel_323().
 */
static dlo_col32_t *lut = lut8bpp;


/** Pointer to an array for storing a stripe of 16bpp pixel information.
 */
static dlo_col16_t *stripe16 = NULL;


/** Pointer to an array for storing a stripe of 8bpp pixel information.
 */
static dlo_col8_t  *stripe8  = NULL;


/* External-scope variables ------------------------------------------------------------*/


/* File-scope function declarations ----------------------------------------------------*/


/** Plot a section of horizontal line in the specified colour at 24 bpp.
 *
 *  @param  dev     Pointer to @a dlo_device_t structure.
 *  @param  base16  Base address of destination 16 bpp pixel data.
 *  @param  base8   Base address of destination 8 bpp pixel data.
 *  @param  len     Length of the line (pixels).
 *  @param  col     Colour of the line.
 *
 *  @return  Return code, zero for no error.
 */
static dlo_retcode_t hline_24bpp(dlo_device_t * const dev, dlo_ptr_t base16, dlo_ptr_t base8, const uint32_t len, const dlo_col32_t col);


/** Copy a section of horizontal line from one location to another at 24 bpp.
 *
 *  @param  dev          Pointer to @a dlo_device_t structure.
 *  @param  src_base16   Base address of source 16 bpp pixel data.
 *  @param  dest_base16  Base address of destination 16 bpp pixel data.
 *  @param  src_base8    Base address of source 8 bpp pixel data.
 *  @param  dest_base8   Base address of destination 8 bpp pixel data.
 *  @param  len          Length of the line (pixels).
 *
 *  @return  Return code, zero for no error.
 */
static dlo_retcode_t copy_24bpp(dlo_device_t * const dev, dlo_ptr_t src_base16, dlo_ptr_t dest_base16, dlo_ptr_t src_base8, dlo_ptr_t dest_base8, const uint32_t len);


/** Scrape a horizontal line of host-resident pixels at 24 bpp into the device.
 *
 *  @param  dev          Pointer to @a dlo_device_t structure.
 *  @param  rdpx         Pointer to the pixel reading function.
 *  @param  bypp         Bytes per pixel of the source pixels.
 *  @param  swap         Flag: swap the red/blue component order.
 *  @param  src_base     Base address of the source.
 *  @param  dest_base16  Base address of destination 16 bpp pixel data.
 *  @param  dest_base8   Base address of destination 8 bpp pixel data.
 *  @param  width        Width of the scrape (pixels).
 *
 *  @return  Return code, zero for no error.
 */
static dlo_retcode_t scrape_24bpp(dlo_device_t * const dev, const read_pixel_t rdpx, const uint32_t bypp, const bool swap,
                                   const uint8_t *src_base, dlo_ptr_t dest_base16, dlo_ptr_t dest_base8, const uint32_t width);


/** Dump the contents of the scrape buffers as a horizontal pixel row at 24 bpp.
 *
 *  @param  dev     Pointer to @a dlo_device_t structure.
 *  @param  base16  Base address of destination 16 bpp pixel data.
 *  @param  base8   Base address of destination 8 bpp pixel data.
 *  @param  width   Width of the scrape (pixels).
 *
 *  @return  Return code, zero for no error.
 */
static dlo_retcode_t cmd_stripe24(dlo_device_t * const dev, dlo_ptr_t base16, dlo_ptr_t base8, const uint32_t width);


/** Given a 32 bpp colour number, return an 8 bpp colour number.
 *
 *  @param  col  32 bpp colour number.
 *
 *  @return  8 bpp colour number.
 */
static dlo_col8_t rgb8(dlo_col32_t col);


/** Given a 32 bpp colour number, return a 16 bpp colour number.
 *
 *  @param  col  32 bpp colour number.
 *
 *  @return  16 bpp colour number.
 */
static dlo_col16_t rgb16(dlo_col32_t col);


/** Dummy pixel reading function for an uninitialised entry.
 *
 *  @param  ptr   Pointer to pixel.
 *  @param  swap  Flag to indicate red and blue components need to be swapped.
 *
 *  @return  Colour number for read pixel.
 */
static dlo_col32_t read_pixel_NULL(const uint8_t * const ptr, const bool swap);


/** Read an 8 bpp pixel in 323 format.
 *
 *  @param  ptr   Pointer to pixel.
 *  @param  swap  Flag to indicate red and blue components need to be swapped.
 *
 *  @return  Colour number for read pixel.
 */
static dlo_col32_t read_pixel_323(const uint8_t * const ptr, const bool swap);


/** Read a 16 bpp pixel in 565 format.
 *
 *  @param  ptr   Pointer to pixel.
 *  @param  swap  Flag to indicate red and blue components need to be swapped.
 *
 *  @return  Colour number for read pixel.
 */
static dlo_col32_t read_pixel_565(const uint8_t * const ptr, const bool swap);


/** Read a 16 bpp pixel in 1555 format.
 *
 *  @param  ptr   Pointer to pixel.
 *  @param  swap  Flag to indicate red and blue components need to be swapped.
 *
 *  @return  Colour number for read pixel.
 */
static dlo_col32_t read_pixel_1555(const uint8_t * const ptr, const bool swap);


/** Read a 24 bpp pixel in 888 format.
 *
 *  @param  ptr   Pointer to pixel.
 *  @param  swap  Flag to indicate red and blue components need to be swapped.
 *
 *  @return  Colour number for read pixel.
 */
static dlo_col32_t read_pixel_888(const uint8_t * const ptr, const bool swap);


/** Read a 32 bpp pixel in 8888 format.
 *
 *  @param  ptr   Pointer to pixel.
 *  @param  swap  Flag to indicate red and blue components need to be swapped.
 *
 *  @return  Colour number for read pixel.
 */
static dlo_col32_t read_pixel_8888(const uint8_t * const ptr, const bool swap);


/* Public function definitions ---------------------------------------------------------*/


dlo_retcode_t dlo_grfx_init(const dlo_init_t flags)
{
  uint32_t i;
  uint8_t  red, grn, blu;
  uint8_t  red8, grn8, blu8;

  stripe16 = malloc(SCRAPE_MAX_PIXELS * sizeof(dlo_col16_t));
  NERR(stripe16);
  stripe8  = malloc(SCRAPE_MAX_PIXELS * sizeof(dlo_col8_t));
  NERR(stripe8);

  /* Initialise the default look-up table for 8 bpp in bgr323 format */
  for (red = 0; red < 8; red++)
  for (grn = 0; grn < 4; grn++)
  for (blu = 0; blu < 8; blu++)
  {
    uint8_t idx = red + (grn << 3) + (blu << 5);

    red8         = (red << 5) | (red << 2) | (red >> 1);
    grn8         = (grn << 6) | (grn << 4) | (grn << 2) | grn;
    blu8         = (blu << 5) | (blu << 2) | (blu >> 1);
    lut8bpp[idx] = DLO_RGB(red8, grn8, blu8);
  }

  /* Initialise the look-up table of pixel formats to pixel reading functions */
  for (i = 0; i < DLO_PIXFMT_MAX; i++)
    fmt_to_fn[i] = read_pixel_NULL;
  fmt_to_fn[dlo_pixfmt_bgr323]   = read_pixel_323;
  fmt_to_fn[dlo_pixfmt_rgb323]   = read_pixel_323;
  fmt_to_fn[dlo_pixfmt_bgr565]   = read_pixel_565;
  fmt_to_fn[dlo_pixfmt_rgb565]   = read_pixel_565;
  fmt_to_fn[dlo_pixfmt_sbgr1555] = read_pixel_1555;
  fmt_to_fn[dlo_pixfmt_srgb1555] = read_pixel_1555;
  fmt_to_fn[dlo_pixfmt_bgr888]   = read_pixel_888;
  fmt_to_fn[dlo_pixfmt_rgb888]   = read_pixel_888;
  fmt_to_fn[dlo_pixfmt_abgr8888] = read_pixel_8888;
  fmt_to_fn[dlo_pixfmt_argb8888] = read_pixel_8888;

  return dlo_ok;
}


dlo_retcode_t dlo_grfx_final(const dlo_final_t flags)
{
  FREE(stripe16);
  FREE(stripe8);

  return dlo_ok;
}


dlo_retcode_t dlo_grfx_fill_rect(dlo_device_t * const dev, const dlo_area_t * const area, const dlo_col32_t col)
{
  dlo_ptr_t base16, base8;
  uint32_t  end;

  ASSERT(dev && area)
  ASSERT(area->view.width && area->view.height);

  /* Only 24 bpp is supported */
  if (area->view.bpp != 24)
    return dlo_err_bad_col;

  /* Compute some useful values */
  base16 = area->view.base;
  base8  = area->base8;
  end    = base16 + (BYTES_PER_16BPP * area->stride * area->view.height);

  /* Plot the rectangle, one pixel row at a time */
  for (; base16 < end; base16 += BYTES_PER_16BPP * area->stride)
  {
    ERR(hline_24bpp(dev, base16, base8, area->view.width, col));
    base8 += BYTES_PER_8BPP * area->stride;
  }
  return dlo_usb_write(dev);
}


dlo_retcode_t dlo_grfx_copy_rect(dlo_device_t * const dev, const dlo_area_t * const src_area, const dlo_area_t * const dest_area, const bool overlap)
{
  dlo_ptr_t src_base16,  src_base8;
  dlo_ptr_t dest_base16, dest_base8;
  uint32_t  end;

  ASSERT(dev && src_area && dest_area);
  ASSERT(src_area->view.width && src_area->view.height);
  ASSERT(src_area->view.width == dest_area->view.width && src_area->view.height == dest_area->view.height);
  ASSERT(src_area->view.bpp == dest_area->view.bpp);

  /* Only 24 bpp is supported */
  if (src_area->view.bpp != 24)
    return dlo_err_bad_col;

  /* Quick exit if we're copying to and from the same location */
  if (src_area->view.base == dest_area->view.base)
    return dlo_ok;

  /* Compute some useful values */
  if (src_area->view.base < dest_area->view.base)
  {
    uint32_t y;

    src_base16  = src_area->view.base  + (BYTES_PER_16BPP * src_area->stride  * src_area->view.height);
    src_base8   = src_area->base8      + (BYTES_PER_8BPP  * src_area->stride  * src_area->view.height);
    dest_base16 = dest_area->view.base + (BYTES_PER_16BPP * dest_area->stride * dest_area->view.height);
    dest_base8  = dest_area->base8     + (BYTES_PER_8BPP  * dest_area->stride * dest_area->view.height);

    /* Copy the rectangle, one pixel row at a time */
    for (y = src_area->view.height; y > 0; y--)
    {
      src_base16  -= BYTES_PER_16BPP * src_area->stride;
      src_base8   -= BYTES_PER_8BPP  * src_area->stride;
      dest_base16 -= BYTES_PER_16BPP * dest_area->stride;
      dest_base8  -= BYTES_PER_8BPP  * dest_area->stride;
      ERR(copy_24bpp(dev, src_base16, dest_base16, src_base8, dest_base8, src_area->view.width));
//      if (overlap)
//        ERR(dlo_usb_write(dev));
    }
  }
  else
  {
    src_base16  = src_area->view.base;
    src_base8   = src_area->base8;
    dest_base16 = dest_area->view.base;
    dest_base8  = dest_area->base8;
    end         = src_base16 + (BYTES_PER_16BPP * src_area->stride * src_area->view.height);

    /* Copy the rectangle, one pixel row at a time */
    for (; src_base16 < end; src_base16 += BYTES_PER_16BPP * src_area->stride)
    {
      ERR(copy_24bpp(dev, src_base16, dest_base16, src_base8, dest_base8, src_area->view.width));
      src_base8   += BYTES_PER_8BPP  * src_area->stride;
      dest_base16 += BYTES_PER_16BPP * dest_area->stride;
      dest_base8  += BYTES_PER_8BPP  * dest_area->stride;
//      if (overlap)
//        ERR(dlo_usb_write(dev));
    }
  }
  return dlo_usb_write(dev);
}


dlo_retcode_t dlo_grfx_copy_host_bmp(dlo_device_t * const dev, const dlo_bmpflags_t flags, const dlo_fbuf_t const *fbuf, const dlo_area_t * const area)
{
  uint8_t     *src_base;
  uint8_t     *end;
  dlo_ptr_t    dest_base16, dest_base8;
  uint32_t     bypp;
  read_pixel_t rdpx;
  bool         swap;

  ASSERT(dev && fbuf && area);
  ASSERT(fbuf->width && fbuf->height && fbuf->base && fbuf->stride);
  ASSERT(fbuf->width == area->view.width && fbuf->height == area->view.height)

  /* Only 24 bpp is supported */
  if (area->view.bpp != 24)
    return dlo_err_bad_col;

  /* Get a pixel reading function pointer for the fbuf */
  if (fbuf->fmt >> DLO_PIXFMT_PTR_SFT)
  {
    bypp = 1;
    rdpx = read_pixel_323;
    swap = false;
    lut  = (dlo_col32_t *)fbuf->fmt;
  }
  else
  {
    if ((int)fbuf->fmt > (int)dlo_pixfmt_argb8888)
      return dlo_err_bad_fmt;

    bypp = FORMAT_TO_BYTES_PER_PIXEL(fbuf->fmt);
    rdpx = fmt_to_fn[fbuf->fmt];
    swap = fbuf->fmt & DLO_PIXFMT_SWP ? true : false;
    lut  = lut8bpp;  /* For unpaletted bitmaps, the LUT is our standard colour palette */
  }

  /* Check that a row from the bitmap will fit into the scrape buffers */
  if (fbuf->width > SCRAPE_MAX_PIXELS)
    return dlo_err_big_scrape;

  /* Set up the destination pointers in the device memory */
  dest_base16 = area->view.base;
  dest_base8  = area->base8;

  /* Either copy down or up the bitmap, depending upon whether we are doing a vertical flip */
  if (flags.v_flip)
  {
    src_base = (uint8_t *)fbuf->base + (bypp * fbuf->stride * (area->view.height - 1));
    end      = (uint8_t *)fbuf->base;

    for (; src_base >= end; src_base -= bypp * fbuf->stride)
    {
      ERR(scrape_24bpp(dev, rdpx, bypp, swap, src_base, dest_base16, dest_base8, fbuf->width));
      dest_base16 += BYTES_PER_16BPP * area->stride;
      dest_base8  += BYTES_PER_8BPP  * area->stride;
    }
  }
  else
  {
    src_base = (uint8_t *)fbuf->base;
    end      = src_base + (bypp * fbuf->stride * area->view.height);

    for (; src_base < end; src_base += bypp * fbuf->stride)
    {
      ERR(scrape_24bpp(dev, rdpx, bypp, swap, src_base, dest_base16, dest_base8, fbuf->width));
      dest_base16 += BYTES_PER_16BPP * area->stride;
      dest_base8  += BYTES_PER_8BPP  * area->stride;
    }
  }
  return dlo_usb_write(dev);
}


/* File-scope function definitions -----------------------------------------------------*/


static dlo_retcode_t hline_24bpp(dlo_device_t * const dev, dlo_ptr_t base16, dlo_ptr_t base8, const uint32_t len, const dlo_col32_t col)
{
  dlo_col16_t col16 = rgb16(col);
  dlo_col8_t  col8  = rgb8(col);
  uint32_t    rem   = len % 256;

  /* Flush the command buffer if it's getting full */
  if (dev->bufend - dev->bufptr < BUF_HIGH_WATER_MARK)
    ERR(dlo_usb_write(dev));

  /* Is this a short line segment? */
  if (len < 256)
  {
    ERR(cmd_hline16(dev, base16, len, col16));
    ERR(cmd_hline8( dev, base8,  len, col8));
  }
  else
  {
    /* Longer line segments require a few commands to complete */
    uint32_t roff16 = base16 + (BYTES_PER_16BPP * (len - rem));
    uint32_t roff8  = base8  + (BYTES_PER_8BPP  * (len - rem));

    for (; base16 < roff16; base16 += BYTES_PER_16BPP * 256)
    {
      ERR(cmd_hline16(dev, base16, 0, col16));
      ERR(cmd_hline8( dev, base8,  0, col8));
      base8 += BYTES_PER_8BPP * 256;
    }
    if (rem)
    {
      ERR(cmd_hline16(dev, roff16, rem, col16));
      ERR(cmd_hline8( dev, roff8,  rem, col8));
    }
  }
  return dlo_ok;
}


static dlo_retcode_t copy_24bpp(dlo_device_t * const dev, dlo_ptr_t src_base16, dlo_ptr_t dest_base16, dlo_ptr_t src_base8, dlo_ptr_t dest_base8, const uint32_t len)
{
  uint32_t rem = len % 256;

  /* Flush the command buffer if it's getting full */
  if (dev->bufend - dev->bufptr < BUF_HIGH_WATER_MARK)
    ERR(dlo_usb_write(dev));

  /* Is this a short line segment? */
  if (len < 256)
  {
    ERR(cmd_copy16(dev, src_base16, len, dest_base16));
    ERR(cmd_copy8( dev, src_base8,  len, dest_base8));
  }
  else
  {
    /* Longer line segments require a few commands to complete */
    uint32_t src_roff16  = src_base16  + (BYTES_PER_16BPP * (len - rem));
    uint32_t dest_roff16 = dest_base16 + (BYTES_PER_16BPP * (len - rem));
    uint32_t src_roff8   = src_base8   + (BYTES_PER_8BPP  * (len - rem));
    uint32_t dest_roff8  = dest_base8  + (BYTES_PER_8BPP  * (len - rem));

    for (; src_base16 < src_roff16; src_base16 += BYTES_PER_16BPP * 256)
    {
      ERR(cmd_copy16(dev, src_base16, 0, dest_base16));
      ERR(cmd_copy8( dev, src_base8,  0, dest_base8));
      dest_base16 += BYTES_PER_16BPP * 256;
      src_base8   += BYTES_PER_8BPP  * 256;
      dest_base8  += BYTES_PER_8BPP  * 256;
    }
    if (rem)
    {
      ERR(cmd_copy16(dev, src_roff16, rem, dest_roff16));
      ERR(cmd_copy8( dev, src_roff8,  rem, dest_roff8));
    }
  }
  return dlo_ok;
}


static dlo_retcode_t scrape_24bpp(dlo_device_t * const dev, const read_pixel_t rdpx, const uint32_t bypp, const bool swap,
                                   const uint8_t *src_base, dlo_ptr_t dest_base16, dlo_ptr_t dest_base8, const uint32_t width)
{
  const uint8_t *end       = src_base + (bypp * width);
  dlo_col16_t   *ptr_col16 = stripe16;
  dlo_col8_t    *ptr_col8  = stripe8;

  /* Read a stripe from the source bitmap into the internal colour format */
  for (; src_base < end; src_base += bypp)
  {
    dlo_col32_t col = rdpx(src_base, swap);

    *ptr_col16++ = rgb16(col);
    *ptr_col8++  = rgb8(col);
  }
  return cmd_stripe24(dev, dest_base16, dest_base8, width);
}


static dlo_retcode_t cmd_stripe24(dlo_device_t * const dev, dlo_ptr_t base16, dlo_ptr_t base8, const uint32_t width)
{
  dlo_col16_t *ptr_col16 = stripe16;
  dlo_col8_t  *ptr_col8  = stripe8;
  dlo_ptr_t    end       = base16 + (BYTES_PER_16BPP * width);
  uint32_t     rem       = width;
  uint32_t     pix;

  /* Flush the command buffer if it's getting full */
  if (dev->bufend - dev->bufptr < BUF_HIGH_WATER_MARK)
    ERR(dlo_usb_write(dev));

  for (; base16 < end; base16 += BYTES_PER_16BPP * RAW_MAX_PIXELS)
  {
    *(dev->bufptr)++ = WRITE_RAW16[0];
    *(dev->bufptr)++ = WRITE_RAW16[1];
    *(dev->bufptr)++ = (char)(base16 >> 16);
    *(dev->bufptr)++ = (char)(base16 >> 8);
    *(dev->bufptr)++ = (char)(base16 & 0xFF);
    *(dev->bufptr)++ = rem >= RAW_MAX_PIXELS ? 0 : rem;

    /* Flush the command buffer if it's getting full */
    if (dev->bufend - dev->bufptr - BYTES_PER_16BPP * RAW_MAX_PIXELS < BUF_HIGH_WATER_MARK)
      ERR(dlo_usb_write(dev));

    for (pix = 0; pix < (rem >= RAW_MAX_PIXELS ? RAW_MAX_PIXELS : rem); pix++)
    {
      dlo_col16_t col = *ptr_col16++;

      *(dev->bufptr)++ = (char)(col >> 8);
      *(dev->bufptr)++ = (char)(col & 0xFF);
    }
    rem -= RAW_MAX_PIXELS;
  }

  end = base8 + (BYTES_PER_8BPP * width);

  for (; base8 < end; base8 += BYTES_PER_8BPP * RAW_MAX_PIXELS)
  {
    *(dev->bufptr)++ = WRITE_RAW8[0];
    *(dev->bufptr)++ = WRITE_RAW8[1];
    *(dev->bufptr)++ = (char)(base8 >> 16);
    *(dev->bufptr)++ = (char)(base8 >> 8);
    *(dev->bufptr)++ = (char)(base8 & 0xFF);
    *(dev->bufptr)++ = rem >= RAW_MAX_PIXELS ? 0 : rem;

    /* Flush the command buffer if it's getting full */
    if (dev->bufend - dev->bufptr - BYTES_PER_8BPP * RAW_MAX_PIXELS < BUF_HIGH_WATER_MARK)
      ERR(dlo_usb_write(dev));

    for (pix = 0; pix < (rem >= RAW_MAX_PIXELS ? RAW_MAX_PIXELS : rem); pix++)
      *(dev->bufptr)++ = (char)(*ptr_col8++);

    rem -= RAW_MAX_PIXELS;
  }
  return dlo_ok;
}


static dlo_col32_t read_pixel_NULL(const uint8_t * const ptr, const bool swap)
{
  DPRINTF("grfx: WARNING: unknown dlo_pixfmt_t doesn't map to a read_pixel_*() function\n");
  return DLO_RGB(0, 0, 0);
}


static dlo_col32_t read_pixel_323(const uint8_t * const ptr, const bool swap)
{
  dlo_col32_t col = lut[*ptr];

  if (swap)
  {
    uint8_t red, grn, blu;

    red = DLO_RGB_GETRED(col);
    grn = DLO_RGB_GETGRN(col);
    blu = DLO_RGB_GETBLU(col);
    return DLO_RGB(blu, grn, red);
  }
  return col;
}


static dlo_col32_t read_pixel_565(const uint8_t * const ptr, const bool swap)
{
  dlo_col32_t col;
  uint16_t   *pix = (uint16_t *)ptr;
  uint8_t     red, grn, blu;

  col  = *pix;
  red  = (col & 0x001F) << 3;
  grn  = (col & 0x07E0) >> 3;
  blu  = (col & 0xF800) >> 8;
  red  = red | (red >> 5);
  grn  = grn | (grn >> 5);
  blu  = blu | (blu >> 5);

  return swap ? DLO_RGB(blu, grn, red) : DLO_RGB(red, grn, blu);
}


static dlo_col32_t read_pixel_1555(const uint8_t * const ptr, const bool swap)
{
  dlo_col32_t col;
  uint16_t   *pix = (uint16_t *)ptr;
  uint8_t     red, grn, blu;

  col  = *pix;
  red  = (col & 0x001F) << 3;
  grn  = (col & 0x03E0) >> 2;
  blu  = (col & 0x7C00) >> 7;
  red  = red | (red >> 5);
  grn  = grn | (grn >> 5);
  blu  = blu | (blu >> 5);

  return swap ? DLO_RGB(blu, grn, red) : DLO_RGB(red, grn, blu);
}


static dlo_col32_t read_pixel_888(const uint8_t * const ptr, const bool swap)
{
  uint8_t red, grn, blu;

  red  = ptr[0];
  grn  = ptr[1];
  blu  = ptr[2];

  return swap ? DLO_RGB(blu, grn, red) : DLO_RGB(red, grn, blu);
}


static dlo_col32_t read_pixel_8888(const uint8_t * const ptr, const bool swap)
{
  dlo_col32_t col;
  uint32_t   *pix = (uint32_t *)ptr;

  col = (dlo_col32_t) *pix;

  if (swap)
  {
    uint8_t red, grn, blu;

    red = DLO_RGB_GETRED(col);
    grn = DLO_RGB_GETGRN(col);
    blu = DLO_RGB_GETBLU(col);
    return DLO_RGB(blu, grn, red);
  }
  return col;
}


static dlo_col8_t rgb8(dlo_col32_t col)
{
  uint8_t red = DLO_RGB_GETRED(col);
  uint8_t grn = DLO_RGB_GETGRN(col);
  uint8_t blu = DLO_RGB_GETBLU(col);

  return DLO_RGB8(red, grn, blu);
}


static dlo_col16_t rgb16(dlo_col32_t col)
{
  uint8_t red = DLO_RGB_GETRED(col);
  uint8_t grn = DLO_RGB_GETGRN(col);
  uint8_t blu = DLO_RGB_GETBLU(col);

  return (DLO_RG16(red, grn) << 8) + DLO_GB16(grn, blu);
}


/* End of file -------------------------------------------------------------------------*/
