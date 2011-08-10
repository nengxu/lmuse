//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: app.cpp,v 1.113.2.68 2009/12/21 14:51:51 spamatica Exp $
//
//  (C) Copyright 1999-2004 Werner Schweer (ws@seh.de)
//=========================================================

#include <QClipboard>
#include <QMessageBox>
#include <QShortcut>
#include <QSignalMapper>
#include <QTimer>
#include <QWhatsThis>
#include <QSettings>
#include <QProgressDialog>
#include <QMdiArea>
#include <QMdiSubWindow>

#include "app.h"
#include "master/lmaster.h"
#include "al/dsp.h"
#include "amixer.h"
#include "appearance.h"
#include "arranger.h"
#include "arrangerview.h"
#include "audio.h"
#include "audiodev.h"
#include "audioprefetch.h"
#include "bigtime.h"
#include "cliplist/cliplist.h"
#include "conf.h"
#include "debug.h"
#include "didyouknow.h"
#include "drumedit.h"
#include "filedialog.h"
#include "gconfig.h"
#include "gui.h"
#include "icons.h"
#include "instruments/editinstrument.h"
#include "listedit.h"
#include "marker/markerview.h"
#include "master/masteredit.h"
#include "metronome.h"
#include "midiseq.h"
#include "mixdowndialog.h"
#include "pianoroll.h"
#include "scoreedit.h"
#include "routepopup.h"
#include "shortcutconfig.h"
#include "songinfo.h"
#include "ticksynth.h"
#include "transport.h"
#include "waveedit.h"
#include "widgets/projectcreateimpl.h"
#include "widgets/menutitleitem.h"
#include "tools.h"
#include "widgets/unusedwavefiles.h"
#include "functions.h"


//extern void cacheJackRouteNames();

static pthread_t watchdogThread;
//ErrorHandler *error;
static const char* fileOpenText =
      QT_TRANSLATE_NOOP("@default", "Click this button to open a <em>new song</em>.<br>"
      "You can also select the <b>Open command</b> from the File menu.");
static const char* fileSaveText =
      QT_TRANSLATE_NOOP("@default", "Click this button to save the song you are "
      "editing.  You will be prompted for a file name.\n"
      "You can also select the Save command from the File menu.");
static const char* fileNewText        = QT_TRANSLATE_NOOP("@default", "Create New Song");

static const char* infoLoopButton     = QT_TRANSLATE_NOOP("@default", "loop between left mark and right mark");
static const char* infoPunchinButton  = QT_TRANSLATE_NOOP("@default", "record starts at left mark");
static const char* infoPunchoutButton = QT_TRANSLATE_NOOP("@default", "record stops at right mark");
static const char* infoStartButton    = QT_TRANSLATE_NOOP("@default", "rewind to start position");
static const char* infoRewindButton   = QT_TRANSLATE_NOOP("@default", "rewind current position");
static const char* infoForwardButton  = QT_TRANSLATE_NOOP("@default", "move current position");
static const char* infoStopButton     = QT_TRANSLATE_NOOP("@default", "stop sequencer");
static const char* infoPlayButton     = QT_TRANSLATE_NOOP("@default", "start sequencer play");
static const char* infoRecordButton   = QT_TRANSLATE_NOOP("@default", "to record press record and then play");
static const char* infoPanicButton    = QT_TRANSLATE_NOOP("@default", "send note off to all midi channels");

#define PROJECT_LIST_LEN  6
static QString* projectList[PROJECT_LIST_LEN];

extern void initMidiSynth();
extern void exitJackAudio();
extern void exitDummyAudio();
extern void exitOSC();

#ifdef HAVE_LASH
#include <lash/lash.h>
lash_client_t * lash_client = 0;
extern snd_seq_t * alsaSeq;
#endif /* HAVE_LASH */

int watchAudio, watchAudioPrefetch, watchMidi;
pthread_t splashThread;





//---------------------------------------------------------
//   sleep function
//---------------------------------------------------------
void microSleep(long msleep)
{
    bool sleepOk=-1;

    while(sleepOk==-1)
        sleepOk=usleep(msleep);
}

//---------------------------------------------------------
//   seqStart
//---------------------------------------------------------

bool MusE::seqStart()
      {
      if (audio->isRunning()) {
            printf("seqStart(): already running\n");
            return true;
            }
      
      if (!audio->start()) {
          QMessageBox::critical( muse, tr("Failed to start audio!"),
              tr("Was not able to start audio, check if jack is running.\n"));
          return false;
          }

      //
      // wait for jack callback
      //
      for(int i = 0; i < 60; ++i) 
      {
        if(audio->isRunning())
          break;
        sleep(1);
      }
      if(!audio->isRunning()) 
      {
        QMessageBox::critical( muse, tr("Failed to start audio!"),
            tr("Timeout waiting for audio to run. Check if jack is running.\n"));
      }
      //
      // now its safe to ask the driver for realtime
      // priority
      
      realTimePriority = audioDevice->realtimePriority();
      if(debugMsg)
        printf("MusE::seqStart: getting audio driver realTimePriority:%d\n", realTimePriority);
      
      int pfprio = 0;
      int midiprio = 0;
      
      // NOTE: realTimeScheduling can be true (gotten using jack_is_realtime()),
      //  while the determined realTimePriority can be 0.
      // realTimePriority is gotten using pthread_getschedparam() on the client thread 
      //  in JackAudioDevice::realtimePriority() which is a bit flawed - it reports there's no RT...
      if(realTimeScheduling) 
      {
        {
          //pfprio = realTimePriority - 5;
          // p3.3.40
          pfprio = realTimePriority + 1;
          
          //midiprio = realTimePriority - 2;
          // p3.3.37
          //midiprio = realTimePriority + 1;
          // p3.3.40
          midiprio = realTimePriority + 2;
        }  
      }
      
      if(midiRTPrioOverride > 0)
        midiprio = midiRTPrioOverride;
      
      // FIXME FIXME: The realTimePriority of the Jack thread seems to always be 5 less than the value passed to jackd command.
      //if(midiprio == realTimePriority)
      //  printf("MusE: WARNING: Midi realtime priority %d is the same as audio realtime priority %d. Try a different setting.\n", 
      //         midiprio, realTimePriority);
      //if(midiprio == pfprio)
      //  printf("MusE: WARNING: Midi realtime priority %d is the same as audio prefetch realtime priority %d. Try a different setting.\n", 
      //         midiprio, pfprio);
      
      audioPrefetch->start(pfprio);
      
      audioPrefetch->msgSeek(0, true); // force
      
      //midiSeqRunning = !midiSeq->start(realTimeScheduling ? realTimePriority : 0);
      // Changed by Tim. p3.3.22
      //midiSeq->start(realTimeScheduling ? realTimePriority : 0);
      midiSeq->start(midiprio);
      
      int counter=0;
      while (++counter) {
        //if (counter > 10) {
        if (counter > 1000) {
            fprintf(stderr,"midi sequencer thread does not start!? Exiting...\n");
            exit(33);
        }
        midiSeqRunning = midiSeq->isRunning();
        if (midiSeqRunning)
          break;
        usleep(1000);
        if(debugMsg)
          printf("looping waiting for sequencer thread to start\n");
      }
      if(!midiSeqRunning)
      {
        fprintf(stderr, "midiSeq is not running! Exiting...\n");
        exit(33);
      }  
      return true;
      }

//---------------------------------------------------------
//   stop
//---------------------------------------------------------

void MusE::seqStop()
      {
      // label sequencer as disabled before it actually happened to minimize race condition
      midiSeqRunning = false;

      song->setStop(true);
      song->setStopPlay(false);
      midiSeq->stop(true);
      audio->stop(true);
      audioPrefetch->stop(true);
      if (realTimeScheduling && watchdogThread)
            pthread_cancel(watchdogThread);
      }

//---------------------------------------------------------
//   seqRestart
//---------------------------------------------------------

bool MusE::seqRestart()
{
    bool restartSequencer = audio->isRunning();
    if (restartSequencer) {
          if (audio->isPlaying()) {
                audio->msgPlay(false);
                while (audio->isPlaying())
                      qApp->processEvents();
                }
          seqStop();
          }
    if(!seqStart())
        return false;

    audioDevice->graphChanged();
    return true;
}

//---------------------------------------------------------
//   addProject
//---------------------------------------------------------

void addProject(const QString& name)
      {
      for (int i = 0; i < PROJECT_LIST_LEN; ++i) {
            if (projectList[i] == 0)
                  break;
            if (name == *projectList[i]) {
                  int dst = i;
                  int src = i+1;
                  int n = PROJECT_LIST_LEN - i - 1;
                  delete projectList[i];
                  for (int k = 0; k < n; ++k)
                        projectList[dst++] = projectList[src++];
                  projectList[dst] = 0;
                  break;
                  }
            }
      QString** s = &projectList[PROJECT_LIST_LEN - 2];
      QString** d = &projectList[PROJECT_LIST_LEN - 1];
      if (*d)
            delete *d;
      for (int i = 0; i < PROJECT_LIST_LEN-1; ++i)
            *d-- = *s--;
      projectList[0] = new QString(name);
      }

//---------------------------------------------------------
//   MusE
//---------------------------------------------------------

