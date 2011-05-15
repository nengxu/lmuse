//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: waveedit.cpp,v 1.5.2.12 2009/04/06 01:24:54 terminator356 Exp $
//  (C) Copyright 2000 Werner Schweer (ws@seh.de)
//=========================================================

#include <QMenu>
#include <QSignalMapper>
#include <QToolBar>
#include <QToolButton>
#include <QLayout>
#include <QSizeGrip>
#include <QScrollBar>
#include <QLabel>
#include <QSlider>
#include <QMenuBar>
#include <QAction>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QSettings>

#include "app.h"
#include "xml.h"
#include "waveedit.h"
#include "mtscale.h"
#include "scrollscale.h"
#include "waveview.h"
#include "ttoolbar.h"
#include "globals.h"
#include "audio.h"
#include "utils.h"
#include "song.h"
#include "poslabel.h"
#include "gconfig.h"
#include "icons.h"
#include "shortcuts.h"


extern QColor readColor(Xml& xml);

int WaveEdit::_widthInit = 600;
int WaveEdit::_heightInit = 400;

//---------------------------------------------------------
//   closeEvent
//---------------------------------------------------------

void WaveEdit::closeEvent(QCloseEvent* e)
      {
      QSettings settings("MusE", "MusE-qt");
      //settings.setValue("Waveedit/geometry", saveGeometry());
      settings.setValue("Waveedit/windowState", saveState());
      emit deleted((unsigned long)this);
      e->accept();
      }

//---------------------------------------------------------
//   WaveEdit
//---------------------------------------------------------

