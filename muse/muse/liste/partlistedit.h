//=============================================================================
//  MusE
//  Linux Music Editor
//  $Id:$
//
//  Copyright (C) 2002-2006 by Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#ifndef __PARTLISTEDIT_H__
#define __PARTLISTEDIT_H__

#include "al/pos.h"
#include "listedit.h"
#include "ui_partlistedit.h"

//---------------------------------------------------------
//   PartListEditor
//---------------------------------------------------------

class PartListEditor : public ListWidget {
      Q_OBJECT

      ListEdit* listEdit;
      Ui::PartListEdit le;
      Part* part;

      void updateList();

   private slots:

   public:
      PartListEditor(ListEdit*, QWidget* parent = 0);
      virtual void setup(const ListType&);
      enum { TICK_COL, TIME_COL };
      };

#endif

