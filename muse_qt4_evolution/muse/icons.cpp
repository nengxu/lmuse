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

#include "globals.h"
#include "gui.h"

#include "xpm/audio_bounce_to_file.xpm"
#include "xpm/audio_bounce_to_track.xpm"
#include "xpm/audio_restartaudio.xpm"
#include "xpm/edit_midi.xpm"
#include "xpm/midi_edit_instrument.xpm"
#include "xpm/midi_init_instr.xpm"
#include "xpm/midi_local_off.xpm"
#include "xpm/midi_reset_instr.xpm"
#include "xpm/settings_appearance_settings.xpm"
#include "xpm/settings_configureshortcuts.xpm"
#include "xpm/settings_follow_song.xpm"
#include "xpm/settings_globalsettings.xpm"
#include "xpm/settings_metronome.xpm"
#include "xpm/settings_midifileexport.xpm"
#include "xpm/settings_midisync.xpm"

#include "xpm/play.xpm"

#include "xpm/stick.xpm"
#include "xpm/wave.xpm"
#include "xpm/cmark.xpm"
#include "xpm/lmark.xpm"
#include "xpm/rmark.xpm"
#include "xpm/steprec.xpm"
#include "xpm/master.xpm"
#include "xpm/filenewS.xpm"
#include "xpm/home.xpm"
#include "xpm/back.xpm"
#include "xpm/forward.xpm"

#include "xpm/up.xpm"
#include "xpm/down.xpm"
#include "xpm/bold.xpm"
#include "xpm/italic.xpm"
#include "xpm/underlined.xpm"
#include "xpm/midiin.xpm"
#include "xpm/sysex.xpm"
#include "xpm/ctrl.xpm"
#include "xpm/meta.xpm"
#include "xpm/pitch.xpm"
#include "xpm/cafter.xpm"
#include "xpm/pafter.xpm"
#include "xpm/flag.xpm"
#include "xpm/flagS.xpm"
#include "xpm/lock.xpm"
#include "xpm/toc.xpm"
#include "xpm/editcut.xpm"
#include "xpm/editcopy.xpm"
#include "xpm/editpaste.xpm"

#include "xpm/speaker.xpm"
#include "xpm/buttondown.xpm"
#include "xpm/configure.xpm"

#include "xpm/mastertrackS.xpm"
#include "xpm/localoffS.xpm"
#include "xpm/miditransformS.xpm"
#include "xpm/midi_plugS.xpm"
#include "xpm/miditransposeS.xpm"
#include "xpm/mixerS.xpm"
#include "xpm/resetS.xpm"
#include "xpm/track_add.xpm"
#include "xpm/track_delete.xpm"
#include "xpm/listS.xpm"
#include "xpm/inputpluginS.xpm"
#include "xpm/cliplistS.xpm"
#include "xpm/mixeraudioS.xpm"
#include "xpm/initS.xpm"

#include "xpm/addtrack_addmiditrack.xpm"
#include "xpm/addtrack_audiogroup.xpm"
#include "xpm/addtrack_audioinput.xpm"
#include "xpm/addtrack_audiooutput.xpm"
#include "xpm/addtrack_drumtrack.xpm"
#include "xpm/addtrack_wavetrack.xpm"
#include "xpm/edit_drumms.xpm"
#include "xpm/edit_list.xpm"
#include "xpm/edit_mastertrack.xpm"
#include "xpm/edit_track_add.xpm"
#include "xpm/edit_track_del.xpm"
#include "xpm/mastertrack_graphic.xpm"
#include "xpm/mastertrack_list.xpm"
#include "xpm/midi_transform.xpm"
#include "xpm/midi_transpose.xpm"
#include "xpm/select.xpm"
// #include "xpm/select_all.xpm"
#include "xpm/select_all_parts_on_track.xpm"
// #include "xpm/select_deselect_all.xpm"
// #include "xpm/select_inside_loop.xpm"
// #include "xpm/select_invert_selection.xpm"
// #include "xpm/select_outside_loop.xpm"

#include "xpm/muse_icon.xpm"
#include "xpm/config.xpm"
#include "xpm/minus.xpm"
#include "xpm/plus.xpm"

QPixmap* mastertrackSIcon;
QPixmap* localoffSIcon;
QPixmap* miditransformSIcon;
QPixmap* midi_plugSIcon;
QPixmap* miditransposeSIcon;
QPixmap* mixerSIcon;
QPixmap* resetSIcon;
QPixmap* track_addIcon;
QPixmap* track_deleteIcon;
QPixmap* listSIcon;
QPixmap* inputpluginSIcon;
QPixmap* cliplistSIcon;
QPixmap* mixerAudioSIcon;
QPixmap* initSIcon;

QPixmap* playIcon;

