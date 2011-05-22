//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: dlist.cpp,v 1.9.2.7 2009/10/16 21:50:16 terminator356 Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#include <QCursor>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

#include <stdio.h>

#include "audio.h"
#include "pitchedit.h"
#include "midiport.h"
#include "drummap.h"
#include "icons.h"
#include "dlist.h"
#include "song.h"
#include "scrollscale.h"


//---------------------------------------------------------
//   draw
//---------------------------------------------------------

void DList::draw(QPainter& p, const QRect& rect)
      {
      int x = rect.x();
      int y = rect.y();
      int w = rect.width();
      int h = rect.height();

      //---------------------------------------------------
      //    Tracks
      //---------------------------------------------------

      p.setPen(Qt::black);

      for (int i = 0; i < DRUM_MAPSIZE; ++i) {
            int yy = i * TH;
            if (yy+TH < y)
                  continue;
            if (yy > y + h)
                  break;
            DrumMap* dm = &drumMap[i];
            if (dm == currentlySelected)
                  p.fillRect(x, yy, w, TH, Qt::yellow);
//            else
//                  p.eraseRect(x, yy, w, TH);
            QHeaderView *h = header;
            for (int k = 0; k < h->count(); ++k) {
                  int x   = h->sectionPosition(k);
                  int w   = h->sectionSize(k);
                  QRect r = p.combinedTransform().mapRect(QRect(x+2, yy, w-4, TH));
                  QString s;
                  int align = Qt::AlignVCenter | Qt::AlignHCenter;

                  p.save();
                  p.setWorldMatrixEnabled(false);
                  switch (k) {
                        case COL_VOL:
                              s.setNum(dm->vol);
                              break;
                        case COL_QNT:
                              s.setNum(dm->quant);
                              break;
                        case COL_LEN:
                              s.setNum(dm->len);
                              break;
                        case COL_ANOTE:
                              s = pitch2string(dm->anote);
                              break;
                        case COL_ENOTE:
                              s = pitch2string(dm->enote);
                              break;
                        case COL_LV1:
                              s.setNum(dm->lv1);
                              break;
                        case COL_LV2:
                              s.setNum(dm->lv2);
                              break;
                        case COL_LV3:
                              s.setNum(dm->lv3);
                              break;
                        case COL_LV4:
                              s.setNum(dm->lv4);
                              break;
                        case COL_MUTE:
                              if (dm->mute) {
                                    p.setPen(Qt::red);
                                    const QPixmap& pm = *muteIcon;
                                    p.drawPixmap(
                                       r.x() + r.width()/2 - pm.width()/2,
                                       r.y() + r.height()/2 - pm.height()/2,
                                       pm);
                                    p.setPen(Qt::black);
                                    }
                              break;
                        case COL_NAME:
                              s = dm->name;
                              align = Qt::AlignVCenter | Qt::AlignLeft;
                              break;
                        case COL_CHANNEL:
                              s.setNum(dm->channel+1);
                              break;
                        case COL_PORT:
                              s.sprintf("%d:%s", dm->port+1, midiPorts[dm->port].portname().toLatin1().constData());
                              align = Qt::AlignVCenter | Qt::AlignLeft;
                              break;
                        }
                  if (!s.isEmpty())
                        p.drawText(r, align, s);
                  p.restore();
                  }
            }

      //---------------------------------------------------
      //    horizontal lines
      //---------------------------------------------------

      p.setPen(Qt::gray);
      int yy  = (y / TH) * TH;
      for (; yy < y + h; yy += TH) {
            p.drawLine(x, yy, x + w, yy);
            }

      if (drag == DRAG) {
            int y  = (startY/TH) * TH;
            int dy = startY - y;
            int yy = curY - dy;
            p.setPen(Qt::green);
            p.drawLine(x, yy, x + w, yy);
            p.drawLine(x, yy+TH, x + w, yy+TH);
            p.setPen(Qt::gray);
            }

      //---------------------------------------------------
      //    vertical Lines
      //---------------------------------------------------

      p.setWorldMatrixEnabled(false);
      int n = header->count();
      x = 0;
      for (int i = 0; i < n; i++) {
            x += header->sectionSize(header->visualIndex(i));
            p.drawLine(x, 0, x, height());
            }
      p.setWorldMatrixEnabled(true);
      }