WaveEdit::WaveEdit(PartList* pl)
   : MidiEditor(1, 1, pl)
      {
      resize(_widthInit, _heightInit);

      QSignalMapper* mapper = new QSignalMapper(this);
      QAction* act;
      
      //---------Pulldown Menu----------------------------
      // We probably don't need an empty menu - Orcan
      //QMenu* menuFile = menuBar()->addMenu(tr("&File"));
      QMenu* menuEdit = menuBar()->addMenu(tr("&Edit"));
      
      menuFunctions = menuBar()->addMenu(tr("Func&tions"));

      menuGain = menuFunctions->addMenu(tr("&Gain"));
      
      act = menuGain->addAction(tr("200%"));
      mapper->setMapping(act, CMD_GAIN_200);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuGain->addAction(tr("150%"));
      mapper->setMapping(act, CMD_GAIN_150);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuGain->addAction(tr("75%"));
      mapper->setMapping(act, CMD_GAIN_75);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuGain->addAction(tr("50%"));
      mapper->setMapping(act, CMD_GAIN_50);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuGain->addAction(tr("25%"));
      mapper->setMapping(act, CMD_GAIN_25);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuGain->addAction(tr("Other"));
      mapper->setMapping(act, CMD_GAIN_FREE);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      connect(mapper, SIGNAL(mapped(int)), this, SLOT(cmd(int)));
      
      menuFunctions->addSeparator();

      copyAction = menuEdit->addAction(tr("&Copy"));
      mapper->setMapping(copyAction, CMD_EDIT_COPY);
      connect(copyAction, SIGNAL(triggered()), mapper, SLOT(map()));

      cutAction = menuEdit->addAction(tr("C&ut"));
      mapper->setMapping(cutAction, CMD_EDIT_CUT);
      connect(cutAction, SIGNAL(triggered()), mapper, SLOT(map()));

      pasteAction = menuEdit->addAction(tr("&Paste"));
      mapper->setMapping(pasteAction, CMD_EDIT_PASTE);
      connect(pasteAction, SIGNAL(triggered()), mapper, SLOT(map()));


      act = menuEdit->addAction(tr("Edit in E&xternal Editor"));
      mapper->setMapping(act, CMD_EDIT_EXTERNAL);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuFunctions->addAction(tr("Mute Selection"));
      mapper->setMapping(act, CMD_MUTE);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuFunctions->addAction(tr("Normalize Selection"));
      mapper->setMapping(act, CMD_NORMALIZE);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuFunctions->addAction(tr("Fade In Selection"));
      mapper->setMapping(act, CMD_FADE_IN);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuFunctions->addAction(tr("Fade Out Selection"));
      mapper->setMapping(act, CMD_FADE_OUT);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      act = menuFunctions->addAction(tr("Reverse Selection"));
      mapper->setMapping(act, CMD_REVERSE);
      connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
      
      select = menuEdit->addMenu(QIcon(*selectIcon), tr("Select"));
      
      selectAllAction = select->addAction(QIcon(*select_allIcon), tr("Select &All"));
      mapper->setMapping(selectAllAction, CMD_SELECT_ALL);
      connect(selectAllAction, SIGNAL(triggered()), mapper, SLOT(map()));
      
      selectNoneAction = select->addAction(QIcon(*select_allIcon), tr("&Deselect All"));
      mapper->setMapping(selectNoneAction, CMD_SELECT_NONE);
      connect(selectNoneAction, SIGNAL(triggered()), mapper, SLOT(map()));
      
      //---------ToolBar----------------------------------
      tools = addToolBar(tr("Wave edit tools"));
      tools->setObjectName("Wave edit tools");

      tools->addActions(undoRedo->actions());

      connect(muse, SIGNAL(configChanged()), SLOT(configChanged()));

      //--------------------------------------------------
      //    Transport Bar
      QToolBar* transport = addToolBar(tr("transport"));
      transport->setObjectName("transport");
      transport->addActions(transportAction->actions());

      //--------------------------------------------------
      //    ToolBar:   Solo  Cursor1 Cursor2

      addToolBarBreak();
      tb1 = addToolBar(tr("Pianoroll tools"));
      tb1->setObjectName("Pianoroll tools");

      //tb1->setLabel(tr("weTools"));
      solo = new QToolButton();
      solo->setText(tr("Solo"));
      solo->setCheckable(true);
      tb1->addWidget(solo);
      connect(solo,  SIGNAL(toggled(bool)), SLOT(soloChanged(bool)));
      
      QLabel* label = new QLabel(tr("Cursor"));
      tb1->addWidget(label);
      label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
      label->setIndent(3);
      pos1 = new PosLabel(0);
      pos1->setFixedHeight(22);
      tb1->addWidget(pos1);
      pos2 = new PosLabel(0);
      pos2->setFixedHeight(22);
      pos2->setSmpte(true);
      tb1->addWidget(pos2);

      //---------------------------------------------------
      //    Rest
      //---------------------------------------------------

      int yscale = 256;
      int xscale;

      if (!parts()->empty()) { // Roughly match total size of part
            Part* firstPart = parts()->begin()->second;
            xscale = 0 - firstPart->lenFrame()/_widthInit;
            }
      else {
            xscale = -8000;
            }

      hscroll = new ScrollScale(1, -32768, xscale, 10000, Qt::Horizontal, mainw, 0, true, 10000.0);
      view    = new WaveView(this, mainw, xscale, yscale);
      wview   = view;   // HACK!

      QSizeGrip* corner    = new QSizeGrip(mainw);
      ymag                 = new QSlider(Qt::Vertical, mainw);
      ymag->setMinimum(1);
      ymag->setMaximum(256);
      ymag->setPageStep(256);
      ymag->setValue(yscale);
       
      time                 = new MTScale(&_raster, mainw, xscale, true);
      ymag->setFixedWidth(16);
      connect(view, SIGNAL(mouseWheelMoved(int)), this, SLOT(moveVerticalSlider(int)));
      connect(ymag, SIGNAL(valueChanged(int)), view, SLOT(setYScale(int)));
      connect(view, SIGNAL(horizontalZoomIn()), SLOT(horizontalZoomIn()));
      connect(view, SIGNAL(horizontalZoomOut()), SLOT(horizontalZoomOut()));

      time->setOrigin(0, 0);

      mainGrid->setRowStretch(0, 100);
      mainGrid->setColumnStretch(0, 100);

      mainGrid->addWidget(time,   0, 0, 1, 2);
      mainGrid->addWidget(hLine(mainw),    1, 0, 1, 2);
      mainGrid->addWidget(view,    2, 0);
      mainGrid->addWidget(ymag,    2, 1);
      mainGrid->addWidget(hscroll, 3, 0);
      mainGrid->addWidget(corner,  3, 1, Qt::AlignBottom | Qt::AlignRight);

      view->setFocus();  // Tim.
      
      connect(hscroll, SIGNAL(scrollChanged(int)), view, SLOT(setXPos(int)));
      connect(hscroll, SIGNAL(scaleChanged(int)),  view, SLOT(setXMag(int)));
      setWindowTitle(view->getCaption());
      connect(view, SIGNAL(followEvent(int)), hscroll, SLOT(setOffset(int)));

      connect(hscroll, SIGNAL(scrollChanged(int)), time,  SLOT(setXPos(int)));
      connect(hscroll, SIGNAL(scaleChanged(int)),  time,  SLOT(setXMag(int)));
//      connect(time,    SIGNAL(timeChanged(unsigned)),  SLOT(setTime(unsigned)));
      connect(view,    SIGNAL(timeChanged(unsigned)),  SLOT(setTime(unsigned)));

      connect(view,  SIGNAL(horizontalScroll(unsigned)),hscroll, SLOT(setPos(unsigned)));

      connect(hscroll, SIGNAL(scaleChanged(int)),  SLOT(updateHScrollRange()));
      connect(song, SIGNAL(songChanged(int)), SLOT(songChanged1(int)));


      initShortcuts();
      
      updateHScrollRange();
      configChanged();
      
      if(!parts()->empty())
      {
        WavePart* part = (WavePart*)(parts()->begin()->second);
        solo->setChecked(part->track()->solo());
      }
      QSettings settings("MusE", "MusE-qt");
      //restoreGeometry(settings.value("Waveedit/geometry").toByteArray());
      restoreState(settings.value("Waveedit/windowState").toByteArray());

      }

