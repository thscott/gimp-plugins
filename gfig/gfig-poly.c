/*
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This is a plug-in for the GIMP.
 *
 * Generates images containing vector type drawings.
 *
 * Copyright (C) 1997 Andy Thomas  alt@picnic.demon.co.uk
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
 */

#include "config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gfig.h"
#include "gfig-dobject.h"
#include "gfig-line.h"

#include "libgimp/stdplugins-intl.h"

static gint poly_num_sides = 3; /* Default to three sided object */

static void       d_draw_poly             (Dobject *obj);
static Dobject  * d_copy_poly             (Dobject * obj);

gboolean
poly_button_press (GtkWidget      *widget,
                   GdkEventButton *event,
                   gpointer        data)
{
  if ((event->type == GDK_2BUTTON_PRESS) &&
      (event->button == 1))
    num_sides_dialog (_("Regular Polygon Number of Sides"),
                      &poly_num_sides, NULL, 3, 200);
  return FALSE;
}

static void
d_draw_poly (Dobject *obj)
{
  DobjPoints * center_pnt;
  DobjPoints * radius_pnt;
  gint16 shift_x;
  gint16 shift_y;
  gdouble ang_grid;
  gdouble ang_loop;
  gdouble radius;
  gdouble offset_angle;
  gint loop;
  GdkPoint start_pnt;
  GdkPoint first_pnt;
  gboolean do_line = FALSE;

  center_pnt = obj->points;

  if (!center_pnt)
    return; /* End-of-line */

  /* First point is the center */
  /* Just draw a control point around it */

  draw_sqr (&center_pnt->pnt);

  /* Next point defines the radius */
  radius_pnt = center_pnt->next; /* this defines the vertices */

  if (!radius_pnt)
    {
#ifdef DEBUG
      g_warning ("Internal error in polygon - no vertice point \n");
#endif /* DEBUG */
      return;
    }

  /* Other control point */
  draw_sqr (&radius_pnt->pnt);

  /* Have center and radius - draw polygon */

  shift_x = radius_pnt->pnt.x - center_pnt->pnt.x;
  shift_y = radius_pnt->pnt.y - center_pnt->pnt.y;

  radius = sqrt ((shift_x*shift_x) + (shift_y*shift_y));

  /* Lines */
  ang_grid = 2 * G_PI / (gdouble) obj->type_data;
  offset_angle = atan2 (shift_y, shift_x);

  for (loop = 0 ; loop < obj->type_data ; loop++)
    {
      gdouble lx, ly;
      GdkPoint calc_pnt;

      ang_loop = (gdouble)loop * ang_grid + offset_angle;
        
      lx = radius * cos (ang_loop);
      ly = radius * sin (ang_loop);

      calc_pnt.x = RINT (lx + center_pnt->pnt.x);
      calc_pnt.y = RINT (ly + center_pnt->pnt.y);

      if (do_line)
        {

          /* Miss out points that come to the same location */
          if (calc_pnt.x == start_pnt.x && calc_pnt.y == start_pnt.y)
            continue;

          gfig_draw_line (calc_pnt.x, calc_pnt.y, start_pnt.x, start_pnt.y);
        }
      else
        {
          do_line = TRUE;
          first_pnt = calc_pnt;
        }
      start_pnt = calc_pnt;
    }

  gfig_draw_line (first_pnt.x, first_pnt.y, start_pnt.x, start_pnt.y);
}

void
d_paint_poly (Dobject *obj)
{
  /* first point center */
  /* Next point is radius */
  gdouble *line_pnts;
  gint seg_count = 0;
  gint i = 0;
  DobjPoints * center_pnt;
  DobjPoints * radius_pnt;
  gint16 shift_x;
  gint16 shift_y;
  gdouble ang_grid;
  gdouble ang_loop;
  gdouble radius;
  gdouble offset_angle;
  gint loop;
  GdkPoint first_pnt, last_pnt;
  gboolean first = TRUE;

  g_assert (obj != NULL);

  /* count - add one to close polygon */
  seg_count = obj->type_data + 1;

  center_pnt = obj->points;

  if (!center_pnt || !seg_count || !center_pnt->next)
    return; /* no-line */

  line_pnts = g_new0 (gdouble, 2 * seg_count + 1);
  
  /* Go around all the points drawing a line from one to the next */

  radius_pnt = center_pnt->next; /* this defines the vetices */

  /* Have center and radius - get lines */
  shift_x = radius_pnt->pnt.x - center_pnt->pnt.x;
  shift_y = radius_pnt->pnt.y - center_pnt->pnt.y;

  radius = sqrt ((shift_x*shift_x) + (shift_y*shift_y));

  /* Lines */
  ang_grid = 2*G_PI/(gdouble) obj->type_data;
  offset_angle = atan2 (shift_y, shift_x);

  for (loop = 0 ; loop < obj->type_data ; loop++)
    {
      gdouble lx, ly;
      GdkPoint calc_pnt;
      
      ang_loop = (gdouble)loop * ang_grid + offset_angle;
        
      lx = radius * cos (ang_loop);
      ly = radius * sin (ang_loop);

      calc_pnt.x = RINT (lx + center_pnt->pnt.x);
      calc_pnt.y = RINT (ly + center_pnt->pnt.y);

      /* Miss out duped pnts */
      if (!first)
        {
          if (calc_pnt.x == last_pnt.x && calc_pnt.y == last_pnt.y)
            {
              continue;
            }
        }

      line_pnts[i++] = calc_pnt.x;
      line_pnts[i++] = calc_pnt.y;
      last_pnt = calc_pnt;

      if (first)
        {
          first_pnt = calc_pnt;
          first = FALSE;
        }
    }

  line_pnts[i++] = first_pnt.x;
  line_pnts[i++] = first_pnt.y;

  /* Reverse line if approp */
  if (selvals.reverselines)
    reverse_pairs_list (&line_pnts[0], i/2);

  /* Scale before drawing */
  if (selvals.scaletoimage)
    scale_to_original_xy (&line_pnts[0], i/2);
  else
    scale_to_xy (&line_pnts[0], i/2);

  /* One go */
  if (selvals.painttype == PAINT_BRUSH_TYPE)
    {
      gfig_paint (selvals.brshtype,
                  gfig_context->drawable_id,
                  i, line_pnts);
    }
  else
    {
      gimp_free_select (gfig_context->image_id,
                        i, line_pnts,
                        selopt.type,
                        selopt.antia,
                        selopt.feather,
                        selopt.feather_radius);
    }

  g_free (line_pnts);
}