//---------------------------------------------------------
//   devicesPopupMenu
//---------------------------------------------------------

void DList::devicesPopupMenu(DrumMap* t, int x, int y, bool changeAll)
      {
      QMenu* p = midiPortsPopup();
      QAction* act = p->exec(mapToGlobal(QPoint(x, y)), 0);
      bool doemit = false;
      if (act) {
            int n = act->data().toInt();
            if (!changeAll)
            {
                if(n != t->port)
                {
                  audio->msgIdle(true);
                  song->remapPortDrumCtrlEvents(getSelectedInstrument(), -1, -1, n);
                  audio->msgIdle(false);
                  t->port = n;
                  doemit = true;
                }  
            }      
            else {
                  audio->msgIdle(true);
                  // Delete all port controller events.
                  song->changeAllPortDrumCtrlEvents(false);
                  
                  for (int i = 0; i < DRUM_MAPSIZE; i++)
                        drumMap[i].port = n;
                  // Add all port controller events.
                  song->changeAllPortDrumCtrlEvents(true);
                  
                  audio->msgIdle(false);
                  doemit = true;
                  }
            }
      delete p;
      if(doemit)
      {
        int instr = getSelectedInstrument();
        if(instr != -1)
          //emit curDrumInstrumentChanged(instr);
          song->update(SC_DRUMMAP);
      }            
    }

//---------------------------------------------------------
//   viewMousePressEvent
//---------------------------------------------------------

