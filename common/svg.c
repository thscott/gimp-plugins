/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* SVG loading file filter for the GIMP
 * (C) Copyright 2003  Dom Lachowicz <cinamod@hotmail.com>
 *
 * Largely rewritten by Sven Neumann <sven@gimp.org>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <librsvg/rsvg.h>

#include <gtk/gtk.h>

#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

#include "libgimp/stdplugins-intl.h"


#define SVG_VERSION      "2.5.0"
#define SVG_BUFFER_SIZE  (8 * 1024)


typedef struct
{
  gdouble  resolution;
  gint     width;
  gint     height;
} SvgLoadVals;

static SvgLoadVals load_vals =
{
  72.0,  /* resolution */
  0,
  0
};

typedef struct
{
  gboolean  run;
} SvgLoadInterface;

static SvgLoadInterface load_interface =
{
  FALSE  /* run  */
};


static void  query (void);
static void  run   (const gchar      *name,
                    gint              nparams,
                    const GimpParam  *param,
                    gint             *nreturn_vals,
                    GimpParam       **return_vals);

static gint32      load_image         (const gchar  *filename);
static GdkPixbuf * load_rsvg_pixbuf   (const gchar  *filename,
                                       SvgLoadVals  *vals,
                                       GError      **error);
static gboolean    load_dialog        (const gchar  *filename);


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

MAIN ()


static void
query (void)
{
  static GimpParamDef load_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode",     "Interactive, non-interactive"        },
    { GIMP_PDB_STRING, "filename",     "The name of the file to load"        },
    { GIMP_PDB_STRING, "raw_filename", "The name of the file to load"        },
    { GIMP_PDB_FLOAT,  "resolution",
      "Resolution to use for rendering the SVG (defaults to 72 dpi"          },
    { GIMP_PDB_FLOAT,  "width",
      "Width (in pixels) to load the SVG in."
      "(0 for original width, a negative width to specify a maximum width)"  },
    { GIMP_PDB_FLOAT,  "height",
      "Height (in pixels) to load the SVG in."
      "(0 for original height, a negative width to specify a maximum height)"}
  };
  static GimpParamDef load_return_vals[] =
  {
    { GIMP_PDB_IMAGE,  "image",        "Output image" }
  };

  gimp_install_procedure ("file_svg_load",
                          "Loads files in the SVG file format",
                          "Renders SVG files to raster graphics using librsvg.",
                          "Dom Lachowicz, Sven Neumann",
                          "Dom Lachowicz <cinamod@hotmail.com>",
                          SVG_VERSION,
			  "<Load>/SVG",
			  NULL,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (load_args),
                          G_N_ELEMENTS (load_return_vals),
                          load_args, load_return_vals);

  gimp_register_magic_load_handler ("file_svg_load",
				    "svg", "",
				    "0,string,<?xml,0,string,<svg");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[2];
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  gint32             image_ID;

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

  /* MUST call this before any RSVG funcs */
  g_type_init ();

  if (strcmp (name, "file_svg_load") == 0)
    {
      gimp_get_data ("file_svg_load", &load_vals);

      if (run_mode == GIMP_RUN_INTERACTIVE)
	{
	  if (!load_dialog (param[1].data.d_string))
	    status = GIMP_PDB_CANCEL;
	}

      if (status == GIMP_PDB_SUCCESS)
	{
	  image_ID = load_image (param[1].data.d_string);

	  gimp_set_data ("file_svg_load", &load_vals, sizeof (load_vals));

	  if (image_ID != -1)
	    {
	      *nreturn_vals = 2;
	      values[1].type         = GIMP_PDB_IMAGE;
	      values[1].data.d_image = image_ID;
	    }
	  else
            {
              status = GIMP_PDB_EXECUTION_ERROR;
            }
        }
    }
  else
    {
      status = GIMP_PDB_CALLING_ERROR;
    }

  values[0].data.d_status = status;
}

