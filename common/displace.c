/* Displace --- image filter plug-in for The Gimp image manipulation program
 * Copyright (C) 1996 Stephen Robert Norris
 * Much of the code taken from the pinch plug-in by 1996 Federico Mena Quintero
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
 *
 * You can contact me at srn@flibble.cs.su.oz.au.
 * Please send me any patches or enhancements to this code.
 * You can contact the original The Gimp authors at gimp@xcf.berkeley.edu
 *
 * Extensive modifications to the dialog box, parameters, and some
 * legibility stuff in displace() by Federico Mena Quintero ---
 * federico@nuclecu.unam.mx.  If there are any bugs in these
 * changes, they are my fault and not Stephen's.
 *
 * JTL: May 29th 1997
 * Added (part of) the patch from Eiichi Takamori -- the part which removes the border artefacts
 * (http://ha1.seikyou.ne.jp/home/taka/gimp/displace/displace.html)
 * Added ability to use transparency as the identity transformation
 * (Full transparency is treated as if it was grey 0.5)
 * and the possibility to use RGB/RGBA pictures where the intensity of the pixel is taken into account
 *
 */

/* Version 1.12. */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"


/* Some useful macros */

#define ENTRY_WIDTH     75
#define TILE_CACHE_SIZE 48

typedef struct
{
  gdouble amount_x;
  gdouble amount_y;
  gint    do_x;
  gint    do_y;
  gint    displace_map_x;
  gint    displace_map_y;
  gint    displace_type;
} DisplaceVals;

typedef struct
{
  gint run;
} DisplaceInterface;

/*
 * Function prototypes.
 */

static void      query  (void);
static void      run    (gchar      *name,
			 gint        nparams,
			 GimpParam  *param,
			 gint       *nreturn_vals,
			 GimpParam **return_vals);

static void      displace        (GimpDrawable *drawable);
static gint      displace_dialog (GimpDrawable *drawable);

static gint      displace_map_constrain    (gint32     image_id,
					    gint32     drawable_id,
					    gpointer   data);
static void      displace_map_x_callback   (gint32     id,
					    gpointer   data);
static void      displace_map_y_callback   (gint32     id,
					    gpointer   data);
static void      displace_ok_callback      (GtkWidget *widget,
					    gpointer   data);
static gdouble   displace_map_give_value   (guchar    *ptr,
					    gint       alpha,
					    gint       bytes);

/***** Local vars *****/

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

static DisplaceVals dvals =
{
  20.0,    /* amount_x */
  20.0,    /* amount_y */
  TRUE,    /* do_x */
  TRUE,    /* do_y */
  -1,      /* displace_map_x */
  -1,      /* displace_map_y */
  PIXEL_WRAP     /* displace_type */
};

static DisplaceInterface dint =
{
  FALSE   /*  run  */
};

/***** Functions *****/

MAIN ()