void
d_poly2lines (Dobject *obj)
{
  /* first point center */
  /* Next point is radius */
  gint seg_count = 0;
  DobjPoints * center_pnt;
  DobjPoints * radius_pnt;
  gint16 shift_x;
  gint16 shift_y;
  gdouble ang_grid;
  gdouble ang_loop;
  gdouble radius;
  gdouble offset_angle;
  gint loop;
  GdkPoint first_pnt, last_pnt;
  gboolean first = TRUE;

  g_assert (obj != NULL);

  /* count - add one to close polygon */
  seg_count = obj->type_data + 1;

  center_pnt = obj->points;

  if (!center_pnt)
    return; /* no-line */

  /* Undraw it to start with - removes control points */ 
  obj->class->drawfunc (obj);

  /* NULL out these points free later */
  obj->points = NULL;

  /* Go around all the points creating line points */

  radius_pnt = center_pnt->next; /* this defines the vertices */

  /* Have center and radius - get lines */
  shift_x = radius_pnt->pnt.x - center_pnt->pnt.x;
  shift_y = radius_pnt->pnt.y - center_pnt->pnt.y;

  radius = sqrt ((shift_x*shift_x) + (shift_y*shift_y));

  /* Lines */
  ang_grid = 2*G_PI/(gdouble) obj->type_data;
  offset_angle = atan2 (shift_y, shift_x);

  for (loop = 0 ; loop < obj->type_data ; loop++)
    {
      gdouble lx, ly;
      GdkPoint calc_pnt;
      
      ang_loop = (gdouble)loop * ang_grid + offset_angle;
        
      lx = radius * cos (ang_loop);
      ly = radius * sin (ang_loop);

      calc_pnt.x = RINT (lx + center_pnt->pnt.x);
      calc_pnt.y = RINT (ly + center_pnt->pnt.y);

      if (!first)
        {
          if (calc_pnt.x == last_pnt.x && calc_pnt.y == last_pnt.y)
            {
              continue;
            }
        }

      d_pnt_add_line (obj, calc_pnt.x, calc_pnt.y, 0);

      last_pnt = calc_pnt;

      if (first)
        {
          first_pnt = calc_pnt;
          first = FALSE;
        }
    }

  d_pnt_add_line (obj, first_pnt.x, first_pnt.y, 0);
  /* Free old pnts */
  d_delete_dobjpoints (center_pnt);

  /* hey we're a line now */
  obj->type = LINE;
  obj->class = &dobj_class[LINE];

  /* draw it + control pnts */
  obj->class->drawfunc (obj);
}

