/* xmp-gen.h - generate XMP metadata from the tree model
 *
 * Copyright (C) 2005, Raphaël Quinet <raphael@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef XMP_GEN_H
#define XMP_GEN_H

#include <glib.h>
#include "xmp-model.h"

G_BEGIN_DECLS

gssize xmp_estimate_size  (XMPModel *xmp_model);

gssize xmp_generate_block (XMPModel *xmp_model,
                           gchar    *buffer,
                           gssize    buffer_size);

G_END_DECLS

#endif /* XMP_GEN_H */