void WaveEdit::initShortcuts()
      {
      cutAction->setShortcut(shortcuts[SHRT_CUT].key);
      copyAction->setShortcut(shortcuts[SHRT_COPY].key);
      pasteAction->setShortcut(shortcuts[SHRT_PASTE].key);
      selectAllAction->setShortcut(shortcuts[SHRT_SELECT_ALL].key);
      selectNoneAction->setShortcut(shortcuts[SHRT_SELECT_NONE].key);
      }

//---------------------------------------------------------
//   configChanged
//---------------------------------------------------------

void WaveEdit::configChanged()
      {
      view->setBg(config.waveEditBackgroundColor);
      selectAllAction->setShortcut(shortcuts[SHRT_SELECT_ALL].key);
      selectNoneAction->setShortcut(shortcuts[SHRT_SELECT_NONE].key);
      }

//---------------------------------------------------------
//   updateHScrollRange
//---------------------------------------------------------
void WaveEdit::updateHScrollRange()
{
      int s, e;
      wview->range(&s, &e);
      // Show one more measure.
      e += AL::sigmap.ticksMeasure(e);  
      // Show another quarter measure due to imprecise drawing at canvas end point.
      e += AL::sigmap.ticksMeasure(e) / 4;
      // Compensate for the vscroll width. 
      //e += wview->rmapxDev(-vscroll->width()); 
      int s1, e1;
      hscroll->range(&s1, &e1);
      if(s != s1 || e != e1) 
        hscroll->setRange(s, e);
}

//---------------------------------------------------------
//   setTime
//---------------------------------------------------------

void WaveEdit::setTime(unsigned samplepos)
      {
//    printf("setTime %d %x\n", samplepos, samplepos);
      unsigned tick = tempomap.frame2tick(samplepos);
      pos1->setValue(tick);
      //pos2->setValue(tick);
      pos2->setValue(samplepos);
      time->setPos(3, tick, false);
      }

//---------------------------------------------------------
//   ~WaveEdit
//---------------------------------------------------------

WaveEdit::~WaveEdit()
      {
      // undoRedo->removeFrom(tools); // p4.0.6 Removed
      }

//---------------------------------------------------------
//   cmd
//---------------------------------------------------------

void WaveEdit::cmd(int n)
      {
      view->cmd(n);
      }

//---------------------------------------------------------
//   loadConfiguration
//---------------------------------------------------------

void WaveEdit::readConfiguration(Xml& xml)
      {
      for (;;) {
            Xml::Token token = xml.parse();
            const QString& tag = xml.s1();
            switch (token) {
                  case Xml::TagStart:
                        if (tag == "bgcolor")
                              config.waveEditBackgroundColor = readColor(xml);
                        else if (tag == "width")
                              _widthInit = xml.parseInt();
                        else if (tag == "height")
                              _heightInit = xml.parseInt();
                        else
                              xml.unknown("WaveEdit");
                        break;
                  case Xml::TagEnd:
                        if (tag == "waveedit")
                              return;
                  default:
                        break;
                  case Xml::Error:
                  case Xml::End:
                        return;
                  }
            }
      }