//MusE::MusE(int argc, char** argv) : QMainWindow(0, "mainwindow")
MusE::MusE(int argc, char** argv) : QMainWindow()
      {
      // By T356. For LADSPA plugins in plugin.cpp
      // QWidgetFactory::addWidgetFactory( new PluginWidgetFactory ); ddskrjo
      
      setIconSize(ICON_SIZE);
      setFocusPolicy(Qt::WheelFocus);
      //setFocusPolicy(Qt::NoFocus);
      muse                  = this;    // hack
      clipListEdit          = 0;
      midiSyncConfig        = 0;
      midiRemoteConfig      = 0;
      midiPortConfig        = 0;
      metronomeConfig       = 0;
      audioConfig           = 0;
      midiFileConfig        = 0;
      midiFilterConfig      = 0;
      midiInputTransform    = 0;
      midiRhythmGenerator   = 0;
      globalSettingsConfig  = 0;
      markerView            = 0;
      arrangerView          = 0;
      softSynthesizerConfig = 0;
      midiTransformerDialog = 0;
      shortcutConfig        = 0;
      appearance            = 0;
      //audioMixer            = 0;
      mixer1                = 0;
      mixer2                = 0;
      watchdogThread        = 0;
      editInstrument        = 0;
      routingPopupMenu      = 0;
      progress              = 0;
      
      appName               = QString("MusE");
      setWindowTitle(appName);
      midiPluginSignalMapper = new QSignalMapper(this);
      followSignalMapper = new QSignalMapper(this);

      song           = new Song("song");
      song->blockSignals(true);
      heartBeatTimer = new QTimer(this);
      heartBeatTimer->setObjectName("timer");
      connect(heartBeatTimer, SIGNAL(timeout()), song, SLOT(beat()));

#ifdef ENABLE_PYTHON
      //---------------------------------------------------
      //    Python bridge
      //---------------------------------------------------
      // Uncomment in order to enable MusE Python bridge:
      if (usePythonBridge) {
            printf("Initializing python bridge!\n");
            if (initPythonBridge() == false) {
                  printf("Could not initialize Python bridge\n");
                  exit(1);
                  }
            }
#endif

      //---------------------------------------------------
      //    undo/redo
      //---------------------------------------------------
      
      undoRedo = new QActionGroup(this);
      undoRedo->setExclusive(false);
      undoAction = new QAction(QIcon(*undoIconS), tr("Und&o"), 
        undoRedo);
      redoAction = new QAction(QIcon(*redoIconS), tr("Re&do"), 
        undoRedo);

      undoAction->setWhatsThis(tr("undo last change to song"));
      redoAction->setWhatsThis(tr("redo last undo"));
      undoAction->setEnabled(false);
      redoAction->setEnabled(false);
      connect(redoAction, SIGNAL(activated()), song, SLOT(redo()));
      connect(undoAction, SIGNAL(activated()), song, SLOT(undo()));

      //---------------------------------------------------
      //    Transport
      //---------------------------------------------------
      
      transportAction = new QActionGroup(this);
      transportAction->setExclusive(false);
      
      loopAction = new QAction(QIcon(*loop1Icon),
         tr("Loop"), transportAction);
      loopAction->setCheckable(true);

      loopAction->setWhatsThis(tr(infoLoopButton));
      connect(loopAction, SIGNAL(toggled(bool)), song, SLOT(setLoop(bool)));
      
      punchinAction = new QAction(QIcon(*punchin1Icon),
         tr("Punchin"), transportAction);
      punchinAction->setCheckable(true);

      punchinAction->setWhatsThis(tr(infoPunchinButton));
      connect(punchinAction, SIGNAL(toggled(bool)), song, SLOT(setPunchin(bool)));

      punchoutAction = new QAction(QIcon(*punchout1Icon),
         tr("Punchout"), transportAction);
      punchoutAction->setCheckable(true);

      punchoutAction->setWhatsThis(tr(infoPunchoutButton));
      connect(punchoutAction, SIGNAL(toggled(bool)), song, SLOT(setPunchout(bool)));

      QAction *tseparator = new QAction(this);
      tseparator->setSeparator(true);
      transportAction->addAction(tseparator);

      startAction = new QAction(QIcon(*startIcon),
         tr("Start"), transportAction);

      startAction->setWhatsThis(tr(infoStartButton));
      connect(startAction, SIGNAL(activated()), song, SLOT(rewindStart()));

      rewindAction = new QAction(QIcon(*frewindIcon),
         tr("Rewind"), transportAction);

      rewindAction->setWhatsThis(tr(infoRewindButton));
      connect(rewindAction, SIGNAL(activated()), song, SLOT(rewind()));

      forwardAction = new QAction(QIcon(*fforwardIcon),
	 tr("Forward"), transportAction);

      forwardAction->setWhatsThis(tr(infoForwardButton));
      connect(forwardAction, SIGNAL(activated()), song, SLOT(forward()));

      stopAction = new QAction(QIcon(*stopIcon),
         tr("Stop"), transportAction);
      stopAction->setCheckable(true);

      stopAction->setWhatsThis(tr(infoStopButton));
      stopAction->setChecked(true);
      connect(stopAction, SIGNAL(toggled(bool)), song, SLOT(setStop(bool)));

      playAction = new QAction(QIcon(*playIcon),
         tr("Play"), transportAction);
      playAction->setCheckable(true);

      playAction->setWhatsThis(tr(infoPlayButton));
      playAction->setChecked(false);
      connect(playAction, SIGNAL(toggled(bool)), song, SLOT(setPlay(bool)));

      recordAction = new QAction(QIcon(*recordIcon),
         tr("Record"), transportAction);
      recordAction->setCheckable(true);
      recordAction->setWhatsThis(tr(infoRecordButton));
      connect(recordAction, SIGNAL(toggled(bool)), song, SLOT(setRecord(bool)));

      panicAction = new QAction(QIcon(*panicIcon),
         tr("Panic"), this);

      panicAction->setWhatsThis(tr(infoPanicButton));
      connect(panicAction, SIGNAL(activated()), song, SLOT(panic()));

      initMidiInstruments();
      initMidiPorts();
      ::initMidiDevices();

      //----Actions
      //-------- File Actions

      fileNewAction = new QAction(QIcon(*filenewIcon), tr("&New"), this); 
      fileNewAction->setToolTip(tr(fileNewText));
      fileNewAction->setWhatsThis(tr(fileNewText));

      fileOpenAction = new QAction(QIcon(*openIcon), tr("&Open"), this); 

      fileOpenAction->setToolTip(tr(fileOpenText));
      fileOpenAction->setWhatsThis(tr(fileOpenText));

      openRecent = new QMenu(tr("Open &Recent"), this);

      fileSaveAction = new QAction(QIcon(*saveIcon), tr("&Save"), this); 

      fileSaveAction->setToolTip(tr(fileSaveText));
      fileSaveAction->setWhatsThis(tr(fileSaveText));

      fileSaveAsAction = new QAction(tr("Save &As"), this);

      fileImportMidiAction = new QAction(tr("Import Midifile"), this);
      fileExportMidiAction = new QAction(tr("Export Midifile"), this);
      fileImportPartAction = new QAction(tr("Import Part"), this);

      fileImportWaveAction = new QAction(tr("Import Wave File"), this);
      fileMoveWaveFiles = new QAction(tr("Find unused wave files"), this);

      quitAction = new QAction(tr("&Quit"), this);

      editSongInfoAction = new QAction(QIcon(*edit_listIcon), tr("Song Info"), this);

      //-------- View Actions
      viewTransportAction = new QAction(QIcon(*view_transport_windowIcon), tr("Transport Panel"), this);
      viewTransportAction->setCheckable(true);
      viewBigtimeAction = new QAction(QIcon(*view_bigtime_windowIcon), tr("Bigtime Window"),  this);
      viewBigtimeAction->setCheckable(true);
      viewMixerAAction = new QAction(QIcon(*mixerSIcon), tr("Mixer A"), this);
      viewMixerAAction->setCheckable(true);
      viewMixerBAction = new QAction(QIcon(*mixerSIcon), tr("Mixer B"), this);
      viewMixerBAction->setCheckable(true);
      viewCliplistAction = new QAction(QIcon(*cliplistSIcon), tr("Cliplist"), this);
      viewCliplistAction->setCheckable(true);
      viewMarkerAction = new QAction(QIcon(*view_markerIcon), tr("Marker View"),  this);
      viewMarkerAction->setCheckable(true);
      viewArrangerAction = new QAction(tr("Arranger View"),  this);
      viewArrangerAction->setCheckable(true);

      //-------- Midi Actions
      menuScriptPlugins = new QMenu(tr("&Plugins"), this);
      midiEditInstAction = new QAction(QIcon(*midi_edit_instrumentIcon), tr("Edit Instrument"), this);
      midiInputPlugins = new QMenu(tr("Input Plugins"), this);
      midiInputPlugins->setIcon(QIcon(*midi_inputpluginsIcon));
      midiTrpAction = new QAction(QIcon(*midi_inputplugins_transposeIcon), tr("Transpose"), this);
      midiInputTrfAction = new QAction(QIcon(*midi_inputplugins_midi_input_transformIcon), tr("Midi Input Transform"), this);
      midiInputFilterAction = new QAction(QIcon(*midi_inputplugins_midi_input_filterIcon), tr("Midi Input Filter"), this);
      midiRemoteAction = new QAction(QIcon(*midi_inputplugins_remote_controlIcon), tr("Midi Remote Control"), this);
#ifdef BUILD_EXPERIMENTAL
      midiRhythmAction = new QAction(QIcon(*midi_inputplugins_random_rhythm_generatorIcon), tr("Rhythm Generator"), this);
#endif
      midiResetInstAction = new QAction(QIcon(*midi_reset_instrIcon), tr("Reset Instr."), this);
      midiInitInstActions = new QAction(QIcon(*midi_init_instrIcon), tr("Init Instr."), this);
      midiLocalOffAction = new QAction(QIcon(*midi_local_offIcon), tr("Local Off"), this);

      //-------- Audio Actions
      audioBounce2TrackAction = new QAction(QIcon(*audio_bounce_to_trackIcon), tr("Bounce to Track"), this);
      audioBounce2FileAction = new QAction(QIcon(*audio_bounce_to_fileIcon), tr("Bounce to File"), this);
      audioRestartAction = new QAction(QIcon(*audio_restartaudioIcon), tr("Restart Audio"), this);

      //-------- Automation Actions
      autoMixerAction = new QAction(QIcon(*automation_mixerIcon), tr("Mixer Automation"), this);
      autoMixerAction->setCheckable(true);
      autoSnapshotAction = new QAction(QIcon(*automation_take_snapshotIcon), tr("Take Snapshot"), this);
      autoClearAction = new QAction(QIcon(*automation_clear_dataIcon), tr("Clear Automation Data"), this);
      autoClearAction->setEnabled(false);


      //-------- Settings Actions
      settingsGlobalAction = new QAction(QIcon(*settings_globalsettingsIcon), tr("Global Settings"), this);
      settingsShortcutsAction = new QAction(QIcon(*settings_configureshortcutsIcon), tr("Configure Shortcuts"), this);
      follow = new QMenu(tr("Follow Song"), this);
      dontFollowAction = new QAction(tr("Don't Follow Song"), this);
      dontFollowAction->setCheckable(true);
      followPageAction = new QAction(tr("Follow Page"), this);
      followPageAction->setCheckable(true);
      followPageAction->setChecked(true);
      followCtsAction = new QAction(tr("Follow Continuous"), this);
      followCtsAction->setCheckable(true);

      settingsMetronomeAction = new QAction(QIcon(*settings_metronomeIcon), tr("Metronome"), this);
      settingsMidiSyncAction = new QAction(QIcon(*settings_midisyncIcon), tr("Midi Sync"), this);
      settingsMidiIOAction = new QAction(QIcon(*settings_midifileexportIcon), tr("Midi File Import/Export"), this);
      settingsAppearanceAction = new QAction(QIcon(*settings_appearance_settingsIcon), tr("Appearance Settings"), this);
      settingsMidiPortAction = new QAction(QIcon(*settings_midiport_softsynthsIcon), tr("Midi Ports / Soft Synth"), this);

      //-------- Help Actions
      helpManualAction = new QAction(tr("&Manual"), this);
      helpHomepageAction = new QAction(tr("&MusE Homepage"), this);
      helpReportAction = new QAction(tr("&Report Bug..."), this);
      helpAboutAction = new QAction(tr("&About MusE"), this);


      //---- Connections
      //-------- File connections

      connect(fileNewAction,  SIGNAL(activated()), SLOT(loadTemplate()));
      connect(fileOpenAction, SIGNAL(activated()), SLOT(loadProject()));
      connect(openRecent, SIGNAL(aboutToShow()), SLOT(openRecentMenu()));
      connect(openRecent, SIGNAL(triggered(QAction*)), SLOT(selectProject(QAction*)));
      
      connect(fileSaveAction, SIGNAL(activated()), SLOT(save()));
      connect(fileSaveAsAction, SIGNAL(activated()), SLOT(saveAs()));

      connect(fileImportMidiAction, SIGNAL(activated()), SLOT(importMidi()));
      connect(fileExportMidiAction, SIGNAL(activated()), SLOT(exportMidi()));
      connect(fileImportPartAction, SIGNAL(activated()), SLOT(importPart()));

      connect(fileImportWaveAction, SIGNAL(activated()), SLOT(importWave()));
      connect(fileMoveWaveFiles, SIGNAL(activated()), SLOT(findUnusedWaveFiles()));
      connect(quitAction, SIGNAL(activated()), SLOT(quitDoc()));

      connect(editSongInfoAction, SIGNAL(activated()), SLOT(startSongInfo()));

      //-------- View connections
      connect(viewTransportAction, SIGNAL(toggled(bool)), SLOT(toggleTransport(bool)));
      connect(viewBigtimeAction, SIGNAL(toggled(bool)), SLOT(toggleBigTime(bool)));
      connect(viewMixerAAction, SIGNAL(toggled(bool)),SLOT(toggleMixer1(bool)));
      connect(viewMixerBAction, SIGNAL(toggled(bool)), SLOT(toggleMixer2(bool)));
      connect(viewCliplistAction, SIGNAL(toggled(bool)), SLOT(startClipList(bool)));
      connect(viewMarkerAction, SIGNAL(toggled(bool)), SLOT(toggleMarker(bool)));
      connect(viewArrangerAction, SIGNAL(toggled(bool)), SLOT(toggleArranger(bool)));

      //-------- Midi connections
      connect(midiEditInstAction, SIGNAL(activated()), SLOT(startEditInstrument()));
      connect(midiResetInstAction, SIGNAL(activated()), SLOT(resetMidiDevices()));
      connect(midiInitInstActions, SIGNAL(activated()), SLOT(initMidiDevices()));
      connect(midiLocalOffAction, SIGNAL(activated()), SLOT(localOff()));

      connect(midiTrpAction, SIGNAL(triggered()), midiPluginSignalMapper, SLOT(map()));
      connect(midiInputTrfAction, SIGNAL(triggered()), midiPluginSignalMapper, SLOT(map()));
      connect(midiInputFilterAction, SIGNAL(triggered()), midiPluginSignalMapper, SLOT(map()));
      connect(midiRemoteAction, SIGNAL(triggered()), midiPluginSignalMapper, SLOT(map()));

      midiPluginSignalMapper->setMapping(midiTrpAction, 0);
      midiPluginSignalMapper->setMapping(midiInputTrfAction, 1);
      midiPluginSignalMapper->setMapping(midiInputFilterAction, 2);
      midiPluginSignalMapper->setMapping(midiRemoteAction, 3);

#ifdef BUILD_EXPERIMENTAL
      connect(midiRhythmAction, SIGNAL(triggered()), midiPluginSignalMapper, SLOT(map()));
      midiPluginSignalMapper->setMapping(midiRhythmAction, 4);
#endif

      connect(midiPluginSignalMapper, SIGNAL(mapped(int)), this, SLOT(startMidiInputPlugin(int)));
      
      //-------- Audio connections
      connect(audioBounce2TrackAction, SIGNAL(activated()), SLOT(bounceToTrack()));
      connect(audioBounce2FileAction, SIGNAL(activated()), SLOT(bounceToFile()));
      connect(audioRestartAction, SIGNAL(activated()), SLOT(seqRestart()));

      //-------- Automation connections
      connect(autoMixerAction, SIGNAL(activated()), SLOT(switchMixerAutomation()));
      connect(autoSnapshotAction, SIGNAL(activated()), SLOT(takeAutomationSnapshot()));
      connect(autoClearAction, SIGNAL(activated()), SLOT(clearAutomation()));

      //-------- Settings connections
      connect(settingsGlobalAction, SIGNAL(activated()), SLOT(configGlobalSettings()));
      connect(settingsShortcutsAction, SIGNAL(activated()), SLOT(configShortCuts()));
      connect(settingsMetronomeAction, SIGNAL(activated()), SLOT(configMetronome()));
      connect(settingsMidiSyncAction, SIGNAL(activated()), SLOT(configMidiSync()));
      connect(settingsMidiIOAction, SIGNAL(activated()), SLOT(configMidiFile()));
      connect(settingsAppearanceAction, SIGNAL(activated()), SLOT(configAppearance()));
      connect(settingsMidiPortAction, SIGNAL(activated()), SLOT(configMidiPorts()));

      connect(dontFollowAction, SIGNAL(triggered()), followSignalMapper, SLOT(map()));
      connect(followPageAction, SIGNAL(triggered()), followSignalMapper, SLOT(map()));
      connect(followCtsAction, SIGNAL(triggered()), followSignalMapper, SLOT(map()));

      followSignalMapper->setMapping(dontFollowAction, CMD_FOLLOW_NO);
      followSignalMapper->setMapping(followPageAction, CMD_FOLLOW_JUMP);
      followSignalMapper->setMapping(followCtsAction, CMD_FOLLOW_CONTINUOUS);

      connect(followSignalMapper, SIGNAL(mapped(int)), this, SLOT(cmd(int)));

      //-------- Help connections
      connect(helpManualAction, SIGNAL(activated()), SLOT(startHelpBrowser()));
      connect(helpHomepageAction, SIGNAL(activated()), SLOT(startHomepageBrowser()));
      connect(helpReportAction, SIGNAL(activated()), SLOT(startBugBrowser()));
      connect(helpAboutAction, SIGNAL(activated()), SLOT(about()));

      //--------------------------------------------------
      //    Toolbar
      //--------------------------------------------------
      
      tools = addToolBar(tr("File Buttons"));
      tools->setObjectName("File Buttons");
      tools->addAction(fileNewAction);
      tools->addAction(fileOpenAction);
      tools->addAction(fileSaveAction);

      
      //
      //    Whats This
      //
      tools->addAction(QWhatsThis::createAction(this));
      
      tools->addSeparator();
      tools->addActions(undoRedo->actions());

      QToolBar* transportToolbar = addToolBar(tr("Transport"));
      transportToolbar->setObjectName("Transport");
      transportToolbar->addActions(transportAction->actions());

      QToolBar* panicToolbar = addToolBar(tr("Panic"));
      panicToolbar->setObjectName("Panic");
      panicToolbar->addAction(panicAction);


      
      //rlimit lim;
      //getrlimit(RLIMIT_RTPRIO, &lim);
      //printf("RLIMIT_RTPRIO soft:%d hard:%d\n", lim.rlim_cur, lim.rlim_max);    // Reported 80, 80 even with non-RT kernel.
      
      if (realTimePriority < sched_get_priority_min(SCHED_FIFO))
            realTimePriority = sched_get_priority_min(SCHED_FIFO);
      else if (realTimePriority > sched_get_priority_max(SCHED_FIFO))
            realTimePriority = sched_get_priority_max(SCHED_FIFO);

      // If we requested to force the midi thread priority...
      if(midiRTPrioOverride > 0)
      {
        if (midiRTPrioOverride < sched_get_priority_min(SCHED_FIFO))
            midiRTPrioOverride = sched_get_priority_min(SCHED_FIFO);
        else if (midiRTPrioOverride > sched_get_priority_max(SCHED_FIFO))
            midiRTPrioOverride = sched_get_priority_max(SCHED_FIFO);
      }
            
      // Changed by Tim. p3.3.17
      //midiSeq       = new MidiSeq(realTimeScheduling ? realTimePriority : 0, "Midi");
      midiSeq       = new MidiSeq("Midi");
      audio         = new Audio();
      //audioPrefetch = new AudioPrefetch(0, "Disc");
      audioPrefetch = new AudioPrefetch("Prefetch");

      //---------------------------------------------------
      //    Popups
      //---------------------------------------------------


      //-------------------------------------------------------------
      //    popup File
      //-------------------------------------------------------------

      menu_file = menuBar()->addMenu(tr("&File"));
      menu_file->addAction(fileNewAction);
      menu_file->addAction(fileOpenAction);
      menu_file->addMenu(openRecent);
      menu_file->addSeparator();
      menu_file->addAction(fileSaveAction);
      menu_file->addAction(fileSaveAsAction);
      menu_file->addSeparator();
      menu_file->addAction(editSongInfoAction);
      menu_file->addSeparator();
      menu_file->addAction(fileImportMidiAction);
      menu_file->addAction(fileExportMidiAction);
      menu_file->addAction(fileImportPartAction);
      menu_file->addSeparator();
      menu_file->addAction(fileImportWaveAction);
      menu_file->addSeparator();
      menu_file->addAction(fileMoveWaveFiles);
      menu_file->addSeparator();
      menu_file->addAction(quitAction);
      menu_file->addSeparator();



      //-------------------------------------------------------------
      //    popup View
      //-------------------------------------------------------------

      menuView = menuBar()->addMenu(tr("&View"));

      menuView->addAction(viewTransportAction);
      menuView->addAction(viewBigtimeAction);
      menuView->addAction(viewMixerAAction);
      menuView->addAction(viewMixerBAction);
      menuView->addAction(viewCliplistAction);
      menuView->addAction(viewMarkerAction);
      menuView->addAction(viewArrangerAction);

      
      //-------------------------------------------------------------
      //    popup Midi
      //-------------------------------------------------------------

      menu_functions = menuBar()->addMenu(tr("&Midi"));
      song->populateScriptMenu(menuScriptPlugins, this);
      menu_functions->addMenu(menuScriptPlugins);
      menu_functions->addAction(midiEditInstAction);
      menu_functions->addMenu(midiInputPlugins);
      midiInputPlugins->addAction(midiTrpAction);
      midiInputPlugins->addAction(midiInputTrfAction);
      midiInputPlugins->addAction(midiInputFilterAction);
      midiInputPlugins->addAction(midiRemoteAction);
#ifdef BUILD_EXPERIMENTAL
      midiInputPlugins->addAction(midiRhythmAction);
#endif

      menu_functions->addSeparator();
      menu_functions->addAction(midiResetInstAction);
      menu_functions->addAction(midiInitInstActions);
      menu_functions->addAction(midiLocalOffAction);
      /*
      **      mpid4 = midiInputPlugins->insertItem(
      **         QIconSet(*midi_inputplugins_random_rhythm_generatorIcon), tr("Random Rhythm Generator"), 4);
      */

      //-------------------------------------------------------------
      //    popup Audio
      //-------------------------------------------------------------

      menu_audio = menuBar()->addMenu(tr("&Audio"));
      menu_audio->addAction(audioBounce2TrackAction);
      menu_audio->addAction(audioBounce2FileAction);
      menu_audio->addSeparator();
      menu_audio->addAction(audioRestartAction);


      //-------------------------------------------------------------
      //    popup Automation
      //-------------------------------------------------------------

      menuAutomation = menuBar()->addMenu(tr("A&utomation"));
      menuAutomation->addAction(autoMixerAction);
      menuAutomation->addSeparator();
      menuAutomation->addAction(autoSnapshotAction);
      menuAutomation->addAction(autoClearAction);

      //-------------------------------------------------------------
      //    popup Settings
      //-------------------------------------------------------------

      menuSettings = menuBar()->addMenu(tr("Se&ttings"));
      menuSettings->addAction(settingsGlobalAction);
      menuSettings->addAction(settingsShortcutsAction);
      menuSettings->addMenu(follow);
      follow->addAction(dontFollowAction);
      follow->addAction(followPageAction);
      follow->addAction(followCtsAction);
      menuSettings->addAction(settingsMetronomeAction);
      menuSettings->addSeparator();
      menuSettings->addAction(settingsMidiSyncAction);
      menuSettings->addAction(settingsMidiIOAction);
      menuSettings->addSeparator();
      menuSettings->addAction(settingsAppearanceAction);
      menuSettings->addSeparator();
      menuSettings->addAction(settingsMidiPortAction);

      //---------------------------------------------------
      //    popup Help
      //---------------------------------------------------

      menu_help = menuBar()->addMenu(tr("&Help"));
      menu_help->addAction(helpManualAction);
      menu_help->addAction(helpHomepageAction);
      menu_help->addSeparator();
      menu_help->addAction(helpReportAction);
      menu_help->addSeparator();
      menu_help->addAction(helpAboutAction);

      //menu_help->insertItem(tr("About&Qt"), this, SLOT(aboutQt()));
      //menu_help->addSeparator();
      //menu_ids[CMD_START_WHATSTHIS] = menu_help->insertItem(tr("What's &This?"), this, SLOT(whatsThis()), 0);


      //---------------------------------------------------
      //    Central Widget
      //---------------------------------------------------

      
      mdiArea=new QMdiArea(this);
      setCentralWidget(mdiArea);


      arrangerView = new ArrangerView(this);
      connect(arrangerView, SIGNAL(closed()), SLOT(arrangerClosed()));
      toplevels.push_back(Toplevel(Toplevel::ARRANGER, (unsigned long)(arrangerView), arrangerView));
      arrangerView->hide();
      arranger=arrangerView->getArranger();
      
      //QMdiSubWindow* subwin=new QMdiSubWindow(this); //FINDMICHJETZT
      //subwin->setWidget(arrangerView);
      //mdiArea->addSubWindow(subwin);
      mdiArea->addSubWindow(arrangerView->createMdiWrapper());
      
      //---------------------------------------------------
      //  read list of "Recent Projects"
      //---------------------------------------------------

      QString prjPath(configPath);
      prjPath += QString("/projects");
      FILE* f = fopen(prjPath.toLatin1().constData(), "r");
      if (f == 0) {
            perror("open projectfile");
            for (int i = 0; i < PROJECT_LIST_LEN; ++i)
                  projectList[i] = 0;
            }
      else {
            for (int i = 0; i < PROJECT_LIST_LEN; ++i) {
                  char buffer[256];
                  if (fgets(buffer, 256, f)) {
                        int n = strlen(buffer);
                        if (n && buffer[n-1] == '\n')
                              buffer[n-1] = 0;
                        projectList[i] = *buffer ? new QString(buffer) : 0;
                        }
                  else
                        break;
                  }
            fclose(f);
            }

      initMidiSynth();
      
      arrangerView->populateAddTrack();
      arrangerView->updateShortcuts();
      
      transport = new Transport(this, "transport");
      bigtime   = 0;

      //---------------------------------------------------
      //  load project
      //    if no songname entered on command line:
      //    startMode: 0  - load last song
      //               1  - load default template
      //               2  - load configured start song
      //---------------------------------------------------

      QString name;
      bool useTemplate = false;
      if (argc >= 2)
            name = argv[0];
      else if (config.startMode == 0) {
            if (argc < 2)
                  name = projectList[0] ? *projectList[0] : QString("untitled");
            else
                  name = argv[0];
            printf("starting with selected song %s\n", config.startSong.toLatin1().constData());
            }
      else if (config.startMode == 1) {
            printf("starting with default template\n");
            name = museGlobalShare + QString("/templates/default.med");
            useTemplate = true;
            }
      else if (config.startMode == 2) {
            printf("starting with pre configured song %s\n", config.startSong.toLatin1().constData());
            name = config.startSong;
      }
      song->blockSignals(false);
      loadProjectFile(name, useTemplate, true);

      changeConfig(false);
      QSettings settings("MusE", "MusE-qt");
      //restoreGeometry(settings.value("MusE/geometry").toByteArray());
      restoreState(settings.value("MusE/windowState").toByteArray());

      song->update();
      }