static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run_mode",       "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",          "Input image (unused)" },
    { GIMP_PDB_DRAWABLE, "drawable",       "Input drawable" },
    { GIMP_PDB_FLOAT,    "amount_x",       "Displace multiplier for X direction" },
    { GIMP_PDB_FLOAT,    "amount_y",       "Displace multiplier for Y direction" },
    { GIMP_PDB_INT32,    "do_x",           "Displace in X direction?" },
    { GIMP_PDB_INT32,    "do_y",           "Displace in Y direction?" },
    { GIMP_PDB_DRAWABLE, "displace_map_x", "Displacement map for X direction" },
    { GIMP_PDB_DRAWABLE, "displace_map_y", "Displacement map for Y direction" },
    { GIMP_PDB_INT32,    "displace_type",  "Edge behavior: { WRAP (0), SMEAR (1), BLACK (2) }" }
  };

  gimp_install_procedure ("plug_in_displace",
			  "Displace the contents of the specified drawable",
			  "Displaces the contents of the specified drawable "
			  "by the amounts specified by 'amount_x' and "
			  "'amount_y' multiplied by the intensity of "
			  "corresponding pixels in the 'displace_map' "
			  "drawables.  Both 'displace_map' drawables must be "
			  "of type GIMP_GRAY_IMAGE for this operation to succeed.",
			  "Stephen Robert Norris & (ported to 1.0 by) "
			  "Spencer Kimball",
			  "Stephen Robert Norris",
			  "1996",
			  N_("<Image>/Filters/Map/Displace..."),
			  "RGB*, GRAY*",
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
  GimpDrawable      *drawable;
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;

  run_mode = param[0].data.d_int32;

  INIT_I18N ();

  /*  Get the specified drawable  */
  drawable = gimp_drawable_get (param[2].data.d_drawable);

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      /*  Possibly retrieve data  */
      gimp_get_data ("plug_in_displace", &dvals);

      /*  First acquire information with a dialog  */
      if (! displace_dialog (drawable))
	return;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /*  Make sure all the arguments are there!  */
      if (nparams != 10)
	{
	  status = GIMP_PDB_CALLING_ERROR;
	}
      else
	{
	  dvals.amount_x       = param[3].data.d_float;
	  dvals.amount_y       = param[4].data.d_float;
	  dvals.do_x           = param[5].data.d_int32;
	  dvals.do_y           = param[6].data.d_int32;
	  dvals.displace_map_x = param[7].data.d_int32;
	  dvals.displace_map_y = param[8].data.d_int32;
	  dvals.displace_type  = param[9].data.d_int32;
	}
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /*  Possibly retrieve data  */
      gimp_get_data ("plug_in_displace", &dvals);
      break;

    default:
      break;
    }

  if (status == GIMP_PDB_SUCCESS && (dvals.do_x || dvals.do_y))
    {
      gimp_progress_init (_("Displacing..."));

      /*  set the tile cache size  */
      gimp_tile_cache_ntiles (TILE_CACHE_SIZE);

      /*  run the displace effect  */
      displace (drawable);

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
	gimp_displays_flush ();

      /*  Store data  */
      if (run_mode == GIMP_RUN_INTERACTIVE)
	gimp_set_data ("plug_in_displace", &dvals, sizeof (DisplaceVals));
    }

  values[0].data.d_status = status;

  gimp_drawable_detach (drawable);
}

