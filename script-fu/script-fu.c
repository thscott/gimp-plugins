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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>
#include <libgimp/gimp.h>

#include "siod-wrapper.h"
#include "script-fu-console.h"
#include "script-fu-constants.h"
#include "script-fu-scripts.h"
#include "script-fu-server.h"
#include "script-fu-text-console.h"

#include "script-fu-intl.h"

/* External functions
 */
extern void  gimp_extension_process (guint timeout);
extern void  gimp_extension_ack     (void);


/* Declare local functions.
 */
static void      script_fu_quit   (void);
static void      script_fu_query  (void);
static void      script_fu_run    (gchar        *name,
				   gint          nparams,
				   GimpParam    *param,
				   gint         *nreturn_vals,
				   GimpParam   **return_vals);


static void      script_fu_auxillary_init (void);
static void      script_fu_refresh_proc   (gchar      *name,
					   gint        nparams,
					   GimpParam  *params,
					   gint       *nreturn_vals,
					   GimpParam **return_vals);


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,             /* init_proc  */
  script_fu_quit,   /* quit_proc  */
  script_fu_query,  /* query_proc */
  script_fu_run,    /* run_proc   */
};



MAIN ()

static void
script_fu_quit (void)
{
}

static void
script_fu_query (void)
{
  static GimpParamDef console_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode", "Interactive, [non-interactive]" }
  };
  static gint nconsole_args = sizeof (console_args) / sizeof (console_args[0]);

  static GimpParamDef textconsole_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode", "Interactive, [non-interactive]" }
  };
  static gint ntextconsole_args = sizeof (textconsole_args) / sizeof (textconsole_args[0]);

  static GimpParamDef eval_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode", "[Interactive], non-interactive" },
    { GIMP_PDB_STRING, "code",     "The code to evaluate" }
  };
  static gint neval_args = sizeof (eval_args) / sizeof (eval_args[0]);

  static GimpParamDef server_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode", "[Interactive], non-interactive" },
    { GIMP_PDB_INT32,  "port",     "The port on which to listen for requests" },
    { GIMP_PDB_STRING, "logfile",  "The file to log server activity to" }
  };
  static gint nserver_args = sizeof (server_args) / sizeof (server_args[0]);

  gimp_plugin_domain_register ("gimp-script-fu", NULL);

  gimp_install_procedure ("extension_script_fu",
			  "A scheme interpreter for scripting GIMP operations",
			  "More help here later",
			  "Spencer Kimball & Peter Mattis",
			  "Spencer Kimball & Peter Mattis",
			  "1997",
			  NULL,
			  NULL,
			  GIMP_EXTENSION,
			  0, 0, NULL, NULL);

  gimp_install_procedure ("extension_script_fu_console",
			  "Provides a console mode for script-fu development",
			  "Provides an interface which allows interactive scheme development.",
			  "Spencer Kimball & Peter Mattis",
			  "Spencer Kimball & Peter Mattis",
			  "1997",
			  N_("<Toolbox>/Xtns/Script-Fu/Console..."),
			  NULL,
			  GIMP_EXTENSION,
			  nconsole_args, 0,
			  console_args, NULL);

  gimp_install_procedure ("extension_script_fu_text_console",
                          "Provides a text console mode for script-fu development",
                          "Provides an interface which allows interactive scheme development.",
                          "Spencer Kimball & Peter Mattis",
                          "Spencer Kimball & Peter Mattis",
                          "1997",
			  NULL,
                          NULL,
                          GIMP_EXTENSION,
                          ntextconsole_args, 0,
                          textconsole_args, NULL);

#ifndef G_OS_WIN32
  gimp_install_procedure ("extension_script_fu_server",
			  "Provides a server for remote script-fu operation",
			  "Provides a server for remote script-fu operation",
			  "Spencer Kimball & Peter Mattis",
			  "Spencer Kimball & Peter Mattis",
			  "1997",
			  N_("<Toolbox>/Xtns/Script-Fu/Server..."),
			  NULL,
			  GIMP_EXTENSION,
			  nserver_args, 0,
			  server_args, NULL);