MusE::~MusE()
{
}

//---------------------------------------------------------
//   setHeartBeat
//---------------------------------------------------------

void MusE::setHeartBeat()
      {
      heartBeatTimer->start(1000/config.guiRefresh);
      }

//---------------------------------------------------------
//   resetDevices
//---------------------------------------------------------

void MusE::resetMidiDevices()
      {
      audio->msgResetMidiDevices();
      }

//---------------------------------------------------------
//   initMidiDevices
//---------------------------------------------------------

void MusE::initMidiDevices()
      {
      // Added by T356
      //audio->msgIdle(true);
      
      audio->msgInitMidiDevices();
      
      //audio->msgIdle(false);
      }

//---------------------------------------------------------
//   localOff
//---------------------------------------------------------

void MusE::localOff()
      {
      audio->msgLocalOff();
      }

//---------------------------------------------------------
//   loadProjectFile
//    load *.med, *.mid, *.kar
//
//    template - if true, load file but do not change
//                project name
//---------------------------------------------------------

// for drop:
void MusE::loadProjectFile(const QString& name)
      {
      loadProjectFile(name, false, false);
      }

void MusE::loadProjectFile(const QString& name, bool songTemplate, bool loadAll)
      {
      QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

      if(!progress)
        progress = new QProgressDialog();
      QString label = "loading project "+QFileInfo(name).fileName();
        if (!songTemplate) {
          switch (random()%10) {
        case 0:
            label.append("\nThe best song in the world?");
          break;
        case 1:
            label.append("\nAwesome stuff!");
          break;
        case 2:
            label.append("\nCool rhythms!");
          break;
        case 3:
            label.append("\nA truly lovely song");
          break;
        default:
          break;
        }
      }
      progress->setLabelText(label);
      progress->setWindowModality(Qt::WindowModal);
      progress->setCancelButton(0);
      if (!songTemplate)
        progress->setMinimumDuration(0); // if we are loading a template it will probably be fast and we can wait before showing the dialog
      //progress->show();
      //
      // stop audio threads if running
      //
      progress->setValue(0);
      bool restartSequencer = audio->isRunning();
      if (restartSequencer) {
            if (audio->isPlaying()) {
                  audio->msgPlay(false);
                  while (audio->isPlaying())
                        qApp->processEvents();
                  }
            seqStop();
            }
      microSleep(100000);
      progress->setValue(10);
      loadProjectFile1(name, songTemplate, loadAll);
      microSleep(100000);
      progress->setValue(90);
      if (restartSequencer)
            seqStart();

      arrangerView->updateVisibleTracksButtons();
      progress->setValue(100);
      delete progress;
      progress=0;

      QApplication::restoreOverrideCursor();

      if (song->getSongInfo().length()>0 && song->showSongInfoOnStartup()) {
          startSongInfo(false);
        }
      }

