/** @file dlo_structs.h
 *
 *  @brief This file defines all of the internal structures used by libdlo.
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

#ifndef DLO_STRUCTS_H
#define DLO_STRUCTS_H        /**< Avoid multiple inclusion. */

#include "libdlo.h"
#include "dlo_data.h"


/** Structure holding all of the information specific to a particular device.
 */
typedef struct dlo_device_s dlo_device_t;


/** A mode number used to index a specific mode from the list defined in dlo_mode_data.c.
 */
typedef uint32_t dlo_modenum_t;


/** Structure used internally by dlo_usb.c (stored as dev->cnct in @a dlo_device_t structure).
 *
 *  This is required to keep track of which USB device a given @a dlo_device_t structure
 *  represents so that our various functions can do their stuff with libusb.
 */
typedef struct dlo_usb_dev_s
{
  struct usb_device *udev;   /**< Pointer to USB device structure for given device. */
  usb_dev_handle    *uhand;  /**< USB device handle (once device is "opened"). */
} dlo_usb_dev_t;             /**< A struct @a dlo_usb_dev_s. */


/** An internal representation of a viewport within the DisplayLink device.
 *
 *  An area is generated from a viewport and a rectangle within that viewport (which
 *  has no parts lying outside but may cover the complete extent of the viewport). It
 *  has a base address for both the 16 bpp component of a pixel's colour and the 8 bpp
 *  fine detail component. It also requires a stride in the case where the rectangle
 *  didn't fully occupy the horizontal extent of the viewport.
 */
typedef struct dlo_area_s
{
  dlo_view_t view;           /**< Viewport information (normalised to a specific rectangle within a viewport). */
  dlo_ptr_t  base8;          /**< The base address of the 8 bpp fine detail colour information. */
  uint32_t   stride;         /**< The stride (pixels) from one pixel in the area to the one directly below. */
} dlo_area_t;                /**< A struct @a dlo_area_s. */


/** Structure holding all of the information specific to a particular device.
 */
struct dlo_device_s
{
  dlo_device_t  *prev;       /**< Pointer to previous node on device list. */
  dlo_device_t  *next;       /**< Pointer to next node on device list. */
  dlo_devtype_t  type;       /**< Type of DisplayLink device. */
  char          *serial;     /**< Pointer to device serial number string. */
  bool           claimed;    /**< Has the device been claimed by someone? */
  bool           check;      /**< Flag is toggled for each enumeration to spot dead nodes in device list. */
  uint32_t       timeout;    /**< Timeout for bulk communications (milliseconds). */
  uint32_t       memory;     /**< Total size of storage in the device (bytes). */
  char          *buffer;     /**< Pointer to the base of the command buffer. */
  char          *bufptr;     /**< Pointer to the first free byte in the command buffer. */
  char          *bufend;     /**< Pointer to the byte after the end byte of the command buffer. */
  dlo_usb_dev_t *cnct;       /**< Private word for connection specific data or structure pointer. */
  dlo_mode_t     mode;       /**< Current display mode information. */
  dlo_ptr_t      base8;      /**< Pointer to the base of the 8bpp segment (if any). */
  bool           low_blank;  /**< The current raster screen mode has reduced blanking. */
  dlo_mode_t     native;     /**< Mode number of the display's native screen mode (if any). */
  dlo_modenum_t  supported[DLO_MODE_DATA_NUM];  /**< Array of supported mode numbers. */
};


#endif