QPixmap* stopIcon;
QPixmap* fforwardIcon;
QPixmap* frewindIcon;
QPixmap* stickIcon;
QPixmap* waveIcon;
QPixmap* markIcon[3];
QPixmap* steprecIcon;
QPixmap* openIcon;
QPixmap* saveIcon;
QPixmap* masterIcon;
QPixmap* filenewIcon;
QPixmap* filenewIconS;
QPixmap* homeIcon;
QPixmap* backIcon;
QPixmap* forwardIcon;
QPixmap* muteIcon;
QPixmap* upIcon;
QPixmap* downIcon;
QPixmap* boldIcon;
QPixmap* italicIcon;
QPixmap* underlinedIcon;
QPixmap* midiinIcon;
QPixmap* sysexIcon;
QPixmap* ctrlIcon;
QPixmap* metaIcon;
QPixmap* pitchIcon;
QPixmap* cafterIcon;
QPixmap* pafterIcon;
QPixmap* flagIcon;
QPixmap* flagIconS;
QPixmap* lockIcon;
QPixmap* tocIcon;

QPixmap* speakerIcon;
QPixmap* buttondownIcon;
QPixmap* configureIcon;

QIcon* editcutIconSet;
QIcon* editcopyIconSet;
QIcon* editpasteIconSet;
QIcon* recordIcon;
QIcon* onOffIcon;
QPixmap* offIcon;

QPixmap* addtrack_addmiditrackIcon;
QPixmap* addtrack_audiogroupIcon;
QPixmap* addtrack_audioinputIcon;
QPixmap* addtrack_audiooutputIcon;
QPixmap* addtrack_drumtrackIcon;
QPixmap* addtrack_wavetrackIcon;
QPixmap* edit_drummsIcon;
QPixmap* edit_listIcon;
QPixmap* edit_mastertrackIcon;
QPixmap* edit_track_addIcon;
QPixmap* edit_track_delIcon;
QPixmap* mastertrack_graphicIcon;
QPixmap* mastertrack_listIcon;
QPixmap* midi_transformIcon;
QPixmap* midi_transposeIcon;
QPixmap* selectIcon;
// QPixmap* select_allIcon;
QPixmap* select_all_parts_on_trackIcon;
// QPixmap* select_deselect_allIcon;
// QPixmap* select_inside_loopIcon;
// QPixmap* select_invert_selectionIcon;
// QPixmap* select_outside_loopIcon;

QPixmap* audio_bounce_to_fileIcon;
QPixmap* audio_bounce_to_trackIcon;
QPixmap* audio_restartaudioIcon;
QPixmap* edit_midiIcon;
QPixmap* midi_edit_instrumentIcon;
QPixmap* midi_init_instrIcon;
QPixmap* midi_local_offIcon;
QPixmap* midi_reset_instrIcon;
QPixmap* settings_appearance_settingsIcon;
QPixmap* settings_configureshortcutsIcon;
QPixmap* settings_follow_songIcon;
QPixmap* settings_globalsettingsIcon;
QPixmap* settings_metronomeIcon;
QPixmap* settings_midifileexportIcon;
QPixmap* settings_midisyncIcon;

QPixmap* museIcon;
QPixmap* museIcon64;
QPixmap* configIcon;
QPixmap* minusIcon;
QPixmap* plusIcon;

//---------------------------------------------------------
//   initIcons
//---------------------------------------------------------