static gint32
load_image (const gchar *filename)
{
  gint32        image;
  gint32	layer;
  GimpDrawable *drawable;
  GimpPixelRgn	rgn;
  GdkPixbuf    *pixbuf;
  gchar        *status;
  gchar        *pixels;
  gint          width;
  gint          height;
  gint          rowstride;
  gint          bpp;
  gpointer      pr;
  GError       *error = NULL;

  pixbuf = load_rsvg_pixbuf (filename, &load_vals, &error);
  if (!pixbuf)
    {
      /*  Do not rely on librsvg setting GError on failure!  */
      g_message (_("Can't open '%s':\n"
                   "%s"),
                 filename, error ? error->message : "unknown reason");
      gimp_quit ();
    }

  status = g_strdup_printf (_("Loading %s:"), filename);
  gimp_progress_init (status);
  g_free (status);

  width  = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  image = gimp_image_new (width, height, GIMP_RGB);
  gimp_image_set_filename (image, filename);
  gimp_image_set_resolution (image,
                             load_vals.resolution, load_vals.resolution);

  layer = gimp_layer_new (image, _("Rendered SVG"), width, height,
                          GIMP_RGBA_IMAGE, 100, GIMP_NORMAL_MODE);

  drawable = gimp_drawable_get (layer);

  gimp_pixel_rgn_init (&rgn, drawable, 0, 0, width, height, TRUE, FALSE);

  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  bpp       = gdk_pixbuf_get_n_channels (pixbuf);
  pixels    = gdk_pixbuf_get_pixels (pixbuf);

  g_assert (bpp == rgn.bpp);

  for (pr = gimp_pixel_rgns_register (1, &rgn);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      const guchar *src;
      guchar       *dest;
      gint          y;

      src  = pixels + rgn.y * rowstride + rgn.x * bpp;
      dest = rgn.data;

      for (y = 0; y < rgn.h; y++)
        {
          memcpy (dest, src, rgn.w * rgn.bpp);

          src  += rowstride;
          dest += rgn.rowstride;
        }
    }

  gimp_drawable_detach (drawable);
  g_object_unref (pixbuf);

  gimp_progress_update (1.0);

  gimp_image_add_layer (image, layer, 0);

  return image;
}

static void
load_size_callback (gint     *width,
                    gint     *height,
                    gpointer  data)
{
  SvgLoadVals *vals = data;

  g_return_if_fail (*width > 0 && *height > 0);

  if (!vals->width || !vals->height)
    return;

  /*  either both arguments negative or none  */
  if ((vals->width * vals->height) < 0)
    return;

  if (vals->width > 0)
    {
      *width  = vals->width;
      *height = vals->height;
    }
  else
    {
      gdouble w      = *width;
      gdouble h      = *height;
      gdouble aspect = (gdouble) vals->width / (gdouble) vals->height;

      if (aspect > (w / h))
        {
          *height = abs (vals->height);
          *width  = (gdouble) abs (vals->width) / (w / h) + 0.5;
        }
      else
        {
          *width  = abs (vals->width);
          *height = (gdouble) abs (vals->height) / (w / h) + 0.5;
        }

      vals->width  = *width;
      vals->height = *height;
    }
}

static GdkPixbuf *
load_rsvg_pixbuf (const gchar  *filename,
                  SvgLoadVals  *vals,
                  GError      **error)
{
  GdkPixbuf  *pixbuf  = NULL;
  RsvgHandle *handle;
  GIOChannel *io;
  GIOStatus   status  = G_IO_STATUS_NORMAL;
  gboolean    success = TRUE;

  io = g_io_channel_new_file (filename, "r", error);
  if (!io)
    return NULL;

  g_io_channel_set_encoding (io, NULL, NULL);

  handle = rsvg_handle_new ();

  rsvg_handle_set_dpi (handle, vals->resolution);
  rsvg_handle_set_size_callback (handle, load_size_callback, vals, NULL);

  while (success && status != G_IO_STATUS_EOF)
    {
      guchar buf[SVG_BUFFER_SIZE];
      gsize  len;

      status = g_io_channel_read_chars (io, buf, sizeof (buf), &len, error);

      switch (status)
        {
        case G_IO_STATUS_ERROR:
          success = FALSE;
          break;
        case G_IO_STATUS_EOF:
          success = rsvg_handle_close (handle, error);
          break;
        case G_IO_STATUS_NORMAL:
          success = rsvg_handle_write (handle, buf, len, error);
          break;
        case G_IO_STATUS_AGAIN:
          break;
        }
    }

  g_io_channel_unref (io);

  if (success)
    pixbuf = rsvg_handle_get_pixbuf (handle);

  rsvg_handle_free (handle);

  return pixbuf;
}