void
d_star2lines (Dobject *obj)
{
  /* first point center */
  /* Next point is radius */
  gint seg_count = 0;
  DobjPoints * center_pnt;
  DobjPoints * outer_radius_pnt;
  DobjPoints * inner_radius_pnt;
  gint16 shift_x;
  gint16 shift_y;
  gdouble ang_grid;
  gdouble ang_loop;
  gdouble outer_radius;
  gdouble inner_radius;
  gdouble offset_angle;
  gint loop;
  GdkPoint first_pnt, last_pnt;
  gboolean first = TRUE;

  g_assert (obj != NULL);

  /* count - add one to close polygon */
  seg_count = 2*obj->type_data + 1;

  center_pnt = obj->points;

  if (!center_pnt)
    return; /* no-line */

  /* Undraw it to start with - removes control points */ 
  obj->class->drawfunc (obj);

  /* NULL out these points free later */
  obj->points = NULL;

  /* Go around all the points creating line points */
  /* Next point defines the radius */
  outer_radius_pnt = center_pnt->next; /* this defines the vetices */

  if (!outer_radius_pnt)
    {
#ifdef DEBUG
      g_warning ("Internal error in star - no outer vertice point \n");
#endif /* DEBUG */
      return;
    }

  inner_radius_pnt = outer_radius_pnt->next; /* this defines the vetices */

  if (!inner_radius_pnt)
    {
#ifdef DEBUG
      g_warning ("Internal error in star - no inner vertice point \n");
#endif /* DEBUG */
      return;
    }

  shift_x = outer_radius_pnt->pnt.x - center_pnt->pnt.x;
  shift_y = outer_radius_pnt->pnt.y - center_pnt->pnt.y;

  outer_radius = sqrt ((shift_x*shift_x) + (shift_y*shift_y));

  /* Lines */
  ang_grid = 2*G_PI/(2.0*(gdouble) obj->type_data);
  offset_angle = atan2 (shift_y, shift_x);

  shift_x = inner_radius_pnt->pnt.x - center_pnt->pnt.x;
  shift_y = inner_radius_pnt->pnt.y - center_pnt->pnt.y;

  inner_radius = sqrt ((shift_x*shift_x) + (shift_y*shift_y));

  for (loop = 0 ; loop < 2*obj->type_data ; loop++)
    {
      gdouble lx, ly;
      GdkPoint calc_pnt;
      
      ang_loop = (gdouble)loop * ang_grid + offset_angle;

      if (loop%2)
        {
          lx = inner_radius * cos (ang_loop);
          ly = inner_radius * sin (ang_loop);
        }
      else
        {
          lx = outer_radius * cos (ang_loop);
          ly = outer_radius * sin (ang_loop);
        }

      calc_pnt.x = RINT (lx + center_pnt->pnt.x);
      calc_pnt.y = RINT (ly + center_pnt->pnt.y);

      if (!first)
        {
          if (calc_pnt.x == last_pnt.x && calc_pnt.y == last_pnt.y)
            {
              continue;
            }
        }

      d_pnt_add_line (obj, calc_pnt.x, calc_pnt.y, 0);

      last_pnt = calc_pnt;

      if (first)
        {
          first_pnt = calc_pnt;
          first = FALSE;
        }
    }

  d_pnt_add_line (obj, first_pnt.x, first_pnt.y, 0);
  /* Free old pnts */
  d_delete_dobjpoints (center_pnt);

  /* hey we're a line now */
  obj->type = LINE;
  obj->class = &dobj_class[LINE];

  /* draw it + control pnts */
  obj->class->drawfunc (obj);
}

static Dobject *
d_copy_poly (Dobject *obj)
{
  Dobject *np;

  g_assert (obj->type == POLY);

  np = d_new_object (POLY, obj->points->pnt.x, obj->points->pnt.y);
  np->points->next = d_copy_dobjpoints (obj->points->next);
  np->type_data = obj->type_data;

  return np;
}

void
d_poly_object_class_init ()
{
  DobjClass *class = &dobj_class[POLY];

  class->type      = POLY;
  class->name      = "Poly";
  class->drawfunc  = d_draw_poly;
  class->paintfunc = d_paint_poly;
  class->copyfunc  = d_copy_poly;
}

void
d_update_poly (GdkPoint *pnt)
{
  DobjPoints *center_pnt, *edge_pnt;
  gint saved_cnt_pnt = selvals.opts.showcontrol;

  /* Undraw last one then draw new one */
  center_pnt = obj_creating->points;
  
  if (!center_pnt)
    return; /* No points */

  /* Leave the first pnt alone -
   * Edge point defines "radius"
   * Only undraw if already have edge point.
   */

  /* Hack - turn off cnt points in draw routine 
   * Looking back over the other update routines I could
   * use this trick again and cut down on code size!
   */


  if ((edge_pnt = center_pnt->next))
    {
      /* Undraw */
      draw_circle (&edge_pnt->pnt);
      selvals.opts.showcontrol = 0;
      d_draw_poly (obj_creating);

      edge_pnt->pnt = *pnt;
    }
  else
    {
      /* Radius is a few pixels away */
      /* First edge point */
      d_pnt_add_line (obj_creating, pnt->x, pnt->y, -1);
      edge_pnt = center_pnt->next;
    }

  /* draw it */
  selvals.opts.showcontrol = 0;
  d_draw_poly (obj_creating);
  selvals.opts.showcontrol = saved_cnt_pnt;

  /* Realy draw the control points */
  draw_circle (&edge_pnt->pnt);
}

void
d_poly_start (GdkPoint *pnt,
              gint      shift_down)
{
  obj_creating = d_new_object (POLY, pnt->x, pnt->y);
  obj_creating->type_data = poly_num_sides;
}

void
d_poly_end (GdkPoint *pnt,
            gint      shift_down)
{
  draw_circle (pnt);
  add_to_all_obj (gfig_context->current_obj, obj_creating);
  obj_creating = NULL;
}