#endif

  gimp_install_procedure ("extension_script_fu_eval",
			  "Evaluate scheme code",
			  "Evaluate the code under the scheme interpeter (primarily for batch mode)",
			  "Manish Singh",
			  "Manish Singh",
			  "1998",
			  NULL,
			  NULL,
			  GIMP_EXTENSION,
			  neval_args, 0,
			  eval_args, NULL);
}

static void
script_fu_run (gchar      *name,
	       gint        nparams,
	       GimpParam  *param,
	       gint       *nreturn_vals,
	       GimpParam **return_vals)
{
  siod_set_output_file (stdout);

  /*  Determine before we allow scripts to register themselves
   *   whether this is the base, automatically installed script-fu extension
   */
  if (strcmp (name, "extension_script_fu") == 0)
    {
      /*  Setup auxillary temporary procedures for the base extension  */
      script_fu_auxillary_init ();

      /*  Init the interpreter  */
      siod_init (TRUE);
    }
  else
    {
      /*  Init the interpreter  */
      siod_init (FALSE);
    }

  /*  Load all of the available scripts  */
  script_fu_find_scripts ();

  if (strcmp (name, "extension_script_fu") == 0)
    {
      /*
       *  The main, automatically installed script fu extension.  For
       *  things like logos and effects that are runnable from GIMP
       *  menus.
       */

      static GimpParam  values[1];
      GimpPDBStatusType status = GIMP_PDB_SUCCESS;

      /*  Acknowledge that the extension is properly initialized  */
      gimp_extension_ack ();

      while (TRUE)
	gimp_extension_process (0);

      *nreturn_vals = 1;
      *return_vals  = values;

      values[0].type          = GIMP_PDB_STATUS;
      values[0].data.d_status = status;
    }
  else if (strcmp (name, "extension_script_fu_text_console") == 0)
    {
      /*
       *  The script-fu text console for interactive SIOD development
       */

      script_fu_text_console_run (name, nparams, param,
				  nreturn_vals, return_vals);
    }
  else if (strcmp (name, "extension_script_fu_console") == 0)
    {
      /*
       *  The script-fu console for interactive SIOD development
       */

      script_fu_console_run (name, nparams, param,
			     nreturn_vals, return_vals);
    }
#ifndef G_OS_WIN32
  else if (strcmp (name, "extension_script_fu_server") == 0)
    {
      /*
       *  The script-fu server for remote operation
       */

      script_fu_server_run (name, nparams, param,
			    nreturn_vals, return_vals);
    }
#endif
  else if (strcmp (name, "extension_script_fu_eval") == 0)
    {
      /*
       *  A non-interactive "console" (for batch mode)
       */

      script_fu_eval_run (name, nparams, param,
			  nreturn_vals, return_vals);
    }
}


static void
script_fu_auxillary_init (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "[Interactive], non-interactive" }
  };
  static gint nargs = sizeof (args) / sizeof (args[0]);

  INIT_I18N_UI();

  gimp_install_temp_proc ("script_fu_refresh",
			  _("Re-read all available scripts"),
			  _("Re-read all available scripts"),
			  "Spencer Kimball & Peter Mattis",
			  "Spencer Kimball & Peter Mattis",
			  "1997",
			  N_("<Toolbox>/Xtns/Script-Fu/Refresh"),
			  NULL,
			  GIMP_TEMPORARY,
			  nargs, 0,
			  args, NULL,
			  script_fu_refresh_proc);
}

static void
script_fu_refresh_proc (gchar       *name,
			gint         nparams,
			GimpParam   *params,
			gint        *nreturn_vals,
			GimpParam  **return_vals)
{
  static GimpParam  values[1];
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /*  Reload all of the available scripts  */
  script_fu_find_scripts ();

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}