//---------------------------------------------------------
//   loadProjectFile
//    load *.med, *.mid, *.kar
//
//    template - if true, load file but do not change
//                project name
//    loadAll  - load song data + configuration data
//---------------------------------------------------------

void MusE::loadProjectFile1(const QString& name, bool songTemplate, bool loadAll)
      {
      //if (audioMixer)
      //      audioMixer->clear();
      if (mixer1)
            mixer1->clear();
      if (mixer2)
            mixer2->clear();
      arranger->clear();      // clear track info
      //if (clearSong())
      if (clearSong(loadAll))  // Allow not touching things like midi ports. p4.0.17 TESTING: Maybe some problems...
            return;
      progress->setValue(20);

      QFileInfo fi(name);
      if (songTemplate) {
            if (!fi.isReadable()) {
                  QMessageBox::critical(this, QString("MusE"),
                     tr("Cannot read template"));
                  QApplication::restoreOverrideCursor();
                  return;
                  }
            project.setFile("untitled");
            museProject = museProjectInitPath;
            }
      else {
            printf("Setting project path to %s\n", fi.absolutePath().toLatin1().constData());
            museProject = fi.absolutePath();
            project.setFile(name);
            }
      // Changed by T356. 01/19/2010. We want the complete extension here. 
      //QString ex = fi.extension(false).toLower();
      //if (ex.length() == 3)
      //      ex += ".";
      //ex = ex.left(4);
      QString ex = fi.completeSuffix().toLower();
      QString mex = ex.section('.', -1, -1);  
      if((mex == "gz") || (mex == "bz2"))
        mex = ex.section('.', -2, -2);  
        
      if (ex.isEmpty() || mex == "med") {
            //
            //  read *.med file
            //
            bool popenFlag;
            FILE* f = fileOpen(this, fi.filePath(), QString(".med"), "r", popenFlag, true);
            if (f == 0) {
                  if (errno != ENOENT) {
                        QMessageBox::critical(this, QString("MusE"),
                           tr("File open error"));
                        setUntitledProject();
                        }
                  else
                        setConfigDefaults();
                  }
            else {
                  Xml xml(f);
                  read(xml, !loadAll, songTemplate);
                  bool fileError = ferror(f);
                  popenFlag ? pclose(f) : fclose(f);
                  if (fileError) {
                        QMessageBox::critical(this, QString("MusE"),
                           tr("File read error"));
                        setUntitledProject();
                        }
                  }
            }
      //else if (ex == "mid." || ex == "kar.") {
      else if (mex == "mid" || mex == "kar") {
            setConfigDefaults();
            if (!importMidi(name, false))
                  setUntitledProject();
            }
      else {
            QMessageBox::critical(this, QString("MusE"),
               tr("Unknown File Format: ") + ex);
            setUntitledProject();
            }
      if (!songTemplate) {
            addProject(project.absoluteFilePath());
            setWindowTitle(QString("MusE: Song: ") + project.completeBaseName());
            }
      song->dirty = false;
      progress->setValue(30);

      viewTransportAction->setChecked(config.transportVisible);
      viewBigtimeAction->setChecked(config.bigTimeVisible);
      viewMarkerAction->setChecked(config.markerVisible);
      viewArrangerAction->setChecked(config.arrangerVisible);

      autoMixerAction->setChecked(automation);

      if (loadAll) {
            showBigtime(config.bigTimeVisible);
            //showMixer(config.mixerVisible);
            showMixer1(config.mixer1Visible);
            showMixer2(config.mixer2Visible);
            
            // Added p3.3.43 Make sure the geometry is correct because showMixerX() will NOT 
            //  set the geometry if the mixer has already been created.
            if(mixer1)
            {
              //if(mixer1->geometry().size() != config.mixer1.geometry.size())   // p3.3.53 Moved below
              //  mixer1->resize(config.mixer1.geometry.size());
              
              if(mixer1->geometry().topLeft() != config.mixer1.geometry.topLeft())
                mixer1->move(config.mixer1.geometry.topLeft());
            }
            if(mixer2)
            {
              //if(mixer2->geometry().size() != config.mixer2.geometry.size())   // p3.3.53 Moved below
              //  mixer2->resize(config.mixer2.geometry.size());
              
              if(mixer2->geometry().topLeft() != config.mixer2.geometry.topLeft())
                mixer2->move(config.mixer2.geometry.topLeft());
            }
            
            //showMarker(config.markerVisible);  // Moved below. Tim.
            resize(config.geometryMain.size());
            move(config.geometryMain.topLeft());

            if (config.transportVisible)
                  transport->show();
            transport->move(config.geometryTransport.topLeft());
            showTransport(config.transportVisible);
            }
      progress->setValue(40);

      transport->setMasterFlag(song->masterFlag());
      punchinAction->setChecked(song->punchin());
      punchoutAction->setChecked(song->punchout());
      loopAction->setChecked(song->loop());
      song->update();
      song->updatePos();
      arrangerView->clipboardChanged(); // enable/disable "Paste"
      arrangerView->selectionChanged(); // enable/disable "Copy" & "Paste"
      arrangerView->scoreNamingChanged(); // inform the score menus about the new scores and their names
      progress->setValue(50);

      // p3.3.53 Try this AFTER the song update above which does a mixer update... Tested OK - mixers resize properly now.
      if (loadAll) 
      {
        if(mixer1)
        {
          if(mixer1->geometry().size() != config.mixer1.geometry.size())
          {
            //printf("MusE::loadProjectFile1 resizing mixer1 x:%d y:%d w:%d h:%d\n", config.mixer1.geometry.x(), 
            //                                                                       config.mixer1.geometry.y(), 
            //                                                                       config.mixer1.geometry.width(), 
            //                                                                       config.mixer1.geometry.height()
            //                                                                       );  
            mixer1->resize(config.mixer1.geometry.size());
          }
        }  
        if(mixer2)
        {
          if(mixer2->geometry().size() != config.mixer2.geometry.size())
          {
            //printf("MusE::loadProjectFile1 resizing mixer2 x:%d y:%d w:%d h:%d\n", config.mixer2.geometry.x(), 
            //                                                                       config.mixer2.geometry.y(), 
            //                                                                       config.mixer2.geometry.width(), 
            //                                                                       config.mixer2.geometry.height()
            //                                                                       );  
            mixer2->resize(config.mixer2.geometry.size());
          }
        }  
        
        // Moved here from above due to crash with a song loaded and then File->New.
        // Marker view list was not updated, had non-existent items from marker list (cleared in ::clear()).
        showMarker(config.markerVisible); 
      }
      
      }

//---------------------------------------------------------
//   setUntitledProject
//---------------------------------------------------------

void MusE::setUntitledProject()
      {
      setConfigDefaults();
      QString name("untitled");
      museProject = "./"; //QFileInfo(name).absolutePath();
      project.setFile(name);
      setWindowTitle(tr("MusE: Song: ") + project.completeBaseName());
      }

//---------------------------------------------------------
//   setConfigDefaults
//---------------------------------------------------------

void MusE::setConfigDefaults()
      {
      readConfiguration();    // used for reading midi files
      song->dirty = false;
      }

//---------------------------------------------------------
//   setFollow
//---------------------------------------------------------

void MusE::setFollow()
      {
      Song::FollowMode fm = song->follow();
      
      dontFollowAction->setChecked(fm == Song::NO);
      followPageAction->setChecked(fm == Song::JUMP);
      followCtsAction->setChecked(fm == Song::CONTINUOUS);
      }

//---------------------------------------------------------
//   MusE::loadProject
//---------------------------------------------------------

void MusE::loadProject()
      {
      bool loadAll;
      QString fn = getOpenFileName(QString(""), med_file_pattern, this,
         tr("MusE: load project"), &loadAll);
      if (!fn.isEmpty()) {
            museProject = QFileInfo(fn).absolutePath();
            loadProjectFile(fn, false, loadAll);
            }
      }

//---------------------------------------------------------
//   loadTemplate
//---------------------------------------------------------