//---------------------------------------------------------
//   saveConfiguration
//---------------------------------------------------------

void WaveEdit::writeConfiguration(int level, Xml& xml)
      {
      xml.tag(level++, "waveedit");
      xml.colorTag(level, "bgcolor", config.waveEditBackgroundColor);
      xml.intTag(level, "width", _widthInit);
      xml.intTag(level, "height", _heightInit);
      xml.tag(level, "/waveedit");
      }

//---------------------------------------------------------
//   writeStatus
//---------------------------------------------------------

void WaveEdit::writeStatus(int level, Xml& xml) const
      {
      writePartList(level, xml);
      xml.tag(level++, "waveedit");
      MidiEditor::writeStatus(level, xml);
      xml.intTag(level, "xpos", hscroll->pos());
      xml.intTag(level, "xmag", hscroll->mag());
      xml.intTag(level, "ymag", ymag->value());
      xml.tag(level, "/waveedit");
      }

//---------------------------------------------------------
//   readStatus
//---------------------------------------------------------

void WaveEdit::readStatus(Xml& xml)
      {
      for (;;) {
            Xml::Token token = xml.parse();
            if (token == Xml::Error || token == Xml::End)
                  break;
            QString tag = xml.s1();
            switch (token) {
                  case Xml::TagStart:
                        if (tag == "midieditor")
                              MidiEditor::readStatus(xml);
                        else if (tag == "xmag")
                              hscroll->setMag(xml.parseInt());
                        else if (tag == "ymag")
                              ymag->setValue(xml.parseInt());
                        else if (tag == "xpos")
                              hscroll->setPos(xml.parseInt());
                        else
                              xml.unknown("WaveEdit");
                        break;
                  case Xml::TagEnd:
                        if (tag == "waveedit")
                              return;
                  default:
                        break;
                  }
            }
      }

//---------------------------------------------------------
//   resizeEvent
//---------------------------------------------------------

void WaveEdit::resizeEvent(QResizeEvent* ev)
      {
      QWidget::resizeEvent(ev);
      _widthInit = ev->size().width();
      _heightInit = ev->size().height();
      }

//---------------------------------------------------------
//   songChanged1
//    signal from "song"
//---------------------------------------------------------

void WaveEdit::songChanged1(int bits)
      {
        
        if (bits & SC_SOLO)
        {
          WavePart* part = (WavePart*)(parts()->begin()->second);
          solo->blockSignals(true);
          solo->setChecked(part->track()->solo());
          solo->blockSignals(false);
        }  
        
        songChanged(bits);
      }


//---------------------------------------------------------
//   soloChanged
//    signal from solo button
//---------------------------------------------------------

void WaveEdit::soloChanged(bool flag)
      {
      WavePart* part = (WavePart*)(parts()->begin()->second);
      audio->msgSetSolo(part->track(), flag);
      song->update(SC_SOLO);
      }

//---------------------------------------------------------
//   viewKeyPressEvent
//---------------------------------------------------------

void WaveEdit::keyPressEvent(QKeyEvent* event)
      {
      int key = event->key();
      if (key == Qt::Key_Escape) {
            close();
            return;
            }
      else {
            event->ignore();
            }
      }

//---------------------------------------------------------
//   moveVerticalSlider
//---------------------------------------------------------

void WaveEdit::moveVerticalSlider(int val)
      {
      ymag->setValue(ymag->value() + val);
      }


void WaveEdit::horizontalZoomIn()
{
  int mag = hscroll->mag();
  int zoomlvl = ScrollScale::getQuickZoomLevel(mag);
  if (zoomlvl < 23)
        zoomlvl++;

  int newmag = ScrollScale::convertQuickZoomLevelToMag(zoomlvl);

  hscroll->setMag(newmag);

}

void WaveEdit::horizontalZoomOut()
{
  int mag = hscroll->mag();
  int zoomlvl = ScrollScale::getQuickZoomLevel(mag);
  if (zoomlvl > 1)
        zoomlvl--;

  int newmag = ScrollScale::convertQuickZoomLevelToMag(zoomlvl);

  hscroll->setMag(newmag);

}
