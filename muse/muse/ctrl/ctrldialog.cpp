//=============================================================================
//  MusE
//  Linux Music Editor
//  $Id:$
//
//  Copyright (C) 2006 by Werner Schweer and others
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

#include "ctrldialog.h"
#include "midictrl.h"
#include "track.h"
#include "miditrack.h"
#include "audiotrack.h"
#include "plugin.h"

//---------------------------------------------------------
//   CtrlDialog
//---------------------------------------------------------

CtrlDialog::CtrlDialog(Track* track, int currentId, QWidget* parent)
  : QDialog(parent)
      {
      setupUi(this);
      QTreeWidgetItem* header = tw->headerItem();
      header->setTextAlignment(0, Qt::AlignLeft);
      header->setTextAlignment(1, Qt::AlignHCenter);
      header->setSizeHint(1, QSize(30, 20));
      header->setToolTip(0, tr("controller name"));
      header->setToolTip(1, tr("flag if controller contains data"));

      QTreeWidgetItem* ci;
      if (track->type() == Track::MIDI) {
            //
            //    add special controll for midi tracks
            //
            ci = new QTreeWidgetItem(tw, CTRL_VELOCITY);
            ci->setText(0, "Velocity");
            if (CTRL_VELOCITY == currentId) {
                  tw->setCurrentItem(ci);
                  tw->setItemSelected(ci, true);
                  }
            if (((MidiTrack*)(track))->drumMap()) {
                  ci = new QTreeWidgetItem(tw, CTRL_SVELOCITY);
                  ci->setText(0, "Single Velocity");

            	if (CTRL_SVELOCITY == currentId) {
                        tw->setCurrentItem(ci);
                        tw->setItemSelected(ci, true);
                        }
                  }
            }
      else if (!track->isMidiTrack()) {
            //
            // present plugin parameter
            //
            Pipeline* pl = ((AudioTrack*)track)->efxPipe();
            int idx = 0;
            for (iPluginI i = pl->begin(); i != pl->end(); ++i, ++idx) {
                  PluginI* plugin = *i;
                  if (plugin == 0)
                        continue;
                  ci = new QTreeWidgetItem(tw, CTRL_NO_CTRL);
                  ci->setText(0, plugin->name());
                  int ncontroller = plugin->plugin()->parameter();
                  for (int i = 0; i < ncontroller; ++i) {
                        QString name(plugin->getParameterName(i));
                        int id = (idx + 1) * 0x1000 + i;
                        QTreeWidgetItem* cci = new QTreeWidgetItem(ci, id);
                        cci->setText(0, name);
                        Ctrl* ctrl = track->getController(id);
                        if (!ctrl->empty())
                              cci->setText(1, "*");
                        if (id == currentId) {
                              tw->setCurrentItem(cci);
                              tw->setItemSelected(cci, true);
                              }
                        }
                  }
            //
            //    add rest parameter
            //
            ControllerNameList* cn = track->controllerNames();
            for (iControllerName i = cn->begin(); i != cn->end(); ++i) {
                  if (i->id & 0xfffff000)
                        continue;
                  ci = new QTreeWidgetItem(tw, i->id);
                  ci->setText(0, i->name);
                  Ctrl* ctrl = track->getController(i->id);
                  if (!ctrl->empty())
                        ci->setText(1, "*");
                  if (i->id == currentId) {
                        tw->setCurrentItem(ci);
                        tw->setItemSelected(ci, true);
                        }
                  }
            }

      else {
            ControllerNameList* cn = track->controllerNames();
            for (iControllerName i = cn->begin(); i != cn->end(); ++i) {
                  ci = new QTreeWidgetItem(tw, i->id);
                  ci->setText(0, i->name);
                  Ctrl* ctrl = track->getController(i->id);
                  if (!ctrl->empty())
                        ci->setText(1, "*");

                  if (i->id == currentId) {
                        tw->setCurrentItem(ci);
                        tw->setItemSelected(ci, true);
                        }
                  }
            }
      if (track->type() == Track::MIDI) {
            //
            //    add midi channel controller
            //
            MidiChannel* mc = ((MidiTrack*)track)->channel();
            if (mc) {
                  ci = new QTreeWidgetItem(tw, CTRL_NO_CTRL);
                  ci->setText(0, tr("Midi Channel Controller"));

                  ControllerNameList* cn = mc->controllerNames();
                  for (iControllerName i = cn->begin(); i != cn->end(); ++i) {
                        QTreeWidgetItem* cci = new QTreeWidgetItem(ci, i->id);
                        cci->setText(0, i->name);
                        Ctrl* ctrl = mc->getController(i->id);
                        if (!ctrl->empty())
                              cci->setText(1, "*");

                        if (i->id == currentId) {
                              tw->setCurrentItem(cci);
                              tw->setItemSelected(cci, true);
                              }
                        }
                  }
            }
      ci = new QTreeWidgetItem(tw, CTRL_OTHER);
      ci->setText(0, tr("other"));
      connect(tw, 
         SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
         SLOT(itemDoubleClicked(QTreeWidgetItem*, int)));
      }

//---------------------------------------------------------
//   itemDoubleClicked
//---------------------------------------------------------

void CtrlDialog::itemDoubleClicked(QTreeWidgetItem* item, int)
      {
      if (item->type() != CTRL_NO_CTRL)
            accept();
      }

//---------------------------------------------------------
//   CtrlDialog
//---------------------------------------------------------

int CtrlDialog::curId() const
      {
      QTreeWidgetItem* item = tw->currentItem();
      if (item == 0)
            return CTRL_NO_CTRL;
      return item->type();
      }