void DList::viewMousePressEvent(QMouseEvent* ev)
      {
      int x      = ev->x();
      int y      = ev->y();
      int button = ev->button();
      unsigned pitch = y / TH;
      DrumMap* dm = &drumMap[pitch];

      setCurDrumInstrument(pitch);

      startY = y;
      sPitch = pitch;
      drag   = START_DRAG;

      DCols col = DCols(x2col(x));

      int val;
      int incVal = 0;
      if (button == Qt::RightButton)
            incVal = 1;
      else if (button == Qt::MidButton)
            incVal = -1;

      // Check if we're already editing anything and have pressed the mouse
      // elsewhere
      // In that case, treat it as if a return was pressed

      if (button == Qt::LeftButton) {
            if (((editEntry && editEntry != dm)  || col != selectedColumn) && editEntry != 0) {
                  returnPressed();
                  }
            }

      switch (col) {
            case COL_NONE:
                  break;
            case COL_MUTE:
                  if (button == Qt::LeftButton)
                        dm->mute = !dm->mute;
                  break;
            case COL_PORT:
                  if (button == Qt::RightButton) {
                        bool changeAll = ev->modifiers() & Qt::ControlModifier;
                        devicesPopupMenu(dm, mapx(x), mapy(pitch * TH), changeAll);
                        }
                  break;
            case COL_VOL:
                  val = dm->vol + incVal;
                  if (val < 0)
                        val = 0;
                  else if (val > 200)
                        val = 200;
                  dm->vol = (unsigned char)val;      
                  break;
            case COL_QNT:
                  dm->quant += incVal;
                  // ?? range
                  break;
            case COL_ENOTE:
                  val = dm->enote + incVal;
                  if (val < 0)
                        val = 0;
                  else if (val > 127)
                        val = 127;
                  //Check if there is any other drumMap with the same inmap value (there should be one (and only one):-)
                  //If so, switch the inmap between the instruments
                  for (int i=0; i<DRUM_MAPSIZE; i++) {
                        if (drumMap[i].enote == val && &drumMap[i] != dm) {
                              drumInmap[int(dm->enote)] = i;
                              drumMap[i].enote = dm->enote;
                              break;
                              }
                        }
                  //TODO: Set all the notes on the track with pitch=dm->enote to pitch=val
                  dm->enote = val;
                  drumInmap[val] = pitch;
                  break;
            case COL_LEN:
                  val = dm->len + incVal;
                  if (val < 0)
                        val = 0;
                  dm->len = val;
                  break;
            case COL_ANOTE:
                  {
                    val = dm->anote + incVal;
                    if (val < 0)
                          val = 0;
                    else if (val > 127)
                          val = 127;
                    if(val != dm->anote)
                    {
                      audio->msgIdle(true);
                      //audio->msgRemapPortDrumCtlEvents(pitch, val, -1, -1);
                      song->remapPortDrumCtrlEvents(pitch, val, -1, -1);
                      audio->msgIdle(false);
                      dm->anote = val;
                      song->update(SC_DRUMMAP);
                    }
                    int velocity = 127 * float(ev->x()) / width();
                    emit keyPressed(pitch, velocity);//(dm->anote, shift);
                  }
                  break;
            case COL_CHANNEL:
                  val = dm->channel + incVal;
                  if (val < 0)
                        val = 0;
                  else if (val > 127)
                        val = 127;
                  
                  if (ev->modifiers() & Qt::ControlModifier) {
                        audio->msgIdle(true);
                        // Delete all port controller events.
                        song->changeAllPortDrumCtrlEvents(false, true);
                        
                        for (int i = 0; i < DRUM_MAPSIZE; i++)
                              drumMap[i].channel = val;
                        // Add all port controller events.
                        song->changeAllPortDrumCtrlEvents(true, true);
                        audio->msgIdle(false);
                        song->update(SC_DRUMMAP);
                        }
                  else
                  {
                      if(val != dm->channel)
                      {
                        audio->msgIdle(true);
                        song->remapPortDrumCtrlEvents(pitch, -1, val, -1);
                        audio->msgIdle(false);
                        dm->channel = val;
                        song->update(SC_DRUMMAP);
                      }  
                  }      
                  break;
            case COL_LV1:
                  val = dm->lv1 + incVal;
                  if (val < 0)
                        val = 0;
                  else if (val > 127)
                        val = 127;
                  dm->lv1 = val;
                  break;
            case COL_LV2:
                  val = dm->lv2 + incVal;
                  if (val < 0)
                        val = 0;
                  else if (val > 127)
                        val = 127;
                  dm->lv2 = val;
                  break;
            case COL_LV3:
                  val = dm->lv3 + incVal;
                  if (val < 0)
                        val = 0;
                  else if (val > 127)
                        val = 127;
                  dm->lv3 = val;
                  break;
            case COL_LV4:
                  val = dm->lv4 + incVal;
                  if (val < 0)
                        val = 0;
                  else if (val > 127)
                        val = 127;
                  dm->lv4 = val;
                  break;
            case COL_NAME:
                  emit keyPressed(pitch, 100); //Mapping done on other side, send index
                  break;
#if 0
            case COL_CHANNEL:
                  {
                  int channel = t->channel();
                  if (button == Qt::RightButton) {
                        if (channel < 15)
                              ++channel;
                        }
                  else if (button == Qt::MidButton) {
                        if (channel > 0)
                              --channel;
                        }
                  if (channel != t->channel()) {
                        t->setChannel(channel);
                        emit channelChanged();
                        }
                  }
#endif
            default:
                  break;
            }
      redraw();
      }

//---------------------------------------------------------
//   viewMouseDoubleClickEvent
//---------------------------------------------------------

