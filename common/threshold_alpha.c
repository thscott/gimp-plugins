/* threshold_alpha.c -- This is a plug-in for the GIMP (1.0's API)
 * Author: Shuji Narazaki <narazaki@InetQ.or.jp>
 * Time-stamp: <2000-01-09 13:25:30 yasuhiro>
 * Version: 0.13A (the 'A' is for Adam who hacked in greyscale
 *                 support - don't know if there's a more recent official
 *                 version)
 *
 * Copyright (C) 1997 Shuji Narazaki <narazaki@InetQ.or.jp>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"


#define	PLUG_IN_NAME        "plug_in_threshold_alpha"
#define SHORT_NAME          "threshold_alpha"
#define PROGRESS_UPDATE_NUM 100
#define SCALE_WIDTH         120


static void              query (void);
static void              run   (gchar      *name,
                                gint        nparams,
                                GimpParam  *param,
                                gint       *nreturn_vals,
                                GimpParam **return_vals);

static GimpPDBStatusType threshold_alpha             (gint32     drawable_id);

static gint              threshold_alpha_dialog      (void);
static void              threshold_alpha_ok_callback (GtkWidget *widget,
						      gpointer   data);


static GimpRunMode        run_mode;

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

typedef struct
{
  gint	threshold;
} ValueType;

static ValueType VALS = 
{
  127
};

typedef struct 
{
  gint run;
} Interface;

static Interface INTERFACE =
{
  FALSE
};

MAIN ()

static void
query (void)
{
  static GimpParamDef args [] =
  {
    { GIMP_PDB_INT32,    "run_mode",  "Interactive, non-interactive"},
    { GIMP_PDB_IMAGE,    "image",     "Input image (not used)"},
    { GIMP_PDB_DRAWABLE, "drawable",  "Input drawable" },
    { GIMP_PDB_INT32,    "threshold", "Threshold" }
  };

  gimp_install_procedure (PLUG_IN_NAME,
			  "",
			  "",
			  "Shuji Narazaki (narazaki@InetQ.or.jp)",
			  "Shuji Narazaki",
			  "1997",
			  N_("<Image>/Layer/Transparency/Threshold Alpha..."),
			  "RGBA,GRAYA",
			  GIMP_PLUGIN,
			  G_N_ELEMENTS (args), 0,
			  args, NULL);
}

static void
run (gchar      *name,
     gint        nparams,
     GimpParam  *param,
     gint       *nreturn_vals,
     GimpParam **return_vals)
{
  static GimpParam   values[1];
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  gint               drawable_id;
  
  run_mode    = param[0].data.d_int32;
  drawable_id = param[2].data.d_int32;

  INIT_I18N ();

  *nreturn_vals = 1;
  *return_vals = values;
  
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      /* Since a channel might be selected, we must check wheter RGB or not. */
      if (gimp_layer_get_preserve_transparency (drawable_id))
	{
	  g_message (_("The layer preserves transparency."));
	  return;
	}
      if (!gimp_drawable_is_rgb (drawable_id) &&
	  !gimp_drawable_is_gray (drawable_id))
	{
	  g_message (_("RGBA/GRAYA drawable is not selected."));
	  return;
	}
      gimp_get_data (PLUG_IN_NAME, &VALS);
      if (! threshold_alpha_dialog ())
	return;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      if (nparams != 4)
	{
	  status = GIMP_PDB_CALLING_ERROR;
	}
      else
	{
	  VALS.threshold = param[3].data.d_int32;
	} 
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      gimp_get_data (PLUG_IN_NAME, &VALS);
      break;
    }
  
  if (status == GIMP_PDB_SUCCESS)
    {
      status = threshold_alpha (drawable_id);

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
	gimp_displays_flush ();
      if (run_mode == GIMP_RUN_INTERACTIVE && status == GIMP_PDB_SUCCESS)
	gimp_set_data (PLUG_IN_NAME, &VALS, sizeof (ValueType));
    }

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}

static void 
threshold_alpha_func (guchar *src, guchar *dest, gint bpp, gpointer data)
{
  gint gap;

  for (gap = GPOINTER_TO_INT(data); gap; gap--)
    *dest++ = *src++;
  *dest = (VALS.threshold < *src) ? 255 : 0;
}

static GimpPDBStatusType
threshold_alpha (gint32 drawable_id)
{
  GimpDrawable *drawable;
  gint gap;

  drawable = gimp_drawable_get (drawable_id);
  if (! gimp_drawable_has_alpha (drawable_id))
    return GIMP_PDB_EXECUTION_ERROR;

  gimp_tile_cache_ntiles (2 * (drawable->width / gimp_tile_width () + 1));
  gimp_progress_init (_("Threshold Alpha: Coloring Transparency..."));

  gap = (gimp_drawable_is_rgb (drawable_id)) ? 3 : 1;

  gimp_rgn_iterate2 (drawable, run_mode, threshold_alpha_func, 
		     GINT_TO_POINTER(gap));

  gimp_drawable_detach (drawable);

  return GIMP_PDB_SUCCESS;
}

static gint
threshold_alpha_dialog (void)
{
  GtkWidget *dlg;
  GtkWidget *frame;
  GtkWidget *table;
  GtkObject *adj;

  gimp_ui_init ("threshold_alpha", FALSE);

  dlg = gimp_dialog_new (_("Threshold Alpha"), "threshold_alpha",
			 gimp_standard_help_func, "filters/threshold_alpha.html",
			 GTK_WIN_POS_MOUSE,
			 FALSE, TRUE, FALSE,

			 GTK_STOCK_CANCEL, gtk_widget_destroy,
			 NULL, 1, NULL, FALSE, TRUE,
			 GTK_STOCK_OK, threshold_alpha_ok_callback,
			 NULL, NULL, NULL, TRUE, FALSE,

			 NULL);

  g_signal_connect (dlg, "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  frame = gtk_frame_new (_("Parameter Settings"));
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (1 ,3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
			      _("Threshold:"), SCALE_WIDTH, 0,
			      VALS.threshold, 0, 255, 1, 8, 0,
			      TRUE, 0, 0,
			      NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &VALS.threshold);

  gtk_widget_show (dlg);

  gtk_main ();
  gdk_flush ();

  return INTERFACE.run;
}

static void
threshold_alpha_ok_callback (GtkWidget *widget,
			     gpointer   data)
{
  INTERFACE.run = TRUE;

  gtk_widget_destroy (GTK_WIDGET (data));
}