void MusE::loadTemplate()
      {
      QString fn = getOpenFileName(QString("templates"), med_file_pattern, this,
                                   tr("MusE: load template"), 0, MFileDialog::GLOBAL_VIEW);
      if (!fn.isEmpty()) {
            // museProject = QFileInfo(fn).absolutePath();
            
            loadProjectFile(fn, true, true);
            // With templates, don't clear midi ports. 
            // Any named ports in the template file are useless since they likely 
            //  would not be found on other users' machines.
            // So keep whatever the user currently has set up for ports.  
            // Note that this will also keep the current window configurations etc.
            //  but actually that's also probably a good thing. p4.0.17 Tim.  TESTING: Maybe some problems...
            //loadProjectFile(fn, true, false);
            
            setUntitledProject();
            }
      }

//---------------------------------------------------------
//   save
//---------------------------------------------------------

bool MusE::save()
      {
      if (project.completeBaseName() == "untitled")
            return saveAs();
      else
            return save(project.filePath(), false);
      }

//---------------------------------------------------------
//   save
//---------------------------------------------------------

bool MusE::save(const QString& name, bool overwriteWarn)
      {
      QString backupCommand;

      // By T356. Cache the jack in/out route names BEFORE saving. 
      // Because jack often shuts down during save, causing the routes to be lost in the file.
      // Not required any more...
      //cacheJackRouteNames();
      
      if (QFile::exists(name)) {
            backupCommand.sprintf("cp \"%s\" \"%s.backup\"", name.toLatin1().constData(), name.toLatin1().constData());
            }
      else if (QFile::exists(name + QString(".med"))) {
            backupCommand.sprintf("cp \"%s.med\" \"%s.med.backup\"", name.toLatin1().constData(), name.toLatin1().constData());
            }
      if (!backupCommand.isEmpty())
            system(backupCommand.toLatin1().constData());

      bool popenFlag;
      FILE* f = fileOpen(this, name, QString(".med"), "w", popenFlag, false, overwriteWarn);
      if (f == 0)
            return false;
      Xml xml(f);
      write(xml);
      if (ferror(f)) {
            QString s = "Write File\n" + name + "\nfailed: "
               //+ strerror(errno);
               + QString(strerror(errno));                 
            QMessageBox::critical(this,
               tr("MusE: Write File failed"), s);
            popenFlag? pclose(f) : fclose(f);
            unlink(name.toLatin1().constData());
            return false;
            }
      else {
            popenFlag? pclose(f) : fclose(f);
            song->dirty = false;
            return true;
            }
      }

//---------------------------------------------------------
//   quitDoc
//---------------------------------------------------------

void MusE::quitDoc()
      {
      close();
      }

//---------------------------------------------------------
//   closeEvent
//---------------------------------------------------------

void MusE::closeEvent(QCloseEvent* event)
      {
      QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
      song->setStop(true);
      //
      // wait for sequencer
      //
      while (audio->isPlaying()) {
            qApp->processEvents();
            }
      if (song->dirty) {
            int n = 0;
            n = QMessageBox::warning(this, appName,
               tr("The current Project contains unsaved data\n"
               "Save Current Project?"),
               tr("&Save"), tr("&Skip"), tr("&Cancel"), 0, 2);
            if (n == 0) {
                  if (!save())      // dont quit if save failed
                  {
                        event->ignore();
                        QApplication::restoreOverrideCursor();
                        return;
                  }      
                  }
            else if (n == 2)
            {
                  event->ignore();
                  QApplication::restoreOverrideCursor();
                  return;
            }      
            }
      seqStop();

      WaveTrackList* wt = song->waves();
      for (iWaveTrack iwt = wt->begin(); iwt != wt->end(); ++iwt) {
            WaveTrack* t = *iwt;
            if (t->recFile() && t->recFile()->samples() == 0) {
                  t->recFile()->remove();
                  }
            }

      QSettings settings("MusE", "MusE-qt");
      //settings.setValue("MusE/geometry", saveGeometry());
      settings.setValue("MusE/windowState", saveState());

      // save "Open Recent" list
      QString prjPath(configPath);
      prjPath += "/projects";
      FILE* f = fopen(prjPath.toLatin1().constData(), "w");
      if (f) {
            for (int i = 0; i < PROJECT_LIST_LEN; ++i) {
                  fprintf(f, "%s\n", projectList[i] ? projectList[i]->toLatin1().constData() : "");
                  }
            fclose(f);
            }
      if(debugMsg)
        printf("MusE: Exiting JackAudio\n");
      exitJackAudio();
      if(debugMsg)
        printf("MusE: Exiting DummyAudio\n");
      exitDummyAudio();
      if(debugMsg)
        printf("MusE: Exiting Metronome\n");
      exitMetronome();
      
      // Make sure to delete the menu. ~routingPopupMenu() will NOT be called automatically.
      // Even though it is a child of MusE, it just passes MusE onto the underlying PopupMenus. p4.0.26
      if(routingPopupMenu)
        delete routingPopupMenu;     
      #if 0
      if(routingPopupView)
      {
        routingPopupView->clear();
        delete routingPopupView;
      }  
      #endif
      
      // Changed by Tim. p3.3.14
      //SynthIList* sl = song->syntis();
      //for (iSynthI i = sl->begin(); i != sl->end(); ++i)
      //      delete *i;
      song->cleanupForQuit();

      if(debugMsg)
        printf("Muse: Cleaning up temporary wavefiles + peakfiles\n");
      // Cleanup temporary wavefiles + peakfiles used for undo
      for (std::list<QString>::iterator i = temporaryWavFiles.begin(); i != temporaryWavFiles.end(); i++) {
            QString filename = *i;
            QFileInfo f(filename);
            QDir d = f.dir();
            d.remove(filename);
            d.remove(f.completeBaseName() + ".wca");
            }
      
#ifdef HAVE_LASH
      // Disconnect gracefully from LASH. Tim. p3.3.14
      if(lash_client)
      {
        if(debugMsg)
          printf("MusE: Disconnecting from LASH\n");
        lash_event_t* lashev = lash_event_new_with_type (LASH_Quit);
        lash_send_event(lash_client, lashev);
      }
#endif      
      
      if(debugMsg)
        printf("MusE: Exiting Dsp\n");
      AL::exitDsp();
      
      if(debugMsg)
        printf("MusE: Exiting OSC\n");
      exitOSC();
      
      // p3.3.47
      delete audioPrefetch;
      delete audio;
      delete midiSeq;
      delete song;
      
      qApp->quit();
      }

//---------------------------------------------------------
//   toggleMarker
//---------------------------------------------------------

void MusE::toggleMarker(bool checked)
      {
      showMarker(checked);
      }

//---------------------------------------------------------
//   showMarker
//---------------------------------------------------------

void MusE::showMarker(bool flag)
      {
      //printf("showMarker %d\n",flag);
      if (markerView == 0) {
            markerView = new MarkerView(this);

            connect(markerView, SIGNAL(closed()), SLOT(markerClosed()));
            toplevels.push_back(Toplevel(Toplevel::MARKER, (unsigned long)(markerView), markerView));
            markerView->show();
            }
      markerView->setVisible(flag);
      viewMarkerAction->setChecked(flag);
      }

//---------------------------------------------------------
//   markerClosed
//---------------------------------------------------------

void MusE::markerClosed()
      {
      viewMarkerAction->setChecked(false);
      }

//---------------------------------------------------------
//   toggleArranger
//---------------------------------------------------------

void MusE::toggleArranger(bool checked)
      {
      showArranger(checked);
      }

//---------------------------------------------------------
//   showArranger
//---------------------------------------------------------

void MusE::showArranger(bool flag)
      {
      arrangerView->setVisible(flag);
      viewArrangerAction->setChecked(flag);
      }

//---------------------------------------------------------
//   arrangerClosed
//---------------------------------------------------------

void MusE::arrangerClosed()
      {
      viewArrangerAction->setChecked(false);
      }

//---------------------------------------------------------
//   toggleTransport
//---------------------------------------------------------

void MusE::toggleTransport(bool checked)
      {
      showTransport(checked);
      }

//---------------------------------------------------------
//   showTransport
//---------------------------------------------------------

void MusE::showTransport(bool flag)
      {
      transport->setVisible(flag);
      viewTransportAction->setChecked(flag);
      }

//---------------------------------------------------------
//   getRoutingPopupMenu
//   Get the special common routing popup menu. Used (so far) 
//    by audio strip, midi strip, and midi trackinfo.
//---------------------------------------------------------

RoutePopupMenu* MusE::getRoutingPopupMenu()
{
  if(!routingPopupMenu)
    routingPopupMenu = new RoutePopupMenu(this);
  return routingPopupMenu;
}

//---------------------------------------------------------
//   saveAs
//---------------------------------------------------------

bool MusE::saveAs()
      {
      QString name;
      if (museProject == museProjectInitPath ) {
        if (config.useProjectSaveDialog) {
            ProjectCreateImpl pci(muse);
            if (pci.exec() == QDialog::Rejected) {
              return false;
            }

            song->setSongInfo(pci.getSongInfo(), true);
            name = pci.getProjectPath();
          } else {
            name = getSaveFileName(QString(""), med_file_save_pattern, this, tr("MusE: Save As"));
            if (name.isEmpty())
              return false;
          }

        museProject = QFileInfo(name).absolutePath();
        QDir dirmanipulator;
        if (!dirmanipulator.mkpath(museProject)) {
          QMessageBox::warning(this,"Path error","Can't create project path", QMessageBox::Ok);
          return false;
        }
      }
      else {
        name = getSaveFileName(QString(""), med_file_save_pattern, this, tr("MusE: Save As"));
      }
      bool ok = false;
      if (!name.isEmpty()) {
            QString tempOldProj = museProject;
            museProject = QFileInfo(name).absolutePath();
            ok = save(name, true);
            if (ok) {
                  project.setFile(name);
                  setWindowTitle(tr("MusE: Song: ") + project.completeBaseName());
                  addProject(name);
                  }
            else
                  museProject = tempOldProj;
            }

      return ok;
      }

//---------------------------------------------------------
//   startEditor
//---------------------------------------------------------

void MusE::startEditor(PartList* pl, int type)
      {
      switch (type) {
            case 0: startPianoroll(pl, true); break;
            case 1: startListEditor(pl); break;
            case 3: startDrumEditor(pl, true); break;
            case 4: startWaveEditor(pl); break;
            //case 5: startScoreEdit(pl, true); break;
            }
      }

//---------------------------------------------------------
//   startEditor
//---------------------------------------------------------

void MusE::startEditor(Track* t)
      {
      switch (t->type()) {
            case Track::MIDI: startPianoroll(); break;  
            case Track::DRUM: startDrumEditor(); break;
            case Track::WAVE: startWaveEditor(); break;
            default:
                  break;
            }
      }

//---------------------------------------------------------
//   getMidiPartsToEdit
//---------------------------------------------------------

PartList* MusE::getMidiPartsToEdit()
      {
      PartList* pl = song->getSelectedMidiParts();
      if (pl->empty()) {
            QMessageBox::critical(this, QString("MusE"), tr("Nothing to edit"));
            return 0;
            }
      return pl;
      }


//---------------------------------------------------------
//   startScoreEdit
//---------------------------------------------------------

void MusE::openInScoreEdit_oneStaffPerTrack(QWidget* dest)
{
	openInScoreEdit((ScoreEdit*)dest, false);
}

void MusE::openInScoreEdit_allInOne(QWidget* dest)
{
	openInScoreEdit((ScoreEdit*)dest, true);
}

void MusE::openInScoreEdit(ScoreEdit* destination, bool allInOne)
{
	PartList* pl = getMidiPartsToEdit();
	if (pl == 0)
				return;
	openInScoreEdit(destination, pl, allInOne);
}

void MusE::openInScoreEdit(ScoreEdit* destination, PartList* pl, bool allInOne)
{
	if (destination==NULL) // if no destination given, create a new one
	{
      destination = new ScoreEdit(this, 0, arranger->cursorValue());
      destination->show();
      toplevels.push_back(Toplevel(Toplevel::SCORE, (unsigned long)(destination), destination));
      connect(destination, SIGNAL(deleted(unsigned long)), SLOT(toplevelDeleted(unsigned long)));
      connect(destination, SIGNAL(name_changed()), arrangerView, SLOT(scoreNamingChanged()));
      //connect(muse, SIGNAL(configChanged()), destination, SLOT(config_changed()));
      //commented out by flo, because the ScoreEditor connects to all 
      //relevant signals on his own
      
      arrangerView->updateScoreMenus();
  }
  
  destination->add_parts(pl, allInOne);
}