void DList::viewMouseDoubleClickEvent(QMouseEvent* ev)
      {
      int x = ev->x();
      int y = ev->y();
      unsigned pitch = y / TH;

      int section = header->logicalIndexAt(x);

      if ((section == COL_NAME || section == COL_VOL || section == COL_LEN || section == COL_LV1 ||
         section == COL_LV2 || section == COL_LV3 || section == COL_LV4) && (ev->button() == Qt::LeftButton))
         {
           lineEdit(pitch, section);
         }
      else
            viewMousePressEvent(ev);
      }



//---------------------------------------------------------
//   lineEdit
//---------------------------------------------------------
void DList::lineEdit(int line, int section)
      {
            DrumMap* dm = &drumMap[line];
            editEntry = dm;
            if (editor == 0) {
                  editor = new DLineEdit(this);
                  connect(editor, SIGNAL(returnPressed()),
                     SLOT(returnPressed()));
                  editor->setFrame(true);
                  }
            int colx = mapx(header->sectionPosition(section));
            int colw = rmapx(header->sectionSize(section));
            int coly = mapy(line * TH);
            int colh = rmapy(TH);
            selectedColumn = section; //Store selected column to have an idea of which one was selected when return is pressed
            switch (section) {
                  case COL_NAME:
                  editor->setText(dm->name);
                  break;

                  case COL_VOL: {
                  editor->setText(QString::number(dm->vol));
                  break;
                  }
                  
                  case COL_LEN: {
                  editor->setText(QString::number(dm->len));
                  break;
                  }

                  case COL_LV1:
                  editor->setText(QString::number(dm->lv1));
                  break;

                  case COL_LV2:
                  editor->setText(QString::number(dm->lv2));
                  break;

                  case COL_LV3:
                  editor->setText(QString::number(dm->lv3));
                  break;

                  case COL_LV4:
                  editor->setText(QString::number(dm->lv4));
                  break;
            }

            editor->end(false);
            editor->setGeometry(colx, coly, colw, colh);
            // In all cases but the column name, select all text:
            if (section != COL_NAME)
                  editor->selectAll();
            editor->show();
            editor->setFocus();

     }


//---------------------------------------------------------
//   x2col
//---------------------------------------------------------

int DList::x2col(int x) const
      {
      int col = 0;
      int w = 0;
      for (; col < header->count(); col++) {
            w += header->sectionSize(col);
            if (x < w)
                  break;
            }
      if (col == header->count())
            return -1;
      return header->logicalIndex(col);
      }

//---------------------------------------------------------
//   setCurDrumInstrument
//---------------------------------------------------------

void DList::setCurDrumInstrument(int instr)
      {
      if (instr < 0 || instr >= DRUM_MAPSIZE -1)
        return; // illegal instrument
      DrumMap* dm = &drumMap[instr];
      if (currentlySelected != dm) {
            currentlySelected = &drumMap[instr];
            emit curDrumInstrumentChanged(instr);
            song->update(SC_DRUMMAP);
            }
      }

//---------------------------------------------------------
//   sizeChange
//---------------------------------------------------------

void DList::sizeChange(int, int, int)
      {
      redraw();
      }

//---------------------------------------------------------
//   returnPressed
//---------------------------------------------------------

void DList::returnPressed()
      {
      int val = -1;
      if (selectedColumn != COL_NAME) 
      {
            ///val = atoi(editor->text().ascii());
            val = atoi(editor->text().toAscii().constData());
            if (selectedColumn != COL_LEN) 
            {
              if(selectedColumn == COL_VOL)
              {
                  if (val > 200) //Check bounds for volume
                  val = 200;
                  if (val < 0)
                  val = 0;
              }
              else
              {
                  if (val > 127) //Check bounds for lv1-lv4 values
                  val = 127;
                  if (val < 0)
                  val = 0;
              }    
            }  
      }     

      switch(selectedColumn) {
            case COL_NAME:
                  editEntry->name = editor->text();
                  break;

            case COL_LEN:
                  ///editEntry->len = atoi(editor->text().ascii());
                  editEntry->len = atoi(editor->text().toAscii().constData());
                  break;

            case COL_VOL:
                  editEntry->vol = val;
                  break;

            case COL_LV1:
                  editEntry->lv1 = val;
                  break;

            case COL_LV2:
                  editEntry->lv2 = val;
                  break;

            case COL_LV3:
                  editEntry->lv3 = val;
                  break;

            case COL_LV4:
                  editEntry->lv4 = val;
                  break;

            default:
                  printf("Return pressed in unknown column\n");
                  break;
            }
      selectedColumn = -1;
      editor->hide();
      editEntry = 0;
      setFocus();
      redraw();
      }