static int
displace_dialog (GimpDrawable *drawable)
{
  GtkWidget *dlg;
  GtkWidget *label;
  GtkWidget *toggle;
  GtkWidget *toggle_hbox;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *spinbutton;
  GtkObject *adj;
  GtkWidget *option_menu;
  GtkWidget *menu;
  GtkWidget *sep;
  GSList *group = NULL;

  gimp_ui_init ("displace", FALSE);

  dlg = gimp_dialog_new (_("Displace"), "displace",
			 gimp_standard_help_func, "filters/displace.html",
			 GTK_WIN_POS_MOUSE,
			 FALSE, TRUE, FALSE,

			 GTK_STOCK_CANCEL, gtk_widget_destroy,
			 NULL, 1, NULL, FALSE, TRUE,
			 GTK_STOCK_OK, displace_ok_callback,
			 NULL, NULL, NULL, TRUE, FALSE,

			 NULL);

  g_signal_connect (dlg, "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  /*  The main table  */
  frame = gtk_frame_new (_("Displace Options"));
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), frame, TRUE, TRUE, 0);

  table = gtk_table_new (4, 3, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_container_add (GTK_CONTAINER (frame), table);

  /*  X options  */
  toggle = gtk_check_button_new_with_mnemonic (_("_X Displacement:"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 1, 0, 1,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), dvals.do_x);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &dvals.do_x);

  spinbutton = gimp_spin_button_new (&adj, dvals.amount_x,
				     (gint) drawable->width * -2,
				     drawable->width * 2,
				     1, 10, 0, 1, 2);
  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, 0, 1,
		    GTK_FILL, GTK_FILL, 0, 0);

  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.amount_x);

  gtk_widget_set_sensitive (spinbutton, dvals.do_x);
  g_object_set_data (G_OBJECT (toggle), "set_sensitive", spinbutton);
  gtk_widget_show (spinbutton);

  option_menu = gtk_option_menu_new ();
  gtk_table_attach (GTK_TABLE (table), option_menu, 2, 3, 0, 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  menu = gimp_drawable_menu_new (displace_map_constrain, displace_map_x_callback,
				 drawable, dvals.displace_map_x);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

  gtk_widget_set_sensitive (option_menu, dvals.do_x);
  g_object_set_data (G_OBJECT (spinbutton), "set_sensitive", option_menu);
  gtk_widget_show (option_menu);

  /*  Y Options  */
  toggle = gtk_check_button_new_with_mnemonic (_("_Y Displacement:"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 1, 1, 2,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), dvals.do_y);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &dvals.do_y);

  spinbutton = gimp_spin_button_new (&adj, dvals.amount_y,
				     (gint) drawable->height * -2,
				     drawable->height * 2,
				     1, 10, 0, 1, 2);
  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, 1, 2,
		    GTK_FILL, GTK_FILL, 0, 0);

  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &dvals.amount_y);

  gtk_widget_set_sensitive (spinbutton, dvals.do_y);
  g_object_set_data (G_OBJECT (toggle), "set_sensitive", spinbutton);
  gtk_widget_show (spinbutton);

  option_menu = gtk_option_menu_new ();
  gtk_table_attach (GTK_TABLE (table), option_menu, 2, 3, 1, 2,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  menu = gimp_drawable_menu_new (displace_map_constrain, displace_map_y_callback,
				 drawable, dvals.displace_map_y);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

  gtk_widget_set_sensitive (option_menu, dvals.do_y);
  g_object_set_data (G_OBJECT (spinbutton), "set_sensitive", option_menu);
  gtk_widget_show (option_menu);

  /*  Displacement Type  */
  sep = gtk_hseparator_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), sep, 0, 3, 2, 3);
  gtk_widget_show (sep);

  toggle_hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), toggle_hbox, 0, 3, 3, 4,
		    GTK_FILL, GTK_FILL, 0, 0);

  label = gtk_label_new ( _("On Edges:"));
  gtk_box_pack_start (GTK_BOX (toggle_hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  toggle = gtk_radio_button_new_with_mnemonic (group, _("_Wrap"));
  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
  gtk_box_pack_start (GTK_BOX (toggle_hbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);

  g_object_set_data (G_OBJECT (toggle), "gimp-item-data",
                     GINT_TO_POINTER (PIXEL_WRAP));

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_radio_button_update),
                    &dvals.displace_type);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				dvals.displace_type == PIXEL_WRAP);

  toggle = gtk_radio_button_new_with_mnemonic (group, _("_Smear"));
  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
  gtk_box_pack_start (GTK_BOX (toggle_hbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);

  g_object_set_data (G_OBJECT (toggle), "gimp-item-data",
                     GINT_TO_POINTER (PIXEL_SMEAR));

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_radio_button_update),
                    &dvals.displace_type);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				dvals.displace_type == PIXEL_SMEAR);

  toggle = gtk_radio_button_new_with_mnemonic (group, _("_Black"));
  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
  gtk_box_pack_start (GTK_BOX (toggle_hbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);

  g_object_set_data (G_OBJECT (toggle), "gimp-item-data",
                     GINT_TO_POINTER (PIXEL_BLACK));

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_radio_button_update),
                    &dvals.displace_type);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				dvals.displace_type == PIXEL_BLACK);

  gtk_widget_show (toggle_hbox);
  gtk_widget_show (table);
  gtk_widget_show (frame);
  gtk_widget_show (dlg);

  gtk_main ();
  gdk_flush ();

  return dint.run;
}

/* The displacement is done here. */

