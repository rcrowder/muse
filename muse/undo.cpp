//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: undo.cpp,v 1.12.2.9 2009/05/24 21:43:44 terminator356 Exp $
//
//  (C) Copyright 1999/2000 Werner Schweer (ws@seh.de)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//=========================================================

///#include "sig.h"
#include "al/sig.h"  // Tim.
#include "keyevent.h"

#include "undo.h"
#include "song.h"
#include "globals.h"
#include "audio.h"  // p4.0.46

#include <string.h>
#include <QAction>
#include <set>

namespace MusECore {

// iundo points to last Undo() in Undo-list

static bool undoMode = false;  // for debugging

std::list<QString> temporaryWavFiles;

//---------------------------------------------------------
//   typeName
//---------------------------------------------------------

const char* UndoOp::typeName()
      {
      static const char* name[] = {
            "AddTrack", "DeleteTrack", "ModifyTrack",
            "AddPart",  "DeletePart",  "ModifyPart",
            "AddEvent", "DeleteEvent", "ModifyEvent",
            "AddTempo", "DeleteTempo",
            "AddSig", "DeleteSig",
            "AddKey", "DeleteKey",
            "ModifyTrackName", "ModifyTrackChannel",
            "SwapTrack", 
            "ModifyClip", "ModifyMarker",
            "ModifySongLen", "DoNothing"
            };
      return name[type];
      }

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void UndoOp::dump()
      {
      printf("UndoOp: %s\n   ", typeName());
      switch(type) {
            case AddTrack:
            case DeleteTrack:
                  printf("%d %s\n", trackno, oTrack->name().toLatin1().constData());
                  break;
            case ModifyTrack:
                  printf("%d <%s>-<%s>\n", trackno, oTrack->name().toLatin1().constData(), nTrack->name().toLatin1().constData());
                  break;
            case AddPart:
            case DeletePart:
            case ModifyPart:
                  break;
            case AddEvent:
            case DeleteEvent:
                  printf("old event:\n");
                  oEvent.dump(5);
                  printf("   new event:\n");
                  nEvent.dump(5);
                  printf("   Part:\n");
                  if (part)
                        part->dump(5);
                  break;
            case ModifyTrackName:
                  printf("<%s>-<%s>\n", _oldName, _newName);
                  break;
            case ModifyTrackChannel:
                  printf("<%d>-<%d>\n", a, b);
                  break;
            case ModifyEvent:
            case AddTempo:
            case DeleteTempo:
            case AddSig:
            case SwapTrack:
            case DeleteSig:
            case ModifyClip:
            case ModifyMarker:
            case AddKey:
            case DeleteKey:
            case ModifySongLen:
            case DoNothing:
                  break;
            default:      
                  break;
            }
      }

//---------------------------------------------------------
//    clearDelete
//---------------------------------------------------------

void UndoList::clearDelete()
{
  if(!empty())
  {
    for(iUndo iu = begin(); iu != end(); ++iu)
    {
      Undo& u = *iu;
      for(riUndoOp i = u.rbegin(); i != u.rend(); ++i)
      {
        switch(i->type)
        {
          case UndoOp::DeleteTrack:
                if(i->oTrack)
                {
                  delete i->oTrack;
                  iUndo iu2 = iu;
                  ++iu2;
                  for(; iu2 != end(); ++iu2)
                  {
                    Undo& u2 = *iu2;
                    for(riUndoOp i2 = u2.rbegin(); i2 != u2.rend(); ++i2)
                    {
                      if(i2->type == UndoOp::DeleteTrack)
                      {
                        if(i2->oTrack == i->oTrack)
                          i2->oTrack = 0;
                      }
                    }
                  }
                }
                break;
          case UndoOp::ModifyTrack:
                if(i->oTrack)
                {
                  // Prevent delete i->oTrack from crashing.
                  switch(i->oTrack->type())
                  {
                        case Track::AUDIO_OUTPUT:
                                {
                                AudioOutput* ao = (AudioOutput*)i->oTrack;
                                for(int ch = 0; ch < ao->channels(); ++ch)
                                  ao->setJackPort(ch, 0);
                                }
                              break;
                        case Track::AUDIO_INPUT:
                                {
                                AudioInput* ai = (AudioInput*)i->oTrack;
                                for(int ch = 0; ch < ai->channels(); ++ch)
                                  ai->setJackPort(ch, 0);
                                }
                              break;
                        default:
                              break;
                  }
                  if(!i->oTrack->isMidiTrack())
                    ((AudioTrack*)i->oTrack)->clearEfxList();
                  delete i->oTrack;

                  iUndo iu2 = iu;
                  ++iu2;
                  for(; iu2 != end(); ++iu2)
                  {
                    Undo& u2 = *iu2;
                    for(riUndoOp i2 = u2.rbegin(); i2 != u2.rend(); ++i2)
                    {
                      if(i2->type == UndoOp::ModifyTrack)
                      {
                        if(i2->oTrack == i->oTrack)
                          i2->oTrack = 0;
                      }
                    }
                  }
                }
                break;
          //case UndoOp::DeletePart:
                //delete i->oPart;
          //      break;
          //case UndoOp::DeleteTempo:
          //      break;
          //case UndoOp::DeleteSig:
          //      break;
            case UndoOp::ModifyMarker:
                if (i->copyMarker)
                  delete i->copyMarker;
                break;
          case UndoOp::ModifyTrackName:
                printf("UndoList::clearDelete ModifyTrackName old:%s new:%s\n", i->_oldName, i->_newName);  // REMOVE Tim.
                if (i->_oldName)
                  delete i->_oldName;
                if (i->_newName)
                  delete i->_newName;
                break;
          default:
                break;
        }
      }
      u.clear();
    }
  }

  clear();
}

//---------------------------------------------------------
//    startUndo
//---------------------------------------------------------

void Song::startUndo()
      {
      //redoList->clear(); // added by flo93: redo must be invalidated when a new undo is started
      redoList->clearDelete(); // p4.0.46 Tim
			MusEGlobal::redoAction->setEnabled(false);     // 
			
      undoList->push_back(Undo());
      updateFlags = 0;
      undoMode = true;
      }

//---------------------------------------------------------
//   endUndo
//---------------------------------------------------------

void Song::endUndo(int flags)
      {
      updateFlags |= flags;
      endMsgCmd();
      undoMode = false;
      }


void cleanOperationGroup(Undo& group)
{
	using std::set;
	
	set<Track*> processed_tracks;
	set<Part*> processed_parts;

	for (iUndoOp op=group.begin(); op!=group.end();)
	{
		iUndoOp op_=op;
		op_++;
		
		if ((op->type==UndoOp::ModifyTrack) || (op->type==UndoOp::DeleteTrack))
		{
			if (processed_tracks.find(op->oTrack)!=processed_tracks.end())
				group.erase(op);
			else
				processed_tracks.insert(op->oTrack);
		}
		else if ((op->type==UndoOp::ModifyPart) || (op->type==UndoOp::DeletePart))
		{
			if (processed_parts.find(op->oPart)!=processed_parts.end())
				group.erase(op);
			else
				processed_parts.insert(op->oPart);
		}
		
		op=op_;
	}
}


bool Song::applyOperationGroup(Undo& group, bool doUndo)
{
      if (!group.empty())
      {
            cleanOperationGroup(group);
            //this is a HACK! but it works :)    (added by flo93)
            redoList->push_back(group);
            redo();
            
            if (!doUndo)
            {
                  undoList->pop_back();
                  MusEGlobal::undoAction->setEnabled(!undoList->empty());
            }
            else
            {
                  //redoList->clear(); // added by flo93: redo must be invalidated when a new undo is started
                  redoList->clearDelete(); // p4.0.46 Tim.
                  MusEGlobal::redoAction->setEnabled(false);     // 
						}
            
            return doUndo;
      }
      else
            return false;
}



//---------------------------------------------------------
//   doUndo2
//    real time part
//---------------------------------------------------------

void Song::doUndo2()
      {
      Undo& u = undoList->back();
      for (riUndoOp i = u.rbegin(); i != u.rend(); ++i) {
            switch(i->type) {
                  case UndoOp::AddTrack:
                        removeTrack2(i->oTrack);
                        updateFlags |= SC_TRACK_REMOVED;
                        break;
                  case UndoOp::DeleteTrack:
                        insertTrack2(i->oTrack, i->trackno);
                        // Added by T356.
                        chainTrackParts(i->oTrack, true);
                        
                        updateFlags |= SC_TRACK_INSERTED;
                        break;
                  case UndoOp::ModifyTrack:
                        {
                        // Added by Tim. p3.3.6
                        //printf("Song::doUndo2 ModifyTrack #1 oTrack %p %s nTrack %p %s\n", i->oTrack, i->oTrack->name().toLatin1().constData(), i->nTrack, i->nTrack->name().toLatin1().constData());
                        
                        // Unchain the track parts, but don't touch the ref counts.
                        unchainTrackParts(i->nTrack, false);
                        
                        //Track* track = i->nTrack->clone();
                        Track* track = i->nTrack->clone(false);
                        
                        // A Track custom assignment operator was added by Tim. 
                        *(i->nTrack) = *(i->oTrack);
                        
                        // Added by Tim. p3.3.6
                        //printf("Song::doUndo2 ModifyTrack #2 oTrack %p %s nTrack %p %s\n", i->oTrack, i->oTrack->name().toLatin1().constData(), i->nTrack, i->nTrack->name().toLatin1().constData());
                        
                        // Prevent delete i->oTrack from crashing.
                        switch(i->oTrack->type())
                        {
                              case Track::AUDIO_OUTPUT:
                                      {
                                      AudioOutput* ao = (AudioOutput*)i->oTrack;
                                      for(int ch = 0; ch < ao->channels(); ++ch)
                                        ao->setJackPort(ch, 0);
                                      }
                                    break;
                              case Track::AUDIO_INPUT:
                                      {
                                      AudioInput* ai = (AudioInput*)i->oTrack;
                                      for(int ch = 0; ch < ai->channels(); ++ch)
                                        ai->setJackPort(ch, 0);
                                      }
                                    break;
                              default:
                                    break;
                        }
                        if(!i->oTrack->isMidiTrack())
                          ((AudioTrack*)i->oTrack)->clearEfxList();

                        delete i->oTrack;
                        i->oTrack = track;
                        
                        // Chain the track parts, but don't touch the ref counts.
                        chainTrackParts(i->nTrack, false);

                        // Added by Tim. p3.3.6
                        //printf("Song::doUndo2 ModifyTrack #3 oTrack %p %s nTrack %p %s\n", i->oTrack, i->oTrack->name().toLatin1().constData(), i->nTrack, i->nTrack->name().toLatin1().constData());
                        
                        // Connect and register ports.
                        switch(i->nTrack->type())
                        {
                          case Track::AUDIO_OUTPUT:
                              {
                              AudioOutput* ao = (AudioOutput*)i->nTrack;
                              ao->setName(ao->name());
                              }
                            break;
                          case Track::AUDIO_INPUT:
                              {
                              AudioInput* ai = (AudioInput*)i->nTrack;
                              ai->setName(ai->name());
                              }
                            break;
                          default:
                            break;
                        }

                        // Update solo states, since the user may have changed soloing on other tracks.
                        updateSoloStates();

                        updateFlags |= SC_TRACK_MODIFIED;
                        }
                        break;
                        
                        /*
                        switch(i->nTrack->type())
                        {
                              case Track::AUDIO_OUTPUT:
                                      {
                                      AudioOutput* ao = (AudioOutput*)i->nTrack;
                                      for(int ch = 0; ch < ao->channels(); ++ch)
                                        ao->setJackPort(ch, 0);
                                      }
                                    break;
                              case Track::AUDIO_INPUT:
                                      {
                                      AudioInput* ai = (AudioInput*)i->nTrack;
                                      for(int ch = 0; ch < ai->channels(); ++ch)
                                        ai->setJackPort(ch, 0);
                                      }
                                    break;
                              default:
                                    break;
                        }
                        if(!i->nTrack->isMidiTrack())
                          ((AudioTrack*)i->nTrack)->clearEfxList();

                        //delete i->oTrack;
                        //i->oTrack = track;
                        
                        // Remove the track. removeTrack2 takes care of unchaining the new track.
                        removeTrack2(i->nTrack);
                        
                        // Connect and register ports.
                        switch(i->oTrack->type())
                        {
                          case Track::AUDIO_OUTPUT:
                              {
                              AudioOutput* ao = (AudioOutput*)i->oTrack;
                              ao->setName(ao->name());
                              }
                            break;
                          case Track::AUDIO_INPUT:
                              {
                              AudioInput* ai = (AudioInput*)i->oTrack;
                              ai->setName(ai->name());
                              }
                            break;
                          default:
                            break;
                        }

                        // Insert the old track.
                        insertTrack2(i->oTrack, i->trackno);
                        // Chain the old track parts. (removeTrack2, above, takes care of unchaining the new track).
                        chainTrackParts(i->oTrack, true);
                        
                        // Update solo states, since the user may have changed soloing on other tracks.
                        updateSoloStates();

                        updateFlags |= SC_TRACK_MODIFIED;
                        }
                        break;
                        */
                        
                  case UndoOp::SwapTrack:
                        {
                        updateFlags |= SC_TRACK_MODIFIED;
                        Track* track  = _tracks[i->a];
                        _tracks[i->a] = _tracks[i->b];
                        _tracks[i->b] = track;
                        updateFlags |= SC_TRACK_MODIFIED;
                        }
                        break;
                  case UndoOp::AddPart:
                        {
                        Part* part = i->oPart;
                        removePart(part);
                        updateFlags |= SC_PART_REMOVED;
                        i->oPart->events()->incARef(-1);
                        //i->oPart->unchainClone();
                        unchainClone(i->oPart);
                        }
                        break;
                  case UndoOp::DeletePart:
                        addPart(i->oPart);
                        updateFlags |= SC_PART_INSERTED;
                        i->oPart->events()->incARef(1);
                        //i->oPart->chainClone();
                        chainClone(i->oPart);
                        break;
                  case UndoOp::ModifyPart:
                        if(i->doCtrls)
                          removePortCtrlEvents(i->oPart, i->doClones);
                        changePart(i->oPart, i->nPart);
                        i->oPart->events()->incARef(-1);
                        i->nPart->events()->incARef(1);
                        //i->oPart->replaceClone(i->nPart);
                        replaceClone(i->oPart, i->nPart);
                        if(i->doCtrls)
                          addPortCtrlEvents(i->nPart, i->doClones);
                        updateFlags |= SC_PART_MODIFIED;
                        break;
                  case UndoOp::AddEvent:
                        if(i->doCtrls)
                          removePortCtrlEvents(i->nEvent, i->part, i->doClones);
                        deleteEvent(i->nEvent, i->part);
                        updateFlags |= SC_EVENT_REMOVED;
                        break;
                  case UndoOp::DeleteEvent:
                        addEvent(i->nEvent, i->part);
                        if(i->doCtrls)
                          addPortCtrlEvents(i->nEvent, i->part, i->doClones);
                        updateFlags |= SC_EVENT_INSERTED;
                        break;
                  case UndoOp::ModifyEvent:
                        if(i->doCtrls)
                          removePortCtrlEvents(i->oEvent, i->part, i->doClones);
                        changeEvent(i->oEvent, i->nEvent, i->part);
                        if(i->doCtrls)
                          addPortCtrlEvents(i->nEvent, i->part, i->doClones);
                        updateFlags |= SC_EVENT_MODIFIED;
                        break;
                  case UndoOp::AddTempo:
                        //printf("doUndo2: UndoOp::AddTempo. deleting tempo at: %d\n", i->a);
                        MusEGlobal::tempomap.delTempo(i->a);
                        updateFlags |= SC_TEMPO;
                        break;
                  case UndoOp::DeleteTempo:
                        //printf("doUndo2: UndoOp::DeleteTempo. adding tempo at: %d, tempo=%d\n", i->a, i->b);
                        MusEGlobal::tempomap.addTempo(i->a, i->b);
                        updateFlags |= SC_TEMPO;
                        break;
                  case UndoOp::AddSig:
                        ///sigmap.del(i->a);
                        AL::sigmap.del(i->a);
                        updateFlags |= SC_SIG;
                        break;
                  case UndoOp::DeleteSig:
                        ///sigmap.add(i->a, i->b, i->c);
                        AL::sigmap.add(i->a, AL::TimeSignature(i->b, i->c));
                        updateFlags |= SC_SIG;
                        break;
                  case UndoOp::AddKey:
                        ///sigmap.del(i->a);
                        MusEGlobal::keymap.delKey(i->a);
                        updateFlags |= SC_KEY;
                        break;
                  case UndoOp::DeleteKey:
                        ///sigmap.add(i->a, i->b, i->c);
                        MusEGlobal::keymap.addKey(i->a, (key_enum)i->b);
                        updateFlags |= SC_KEY;
                        break;
                  case UndoOp::ModifySongLen:
                        _len=i->b;
                        updateFlags = -1; // set all flags
                        break;
                  case UndoOp::ModifyClip:
                  case UndoOp::ModifyMarker:
                  case UndoOp::DoNothing:
                        break;
                  default:      
                        break;
                  }
            }
      }

//---------------------------------------------------------
//   Song::doRedo2
//---------------------------------------------------------

void Song::doRedo2()
      {
      Undo& u = redoList->back();
      for (iUndoOp i = u.begin(); i != u.end(); ++i) {
            switch(i->type) {
                  case UndoOp::AddTrack:
                        insertTrack2(i->oTrack, i->trackno);
                        // Added by T356.
                        chainTrackParts(i->oTrack, true);
                        
                        updateFlags |= SC_TRACK_INSERTED;
                        break;
                  case UndoOp::DeleteTrack:
                        removeTrack2(i->oTrack);
                        updateFlags |= SC_TRACK_REMOVED;
                        break;
                  case UndoOp::ModifyTrack:
                        {
                        // Unchain the track parts, but don't touch the ref counts.
                        unchainTrackParts(i->nTrack, false);
                        
                        //Track* track = i->nTrack->clone();
                        Track* track = i->nTrack->clone(false);
                        
                        *(i->nTrack) = *(i->oTrack);

                        // Prevent delete i->oTrack from crashing.
                        switch(i->oTrack->type())
                        {
                              case Track::AUDIO_OUTPUT:
                                      {
                                      AudioOutput* ao = (AudioOutput*)i->oTrack;
                                      for(int ch = 0; ch < ao->channels(); ++ch)
                                        ao->setJackPort(ch, 0);
                                      }
                                    break;
                              case Track::AUDIO_INPUT:
                                      {
                                      AudioInput* ai = (AudioInput*)i->oTrack;
                                      for(int ch = 0; ch < ai->channels(); ++ch)
                                        ai->setJackPort(ch, 0);
                                      }
                                    break;
                              default:
                                    break;
                        }
                        if(!i->oTrack->isMidiTrack())
                          ((AudioTrack*)i->oTrack)->clearEfxList();

                        delete i->oTrack;
                        i->oTrack = track;

                        // Chain the track parts, but don't touch the ref counts.
                        chainTrackParts(i->nTrack, false);

                        // Connect and register ports.
                        switch(i->nTrack->type())
                        {
                          case Track::AUDIO_OUTPUT:
                              {
                              AudioOutput* ao = (AudioOutput*)i->nTrack;
                              ao->setName(ao->name());
                              }
                            break;
                          case Track::AUDIO_INPUT:
                              {
                              AudioInput* ai = (AudioInput*)i->nTrack;
                              ai->setName(ai->name());
                              }
                            break;
                          default:
                            break;
                        }

                        // Update solo states, since the user may have changed soloing on other tracks.
                        updateSoloStates();

                        updateFlags |= SC_TRACK_MODIFIED;
                        }
                        break;
                  
                        /*
                        // Prevent delete i->oTrack from crashing.
                        switch(i->oTrack->type())
                        {
                              case Track::AUDIO_OUTPUT:
                                      {
                                      AudioOutput* ao = (AudioOutput*)i->oTrack;
                                      for(int ch = 0; ch < ao->channels(); ++ch)
                                        ao->setJackPort(ch, 0);
                                      }
                                    break;
                              case Track::AUDIO_INPUT:
                                      {
                                      AudioInput* ai = (AudioInput*)i->oTrack;
                                      for(int ch = 0; ch < ai->channels(); ++ch)
                                        ai->setJackPort(ch, 0);
                                      }
                                    break;
                              default:
                                    break;
                        }
                        if(!i->oTrack->isMidiTrack())
                          ((AudioTrack*)i->oTrack)->clearEfxList();

                        //delete i->oTrack;
                        //i->oTrack = track;

                        // Remove the track. removeTrack2 takes care of unchaining the old track.
                        removeTrack2(i->oTrack);
                        
                        // Connect and register ports.
                        switch(i->nTrack->type())
                        {
                          case Track::AUDIO_OUTPUT:
                              {
                              AudioOutput* ao = (AudioOutput*)i->nTrack;
                              ao->setName(ao->name());
                              }
                            break;
                          case Track::AUDIO_INPUT:
                              {
                              AudioInput* ai = (AudioInput*)i->nTrack;
                              ai->setName(ai->name());
                              }
                            break;
                          default:
                            break;
                        }

                        // Insert the new track.
                        insertTrack2(i->nTrack, i->trackno);
                        // Chain the new track parts. (removeTrack2, above, takes care of unchaining the old track).
                        chainTrackParts(i->nTrack, true);
                        
                        // Update solo states, since the user may have changed soloing on other tracks.
                        updateSoloStates();

                        updateFlags |= SC_TRACK_MODIFIED;
                        }
                        break;
                        */
                  
                  case UndoOp::SwapTrack:
                        {
                        Track* track  = _tracks[i->a];
                        _tracks[i->a] = _tracks[i->b];
                        _tracks[i->b] = track;
                        updateFlags |= SC_TRACK_MODIFIED;
                        }
                        break;
                  case UndoOp::AddPart:
                        addPart(i->oPart);
                        updateFlags |= SC_PART_INSERTED;
                        i->oPart->events()->incARef(1);
                        //i->oPart->chainClone();
                        chainClone(i->oPart);
                        break;
                  case UndoOp::DeletePart:
                        removePart(i->oPart);
                        updateFlags |= SC_PART_REMOVED;
                        i->oPart->events()->incARef(-1);
                        //i->oPart->unchainClone();
                        unchainClone(i->oPart);
                        break;
                  case UndoOp::ModifyPart:
                        if(i->doCtrls)
                          removePortCtrlEvents(i->nPart, i->doClones);
                        changePart(i->nPart, i->oPart);
                        i->oPart->events()->incARef(1);
                        i->nPart->events()->incARef(-1);
                        //i->nPart->replaceClone(i->oPart);
                        replaceClone(i->nPart, i->oPart);
                        if(i->doCtrls)
                          addPortCtrlEvents(i->oPart, i->doClones);
                        updateFlags |= SC_PART_MODIFIED;
                        break;
                  case UndoOp::AddEvent:
                        addEvent(i->nEvent, i->part);
                        if(i->doCtrls)
                          addPortCtrlEvents(i->nEvent, i->part, i->doClones);
                        updateFlags |= SC_EVENT_INSERTED;
                        break;
                  case UndoOp::DeleteEvent:
                        if(i->doCtrls)
                          removePortCtrlEvents(i->nEvent, i->part, i->doClones);
                        deleteEvent(i->nEvent, i->part);
                        updateFlags |= SC_EVENT_REMOVED;
                        break;
                  case UndoOp::ModifyEvent:
                        if(i->doCtrls)
                          removePortCtrlEvents(i->nEvent, i->part, i->doClones);
                        changeEvent(i->nEvent, i->oEvent, i->part);
                        if(i->doCtrls)
                          addPortCtrlEvents(i->oEvent, i->part, i->doClones);
                        updateFlags |= SC_EVENT_MODIFIED;
                        break;
                  case UndoOp::AddTempo:
                        //printf("doRedo2: UndoOp::AddTempo. adding tempo at: %d with tempo=%d\n", i->a, i->b);
                        MusEGlobal::tempomap.addTempo(i->a, i->b);
                        updateFlags |= SC_TEMPO;
                        break;
                  case UndoOp::DeleteTempo:
                        //printf("doRedo2: UndoOp::DeleteTempo. deleting tempo at: %d with tempo=%d\n", i->a, i->b);
                        MusEGlobal::tempomap.delTempo(i->a);
                        updateFlags |= SC_TEMPO;
                        break;
                  case UndoOp::AddSig:
                        ///sigmap.add(i->a, i->b, i->c);
                        AL::sigmap.add(i->a, AL::TimeSignature(i->b, i->c));
                        updateFlags |= SC_SIG;
                        break;
                  case UndoOp::DeleteSig:
                        ///sigmap.del(i->a);
                        AL::sigmap.del(i->a);
                        updateFlags |= SC_SIG;
                        break;
                  case UndoOp::AddKey:
                        MusEGlobal::keymap.addKey(i->a, (key_enum)i->b);
                        updateFlags |= SC_KEY;
                        break;
                  case UndoOp::DeleteKey:
                        MusEGlobal::keymap.delKey(i->a);
                        updateFlags |= SC_KEY;
                        break;
                  case UndoOp::ModifySongLen:
                        _len=i->a;
                        updateFlags = -1; // set all flags
                        break;
                  case UndoOp::ModifyClip:
                  case UndoOp::ModifyMarker:
                  case UndoOp::DoNothing:
                        break;
                  default:      
                        break;
                  }
            }
      }

UndoOp::UndoOp()
{
  type=UndoOp::DoNothing;
}

UndoOp::UndoOp(UndoType type_)
{
	type = type_;
}

UndoOp::UndoOp(UndoType type_, int a_, int b_, int c_)
      {
      type = type_;
      a  = a_;
      b  = b_;
      c  = c_;
      }

UndoOp::UndoOp(UndoType type_, int n, Track* oldTrack, Track* newTrack)
      {
      type    = type_;
      trackno = n;
      oTrack  = oldTrack;
      nTrack  = newTrack;
      }

UndoOp::UndoOp(UndoType type_, int n, Track* track)
      {
      type    = type_;
      trackno = n;
      oTrack  = track;
      }

UndoOp::UndoOp(UndoType type_, Part* part)
      {
      type  = type_;
      oPart = part;
      }

UndoOp::UndoOp(UndoType type_, Event& oev, Event& nev, Part* part_, bool doCtrls_, bool doClones_)
      {
      type   = type_;
      nEvent = nev;
      oEvent = oev;
      part   = part_;
      doCtrls = doCtrls_;
      doClones = doClones_;
      }

UndoOp::UndoOp(UndoType type_, Event& nev, Part* part_, bool doCtrls_, bool doClones_)
      {
      type   = type_;
      nEvent = nev;
      part   = part_;
      doCtrls = doCtrls_;
      doClones = doClones_;
      }

UndoOp::UndoOp(UndoType type_, Part* oPart_, Part* nPart_, bool doCtrls_, bool doClones_)
      {
      type  = type_;
      oPart = nPart_;
      nPart = oPart_;
      doCtrls = doCtrls_;
      doClones = doClones_;
      }

UndoOp::UndoOp(UndoType type_, int c, int ctrl_, int ov, int nv)
      {
      type    = type_;
      channel = c;
      ctrl    = ctrl_;
      oVal    = ov;
      nVal    = nv;
      }

UndoOp::UndoOp(UndoType type_, SigEvent* oevent, SigEvent* nevent)
      {
      type       = type_;
      oSignature = oevent;
      nSignature = nevent;
      }
UndoOp::UndoOp(UndoType type_, Marker* copyMarker_, Marker* realMarker_)
      {
      type    = type_;
      realMarker  = realMarker_;
      copyMarker  = copyMarker_;
      }

UndoOp::UndoOp(UndoType type_, const char* changedFile, const char* changeData, int startframe_, int endframe_)
      {
      type = type_;
      filename   = changedFile;
      tmpwavfile = changeData;
      startframe = startframe_;
      endframe   = endframe_;
      }

UndoOp::UndoOp(UndoOp::UndoType type_, Track* track, const char* old_name, const char* new_name)
{
  type = type_;
  _track = track;
  _oldName = new char[strlen(old_name) + 1];
  _newName = new char[strlen(new_name) + 1];
  strcpy(_oldName, old_name);
  strcpy(_newName, new_name);
}

UndoOp::UndoOp(UndoOp::UndoType type_, Track* track, int old_chan, int new_chan)
{
  type = type_;
  _track = track;
  a = old_chan;
  b = new_chan;
}

void Song::undoOp(UndoOp::UndoType type, const char* changedFile, const char* changeData, int startframe, int endframe)
      {
      addUndo(UndoOp(type,changedFile,changeData,startframe,endframe));
      temporaryWavFiles.push_back(QString(changeData));
      }

//---------------------------------------------------------
//   addUndo
//---------------------------------------------------------

void Song::addUndo(UndoOp i)
      {
      if (!undoMode) {
            printf("internal error: undoOp without startUndo()\n");
            return;
            }
      undoList->back().push_back(i);
      dirty = true;
      }

//---------------------------------------------------------
//   doUndo1
//    non realtime context
//    return true if nothing to do
//---------------------------------------------------------

bool Song::doUndo1()
      {
      if (undoList->empty())
            return true;
      Undo& u = undoList->back();
      for (riUndoOp i = u.rbegin(); i != u.rend(); ++i) {
            switch(i->type) {
                  case UndoOp::AddTrack:
                        removeTrack1(i->oTrack);
                        break;
                  case UndoOp::DeleteTrack:
                        insertTrack1(i->oTrack, i->trackno);

                        // FIXME: Would like to put this part in Undo2, but indications
                        //  elsewhere are that (dis)connecting jack routes must not be
                        //  done in the realtime thread. The result is that we get a few
                        //  "PANIC Process init: No buffer from audio device" messages
                        //  before the routes are (dis)connected. So far seems to do no harm though...
                        switch(i->oTrack->type())
                        {
                              case Track::AUDIO_OUTPUT:
                              case Track::AUDIO_INPUT:
                                      connectJackRoutes((AudioTrack*)i->oTrack, false);
                                    break;
                              //case Track::AUDIO_SOFTSYNTH:
                                      //SynthI* si = (SynthI*)i->oTrack;
                                      //si->synth()->init(
                              //      break;
                              default:
                                    break;
                        }

                        break;
                  case UndoOp::ModifyTrackName:
                          i->_track->setName(i->_oldName);
                          updateFlags |= SC_TRACK_MODIFIED;
                        break;
                  case UndoOp::ModifyClip:
                        MusECore::SndFile::applyUndoFile(i->filename, i->tmpwavfile, i->startframe, i->endframe);
                        break;
                  case UndoOp::ModifyTrackChannel:
                        if (i->_track->isMidiTrack()) 
                        {
                          MusECore::MidiTrack* mt = dynamic_cast<MusECore::MidiTrack*>(i->_track);
                          if (mt == 0 || mt->type() == MusECore::Track::DRUM)
                          //if (mt == 0 || mt->isDrumTrack())  // For Flo later with new drum tracks.  p4.0.46 Tim
                            break;
                          if (i->a != mt->outChannel()) 
                          {
                                //mt->setOutChannel(i->a);
                                MusEGlobal::audio->msgIdle(true);
                                //MusEGlobal::audio->msgSetTrackOutChannel(mt, i->a);
                                mt->setOutChanAndUpdate(i->a);
                                MusEGlobal::audio->msgIdle(false);
                                /* --- I really don't like this, you can mess up the whole map "as easy as dell" Taken from tlist.cpp
                                if (mt->type() == MusECore::MidiTrack::DRUM) {//Change channel on all drum instruments
                                      for (int i=0; i<DRUM_MAPSIZE; i++)
                                            MusEGlobal::drumMap[i].channel = i->a;
                                      }*/
                                //updateFlags |= SC_CHANNELS;
                                MusEGlobal::audio->msgUpdateSoloStates();                   
                                //updateFlags |= SC_MIDI_TRACK_PROP | SC_ROUTE;  
                                updateFlags |= SC_MIDI_TRACK_PROP;               
                          }
                        }
                        else
                        {
                            if(i->_track->type() != MusECore::Track::AUDIO_SOFTSYNTH)
                            {
                              MusECore::AudioTrack* at = dynamic_cast<MusECore::AudioTrack*>(i->_track);
                              if (at == 0)
                                break;
                              if (i->a != at->channels()) {
                                    MusEGlobal::audio->msgSetChannels(at, i->a);
                                    updateFlags |= SC_CHANNELS;
                                    }
                            }         
                        }      
                        break;

                  default:
                        break;
                  }
            }
      return false;
      }

//---------------------------------------------------------
//   doUndo3
//    non realtime context
//---------------------------------------------------------

void Song::doUndo3()
      {
      Undo& u = undoList->back();
      for (riUndoOp i = u.rbegin(); i != u.rend(); ++i) {
            switch(i->type) {
                  case UndoOp::AddTrack:
                        removeTrack3(i->oTrack);
                        break;
                  case UndoOp::DeleteTrack:
                        insertTrack3(i->oTrack, i->trackno);
                        break;
                  case UndoOp::ModifyTrack:
                        // Not much choice but to do this - Tim.
                        //clearClipboardAndCloneList();
                        break;      
                  case UndoOp::ModifyMarker:
                        {
                          //printf("performing undo for one marker at copy %d real %d\n", i->copyMarker, i->realMarker);
                          if (i->realMarker) {
                            Marker tmpMarker = *i->realMarker;
                            *i->realMarker = *i->copyMarker; // swap them
                            *i->copyMarker = tmpMarker;
                          }
                          else {
                            //printf("flipping marker\n");
                            i->realMarker = _markerList->add(*i->copyMarker);
                            delete i->copyMarker;
                            i->copyMarker = 0;
                          }
                        }
                        break;
                  default:
                        break;
                  }
            }
      redoList->push_back(u); // put item on redo list
      undoList->pop_back();
      dirty = true;
      }

//---------------------------------------------------------
//   doRedo1
//    non realtime context
//    return true if nothing to do
//---------------------------------------------------------

bool Song::doRedo1()
      {
      if (redoList->empty())
            return true;
      Undo& u = redoList->back();
      for (iUndoOp i = u.begin(); i != u.end(); ++i) {
            switch(i->type) {
                  case UndoOp::AddTrack:
                        insertTrack1(i->oTrack, i->trackno);

                        // FIXME: See comments in Undo1.
                        switch(i->oTrack->type())
                        {
                              case Track::AUDIO_OUTPUT:
                              case Track::AUDIO_INPUT:
                                      connectJackRoutes((AudioTrack*)i->oTrack, false);
                                    break;
                              //case Track::AUDIO_SOFTSYNTH:
                                      //SynthI* si = (SynthI*)i->oTrack;
                                      //si->synth()->init(
                              //      break;
                              default:
                                    break;
                        }

                        break;
                  case UndoOp::DeleteTrack:
                        removeTrack1(i->oTrack);
                        break;
                  case UndoOp::ModifyTrackName:
                          i->_track->setName(i->_newName);
                          updateFlags |= SC_TRACK_MODIFIED;
                        break;
                  case UndoOp::ModifyClip:
                        MusECore::SndFile::applyUndoFile(i->filename, i->tmpwavfile, i->startframe, i->endframe);
                        break;
                  case UndoOp::ModifyTrackChannel:
                        if (i->_track->isMidiTrack()) 
                        {
                          MusECore::MidiTrack* mt = dynamic_cast<MusECore::MidiTrack*>(i->_track);
                          if (mt == 0 || mt->type() == MusECore::Track::DRUM)
                          //if (mt == 0 || mt->isDrumTrack())  // For Flo later with new drum tracks.  p4.0.46 Tim
                            break;
                          if (i->b != mt->outChannel()) 
                          {
                                //mt->setOutChannel(i->b);
                                MusEGlobal::audio->msgIdle(true);
                                //MusEGlobal::audio->msgSetTrackOutChannel(mt, i->b);
                                mt->setOutChanAndUpdate(i->b);
                                MusEGlobal::audio->msgIdle(false);
                                /* --- I really don't like this, you can mess up the whole map "as easy as dell" Taken from tlist.cpp
                                if (mt->type() == MusECore::MidiTrack::DRUM) {//Change channel on all drum instruments
                                      for (int i=0; i<DRUM_MAPSIZE; i++)
                                            MusEGlobal::drumMap[i].channel = i->b;
                                      }*/
                                //updateFlags |= SC_CHANNELS;
                                MusEGlobal::audio->msgUpdateSoloStates();                   
                                //updateFlags |= SC_MIDI_TRACK_PROP | SC_ROUTE;  
                                updateFlags |= SC_MIDI_TRACK_PROP;               
                          }
                        }
                        else
                        {
                            if(i->_track->type() != MusECore::Track::AUDIO_SOFTSYNTH)
                            {
                              MusECore::AudioTrack* at = dynamic_cast<MusECore::AudioTrack*>(i->_track);
                              if (at == 0)
                                break;
                              if (i->b != at->channels()) {
                                    MusEGlobal::audio->msgSetChannels(at, i->b);
                                    updateFlags |= SC_CHANNELS;
                                    }
                            }         
                        }      
                        break;
                        
                  default:
                        break;
                  }
            }
      return false;
      }

//---------------------------------------------------------
//   doRedo3
//    non realtime context
//---------------------------------------------------------

void Song::doRedo3()
      {
      Undo& u = redoList->back();
      for (iUndoOp i = u.begin(); i != u.end(); ++i) {
            switch(i->type) {
                  case UndoOp::AddTrack:
                        insertTrack3(i->oTrack, i->trackno);
                        break;
                  case UndoOp::DeleteTrack:
                        removeTrack3(i->oTrack);
                        break;
                  case UndoOp::ModifyTrack:
                        // Not much choice but to do this - Tim.
                        //clearClipboardAndCloneList();
                        break;      
                  case UndoOp::ModifyMarker:
                        {
                          //printf("performing redo for one marker at copy %d real %d\n", i->copyMarker, i->realMarker);
                          if (i->copyMarker) {
                            Marker tmpMarker = *i->realMarker;
                            *i->realMarker = *i->copyMarker; // swap them
                            *i->copyMarker = tmpMarker;
                          } else {
                            i->copyMarker = new Marker(*i->realMarker);
                            _markerList->remove(i->realMarker);
                            i->realMarker = 0;
                          }
                        }
                        break;
                   default:
                        break;
                  }
            }
      undoList->push_back(u); // put item on undo list
      redoList->pop_back();
      dirty = true;
      }


bool Undo::empty() const
{
  if (std::list<UndoOp>::empty()) return true;
  
  for (const_iterator it=begin(); it!=end(); it++)
    if (it->type!=UndoOp::DoNothing)
      return false;
  
  return true;
}

} // namespace MusECore