//---------------------------------------------------------
//   moved
//---------------------------------------------------------

void DList::moved(int, int, int)
      {
      redraw();
      }

//---------------------------------------------------------
//   tracklistChanged
//---------------------------------------------------------

void DList::tracklistChanged()
      {
      }

//---------------------------------------------------------
//   songChanged
//---------------------------------------------------------

void DList::songChanged(int flags)
      {
      if (flags & SC_DRUMMAP) {
            redraw();
            }
      }

//---------------------------------------------------------
//   DList
//---------------------------------------------------------

DList::DList(QHeaderView* h, QWidget* parent, int ymag)
   : View(parent, 1, ymag)
      {
      setBg(Qt::white);
      if (!h){
      h = new QHeaderView(Qt::Horizontal, parent);}
      header = h;
      scroll = 0;
      //ORCAN- CHECK if really needed: header->setTracking(true);
      connect(header, SIGNAL(sectionResized(int,int,int)),
         SLOT(sizeChange(int,int,int)));
      connect(header, SIGNAL(sectionMoved(int, int,int)), SLOT(moved(int,int,int)));
      setFocusPolicy(Qt::StrongFocus);
      drag = NORMAL;
      editor = 0;
      editEntry = 0;
      // always select a drum instrument
      currentlySelected = &drumMap[0];
      selectedColumn = -1;
      }

//---------------------------------------------------------
//   ~DList
//---------------------------------------------------------

DList::~DList()
      {
      }

//---------------------------------------------------------
//   viewMouseMoveEvent
//---------------------------------------------------------

void DList::viewMouseMoveEvent(QMouseEvent* ev)
      {
      curY = ev->y();
      int delta = curY - startY;
      switch (drag) {
            case START_DRAG:
                  if (delta < 0)
                        delta = -delta;
                  if (delta <= 2)
                        return;
                  drag = DRAG;
                  setCursor(QCursor(Qt::SizeVerCursor));
                  redraw();
                  break;
            case NORMAL:
                  break;
            case DRAG:
                  redraw();
                  break;
            }
      }

//---------------------------------------------------------
//   viewMouseReleaseEvent
//---------------------------------------------------------

void DList::viewMouseReleaseEvent(QMouseEvent* ev)
      {
      if (drag == DRAG) {
            int y = ev->y();
            unsigned dPitch = y / TH;
            setCursor(QCursor(Qt::ArrowCursor));
            currentlySelected = &drumMap[int(dPitch)];
            emit curDrumInstrumentChanged(dPitch);
            emit mapChanged(sPitch, dPitch); //Track pitch change done in canvas
            }
      drag = NORMAL;
//??      redraw();
      if (editEntry)
            editor->setFocus();
      int x = ev->x();
      int y = ev->y();
      bool shift = ev->modifiers() & Qt::ShiftModifier;
      unsigned pitch = y / TH;

      DCols col = DCols(x2col(x));

      switch (col) {
            case COL_NAME:
                  emit keyReleased(pitch, shift);
                  break;
            case COL_ANOTE:
                  emit keyReleased(pitch, shift);
                  break;
            default:
                  break;
            }
      }

//---------------------------------------------------------
//   getSelectedInstrument
//---------------------------------------------------------

int DList::getSelectedInstrument()
      {
      if (currentlySelected == 0)
            return -1;
      return drumInmap[int(currentlySelected->enote)];
      }