static void
displace (GimpDrawable *drawable)
{
  GimpDrawable *map_x;
  GimpDrawable *map_y;
  GimpPixelRgn dest_rgn;
  GimpPixelRgn map_x_rgn;
  GimpPixelRgn map_y_rgn;
  gpointer  pr;
  GimpPixelFetcher *pft;

  gint    width;
  gint    height;
  gint    bytes;
  guchar *destrow, *dest;
  guchar *mxrow, *mx;
  guchar *myrow, *my;
  guchar  pixel[4][4];
  gint    x1, y1, x2, y2;
  gint    x, y;
  gint    progress, max_progress;

  gdouble amnt;
  gdouble needx, needy;
  gint    xi, yi;

  guchar  values[4];
  guchar  val;

  gint k;

  gdouble xm_val, ym_val;
  gint    xm_alpha = 0;
  gint    ym_alpha = 0;
  gint    xm_bytes = 1;
  gint    ym_bytes = 1;

  /* initialize */

  mxrow = NULL;
  myrow = NULL;
  
  pft = gimp_pixel_fetcher_new (drawable);

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  width  = drawable->width;
  height = drawable->height;
  bytes  = drawable->bpp;

  progress     = 0;
  max_progress = (x2 - x1) * (y2 - y1);

  /*
   * The algorithm used here is simple - see
   * http://the-tech.mit.edu/KPT/Tips/KPT7/KPT7.html for a description.
   */

  /* Get the drawables  */
  if (dvals.displace_map_x != -1 && dvals.do_x)
    {
      map_x = gimp_drawable_get (dvals.displace_map_x);
      gimp_pixel_rgn_init (&map_x_rgn, map_x,
			   x1, y1, (x2 - x1), (y2 - y1), FALSE, FALSE);
      if (gimp_drawable_has_alpha(map_x->drawable_id))
	xm_alpha = 1;
      xm_bytes = gimp_drawable_bpp(map_x->drawable_id);
    }
  else
    map_x = NULL;

  if (dvals.displace_map_y != -1 && dvals.do_y)
    {
      map_y = gimp_drawable_get (dvals.displace_map_y);
      gimp_pixel_rgn_init (&map_y_rgn, map_y,
			   x1, y1, (x2 - x1), (y2 - y1), FALSE, FALSE);
      if (gimp_drawable_has_alpha(map_y->drawable_id))
	ym_alpha = 1;
      ym_bytes = gimp_drawable_bpp(map_y->drawable_id);
    }
  else
    map_y = NULL;

  gimp_pixel_rgn_init (&dest_rgn, drawable,
		       x1, y1, (x2 - x1), (y2 - y1), TRUE, TRUE);

  /*  Register the pixel regions  */
  if (dvals.do_x && dvals.do_y)
    pr = gimp_pixel_rgns_register (3, &dest_rgn, &map_x_rgn, &map_y_rgn);
  else if (dvals.do_x)
    pr = gimp_pixel_rgns_register (2, &dest_rgn, &map_x_rgn);
  else if (dvals.do_y)
    pr = gimp_pixel_rgns_register (2, &dest_rgn, &map_y_rgn);
  else
    pr = NULL;

  for (pr = pr; pr != NULL; pr = gimp_pixel_rgns_process (pr))
    {
      destrow = dest_rgn.data;
      if (dvals.do_x)
	mxrow = map_x_rgn.data;
      if (dvals.do_y)
	myrow = map_y_rgn.data;
      
      for (y = dest_rgn.y; y < (dest_rgn.y + dest_rgn.h); y++)
	{
	  dest = destrow;
	  mx = mxrow;
	  my = myrow;
	  
	  /*
	   * We could move the displacement image address calculation out of here,
	   * but when we can have different sized displacement and destination
	   * images we'd have to move it back anyway.
	   */
	  
	  for (x = dest_rgn.x; x < (dest_rgn.x + dest_rgn.w); x++)
	    {
	      if (dvals.do_x)
		{
		  xm_val = displace_map_give_value(mx, xm_alpha, xm_bytes); 
		  amnt = dvals.amount_x * (xm_val - 127.5) / 127.5;
		  needx = x + amnt;
		  mx += xm_bytes;
		}
	      else
		needx = x;
	      
	      if (dvals.do_y)
		{
		  ym_val = displace_map_give_value(my, ym_alpha, ym_bytes);
		  amnt = dvals.amount_y * (ym_val - 127.5) / 127.5;
		  needy = y + amnt;
		  my += ym_bytes;
		}
	      else
		needy = y;
	      
	      /* Calculations complete; now copy the proper pixel */
	      
	      if (needx >= 0.0)
		xi = (int) needx;
	      else
		xi = -((int) -needx + 1);

	      if (needy >= 0.0)
		yi = (int) needy;
	      else
		yi = -((int) -needy + 1);
	      
	      gimp_pixel_fetcher_get_pixel2 (pft, xi, yi,
					     dvals.displace_type, pixel[0]);
	      gimp_pixel_fetcher_get_pixel2 (pft, xi + 1, yi,
					     dvals.displace_type, pixel[1]);
	      gimp_pixel_fetcher_get_pixel2 (pft, xi, yi + 1,
					     dvals.displace_type, pixel[2]);
	      gimp_pixel_fetcher_get_pixel2 (pft, xi + 1, yi + 1,
					     dvals.displace_type, pixel[3]);
	      
	      for (k = 0; k < bytes; k++)
		{
		  values[0] = pixel[0][k];
		  values[1] = pixel[1][k];
		  values[2] = pixel[2][k];
		  values[3] = pixel[3][k];
		  val = gimp_bilinear_8 (needx, needy, values);
		  
		  *dest++ = val;
		} /* for */
	    }
	  
	  destrow += dest_rgn.rowstride;
	  
	  if (dvals.do_x)
	    mxrow += map_x_rgn.rowstride;
	  if (dvals.do_y)
	    myrow += map_y_rgn.rowstride;
	}
      
      progress += dest_rgn.w * dest_rgn.h;
      gimp_progress_update ((double) progress / (double) max_progress);
    } /* for */

  gimp_pixel_fetcher_destroy (pft);

  /*  detach from the map drawables  */
  if (dvals.do_x)
    gimp_drawable_detach (map_x);
  if (dvals.do_y)
    gimp_drawable_detach (map_y);

  /*  update the region  */
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x1, y1, (x2 - x1), (y2 - y1));
}