void initIcons()
      {
      playIcon     = new QPixmap(play_xpm);

      stickIcon    = new QPixmap(stick_xpm);
      waveIcon     = new QPixmap(wave_xpm);
      markIcon[0]  = new QPixmap(cmark_xpm);
      markIcon[1]  = new QPixmap(lmark_xpm);
      markIcon[2]  = new QPixmap(rmark_xpm);
      steprecIcon  = new QPixmap(steprec_xpm);
      saveIcon     = new QPixmap(":/xpm/filesave.png");
      openIcon     = new QPixmap(":/xpm/fileopen.png");
      masterIcon   = new QPixmap(master_xpm);
      filenewIcon  = new QPixmap(":/xpm/filenew.png");
      filenewIconS  = new QPixmap(filenewS_xpm);
      homeIcon     = new QPixmap(home_xpm);
      backIcon     = new QPixmap(back_xpm);
      forwardIcon  = new QPixmap(forward_xpm);
      upIcon       = new QPixmap(up_xpm);
      downIcon     = new QPixmap(down_xpm);
      boldIcon     = new QPixmap(bold_xpm);
      italicIcon     = new QPixmap(italic_xpm);
      underlinedIcon = new QPixmap(underlined_xpm);
      midiinIcon = new QPixmap(midiin_xpm);
      sysexIcon   = new QPixmap(sysex_xpm);
      ctrlIcon    = new QPixmap(ctrl_xpm);
      metaIcon    = new QPixmap(meta_xpm);
      pitchIcon   = new QPixmap(pitch_xpm);
      cafterIcon  = new QPixmap(cafter_xpm);
      pafterIcon  = new QPixmap(pafter_xpm);
      flagIcon    = new QPixmap(flag_xpm);
      flagIconS   = new QPixmap(flagS_xpm);
      lockIcon    = new QPixmap(lock_xpm);
      tocIcon     = new QPixmap(toc_xpm);

      speakerIcon    = new QPixmap(speaker_xpm);
      buttondownIcon = new QPixmap(buttondown_xpm);
      configureIcon  = new QPixmap(configure_xpm);

      editcutIconSet       = new QIcon(QPixmap(editcut_xpm));
      editcopyIconSet      = new QIcon(QPixmap(editcopy_xpm));
      editpasteIconSet     = new QIcon(QPixmap(editpaste_xpm));

      mastertrackSIcon     = new QPixmap(mastertrackS_xpm);
      localoffSIcon        = new QPixmap(localoffS_xpm);
      miditransformSIcon   = new QPixmap(miditransformS_xpm);
      midi_plugSIcon       = new QPixmap(midi_plugS_xpm);
      miditransposeSIcon   = new QPixmap(miditransposeS_xpm);
      mixerSIcon           = new QPixmap(mixerS_xpm);
      resetSIcon           = new QPixmap(resetS_xpm);
      track_addIcon        = new QPixmap(track_add_xpm);
      track_deleteIcon     = new QPixmap(track_delete_xpm);
      listSIcon            = new QPixmap(listS_xpm);
      inputpluginSIcon     = new QPixmap(inputpluginS_xpm);
      cliplistSIcon        = new QPixmap(cliplistS_xpm);
      mixerAudioSIcon      = new QPixmap(mixerAudioS_xpm);
      initSIcon            = new QPixmap(initS_xpm);

      addtrack_addmiditrackIcon     = new QPixmap(addtrack_addmiditrack_xpm);
      addtrack_audiogroupIcon       = new QPixmap(addtrack_audiogroup_xpm);
      addtrack_audioinputIcon       = new QPixmap(addtrack_audioinput_xpm);
      addtrack_audiooutputIcon      = new QPixmap(addtrack_audiooutput_xpm);
      addtrack_drumtrackIcon        = new QPixmap(addtrack_drumtrack_xpm);
      addtrack_wavetrackIcon        = new QPixmap(addtrack_wavetrack_xpm);
      edit_drummsIcon               = new QPixmap(edit_drumms_xpm);
      edit_listIcon                 = new QPixmap(edit_list_xpm);
      edit_mastertrackIcon          = new QPixmap(edit_mastertrack_xpm);
      edit_track_addIcon            = new QPixmap(edit_track_add_xpm);
      edit_track_delIcon            = new QPixmap(edit_track_del_xpm);
      mastertrack_graphicIcon       = new QPixmap(mastertrack_graphic_xpm);
      mastertrack_listIcon          = new QPixmap(mastertrack_list_xpm);
      midi_transformIcon            = new QPixmap(midi_transform_xpm);
      midi_transposeIcon            = new QPixmap(midi_transpose_xpm);
      selectIcon                    = new QPixmap(select_xpm);
//      select_allIcon                = new QPixmap(select_all_xpm);
      select_all_parts_on_trackIcon = new QPixmap(select_all_parts_on_track_xpm);
//      select_deselect_allIcon       = new QPixmap(select_deselect_all);
//      select_inside_loopIcon        = new QPixmap(select_inside_loop_xpm);
//      select_invert_selectionIcon   = new QPixmap(select_invert_selection);
//      select_outside_loopIcon       = new QPixmap(select_outside_loop_xpm);

      audio_bounce_to_fileIcon         = new QPixmap(audio_bounce_to_file_xpm);
      audio_bounce_to_trackIcon        = new QPixmap(audio_bounce_to_track_xpm);
      audio_restartaudioIcon           = new QPixmap(audio_restartaudio_xpm);
      edit_midiIcon                    = new QPixmap(edit_midi_xpm);
      midi_edit_instrumentIcon         = new QPixmap(midi_edit_instrument_xpm);
      midi_init_instrIcon              = new QPixmap(midi_init_instr_xpm);
      midi_local_offIcon               = new QPixmap(midi_local_off_xpm);
      midi_reset_instrIcon             = new QPixmap(midi_reset_instr_xpm);
      settings_appearance_settingsIcon = new QPixmap(settings_appearance_settings_xpm);
      settings_configureshortcutsIcon  = new QPixmap(settings_configureshortcuts_xpm);
      settings_follow_songIcon         = new QPixmap(settings_follow_song_xpm);
      settings_globalsettingsIcon      = new QPixmap(settings_globalsettings_xpm);
      settings_metronomeIcon           = new QPixmap(settings_metronome_xpm);
      settings_midifileexportIcon      = new QPixmap(settings_midifileexport_xpm);
      settings_midisyncIcon            = new QPixmap(settings_midisync_xpm);

      museIcon   = new QPixmap(muse_icon_xpm);
      configIcon = new QPixmap(config_xpm);
      minusIcon  = new QPixmap(minus_xpm);
      plusIcon   = new QPixmap(plus_xpm);

      recordIcon = new QIcon;
      recordIcon->addFile(":/xpm/recordOn.svg",  ICON_SIZE, QIcon::Normal, QIcon::On);
      recordIcon->addFile(":/xpm/recordOff.svg", ICON_SIZE, QIcon::Normal, QIcon::Off);

      onOffIcon = new QIcon;
      onOffIcon->addFile(":/xpm/on.svg",  ICON_SIZE, QIcon::Normal, QIcon::On);
      onOffIcon->addFile(":/xpm/off.svg", ICON_SIZE, QIcon::Normal, QIcon::Off);
      }