static void
load_ok_callback (GtkWidget *widget,
                  gpointer   data)

{
  GimpSizeEntry *size = g_object_get_data (G_OBJECT (data), "size-entry");

  load_vals.width  = gimp_size_entry_get_refval (size, 0);
  load_vals.height = gimp_size_entry_get_refval (size, 1);

  load_interface.run = TRUE;

  gtk_widget_destroy (GTK_WIDGET (data));
}

static gboolean
load_dialog (const gchar *filename)
{
  GtkWidget *dialog;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *table;
  GtkWidget *abox;
  GtkWidget *size;
  GtkWidget *label;
  GtkWidget *spinbutton;
  GtkWidget *image;
  GdkPixbuf *preview;
  GtkObject *adj;
  GError    *error = NULL;

  SvgLoadVals  preview_vals = { 72.0, -128, -128 };

  preview = load_rsvg_pixbuf (filename, &preview_vals, &error);

  if (!preview)
    {
      /*  Do not rely on librsvg setting GError on failure!  */
      g_message (_("Can't open '%s':\n"
                   "%s"),
                 filename, error ? error->message : "unknown reason");
      return FALSE;
    }

  gimp_ui_init ("svg", FALSE);

  dialog = gimp_dialog_new (_("Load SVG"), "svg",
                            NULL, NULL,
                            GTK_WIN_POS_MOUSE,
                            FALSE, TRUE, FALSE,

                            GTK_STOCK_CANCEL, gtk_widget_destroy,
                            NULL, 1, NULL, FALSE, TRUE,

                            GTK_STOCK_OK, load_ok_callback,
                            NULL, NULL, NULL, TRUE, FALSE,

                            NULL);

  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  /* Scalable Vector Graphics is SVG, should perhaps not be translated */
  frame = gtk_frame_new (_("Render Scalable Vector Graphics"));
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame,
		      TRUE, TRUE, 0);
  gtk_widget_show (frame);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  image = gtk_image_new_from_pixbuf (preview);
  gtk_container_add (GTK_CONTAINER (frame), image);
  gtk_widget_show (image);

  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_row_spacing (GTK_TABLE (table), 1, 4);
  gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  label = gtk_label_new (_("Width:"));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Height:"));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  abox = gtk_alignment_new (0.0, 0.5, 0.0, 1.0);
  gtk_table_attach (GTK_TABLE (table), abox, 1, 2, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (abox);

  spinbutton = gimp_spin_button_new (&adj, 1, 1, 1, 1, 10, 1, 1, 2);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), 10);
  gtk_container_add (GTK_CONTAINER (abox), spinbutton);
  gtk_widget_show (spinbutton);

  abox = gtk_alignment_new (0.0, 0.5, 0.0, 1.0);
  gtk_table_attach (GTK_TABLE (table), abox, 1, 2, 1, 2,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (abox);

  size = gimp_size_entry_new (1, GIMP_UNIT_PIXEL, "%a",
                              TRUE, FALSE, FALSE, 10,
                              GIMP_SIZE_ENTRY_UPDATE_SIZE);

  g_object_set_data (G_OBJECT (dialog), "size-entry", size);

  gtk_table_set_col_spacing (GTK_TABLE (size), 1, 4);

  gimp_size_entry_add_field (GIMP_SIZE_ENTRY (size),
                             GTK_SPIN_BUTTON (spinbutton), NULL);

  gtk_container_add (GTK_CONTAINER (abox), size);
  gtk_widget_show (size);

  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (size), 0, preview_vals.width);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (size), 1, preview_vals.height);

  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (size), 0,
				  load_vals.resolution, FALSE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (size), 1,
				  load_vals.resolution, FALSE);

  gtk_widget_show (dialog);

  gtk_main ();

  return load_interface.run;
}
