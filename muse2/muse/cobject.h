//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: cobject.h,v 1.3.2.1 2005/12/11 21:29:24 spamatica Exp $
//
//  (C) Copyright 1999/2000 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __COBJECT_H__
#define __COBJECT_H__

#include "config.h"

#include <QMainWindow>
#include <list>

class QMdiSubWindow;
class QFocusEvent;
class QToolBar;
class Xml;

//---------------------------------------------------------
//   TopWin
//---------------------------------------------------------

class TopWin : public QMainWindow
      {
      Q_OBJECT

   public:
      virtual void readStatus(Xml&);
      virtual void writeStatus(int, Xml&) const;
      
      virtual QMdiSubWindow* createMdiWrapper();
      bool isMdiWin();
      void setFree();

      TopWin(QWidget* parent=0, const char* name=0,
         Qt::WindowFlags f = Qt::Window);
         
      bool sharesToolsAndMenu() { return _sharesToolsAndMenu; }
      void shareToolsAndMenu(bool);
      const std::list<QToolBar*>& toolbars() { return _toolbars; }
      
      void addToolBar(QToolBar* toolbar);
      QToolBar* addToolBar(const QString& title);
         
  private:
      QMdiSubWindow* mdisubwin;
      bool _sharesToolsAndMenu;
      std::list<QToolBar*> _toolbars;
      
      void insertToolBar(QToolBar*, QToolBar*);
      void insertToolBarBreak(QToolBar*);
      void removeToolBar(QToolBar*);
      void removeToolBarBreak(QToolBar*);
      void addToolBar(Qt::ToolBarArea, QToolBar*);

  public slots:
      virtual void hide();
      virtual void show();
      virtual void setVisible(bool);
  
  signals:
      void toolsAndMenuSharingChanged(bool);
      };

//---------------------------------------------------------
//   Toplevel
//---------------------------------------------------------

class Toplevel {
   public:
      enum ToplevelType { PIANO_ROLL, LISTE, DRUM, MASTER, WAVE, 
         LMASTER, CLIPLIST, MARKER, SCORE, ARRANGER
#ifdef PATCHBAY
         , M_PATCHBAY
#endif /* PATCHBAY */
         };
      Toplevel(ToplevelType t, TopWin* obj) {
            _type = t;
            _object = obj;
            }
      ToplevelType type() const { return _type; }
      TopWin* object()   const { return _object; }
      
   private:
      ToplevelType _type;
      TopWin* _object;
      };

typedef std::list <Toplevel> ToplevelList;
typedef ToplevelList::iterator iToplevel;
typedef ToplevelList::const_iterator ciToplevel;

#endif