void MusE::startScoreQuickly()
{
	openInScoreEdit_oneStaffPerTrack(NULL);
}

//---------------------------------------------------------
//   startPianoroll
//---------------------------------------------------------

void MusE::startPianoroll()
      {
      PartList* pl = getMidiPartsToEdit();
      if (pl == 0)
            return;
      startPianoroll(pl, true);
      }

void MusE::startPianoroll(PartList* pl, bool showDefaultCtrls)
      {
      
      PianoRoll* pianoroll = new PianoRoll(pl, this, 0, arranger->cursorValue());
      if(showDefaultCtrls)       // p4.0.12
        pianoroll->addCtrl();
      pianoroll->show();
      toplevels.push_back(Toplevel(Toplevel::PIANO_ROLL, (unsigned long)(pianoroll), pianoroll));
      connect(pianoroll, SIGNAL(deleted(unsigned long)), SLOT(toplevelDeleted(unsigned long)));
      connect(muse, SIGNAL(configChanged()), pianoroll, SLOT(configChanged()));
      }

//---------------------------------------------------------
//   startListenEditor
//---------------------------------------------------------

void MusE::startListEditor()
      {
      PartList* pl = getMidiPartsToEdit();
      if (pl == 0)
            return;
      startListEditor(pl);
      }

void MusE::startListEditor(PartList* pl)
      {
      ListEdit* listEditor = new ListEdit(pl);
      listEditor->show();
      toplevels.push_back(Toplevel(Toplevel::LISTE, (unsigned long)(listEditor), listEditor));
      connect(listEditor, SIGNAL(deleted(unsigned long)), SLOT(toplevelDeleted(unsigned long)));
      connect(muse,SIGNAL(configChanged()), listEditor, SLOT(configChanged()));
      }

//---------------------------------------------------------
//   startMasterEditor
//---------------------------------------------------------

void MusE::startMasterEditor()
      {
      MasterEdit* masterEditor = new MasterEdit();
      masterEditor->show();
      toplevels.push_back(Toplevel(Toplevel::MASTER, (unsigned long)(masterEditor), masterEditor));
      connect(masterEditor, SIGNAL(deleted(unsigned long)), SLOT(toplevelDeleted(unsigned long)));
      }

//---------------------------------------------------------
//   startLMasterEditor
//---------------------------------------------------------

void MusE::startLMasterEditor()
      {
      LMaster* lmaster = new LMaster();
      lmaster->show();
      toplevels.push_back(Toplevel(Toplevel::LMASTER, (unsigned long)(lmaster), lmaster));
      connect(lmaster, SIGNAL(deleted(unsigned long)), SLOT(toplevelDeleted(unsigned long)));
      connect(muse, SIGNAL(configChanged()), lmaster, SLOT(configChanged()));
      }

//---------------------------------------------------------
//   startDrumEditor
//---------------------------------------------------------

void MusE::startDrumEditor()
      {
      PartList* pl = getMidiPartsToEdit();
      if (pl == 0)
            return;
      startDrumEditor(pl, true);
      }

void MusE::startDrumEditor(PartList* pl, bool showDefaultCtrls)
      {
      
      DrumEdit* drumEditor = new DrumEdit(pl, this, 0, arranger->cursorValue());
      if(showDefaultCtrls)       // p4.0.12
        drumEditor->addCtrl();
      drumEditor->show();
      toplevels.push_back(Toplevel(Toplevel::DRUM, (unsigned long)(drumEditor), drumEditor));
      connect(drumEditor, SIGNAL(deleted(unsigned long)), SLOT(toplevelDeleted(unsigned long)));
      connect(muse, SIGNAL(configChanged()), drumEditor, SLOT(configChanged()));
      }

//---------------------------------------------------------
//   startWaveEditor
//---------------------------------------------------------

void MusE::startWaveEditor()
      {
      PartList* pl = song->getSelectedWaveParts();
      if (pl->empty()) {
            QMessageBox::critical(this, QString("MusE"), tr("Nothing to edit"));
            return;
            }
      startWaveEditor(pl);
      }

void MusE::startWaveEditor(PartList* pl)
      {
      WaveEdit* waveEditor = new WaveEdit(pl);
      waveEditor->show();
      connect(muse, SIGNAL(configChanged()), waveEditor, SLOT(configChanged()));
      toplevels.push_back(Toplevel(Toplevel::WAVE, (unsigned long)(waveEditor), waveEditor));
      connect(waveEditor, SIGNAL(deleted(unsigned long)), SLOT(toplevelDeleted(unsigned long)));
      }


//---------------------------------------------------------
//   startSongInfo
//---------------------------------------------------------
void MusE::startSongInfo(bool editable)
      {
        SongInfoWidget info;
        info.viewCheckBox->setChecked(song->showSongInfoOnStartup());
        info.viewCheckBox->setEnabled(editable);
        info.songInfoText->setPlainText(song->getSongInfo());
        info.songInfoText->setReadOnly(!editable);
        info.setModal(true);
        info.show();
        if( info.exec() == QDialog::Accepted) {
          if (editable) {
            song->setSongInfo(info.songInfoText->toPlainText(), info.viewCheckBox->isChecked());
          }
        }

      }

//---------------------------------------------------------
//   showDidYouKnowDialog
//---------------------------------------------------------
void MusE::showDidYouKnowDialog()
      {
      if ((bool)config.showDidYouKnow == true) {
            DidYouKnowWidget dyk;
            dyk.tipText->setText("To get started with MusE why don't you try some demo songs available at http://demos.muse-sequencer.org/");
            dyk.show();
            if( dyk.exec()) {
                  if (dyk.dontShowCheckBox->isChecked()) {
                        //printf("disables dialog!\n");
                        config.showDidYouKnow=false;
                        muse->changeConfig(true);    // save settings
                        }
                  }
            }
      }
//---------------------------------------------------------
//   startDefineController
//---------------------------------------------------------


//---------------------------------------------------------
//   startClipList
//---------------------------------------------------------

void MusE::startClipList(bool checked)
      {
      if (clipListEdit == 0) {
            //clipListEdit = new ClipListEdit();
            clipListEdit = new ClipListEdit(this);
            toplevels.push_back(Toplevel(Toplevel::CLIPLIST, (unsigned long)(clipListEdit), clipListEdit));
            connect(clipListEdit, SIGNAL(deleted(unsigned long)), SLOT(toplevelDeleted(unsigned long)));
            }
      clipListEdit->show();
      viewCliplistAction->setChecked(checked);
      }

//---------------------------------------------------------
//   fileMenu
//---------------------------------------------------------

void MusE::openRecentMenu()
      {
      openRecent->clear();
      for (int i = 0; i < PROJECT_LIST_LEN; ++i) {
            if (projectList[i] == 0)
                  break;
            QByteArray ba = projectList[i]->toLatin1();
            const char* path = ba.constData();
            const char* p = strrchr(path, '/');
            if (p == 0)
                  p = path;
            else
                  ++p;
	    QAction *act = openRecent->addAction(QString(p));
	    act->setData(i);
            }
      }

//---------------------------------------------------------
//   selectProject
//---------------------------------------------------------

void MusE::selectProject(QAction* act)
      {
      if (!act)
            return;
      int id = act->data().toInt();
      assert(id < PROJECT_LIST_LEN);
      QString* name = projectList[id];
      if (name == 0)
            return;
      loadProjectFile(*name, false, true);
      }

//---------------------------------------------------------
//   toplevelDeleted
//---------------------------------------------------------

void MusE::toplevelDeleted(unsigned long tl)
      {
      for (iToplevel i = toplevels.begin(); i != toplevels.end(); ++i) {
            if (i->object() == tl) {
                  bool mustUpdateScoreMenus=false;
                  switch(i->type()) {
                        case Toplevel::MARKER:
                        case Toplevel::ARRANGER:
                              break;
                        case Toplevel::CLIPLIST:
                              // ORCAN: This needs to be verified. aid2 used to correspond to Cliplist:
                              //menu_audio->setItemChecked(aid2, false);
                              viewCliplistAction->setChecked(false);  
                              return;
                              //break;
                        // the followin editors can exist in more than
                        // one instantiation:
                        case Toplevel::PIANO_ROLL:
                        case Toplevel::LISTE:
                        case Toplevel::DRUM:
                        case Toplevel::MASTER:
                        case Toplevel::WAVE:
                        case Toplevel::LMASTER:
                              break;
                        case Toplevel::SCORE:
                              mustUpdateScoreMenus=true;
                        }
                  toplevels.erase(i);
                  if (mustUpdateScoreMenus)
                        arrangerView->updateScoreMenus();
                  return;
                  }
            }
      printf("topLevelDeleted: top level %lx not found\n", tl);
      //assert(false);
      }



//---------------------------------------------------------
//   kbAccel
//---------------------------------------------------------

void MusE::kbAccel(int key)
      {
      if (key == shortcuts[SHRT_TOGGLE_METRO].key) {
            song->setClick(!song->click());
            }
      else if (key == shortcuts[SHRT_PLAY_TOGGLE].key) {
            if (audio->isPlaying())
                  //song->setStopPlay(false);
                  song->setStop(true);
            else if (!config.useOldStyleStopShortCut)
                  song->setPlay(true);
            else if (song->cpos() != song->lpos())
                  song->setPos(0, song->lPos());
            else {
                  Pos p(0, true);
                  song->setPos(0, p);
                  }
            }
      else if (key == shortcuts[SHRT_STOP].key) {
            //song->setPlay(false);
            song->setStop(true);
            }
      else if (key == shortcuts[SHRT_GOTO_START].key) {
            Pos p(0, true);
            song->setPos(0, p);
            }
      else if (key == shortcuts[SHRT_PLAY_SONG].key ) {
            song->setPlay(true);
            }
      
      // p4.0.10 Tim. Normally each editor window handles these, to inc by the editor's raster snap value.
      // But users were asking for a global version - "they don't work when I'm in mixer or transport".
      // Since no editor claimed the key event, we don't know a specific editor's snap setting,
      //  so adopt a policy where the arranger is the 'main' raster reference, I guess...
      else if (key == shortcuts[SHRT_POS_DEC].key) {
            int spos = song->cpos();
            if(spos > 0) 
            {
              spos -= 1;     // Nudge by -1, then snap down with raster1.
              spos = AL::sigmap.raster1(spos, song->arrangerRaster());
            }  
            if(spos < 0)
              spos = 0;
            Pos p(spos,true);
            song->setPos(0, p, true, true, true);
            return;
            }
      else if (key == shortcuts[SHRT_POS_INC].key) {
            int spos = AL::sigmap.raster2(song->cpos() + 1, song->arrangerRaster());    // Nudge by +1, then snap up with raster2.
            Pos p(spos,true);
            song->setPos(0, p, true, true, true); //CDW
            return;
            }
      else if (key == shortcuts[SHRT_POS_DEC_NOSNAP].key) {
            int spos = song->cpos() - AL::sigmap.rasterStep(song->cpos(), song->arrangerRaster());
            if(spos < 0)
              spos = 0;
            Pos p(spos,true);
            song->setPos(0, p, true, true, true);
            return;
            }
      else if (key == shortcuts[SHRT_POS_INC_NOSNAP].key) {
            Pos p(song->cpos() + AL::sigmap.rasterStep(song->cpos(), song->arrangerRaster()), true);
            song->setPos(0, p, true, true, true);
            return;
            }
            
      else if (key == shortcuts[SHRT_GOTO_LEFT].key) {
            if (!song->record())
                  song->setPos(0, song->lPos());
            }
      else if (key == shortcuts[SHRT_GOTO_RIGHT].key) {
            if (!song->record())
                  song->setPos(0, song->rPos());
            }
      else if (key == shortcuts[SHRT_TOGGLE_LOOP].key) {
            song->setLoop(!song->loop());
            }
      else if (key == shortcuts[SHRT_START_REC].key) {
            if (!audio->isPlaying()) {
                  song->setRecord(!song->record());
                  }
            }
      else if (key == shortcuts[SHRT_REC_CLEAR].key) {
            if (!audio->isPlaying()) {
                  song->clearTrackRec();
                  }
            }
      else if (key == shortcuts[SHRT_OPEN_TRANSPORT].key) {
            toggleTransport(!viewTransportAction->isChecked());
            }
      else if (key == shortcuts[SHRT_OPEN_BIGTIME].key) {
            toggleBigTime(!viewBigtimeAction->isChecked());
            }
      //else if (key == shortcuts[SHRT_OPEN_MIXER].key) {
      //      toggleMixer();
      //      }
      else if (key == shortcuts[SHRT_OPEN_MIXER].key) {
            toggleMixer1(!viewMixerAAction->isChecked());
            }
      else if (key == shortcuts[SHRT_OPEN_MIXER2].key) {
            toggleMixer2(!viewMixerBAction->isChecked());
            }
      else if (key == shortcuts[SHRT_NEXT_MARKER].key) {
            if (markerView)
              markerView->nextMarker();
            }
      else if (key == shortcuts[SHRT_PREV_MARKER].key) {
            if (markerView)
              markerView->prevMarker();
            }
      else {
            if (debugMsg)
                  printf("unknown kbAccel 0x%x\n", key);
            }
      }