static gdouble
displace_map_give_value (guchar *pt,
			 gint    alpha,
			 gint    bytes)
{
  gdouble ret, val_alpha;
  
  if (bytes >= 3)
    ret =  INTENSITY (pt[0], pt[1], pt[2]);
  else
    ret = (gdouble) *pt;
  
  if (alpha)
    {
      val_alpha = pt[bytes - 1];
      ret = ((ret - 127.5) * val_alpha / 255.0) + 127.5;
    }
  
  return ret;
}

/*  Displace interface functions  */

static gint
displace_map_constrain (gint32   image_id,
			gint32   drawable_id,
			gpointer data)
{
  GimpDrawable *drawable;

  drawable = (GimpDrawable *) data;

  if (drawable_id == -1)
    return TRUE;

  return (gimp_drawable_width (drawable_id) == drawable->width &&
	  gimp_drawable_height (drawable_id) == drawable->height);
}

static void
displace_map_x_callback (gint32   id,
			 gpointer data)
{
  dvals.displace_map_x = id;
}

static void
displace_map_y_callback (gint32   id,
			 gpointer data)
{
  dvals.displace_map_y = id;
}

static void
displace_ok_callback (GtkWidget *widget,
		      gpointer   data)
{
  dint.run = TRUE;
  gtk_widget_destroy (GTK_WIDGET (data));
}