//---------------------------------------------------------
//   catchSignal
//    only for debugging
//---------------------------------------------------------

#if 0
static void catchSignal(int sig)
      {
      if (debugMsg)
            fprintf(stderr, "MusE: signal %d catched\n", sig);
      if (sig == SIGSEGV) {
            fprintf(stderr, "MusE: segmentation fault\n");
            abort();
            }
      if (sig == SIGCHLD) {
            M_DEBUG("caught SIGCHLD - child died\n");
            int status;
            int n = waitpid (-1, &status, WNOHANG);
            if (n > 0) {
                  fprintf(stderr, "SIGCHLD for unknown process %d received\n", n);
                  }
            }
      }
#endif

//---------------------------------------------------------
//   cmd
//    some cmd's from pulldown menu
//---------------------------------------------------------

void MusE::cmd(int cmd)
      {
      switch(cmd) {
            case CMD_FOLLOW_NO:
                  song->setFollow(Song::NO);
                  setFollow();
                  break;
            case CMD_FOLLOW_JUMP:
                  song->setFollow(Song::JUMP);
                  setFollow();
                  break;
            case CMD_FOLLOW_CONTINUOUS:
                  song->setFollow(Song::CONTINUOUS);
                  setFollow();
                  break;
            }
      }





//---------------------------------------------------------
//   configAppearance
//---------------------------------------------------------

void MusE::configAppearance()
      {
      if (!appearance)
            appearance = new Appearance(arranger);
      appearance->resetValues();
      if(appearance->isVisible()) {
          appearance->raise();
          appearance->activateWindow();
          }
      else
          appearance->show();
      }

//---------------------------------------------------------
//   loadTheme
//---------------------------------------------------------

void MusE::loadTheme(const QString& s)
      {
      if (style()->objectName() != s)
            QApplication::setStyle(s);
      }

//---------------------------------------------------------
//   loadStyleSheetFile
//---------------------------------------------------------

void MusE::loadStyleSheetFile(const QString& s)
{
    if(s.isEmpty())
    {
      qApp->setStyleSheet(s);
      return;
    }
      
    QFile cf(s);
    if (cf.open(QIODevice::ReadOnly)) {
          QByteArray ss = cf.readAll();
          QString sheet(QString::fromUtf8(ss.data()));
          qApp->setStyleSheet(sheet);
          cf.close();
          }
    else
          printf("loading style sheet <%s> failed\n", qPrintable(s));
}

//---------------------------------------------------------
//   configChanged
//    - called whenever configuration has changed
//    - when configuration has changed by user, call with
//      writeFlag=true to save configuration in ~/.MusE
//---------------------------------------------------------

void MusE::changeConfig(bool writeFlag)
      {
      if (writeFlag)
            writeGlobalConfiguration();
      
      //loadStyleSheetFile(config.styleSheetFile);
      loadTheme(config.style);
      QApplication::setFont(config.fonts[0]);
      loadStyleSheetFile(config.styleSheetFile);
      
      emit configChanged();
      updateConfiguration();
      }

//---------------------------------------------------------
//   configMetronome
//---------------------------------------------------------

void MusE::configMetronome()
      {
      if (!metronomeConfig)
          metronomeConfig = new MetronomeConfig;

      if(metronomeConfig->isVisible()) {
          metronomeConfig->raise();
          metronomeConfig->activateWindow();
          }
      else
          metronomeConfig->show();
      }


//---------------------------------------------------------
//   configShortCuts
//---------------------------------------------------------

void MusE::configShortCuts()
      {
      if (!shortcutConfig)
            shortcutConfig = new ShortcutConfig(this);
      shortcutConfig->_config_changed = false;
      if (shortcutConfig->exec())
            changeConfig(true);
      }


#if 0
//---------------------------------------------------------
//   openAudioFileManagement
//---------------------------------------------------------
void MusE::openAudioFileManagement()
      {
      if (!audioFileManager) {
            audioFileManager = new AudioFileManager(this, "audiofilemanager", false);
            audioFileManager->show();
            }
      audioFileManager->setVisible(true);
      }
#endif
//---------------------------------------------------------
//   bounceToTrack
//---------------------------------------------------------

void MusE::bounceToTrack()
      {
      if(audio->bounce())
        return;
      
      song->bounceOutput = 0;
      
      if(song->waves()->empty())
      {
        QMessageBox::critical(this,
            tr("MusE: Bounce to Track"),
            tr("No wave tracks found")
            );
        return;
      }
      
      OutputList* ol = song->outputs();
      if(ol->empty())
      {
        QMessageBox::critical(this,
            tr("MusE: Bounce to Track"),
            tr("No audio output tracks found")
            );
        return;
      }
      
      if(checkRegionNotNull())
        return;
      
      AudioOutput* out = 0;
      // If only one output, pick it, else pick the first selected.
      if(ol->size() == 1)
        out = ol->front();
      else
      {
        for(iAudioOutput iao = ol->begin(); iao != ol->end(); ++iao) 
        {
          AudioOutput* o = *iao;
          if(o->selected()) 
          {
            if(out) 
            {
              out = 0;
              break;
            }
            out = o;
          }
        }
        if(!out) 
        {
          QMessageBox::critical(this,
              tr("MusE: Bounce to Track"),
              tr("Select one audio output track,\nand one target wave track")
              );
          return;
        }
      }
      
      // search target track
      TrackList* tl = song->tracks();
      WaveTrack* track = 0;
      
      for (iTrack it = tl->begin(); it != tl->end(); ++it) {
            Track* t = *it;
            if (t->selected()) {
                    if(t->type() != Track::WAVE && t->type() != Track::AUDIO_OUTPUT) {
                        track = 0;
                        break;
                    }
                    if(t->type() == Track::WAVE)
                    { 
                      if(track)
                      {
                        track = 0;
                        break;
                      }
                      track = (WaveTrack*)t;
                    }  
                    
                  }  
            }
            
      if (track == 0) {
          if(ol->size() == 1) {
            QMessageBox::critical(this,
               tr("MusE: Bounce to Track"),
               tr("Select one target wave track")
               );
            return;
          }
          else 
          {
            QMessageBox::critical(this,
               tr("MusE: Bounce to Track"),
               tr("Select one target wave track,\nand one audio output track")
               );
            return;
          }  
      }

      song->setPos(0,song->lPos(),0,true,true);
      song->bounceOutput = out;
      song->bounceTrack = track;
      song->setRecord(true);
      song->setRecordFlag(track, true);
      track->prepareRecording();
      audio->msgBounce();
      song->setPlay(true);
      }

//---------------------------------------------------------
//   bounceToFile
//---------------------------------------------------------

void MusE::bounceToFile(AudioOutput* ao)
      {
      if(audio->bounce())
        return;
      song->bounceOutput = 0;
      if(!ao)
      {
        OutputList* ol = song->outputs();
        if(ol->empty())
        {
          QMessageBox::critical(this,
              tr("MusE: Bounce to File"),
              tr("No audio output tracks found")
              );
          return;
        }
        // If only one output, pick it, else pick the first selected.
        if(ol->size() == 1)
          ao = ol->front();
        else
        {
          for(iAudioOutput iao = ol->begin(); iao != ol->end(); ++iao) 
          {
            AudioOutput* o = *iao;
            if(o->selected()) 
            {
              if(ao) 
              {
               ao = 0;
               break;
              }
              ao = o;
            }
          }
          if (ao == 0) {
                QMessageBox::critical(this,
                  tr("MusE: Bounce to File"),
                  tr("Select one audio output track")
                  );
                return;
          }
        }
      }
      
      if (checkRegionNotNull())
            return;
      
      SndFile* sf = getSndFile(0, this);
      if (sf == 0)
            return;
            
      song->setPos(0,song->lPos(),0,true,true);
      song->bounceOutput = ao;
      ao->setRecFile(sf);
      //willfoobar-2011-02-13
      //old code//printf("ao->setRecFile %d\n", sf);
      printf("ao->setRecFile %ld\n", (unsigned long)sf);
      song->setRecord(true, false);
      song->setRecordFlag(ao, true);
      ao->prepareRecording();
      audio->msgBounce();
      song->setPlay(true);
      }


//---------------------------------------------------------
//   checkRegionNotNull
//    return true if (rPos - lPos) <= 0
//---------------------------------------------------------

bool MusE::checkRegionNotNull()
      {
      int start = song->lPos().frame();
      int end   = song->rPos().frame();
      if (end - start <= 0) {
            QMessageBox::critical(this,
               tr("MusE: Bounce"),
               tr("set left/right marker for bounce range")
               );
            return true;
            }
      return false;
      }


#ifdef HAVE_LASH
//---------------------------------------------------------
//   lash_idle_cb
//---------------------------------------------------------
#include <iostream>
void
MusE::lash_idle_cb ()
{
  lash_event_t * event;
  if (!lash_client)
    return;

  while ( (event = lash_get_event (lash_client)) )
    {
      switch (lash_event_get_type (event))
        {
        case LASH_Save_File:
    {
          /* save file */
          QString ss = QString(lash_event_get_string(event)) + QString("/lash-project-muse.med");
          int ok = save (ss.toAscii(), false);
          if (ok) {
            project.setFile(ss.toAscii());
            setWindowTitle(tr("MusE: Song: ") + project.completeBaseName());
            addProject(ss.toAscii());
            museProject = QFileInfo(ss.toAscii()).absolutePath();
          }
          lash_send_event (lash_client, event);
    }
    break;

        case LASH_Restore_File:
    {
          /* load file */
          QString sr = QString(lash_event_get_string(event)) + QString("/lash-project-muse.med");
          loadProjectFile(sr.toAscii(), false, true);
          lash_send_event (lash_client, event);
    }
          break;

        case LASH_Quit:
    {
          /* quit muse */
          std::cout << "MusE::lash_idle_cb Received LASH_Quit"
                    << std::endl;
          lash_event_destroy (event);
    }
    break;

        default:
    {
          std::cout << "MusE::lash_idle_cb Received unknown LASH event of type "
                    << lash_event_get_type (event)
                    << std::endl;
          lash_event_destroy (event);
    }
    break;
        }
    }
}
#endif /* HAVE_LASH */

//---------------------------------------------------------
//   clearSong
//    return true if operation aborted
//    called with sequencer stopped
//    If clear_all is false, it will not touch things like midi ports.
//---------------------------------------------------------

bool MusE::clearSong(bool clear_all)
      {
      if (song->dirty) {
            int n = 0;
            n = QMessageBox::warning(this, appName,
               tr("The current Project contains unsaved data\n"
               "Load overwrites current Project:\n"
               "Save Current Project?"),
               tr("&Save"), tr("&Skip"), tr("&Abort"), 0, 2);
            switch (n) {
                  case 0:
                        if (!save())      // abort if save failed
                              return true;
                        break;
                  case 1:
                        break;
                  case 2:
                        return true;
                  default:
                        printf("InternalError: gibt %d\n", n);
                  }
            }
      if (audio->isPlaying()) {
            audio->msgPlay(false);
            while (audio->isPlaying())
                  qApp->processEvents();
            }
      microSleep(100000);

again:
      for (iToplevel i = toplevels.begin(); i != toplevels.end(); ++i) {
            Toplevel tl = *i;
            unsigned long obj = tl.object();
            switch (tl.type()) {
                  case Toplevel::CLIPLIST:
                  case Toplevel::MARKER:
                  case Toplevel::ARRANGER:
                        break;
                  case Toplevel::PIANO_ROLL:
                  case Toplevel::SCORE:
                  case Toplevel::LISTE:
                  case Toplevel::DRUM:
                  case Toplevel::MASTER:
                  case Toplevel::WAVE:
                  case Toplevel::LMASTER:
                        ((QWidget*)(obj))->close();
                        goto again;
                  }
            }
      microSleep(100000);
      song->clear(true, clear_all);
      microSleep(100000);
      return false;
      }

//---------------------------------------------------------
//   startEditInstrument
//---------------------------------------------------------

void MusE::startEditInstrument()
    {
      if(editInstrument == 0)
      {
            editInstrument = new EditInstrument(this);
            editInstrument->show();
      }
      else
      {
        if(! editInstrument->isHidden())
          editInstrument->hide();
        else      
          editInstrument->show();
      }
      
    }

//---------------------------------------------------------
//   switchMixerAutomation
//---------------------------------------------------------

void MusE::switchMixerAutomation()
      {
      automation = !automation;
      // Clear all pressed and touched and rec event lists.
      song->clearRecAutomation(true);

// printf("automation = %d\n", automation);
      autoMixerAction->setChecked(automation);
      }

//---------------------------------------------------------
//   clearAutomation
//---------------------------------------------------------

void MusE::clearAutomation()
      {
      printf("not implemented\n");
      }

//---------------------------------------------------------
//   takeAutomationSnapshot
//---------------------------------------------------------

void MusE::takeAutomationSnapshot()
      {
      int frame = song->cPos().frame();
      TrackList* tracks = song->tracks();
      for (iTrack i = tracks->begin(); i != tracks->end(); ++i) {
            if ((*i)->isMidiTrack())
                  continue;
            AudioTrack* track = (AudioTrack*)*i;
            CtrlListList* cll = track->controller();
            for (iCtrlList icl = cll->begin(); icl != cll->end(); ++icl) {
                  double val = icl->second->curVal();
                  icl->second->add(frame, val);
                  }
            }
      }

//---------------------------------------------------------
//   updateConfiguration
//    called whenever the configuration has changed
//---------------------------------------------------------

void MusE::updateConfiguration()
      {
      fileOpenAction->setShortcut(shortcuts[SHRT_OPEN].key);
      fileNewAction->setShortcut(shortcuts[SHRT_NEW].key);
      fileSaveAction->setShortcut(shortcuts[SHRT_SAVE].key);
      fileSaveAsAction->setShortcut(shortcuts[SHRT_SAVE_AS].key);

      //menu_file->setShortcut(shortcuts[SHRT_OPEN_RECENT].key, menu_ids[CMD_OPEN_RECENT]);    // Not used.
      fileImportMidiAction->setShortcut(shortcuts[SHRT_IMPORT_MIDI].key);
      fileExportMidiAction->setShortcut(shortcuts[SHRT_EXPORT_MIDI].key);
      fileImportPartAction->setShortcut(shortcuts[SHRT_IMPORT_PART].key);
      fileImportWaveAction->setShortcut(shortcuts[SHRT_IMPORT_AUDIO].key);
      quitAction->setShortcut(shortcuts[SHRT_QUIT].key);
      
      //menu_file->setShortcut(shortcuts[SHRT_LOAD_TEMPLATE].key, menu_ids[CMD_LOAD_TEMPLATE]);  // Not used.

      undoAction->setShortcut(shortcuts[SHRT_UNDO].key);  
      redoAction->setShortcut(shortcuts[SHRT_REDO].key);


      //editSongInfoAction has no acceleration

      viewTransportAction->setShortcut(shortcuts[SHRT_OPEN_TRANSPORT].key);
      viewBigtimeAction->setShortcut(shortcuts[SHRT_OPEN_BIGTIME].key);
      viewMixerAAction->setShortcut(shortcuts[SHRT_OPEN_MIXER].key);
      viewMixerBAction->setShortcut(shortcuts[SHRT_OPEN_MIXER2].key);
      //viewCliplistAction has no acceleration
      viewMarkerAction->setShortcut(shortcuts[SHRT_OPEN_MARKER].key);

      
      // midiEditInstAction does not have acceleration
      midiResetInstAction->setShortcut(shortcuts[SHRT_MIDI_RESET].key);
      midiInitInstActions->setShortcut(shortcuts[SHRT_MIDI_INIT].key);
      midiLocalOffAction->setShortcut(shortcuts[SHRT_MIDI_LOCAL_OFF].key);
      midiTrpAction->setShortcut(shortcuts[SHRT_MIDI_INPUT_TRANSPOSE].key);
      midiInputTrfAction->setShortcut(shortcuts[SHRT_MIDI_INPUT_TRANSFORM].key);
      midiInputFilterAction->setShortcut(shortcuts[SHRT_MIDI_INPUT_FILTER].key);
      midiRemoteAction->setShortcut(shortcuts[SHRT_MIDI_REMOTE_CONTROL].key);
#ifdef BUILD_EXPERIMENTAL
      midiRhythmAction->setShortcut(shortcuts[SHRT_RANDOM_RHYTHM_GENERATOR].key);
#endif

      audioBounce2TrackAction->setShortcut(shortcuts[SHRT_AUDIO_BOUNCE_TO_TRACK].key);
      audioBounce2FileAction->setShortcut(shortcuts[SHRT_AUDIO_BOUNCE_TO_FILE].key);
      audioRestartAction->setShortcut(shortcuts[SHRT_AUDIO_RESTART].key);

      autoMixerAction->setShortcut(shortcuts[SHRT_MIXER_AUTOMATION].key);
      autoSnapshotAction->setShortcut(shortcuts[SHRT_MIXER_SNAPSHOT].key);
      autoClearAction->setShortcut(shortcuts[SHRT_MIXER_AUTOMATION_CLEAR].key);

      settingsGlobalAction->setShortcut(shortcuts[SHRT_GLOBAL_CONFIG].key);
      settingsShortcutsAction->setShortcut(shortcuts[SHRT_CONFIG_SHORTCUTS].key);
      settingsMetronomeAction->setShortcut(shortcuts[SHRT_CONFIG_METRONOME].key);
      settingsMidiSyncAction->setShortcut(shortcuts[SHRT_CONFIG_MIDISYNC].key);
      // settingsMidiIOAction does not have acceleration
      settingsAppearanceAction->setShortcut(shortcuts[SHRT_APPEARANCE_SETTINGS].key);
      settingsMidiPortAction->setShortcut(shortcuts[SHRT_CONFIG_MIDI_PORTS].key);


      dontFollowAction->setShortcut(shortcuts[SHRT_FOLLOW_NO].key);
      followPageAction->setShortcut(shortcuts[SHRT_FOLLOW_JUMP].key);
      followCtsAction->setShortcut(shortcuts[SHRT_FOLLOW_CONTINUOUS].key);
      
      helpManualAction->setShortcut(shortcuts[SHRT_OPEN_HELP].key);
      
      // Orcan: Old stuff, needs to be converted. These aren't used anywhere so I commented them out
      //menuSettings->setAccel(shortcuts[SHRT_CONFIG_AUDIO_PORTS].key, menu_ids[CMD_CONFIG_AUDIO_PORTS]);
      //menu_help->setAccel(menu_ids[CMD_START_WHATSTHIS], shortcuts[SHRT_START_WHATSTHIS].key);
      
      //arrangerView->updateShortcuts(); //commented out by flo: is done via signal
      
      }

//---------------------------------------------------------
//   showBigtime
//---------------------------------------------------------

void MusE::showBigtime(bool on)
      {
      if (on && bigtime == 0) {
            bigtime = new BigTime(0);
            bigtime->setPos(0, song->cpos(), false);
            connect(song, SIGNAL(posChanged(int, unsigned, bool)), bigtime, SLOT(setPos(int, unsigned, bool)));
            connect(muse, SIGNAL(configChanged()), bigtime, SLOT(configChanged()));
            connect(bigtime, SIGNAL(closed()), SLOT(bigtimeClosed()));
            bigtime->resize(config.geometryBigTime.size());
            bigtime->move(config.geometryBigTime.topLeft());
            }
      if (bigtime)
            bigtime->setVisible(on);
      viewBigtimeAction->setChecked(on);
      }

//---------------------------------------------------------
//   toggleBigTime
//---------------------------------------------------------

void MusE::toggleBigTime(bool checked)
      {
      showBigtime(checked);
      }

//---------------------------------------------------------
//   bigtimeClosed
//---------------------------------------------------------

void MusE::bigtimeClosed()
      {
      viewBigtimeAction->setChecked(false);
      }


//---------------------------------------------------------
//   showMixer1
//---------------------------------------------------------

void MusE::showMixer1(bool on)
      {
      if (on && mixer1 == 0) {
            mixer1 = new AudioMixerApp(this, &(config.mixer1));
            connect(mixer1, SIGNAL(closed()), SLOT(mixer1Closed()));
            mixer1->resize(config.mixer1.geometry.size());
            mixer1->move(config.mixer1.geometry.topLeft());
            }
      if (mixer1)
            mixer1->setVisible(on);
      viewMixerAAction->setChecked(on);
      }

//---------------------------------------------------------
//   showMixer2
//---------------------------------------------------------

void MusE::showMixer2(bool on)
      {
      if (on && mixer2 == 0) {
            mixer2 = new AudioMixerApp(this, &(config.mixer2));
            connect(mixer2, SIGNAL(closed()), SLOT(mixer2Closed()));
            mixer2->resize(config.mixer2.geometry.size());
            mixer2->move(config.mixer2.geometry.topLeft());
            }
      if (mixer2)
            mixer2->setVisible(on);
      viewMixerBAction->setChecked(on);
      }

//---------------------------------------------------------
//   toggleMixer1
//---------------------------------------------------------

void MusE::toggleMixer1(bool checked)
      {
      showMixer1(checked);
      }

//---------------------------------------------------------
//   toggleMixer2
//---------------------------------------------------------

void MusE::toggleMixer2(bool checked)
      {
      showMixer2(checked);
      }

//---------------------------------------------------------
//   mixer1Closed
//---------------------------------------------------------

void MusE::mixer1Closed()
      {
      viewMixerAAction->setChecked(false);
      }

//---------------------------------------------------------
//   mixer2Closed
//---------------------------------------------------------

void MusE::mixer2Closed()
      {
      viewMixerBAction->setChecked(false);
      }


QWidget* MusE::mixer1Window()     { return mixer1; }
QWidget* MusE::mixer2Window()     { return mixer2; }

QWidget* MusE::transportWindow() { return transport; }
QWidget* MusE::bigtimeWindow()   { return bigtime; }

//---------------------------------------------------------
//   focusInEvent
//---------------------------------------------------------

void MusE::focusInEvent(QFocusEvent* ev)
      {
      //if (audioMixer)
      //      audioMixer->raise();
      if (mixer1)
            mixer1->raise();
      if (mixer2)
            mixer2->raise();
      raise();
      QMainWindow::focusInEvent(ev);
      }



//---------------------------------------------------------
//   execDeliveredScript
//---------------------------------------------------------
void MusE::execDeliveredScript(int id)
{
      //QString scriptfile = QString(INSTPREFIX) + SCRIPTSSUFFIX + deliveredScriptNames[id];
      song->executeScript(song->getScriptPath(id, true).toLatin1().constData(), song->getSelectedMidiParts(), 0, false); // TODO: get quant from arranger
}

//---------------------------------------------------------
//   execUserScript
//---------------------------------------------------------
void MusE::execUserScript(int id)
{
      song->executeScript(song->getScriptPath(id, false).toLatin1().constData(), song->getSelectedMidiParts(), 0, false); // TODO: get quant from arranger
}

//---------------------------------------------------------
//   findUnusedWaveFiles
//---------------------------------------------------------
void MusE::findUnusedWaveFiles()
{
    UnusedWaveFiles unused(muse);
    unused.exec();
}
