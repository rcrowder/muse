//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: sync.cpp,v 1.6.2.12 2009/06/20 22:20:41 terminator356 Exp $
//
//  (C) Copyright 2003 Werner Schweer (ws@seh.de)
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

#include <cmath>
#include "sync.h"
#include "song.h"
#include "utils.h"
#include "midiport.h"
#include "mididev.h"
#include "globals.h"
#include "midiseq.h"
#include "audio.h"
#include "audiodev.h"
//#include "driver/audiodev.h"  // p4.0.2
#include "gconfig.h"
#include "xml.h"
#include "midi.h"

namespace MusEGlobal {

//int rxSyncPort = -1;         // receive from all ports
//int txSyncPort = 1;
//int rxDeviceId = 0x7f;       // any device
//int txDeviceId = 0x7f;       // any device
//MidiSyncPort midiSyncPorts[MIDI_PORTS];
int volatile curMidiSyncInPort = -1;

bool debugSync = false;

int mtcType     = 1;
MusECore::MTC mtcOffset;
MusECore::BValue extSyncFlag(0, "extSync");       // false - MASTER, true - SLAVE
//bool genMTCSync = false;      // output MTC Sync
//bool genMCSync  = false;      // output MidiClock Sync
//bool genMMC     = false;      // output Midi Machine Control
//bool acceptMTC  = false;
//bool acceptMC   = true;
//bool acceptMMC  = true;
MusECore::BValue useJackTransport(0,"useJackTransport");
bool volatile jackTransportMaster = true;

static MusECore::MTC mtcCurTime;
static int mtcState;    // 0-7 next expected quarter message
static bool mtcValid;
static int mtcLost;
static bool mtcSync;    // receive complete mtc frame?

// p3.3.28
static bool playPendingFirstClock = false;
unsigned int syncSendFirstClockDelay = 1; // In milliseconds.
//static int lastStoppedBeat = 0;
static unsigned int curExtMidiSyncTick = 0;
unsigned int volatile lastExtMidiSyncTick = 0;
double volatile curExtMidiSyncTime = 0.0;
double volatile lastExtMidiSyncTime = 0.0;

// Not used yet.
// static bool mcStart = false;
// static int mcStartTick;

// p3.3.25
// From the "Introduction to the Volatile Keyword" at Embedded dot com
/* A variable should be declared volatile whenever its value could change unexpectedly. 
 ... <such as> global variables within a multi-threaded application    
 ... So all shared global variables should be declared volatile */
unsigned int volatile midiExtSyncTicks = 0;

} // namespace MusEGlobal

namespace MusECore {

//---------------------------------------------------------
//  MidiSyncInfo
//---------------------------------------------------------

MidiSyncInfo::MidiSyncInfo()
{
  _port          = -1;
  _idOut         = 127;
  _idIn          = 127;
  _sendMC        = false;
  _sendMRT       = false;
  _sendMMC       = false;
  _sendMTC       = false;
  _recMC         = false;
  _recMRT        = false;
  _recMMC        = false;
  _recMTC        = false;
  
  _lastClkTime   = 0.0;
  _lastTickTime  = 0.0;
  _lastMRTTime   = 0.0;
  _lastMMCTime   = 0.0;
  _lastMTCTime   = 0.0;
  _clockTrig     = false;
  _tickTrig      = false;
  _MRTTrig       = false;
  _MMCTrig       = false;
  _MTCTrig       = false;
  _clockDetect   = false;
  _tickDetect    = false;
  _MRTDetect     = false;
  _MMCDetect     = false;
  _MTCDetect     = false;
  _recMTCtype    = 0;
  _recRewOnStart  = true;
  //_sendContNotStart = false;
  _actDetectBits = 0;
  for(int i = 0; i < MIDI_CHANNELS; ++i)
  {
    _lastActTime[i] = 0.0;
    _actTrig[i]     = false;
    _actDetect[i]   = false;
  }
}

//---------------------------------------------------------
//   operator =
//---------------------------------------------------------

MidiSyncInfo& MidiSyncInfo::operator=(const MidiSyncInfo &sp)
{
  //_port          = sp._port;
  
  copyParams(sp);
  
  _lastClkTime   = sp._lastClkTime;
  _lastTickTime  = sp._lastTickTime;
  _lastMRTTime   = sp._lastMRTTime;
  _lastMMCTime   = sp._lastMMCTime;
  _lastMTCTime   = sp._lastMTCTime;
  _clockTrig     = sp._clockTrig;
  _tickTrig      = sp._tickTrig;
  _MRTTrig       = sp._MRTTrig;
  _MMCTrig       = sp._MMCTrig;
  _MTCTrig       = sp._MTCTrig;
  _clockDetect   = sp._clockDetect;
  _tickDetect    = sp._tickDetect;
  _MRTDetect     = sp._MRTDetect;
  _MMCDetect     = sp._MMCDetect;
  _MTCDetect     = sp._MTCDetect;
  _recMTCtype    = sp._recMTCtype;
  for(int i = 0; i < MIDI_CHANNELS; ++i)
  {
    _lastActTime[i] = sp._lastActTime[i];
    _actTrig[i]     = sp._actTrig[i];
    _actDetect[i]   = sp._actDetect[i];
  }
  return *this;
}

//---------------------------------------------------------
//   copyParams
//---------------------------------------------------------

MidiSyncInfo& MidiSyncInfo::copyParams(const MidiSyncInfo &sp)
{
  //_port          = sp._port;
  
  _idOut         = sp._idOut;
  _idIn          = sp._idIn;
  _sendMC        = sp._sendMC;
  _sendMRT       = sp._sendMRT;
  _sendMMC       = sp._sendMMC;
  _sendMTC       = sp._sendMTC;
  setMCIn(sp._recMC);
  _recMRT        = sp._recMRT;
  _recMMC        = sp._recMMC;
  _recMTC        = sp._recMTC;
  _recRewOnStart = sp._recRewOnStart;
  //_sendContNotStart = sp._sendContNotStart;
  return *this;
}

//---------------------------------------------------------
//  setTime
//---------------------------------------------------------

void MidiSyncInfo::setTime() 
{ 
  // Note: CurTime() makes a system call to gettimeofday(), 
  //  which apparently can be slow in some cases. So I avoid calling this function
  //  too frequently by calling it (at the heartbeat rate) in Song::beat().  T356
  double t = curTime();
  
  if(_clockTrig)
  {
    _clockTrig = false;
    _lastClkTime = t;  
  }
  else
  if(_clockDetect && (t - _lastClkTime >= 1.0)) // Set detect indicator timeout to about 1 second.
  {
    _clockDetect = false;
    // Give up the current midi sync in port number if we took it...
    if(MusEGlobal::curMidiSyncInPort == _port)
      MusEGlobal::curMidiSyncInPort = -1;
  }
  
  if(_tickTrig)
  {
    _tickTrig = false;
    _lastTickTime = t;  
  }
  else
  if(_tickDetect && (t - _lastTickTime) >= 1.0) // Set detect indicator timeout to about 1 second.
    _tickDetect = false;
    
  if(_MRTTrig)
  {
    _MRTTrig = false;
    _lastMRTTime = t;  
  }
  else
  if(_MRTDetect && (t - _lastMRTTime) >= 1.0) // Set detect indicator timeout to about 1 second.
  {
    _MRTDetect = false;
    // Give up the current midi sync in port number if we took it...
    //if(MusEGlobal::curMidiSyncInPort == _port)
    //  MusEGlobal::curMidiSyncInPort = -1;
  }
    
  if(_MMCTrig)
  {
    _MMCTrig = false;
    _lastMMCTime = t;  
  }
  else
  if(_MMCDetect && (t - _lastMMCTime) >= 1.0) // Set detect indicator timeout to about 1 second.
  {
    _MMCDetect = false;
    // Give up the current midi sync in port number if we took it...
    //if(MusEGlobal::curMidiSyncInPort == _port)
    //  MusEGlobal::curMidiSyncInPort = -1;
  }
    
  if(_MTCTrig)
  {
    _MTCTrig = false;
    _lastMTCTime = t;  
  }
  else
  if(_MTCDetect && (t - _lastMTCTime) >= 1.0) // Set detect indicator timeout to about 1 second.
  {
    _MTCDetect = false;
    // Give up the current midi sync in port number if we took it...
    if(MusEGlobal::curMidiSyncInPort == _port)
      MusEGlobal::curMidiSyncInPort = -1;
  }
    
  for(int i = 0; i < MIDI_CHANNELS; i++)
  {
    if(_actTrig[i])
    {
      _actTrig[i] = false;
      _lastActTime[i] = t;  
    }
    else
    if(_actDetect[i] && (t - _lastActTime[i]) >= 1.0) // Set detect indicator timeout to about 1 second.
    {
      _actDetect[i] = false;
      //_actDetectBits &= ~bitShiftLU[i];
      _actDetectBits &= ~(1 << i);
    }  
  }
}

//---------------------------------------------------------
//  setMCIn
//---------------------------------------------------------

void MidiSyncInfo::setMCIn(const bool v) 
{ 
  _recMC = v; 
  // If sync receive was turned off, clear the current midi sync in port number so another port can grab it.
  if(!_recMC && _port != -1 && MusEGlobal::curMidiSyncInPort == _port)
    MusEGlobal::curMidiSyncInPort = -1;
}

//---------------------------------------------------------
//  setMRTIn
//---------------------------------------------------------

void MidiSyncInfo::setMRTIn(const bool v) 
{ 
  _recMRT = v; 
  // If sync receive was turned off, clear the current midi sync in port number so another port can grab it.
  //if(!_recMRT && _port != -1 && MusEGlobal::curMidiSyncInPort == _port)
  //  MusEGlobal::curMidiSyncInPort = -1;
}

//---------------------------------------------------------
//  setMMCIn
//---------------------------------------------------------

void MidiSyncInfo::setMMCIn(const bool v) 
{ 
  _recMMC = v; 
  // If sync receive was turned off, clear the current midi sync in port number so another port can grab it.
  //if(!_recMMC && _port != -1 && MusEGlobal::curMidiSyncInPort == _port)
  //  MusEGlobal::curMidiSyncInPort = -1;
}

//---------------------------------------------------------
//  setMTCIn
//---------------------------------------------------------

void MidiSyncInfo::setMTCIn(const bool v) 
{ 
  _recMTC = v; 
  // If sync receive was turned off, clear the current midi sync in port number so another port can grab it.
  if(!_recMTC && _port != -1 && MusEGlobal::curMidiSyncInPort == _port)
    MusEGlobal::curMidiSyncInPort = -1;
}

//---------------------------------------------------------
//  trigMCSyncDetect
//---------------------------------------------------------

void MidiSyncInfo::trigMCSyncDetect() 
{ 
  _clockDetect = true;
  _clockTrig = true;
  // Set the current midi sync in port number if it's not taken...
  if(_recMC && MusEGlobal::curMidiSyncInPort == -1)
    MusEGlobal::curMidiSyncInPort = _port;
}

//---------------------------------------------------------
//  trigTickDetect
//---------------------------------------------------------

void MidiSyncInfo::trigTickDetect()   
{ 
  _tickDetect = true;
  _tickTrig = true;
}
    
//---------------------------------------------------------
//  trigMRTDetect
//---------------------------------------------------------

void MidiSyncInfo::trigMRTDetect()   
{ 
  _MRTDetect = true;
  _MRTTrig = true;
  // Set the current midi sync in port number if it's not taken...
  //if(_recMRT && MusEGlobal::curMidiSyncInPort == -1)
  //  MusEGlobal::curMidiSyncInPort = _port;
}
    
//---------------------------------------------------------
//  trigMMCDetect
//---------------------------------------------------------

void MidiSyncInfo::trigMMCDetect()   
{ 
  _MMCDetect = true;
  _MMCTrig = true;
  // Set the current midi sync in port number if it's not taken...
  //if(_recMMC && MusEGlobal::curMidiSyncInPort == -1)
  //  MusEGlobal::curMidiSyncInPort = _port;
}
    
//---------------------------------------------------------
//  trigMTCDetect
//---------------------------------------------------------

void MidiSyncInfo::trigMTCDetect()   
{ 
  _MTCDetect = true;
  _MTCTrig = true;
  // Set the current midi sync in port number if it's not taken...
  if(_recMTC && MusEGlobal::curMidiSyncInPort == -1)
    MusEGlobal::curMidiSyncInPort = _port;
}
    
//---------------------------------------------------------
//  actDetect
//---------------------------------------------------------

bool MidiSyncInfo::actDetect(const int ch) const
{ 
  if(ch < 0 || ch >= MIDI_CHANNELS)
    return false;
    
  return _actDetect[ch]; 
}           

//---------------------------------------------------------
//  trigActDetect
//---------------------------------------------------------

void MidiSyncInfo::trigActDetect(const int ch)   
{ 
  if(ch < 0 || ch >= MIDI_CHANNELS)
    return;
    
  //_actDetectBits |= bitShiftLU[ch];
  _actDetectBits |= (1 << ch);
  _actDetect[ch] = true;
  _actTrig[ch] = true;
}
    
//---------------------------------------------------------
//   isDefault
//---------------------------------------------------------

bool MidiSyncInfo::isDefault() const
{
  return(_idOut == 127 && _idIn == 127 && !_sendMC && !_sendMRT && !_sendMMC && !_sendMTC && 
     /* !_sendContNotStart && */ !_recMC && !_recMRT && !_recMMC && !_recMTC && _recRewOnStart);
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void MidiSyncInfo::read(Xml& xml)
      {
      for (;;) {
            Xml::Token token(xml.parse());
            const QString& tag(xml.s1());
            switch (token) {
                  case Xml::Error:
                  case Xml::End:
                        return;
                  case Xml::TagStart:
                             if (tag == "idOut")
                              _idOut = xml.parseInt();
                        else if (tag == "idIn")
                              _idIn = xml.parseInt();
                        else if (tag == "sendMC")
                              _sendMC = xml.parseInt();
                        else if (tag == "sendMRT")
                              _sendMRT = xml.parseInt();
                        else if (tag == "sendMMC")
                              _sendMMC = xml.parseInt();
                        else if (tag == "sendMTC")
                              _sendMTC = xml.parseInt();
                        //else if (tag == "sendContNotStart")
                        //      _sendContNotStart = xml.parseInt();
                        else if (tag == "recMC")
                              _recMC = xml.parseInt();
                        else if (tag == "recMRT")
                              _recMRT = xml.parseInt();
                        else if (tag == "recMMC")
                              _recMMC = xml.parseInt();
                        else if (tag == "recMTC")
                              _recMTC = xml.parseInt();
                        else if (tag == "recRewStart")
                              _recRewOnStart = xml.parseInt();
                        else
                              xml.unknown("midiSyncInfo");
                        break;
                  case Xml::TagEnd:
                        if (tag == "midiSyncInfo")
                            return;
                  default:
                        break;
                  }
            }
      }

//---------------------------------------------------------
//  write
//---------------------------------------------------------

//void MidiSyncInfo::write(int level, Xml& xml, MidiDevice* md)
void MidiSyncInfo::write(int level, Xml& xml)
{
  //if(!md)
  //  return;
  
  // All defaults? Nothing to write.
  //if(_idOut == 127 && _idIn == 127 && !_sendMC && !_sendMRT && !_sendMMC && !_sendMTC && 
  //   /* !_sendContNotStart && */ !_recMC && !_recMRT && !_recMMC && !_recMTC && _recRewOnStart)
  //  return;
  if(isDefault())  
    return;
  
  xml.tag(level++, "midiSyncInfo");
  //xml.intTag(level, "idx", idx);
  //xml.intTag(level++, "midiSyncPort", idx);
  //xml.tag(level++, "midiSyncInfo idx=\"%d\"", idx);
  
  //xml.strTag(level, "device", md->name());
  
  if(_idOut != 127)
    xml.intTag(level, "idOut", _idOut);
  if(_idIn != 127)
    xml.intTag(level, "idIn", _idIn);
  
  if(_sendMC)
    xml.intTag(level, "sendMC", true);
  if(_sendMRT)
    xml.intTag(level, "sendMRT", true);
  if(_sendMMC)
    xml.intTag(level, "sendMMC", true);
  if(_sendMTC)
    xml.intTag(level, "sendMTC", true);
  //if(_sendContNotStart)
  //  xml.intTag(level, "sendContNotStart", true);
  
  if(_recMC)
    xml.intTag(level, "recMC", true);
  if(_recMRT)
    xml.intTag(level, "recMRT", true);
  if(_recMMC)
    xml.intTag(level, "recMMC", true);
  if(_recMTC)
    xml.intTag(level, "recMTC", true);
  if(!_recRewOnStart)
    xml.intTag(level, "recRewStart", false);
  
  xml.etag(level, "midiSyncInfo");
}

//---------------------------------------------------------
//  mmcInput
//    Midi Machine Control Input received
//---------------------------------------------------------

//void MidiSeq::mmcInput(const unsigned char* p, int n)
void MidiSeq::mmcInput(int port, const unsigned char* p, int n)
      {
      if (MusEGlobal::debugSync)
            printf("mmcInput: n:%d %02x %02x %02x %02x\n",
               n, p[2], p[3], p[4], p[5]);
     
      MidiPort* mp = &MusEGlobal::midiPorts[port];
      MidiSyncInfo& msync = mp->syncInfo();
      // Trigger MMC detect in.
      msync.trigMMCDetect();
      // MMC locate SMPTE time code may contain format type bits. Grab them.
      if(p[3] == 0x44 && p[4] == 6 && p[5] == 1)
        msync.setRecMTCtype((p[6] >> 5) & 3);
      
      // MMC in not turned on? Forget it.
      if(!msync.MMCIn())
        return;
      
      //if (!(MusEGlobal::extSyncFlag.value() && acceptMMC))
      //if(!MusEGlobal::extSyncFlag.value())
      //      return;
      
      switch(p[3]) {
            case 1:
                  if (MusEGlobal::debugSync)
                        printf("  MMC: STOP\n");
                  
                  MusEGlobal::playPendingFirstClock = false;
                  
                  //if ((state == PLAY || state == PRECOUNT))
                  if (MusEGlobal::audio->isPlaying())
                        MusEGlobal::audio->msgPlay(false);
                        playStateExt = false;
                        alignAllTicks();
                        //stopPlay();
                  break;
            case 2:
                  if (MusEGlobal::debugSync)
                        printf("  MMC: PLAY\n");
            case 3:
                  if (MusEGlobal::debugSync)
                        printf("  MMC: DEFERRED PLAY\n");
                  MusEGlobal::mtcState = 0;
                  MusEGlobal::mtcValid = false;
                  MusEGlobal::mtcLost  = 0;
                  MusEGlobal::mtcSync  = false;
                  //startPlay();
                  alignAllTicks();
                  MusEGlobal::audio->msgPlay(true);
                  playStateExt = true;
                  break;

            case 4:
                  printf("MMC: FF not implemented\n");
                  MusEGlobal::playPendingFirstClock = false;
                  break;
            case 5:
                  printf("MMC: REWIND not implemented\n");
                  MusEGlobal::playPendingFirstClock = false;
                  break;
            case 6:
                  printf("MMC: REC STROBE not implemented\n");
                  MusEGlobal::playPendingFirstClock = false;
                  break;
            case 7:
                  printf("MMC: REC EXIT not implemented\n");
                  MusEGlobal::playPendingFirstClock = false;
                  break;
            case 0xd:
                  printf("MMC: RESET not implemented\n");
                  MusEGlobal::playPendingFirstClock = false;
                  break;
            case 0x44:
                  if (p[5] == 0) {
                        printf("MMC: LOCATE IF not implemented\n");
                        break;
                        }
                  else if (p[5] == 1) {
                        if (!MusEGlobal::checkAudioDevice()) return;
                        MTC mtc(p[6] & 0x1f, p[7], p[8], p[9], p[10]);
                        int type = (p[6] >> 5) & 3;
                        //int mmcPos = MusEGlobal::tempomap.frame2tick(lrint(mtc.time()*MusEGlobal::sampleRate));
                        //int mmcPos = lrint(mtc.time()*MusEGlobal::sampleRate);
                        int mmcPos = lrint(mtc.time(type) * MusEGlobal::sampleRate);

                        //Pos tp(mmcPos, true);
                        Pos tp(mmcPos, false);
                        //MusEGlobal::audioDevice->seekTransport(tp.frame());
                        MusEGlobal::audioDevice->seekTransport(tp);
                        alignAllTicks();
                        //seek(tp);
                        if (MusEGlobal::debugSync) {
                              //printf("MMC: %f %d seek ", mtc.time(), mmcPos);
                              printf("MMC: LOCATE mtc type:%d time:%lf frame:%d mtc: ", type, mtc.time(), mmcPos);
                              mtc.print();
                              printf("\n");
                              }
                        //write(sigFd, "G", 1);
                        break;
                        }
                  // fall through
            default:
                  printf("MMC %x %x, unknown\n", p[3], p[4]); break;
            }
      }

//---------------------------------------------------------
//   mtcInputQuarter
//    process Quarter Frame Message
//---------------------------------------------------------

//void MidiSeq::mtcInputQuarter(int, unsigned char c)
void MidiSeq::mtcInputQuarter(int port, unsigned char c)
      {
      static int hour, min, sec, frame;

      //printf("MidiSeq::mtcInputQuarter c:%h\n", c);
      
      int valL = c & 0xf;
      int valH = valL << 4;

      int _state = (c & 0x70) >> 4;
      if (MusEGlobal::mtcState != _state)
            MusEGlobal::mtcLost += _state - MusEGlobal::mtcState;
      MusEGlobal::mtcState = _state + 1;

      switch(_state) {
            case 7:
                  hour  = (hour  & 0x0f) | valH;
                  break;
            case 6:
                  hour  = (hour  & 0xf0) | valL;
                  break;
            case 5:
                  min   = (min   & 0x0f) | valH;
                  break;
            case 4:
                  min   = (min   & 0xf0) | valL;
                  break;
            case 3:
                  sec   = (sec   & 0x0f) | valH;
                  break;
            case 2:
                  sec   = (sec   & 0xf0) | valL;
                  break;
            case 1:
                  frame = (frame & 0x0f) | valH;
                  break;
            case 0:  frame = (frame & 0xf0) | valL;
                  break;
            }
      frame &= 0x1f;    // 0-29
      sec   &= 0x3f;    // 0-59
      min   &= 0x3f;    // 0-59
      int tmphour = hour;
      int type = (hour >> 5) & 3;
      hour  &= 0x1f;

      if(MusEGlobal::mtcState == 8) 
      {
            MusEGlobal::mtcValid = (MusEGlobal::mtcLost == 0);
            MusEGlobal::mtcState = 0;
            MusEGlobal::mtcLost  = 0;
            if(MusEGlobal::mtcValid) 
            {
                  MusEGlobal::mtcCurTime.set(hour, min, sec, frame);
                  if(port != -1)
                  {
                    MidiPort* mp = &MusEGlobal::midiPorts[port];
                    MidiSyncInfo& msync = mp->syncInfo();
                    msync.setRecMTCtype(type);
                    msync.trigMTCDetect();
                    // Not for the current in port? External sync not turned on? MTC in not turned on? Forget it.
                    if(port == MusEGlobal::curMidiSyncInPort && MusEGlobal::extSyncFlag.value() && msync.MTCIn()) 
                    {
                      if(MusEGlobal::debugSync)
                        printf("MidiSeq::mtcInputQuarter hour byte:%hx\n", tmphour);
                      mtcSyncMsg(MusEGlobal::mtcCurTime, type, !MusEGlobal::mtcSync);
                    }  
                  }
                  MusEGlobal::mtcSync = true;
            }
      }      
      else if (MusEGlobal::mtcValid && (MusEGlobal::mtcLost == 0)) 
      {
            //MusEGlobal::mtcCurTime.incQuarter();
            MusEGlobal::mtcCurTime.incQuarter(type);
            //MusEGlobal::mtcSyncMsg(MusEGlobal::mtcCurTime, type, false);
      }
    }

//---------------------------------------------------------
//   mtcInputFull
//    process Frame Message
//---------------------------------------------------------

//void MidiSeq::mtcInputFull(const unsigned char* p, int n)
void MidiSeq::mtcInputFull(int port, const unsigned char* p, int n)
      {
      if (MusEGlobal::debugSync)
            printf("mtcInputFull\n");
      //if (!MusEGlobal::extSyncFlag.value())
      //      return;

      if (p[3] != 1) {
            if (p[3] != 2) {   // silently ignore user bits
                  printf("unknown mtc msg subtype 0x%02x\n", p[3]);
                  dump(p, n);
                  }
            return;
            }
      int hour  = p[4];
      int min   = p[5];
      int sec   = p[6];
      int frame = p[7];

      frame &= 0x1f;    // 0-29
      sec   &= 0x3f;    // 0-59
      min   &= 0x3f;    // 0-59
      int type = (hour >> 5) & 3;
      hour &= 0x1f;

      MusEGlobal::mtcCurTime.set(hour, min, sec, frame);
      MusEGlobal::mtcState = 0;
      MusEGlobal::mtcValid = true;
      MusEGlobal::mtcLost  = 0;
      
      // Added by Tim.
      if(MusEGlobal::debugSync)
        printf("mtcInputFull: time:%lf stime:%lf hour byte (all bits):%hx\n", MusEGlobal::mtcCurTime.time(), MusEGlobal::mtcCurTime.time(type), p[4]);
      if(port != -1)
      {
        MidiPort* mp = &MusEGlobal::midiPorts[port];
        MidiSyncInfo& msync = mp->syncInfo();
        msync.setRecMTCtype(type);
        msync.trigMTCDetect();
        // MTC in not turned on? Forget it.
        //if(MusEGlobal::extSyncFlag.value() && msync.MTCIn())
        if(msync.MTCIn())
        {
          //Pos tp(lrint(MusEGlobal::mtcCurTime.time() * MusEGlobal::sampleRate), false);
          Pos tp(lrint(MusEGlobal::mtcCurTime.time(type) * MusEGlobal::sampleRate), false);
          MusEGlobal::audioDevice->seekTransport(tp);
          alignAllTicks();
        }
      }    
    }

//---------------------------------------------------------
//   nonRealtimeSystemSysex
//---------------------------------------------------------

//void MidiSeq::nonRealtimeSystemSysex(const unsigned char* p, int n)
void MidiSeq::nonRealtimeSystemSysex(int /*port*/, const unsigned char* p, int n)
      {
//      int chan = p[2];
      switch(p[3]) {
            case 4:
                  printf("NRT Setup\n");
                  break;
            default:
                  printf("unknown NRT Msg 0x%02x\n", p[3]);
                  dump(p, n);
                  break;
           }
      }

//---------------------------------------------------------
//   setSongPosition
//    MidiBeat is a 14 Bit value. Each MidiBeat spans
//    6 MIDI Clocks. Inother words, each MIDI Beat is a
//    16th note (since there are 24 MIDI Clocks in a
//    quarter note).
//---------------------------------------------------------

void MidiSeq::setSongPosition(int port, int midiBeat)
      {
      if (MusEGlobal::midiInputTrace)
            printf("set song position port:%d %d\n", port, midiBeat);
      
      //MusEGlobal::midiPorts[port].syncInfo().trigMCSyncDetect();
      MusEGlobal::midiPorts[port].syncInfo().trigMRTDetect();
      
      //if (!MusEGlobal::extSyncFlag.value())
      // External sync not on? Clock in not turned on? 
      //if(!MusEGlobal::extSyncFlag.value() || !MusEGlobal::midiPorts[port].syncInfo().MCIn())
      if(!MusEGlobal::extSyncFlag.value() || !MusEGlobal::midiPorts[port].syncInfo().MRTIn())
            return;
            
      // Re-transmit song position to other devices if clock out turned on.
      for(int p = 0; p < MIDI_PORTS; ++p)
        //if(p != port && MusEGlobal::midiPorts[p].syncInfo().MCOut())
        if(p != port && MusEGlobal::midiPorts[p].syncInfo().MRTOut())
          MusEGlobal::midiPorts[p].sendSongpos(midiBeat);
                  
      MusEGlobal::curExtMidiSyncTick = (MusEGlobal::config.division * midiBeat) / 4;
      MusEGlobal::lastExtMidiSyncTick = MusEGlobal::curExtMidiSyncTick;
      
      //Pos pos((MusEGlobal::config.division * midiBeat) / 4, true);
      Pos pos(MusEGlobal::curExtMidiSyncTick, true);
      
      if (!MusEGlobal::checkAudioDevice()) return;

      //MusEGlobal::audioDevice->seekTransport(pos.frame());
      MusEGlobal::audioDevice->seekTransport(pos);
      alignAllTicks(pos.frame());
      if (MusEGlobal::debugSync)
            printf("setSongPosition %d\n", pos.tick());
      }



//---------------------------------------------------------
//   set all runtime variables to the "in sync" value
//---------------------------------------------------------
void MidiSeq::alignAllTicks(int frameOverride)
      {
      //printf("alignAllTicks audioDriver->framePos=%d, audio->pos().frame()=%d\n", 
      //        MusEGlobal::audioDevice->framePos(), audio->pos().frame());
      unsigned curFrame;
      if (!frameOverride)
        curFrame = MusEGlobal::audio->pos().frame();
      else
        curFrame = frameOverride;

      int tempo = MusEGlobal::tempomap.tempo(0);

      // use the last old values to give start values for the tripple buffering
      int recTickSpan = recTick1 - recTick2;
      int songTickSpan = (int)(songtick1 - songtick2);    //prevent compiler warning:  casting double to int
      storedtimediffs = 0; // pretend there is no sync history

      mclock2=mclock1=0.0; // set all clock values to "in sync"

      recTick = (int) ((double(curFrame)/double(MusEGlobal::sampleRate)) *
                        double(MusEGlobal::config.division * 1000000.0) / double(tempo) //prevent compiler warning:  casting double to int
                );
      songtick1 = recTick - songTickSpan;
      if (songtick1 < 0)
        songtick1 = 0;
      songtick2 = songtick1 - songTickSpan;
      if (songtick2 < 0)
        songtick2 = 0;
      recTick1 = recTick - recTickSpan;
      if (recTick1 < 0)
        recTick1 = 0;
      recTick2 = recTick1 - recTickSpan;
      if (recTick2 < 0)
        recTick2 = 0;
      if (MusEGlobal::debugSync)
        printf("alignAllTicks curFrame=%d recTick=%d tempo=%.3f frameOverride=%d\n",curFrame,recTick,(float)((1000000.0 * 60.0)/tempo), frameOverride);

      }

//---------------------------------------------------------
//   realtimeSystemInput
//    real time message received
//---------------------------------------------------------
void MidiSeq::realtimeSystemInput(int port, int c)
      {

      if (MusEGlobal::midiInputTrace)
            printf("realtimeSystemInput port:%d 0x%x\n", port+1, c);

      //if (MusEGlobal::midiInputTrace && (rxSyncPort != port) && rxSyncPort != -1) {
      //      if (MusEGlobal::debugSync)
      //            printf("rxSyncPort configured as %d; received sync from port %d\n",
      //               rxSyncPort, port);
      //      return;
      //      }
      
      MidiPort* mp = &MusEGlobal::midiPorts[port];
      
      // Trigger on any tick, clock, or realtime command. 
      if(c == ME_TICK) // Tick
        mp->syncInfo().trigTickDetect();
      else
      if(c == ME_CLOCK) // Clock
        mp->syncInfo().trigMCSyncDetect();
      else  
        mp->syncInfo().trigMRTDetect(); // Other
       
      // External sync not on? Clock in not turned on? Otherwise realtime in not turned on?
      if(!MusEGlobal::extSyncFlag.value())
        return;
      if(c == ME_CLOCK)
      { 
        if(!mp->syncInfo().MCIn())
          return; 
      }
      else 
      if(!mp->syncInfo().MRTIn())      
        return;
        
        
      switch(c) {
            case ME_CLOCK:  // midi clock (24 ticks / quarter note)
                  {
                  // Not for the current in port? Forget it.
                  if(port != MusEGlobal::curMidiSyncInPort)
                    break;
                  
                  //printf("midi clock:%f\n", curTime());
                  
                  // Re-transmit clock to other devices if clock out turned on.
                  // Must be careful not to allow more than one clock input at a time.
                  // Would re-transmit mixture of multiple clocks - confusing receivers.
                  // Solution: Added MusEGlobal::curMidiSyncInPort. 
                  // Maybe in MidiSeq::processTimerTick(), call sendClock for the other devices, instead of here.
                  for(int p = 0; p < MIDI_PORTS; ++p)
                    if(p != port && MusEGlobal::midiPorts[p].syncInfo().MCOut())
                      MusEGlobal::midiPorts[p].sendClock();
                  
                  // p3.3.28
                  if(MusEGlobal::playPendingFirstClock)
                  {
                    MusEGlobal::playPendingFirstClock = false;
                    // Hopefully the transport will be ready by now, the seek upon start should mean the 
                    //  audio prefetch has already finished or at least started...
                    // Must comfirm that play does not force a complete prefetch again, but don't think so...
                    if(!MusEGlobal::audio->isPlaying())
                      MusEGlobal::audioDevice->startTransport();
                  }
                  //else
                  // This part will be run on the second and subsequent clocks, after start.
                  // Can't check audio state, might not be playing yet, we might miss some increments.
                  //if(MusEGlobal::audio->isPlaying())
                  if(playStateExt)
                  {
                    MusEGlobal::lastExtMidiSyncTime = MusEGlobal::curExtMidiSyncTime;
                    MusEGlobal::curExtMidiSyncTime = curTime();
                    int div = MusEGlobal::config.division/24;
                    MusEGlobal::midiExtSyncTicks += div;
                    MusEGlobal::lastExtMidiSyncTick = MusEGlobal::curExtMidiSyncTick;
                    MusEGlobal::curExtMidiSyncTick += div;
                  }
                  
//BEGIN : Original code:
                  /*
                  double mclock0 = curTime();
                  // Difference in time last 2 rounds:
                  double tdiff0   = mclock0 - mclock1;
                  double tdiff1   = mclock1 - mclock2;
                  double averagetimediff = 0.0;

                  if (mclock1 != 0.0) {
                        if (storedtimediffs < 24)
                        {
                           timediff[storedtimediffs] = mclock0 - mclock1;
                           storedtimediffs++;
                        }
                        else {
                              for (int i=0; i<23; i++) {
                                    timediff[i] = timediff[i+1];
                                    }
                              timediff[23] = mclock0 - mclock1;
                        }
                        // Calculate average timediff:
                        for (int i=0; i < storedtimediffs; i++) {
                              averagetimediff += timediff[i]/storedtimediffs;
                              }
                        }

                  // Compare w audio if playing:
                  if (playStateExt == true ) {  //MusEGlobal::audio->isPlaying()  state == PLAY
                        //BEGIN standard setup:
                        recTick  += MusEGlobal::config.division / 24; // The one we're syncing to
                        int tempo = MusEGlobal::tempomap.tempo(0);
                        unsigned curFrame = MusEGlobal::audio->pos().frame();
                        double songtick = (double(curFrame)/double(MusEGlobal::sampleRate)) *
                                           double(MusEGlobal::config.division * 1000000.0) / double(tempo);

                        double scale = double(tdiff0/averagetimediff);
                        double tickdiff = songtick - ((double) recTick - 24 + scale*24.0);

                        //END standard setup
                        if (MusEGlobal::debugSync) {
                              int m, b, t;
                              MusEGlobal::audio->pos().mbt(&m, &b, &t);

                              int song_beat = b + m*4; // if the time-signature is different than 4/4, this will be wrong.
                              int sync_beat = recTick/MusEGlobal::config.division;
                              printf("pT=%.3f rT=%d diff=%.3f songB=%d syncB=%d scale=%.3f, curFrame=%d", 
                                      songtick, recTick, tickdiff, song_beat, sync_beat, scale, curFrame);
                              }

                        //if ((mclock2 !=0.0) && (tdiff1 > 0.0) && fabs(tickdiff) > 0.5 && lastTempo != 0) {
                        if ((mclock2 !=0.0) && (tdiff1 > 0.0) && lastTempo != 0) {
                              // Interpolate:
                              double tickdiff1 = songtick1 - recTick1;
                              double tickdiff2 = songtick2 - recTick2;
                              double newtickdiff = (tickdiff1+tickdiff2)/250; 
                                                   //tickdiff/5.0  +
                                                   tickdiff1/16.0 +
                                                   tickdiff2/24.0;  //5 mins 30 secs on 116BPM, -p 512 jackd

                              if (newtickdiff != 0.0) {
                                    int newTempo = MusEGlobal::tempomap.tempo(0);
                                    //newTempo += int(24.0 * newtickdiff * scale);
                                    newTempo += int(24.0 * newtickdiff);
                                    if (MusEGlobal::debugSync)
                                          printf(" tdiff=%f ntd=%f lt=%d tmpo=%.3f", 
                                                tdiff0, newtickdiff, lastTempo, (float)((1000000.0 * 60.0)/newTempo));
                                    //syncTempo = newTempo;
                                    MusEGlobal::tempomap.setTempo(0,newTempo);
                                    }
                              if (MusEGlobal::debugSync)
                                    printf("\n");
                              }
                        else if (MusEGlobal::debugSync)
                              printf("\n");

                        //BEGIN post calc
                        lastTempo = tempo;
                        recTick2 = recTick1;
                        recTick1 = recTick;
                        mclock2 = mclock1;
                        mclock1 = mclock0;
                        songtick2 = songtick1;
                        songtick1 = songtick;
                        //END post calc
                        break;
                        } // END state play
                  //
                  // Pre-sync (when audio is not running)
                  // Calculate tempo depending on time per pulse
                  //
                  if (mclock1 == 0.0) {
                        mp->device()->discardInput();
                        if (MusEGlobal::debugSync)
                           printf("Discarding input from port %d\n", port);
                        }
                  if ((mclock2 != 0.0) && (tdiff0 > 0.0)) {
                        int tempo0 = int(24000000.0 * tdiff0 + .5);
                        int tempo1 = int(24000000.0 * tdiff1 + .5);
                        int tempo = MusEGlobal::tempomap.tempo(0);

                        int diff0 = tempo0 - tempo;
                        int diff1 = tempo1 - tempo0;
                        if (diff0) {
                              int newTempo = tempo + diff0/8 + diff1/16;
                              if (MusEGlobal::debugSync)
                                 printf("setting new tempo %d = %f\n", newTempo, (float)((1000000.0 * 60.0)/newTempo));
                              MusEGlobal::tempomap.setTempo(0, newTempo);
                              }
                        }
                  mclock2 = mclock1;
                  mclock1 = mclock0;
                  */
//END : Original Code
                  
//BEGIN : Using external tempo map:
                  /*
                  double mclock0 = curTime();
                  // Difference in time last 2 rounds:
                  double tdiff0   = mclock0 - mclock1;
                  double tdiff1   = mclock1 - mclock2;
                  double averagetimediff = 0.0;

                  if (mclock1 != 0.0) {
                        if (storedtimediffs < 24)
                        {
                           timediff[storedtimediffs] = mclock0 - mclock1;
                           storedtimediffs++;
                        }
                        else {
                              for (int i=0; i<23; i++) {
                                    timediff[i] = timediff[i+1];
                                    }
                              timediff[23] = mclock0 - mclock1;
                        }
                        // Calculate average timediff:
                        for (int i=0; i < storedtimediffs; i++) {
                              averagetimediff += timediff[i]/storedtimediffs;
                              }
                        }

                  // Compare w audio if playing:
                  //if (playStateExt == true ) {  //MusEGlobal::audio->isPlaying()  state == PLAY
                  if (0) {
                        //BEGIN standard setup:
                        recTick  += MusEGlobal::config.division / 24; // The one we're syncing to
                        int tempo = MusEGlobal::tempomap.tempo(0);
                        //unsigned curFrame = MusEGlobal::audio->pos().frame();
                        //double songtick = (double(curFrame)/double(MusEGlobal::sampleRate)) *
                        //                   double(MusEGlobal::config.division * 1000000.0) / double(tempo);
                        double songtick = MusEGlobal::tempomap.curTickExt(mclock0);
                        
                        double scale = double(tdiff0/averagetimediff);
                        double tickdiff = songtick - ((double) recTick - 24 + scale*24.0);

                        //END standard setup
                        if (MusEGlobal::debugSync) {
                              int m, b, t;
                              MusEGlobal::audio->pos().mbt(&m, &b, &t);

                              int song_beat = b + m*4; // if the time-signature is different than 4/4, this will be wrong.
                              int sync_beat = recTick/MusEGlobal::config.division;
                              printf("pT=%.3f rT=%d diff=%.3f songB=%d syncB=%d scale=%.3f, curFrame=%d averagetimediff:%.3lf", 
                                      songtick, recTick, tickdiff, song_beat, sync_beat, scale, MusEGlobal::audio->pos().frame(), averagetimediff);
                              }

                        //if ((mclock2 !=0.0) && (tdiff1 > 0.0) && fabs(tickdiff) > 0.5 && lastTempo != 0) {
                        if ((mclock2 !=0.0) && (tdiff1 > 0.0) && lastTempo != 0) {
                              // Interpolate:
                              double tickdiff1 = songtick1 - recTick1;
                              double tickdiff2 = songtick2 - recTick2;
                              double newtickdiff = (tickdiff1+tickdiff2)/250; 
                              ////double newtickdiff = (tickdiff1+tickdiff2) / 10.0; 
                              //double newtickdiff = tickdiff/5.0  +
                              //                     tickdiff1/16.0 +
                              //                     tickdiff2/24.0;  //5 mins 30 secs on 116BPM, -p 512 jackd

                              if (newtickdiff != 0.0) {
                                    //int newTempo = MusEGlobal::tempomap.tempo(0);
                                    int newTempo = tempo;
                                    //newTempo += int(24.0 * newtickdiff * scale);
                                    newTempo += int(24.0 * newtickdiff);
                                    if (MusEGlobal::debugSync)
                                          printf(" tdiff=%f ntd=%f lt=%d tmpo=%.3f", 
                                                tdiff0, newtickdiff, lastTempo, (float)((1000000.0 * 60.0)/newTempo));
                                    //syncTempo = newTempo;
                                    //MusEGlobal::tempomap.setTempo(0,newTempo);
                                    // Don't set the last stable tempo.
                                    //MusEGlobal::tempomap.setTempo(0, newTempo, false);
                                    MusEGlobal::tempomap.setExtTempo(newTempo);
                                    }
                              if (MusEGlobal::debugSync)
                                    printf("\n");
                              }
                        else if (MusEGlobal::debugSync)
                              printf("\n");

                        //BEGIN post calc
                        lastTempo = tempo;
                        recTick2 = recTick1;
                        recTick1 = recTick;
                        mclock2 = mclock1;
                        mclock1 = mclock0;
                        songtick2 = songtick1;
                        songtick1 = songtick;
                        //END post calc
                        break;
                        } // END state play
                  //
                  // Pre-sync (when audio is not running)
                  // Calculate tempo depending on time per pulse
                  //
                  if (mclock1 == 0.0) {
                        mp->device()->discardInput();
                        if (MusEGlobal::debugSync)
                           printf("Discarding input from port %d\n", port);
                        }
                  if ((mclock2 != 0.0) && (tdiff0 > 0.0)) {
                        
                        //int tempo0 = int(24000000.0 * tdiff0 + .5);
                        //int tempo1 = int(24000000.0 * tdiff1 + .5);
                        //int tempo = MusEGlobal::tempomap.tempo(0);
                        //int diff0 = tempo0 - tempo;
                        //int diff1 = tempo1 - tempo0;
                        
                        //if (diff0) {
                        //      int newTempo = tempo + diff0/8 + diff1/16;
                        //      if (MusEGlobal::debugSync)
                        //         printf("setting new tempo %d = %f\n", newTempo, (float)((1000000.0 * 60.0)/newTempo));
                              //MusEGlobal::tempomap.setTempo(0, newTempo);
                              // Don't set the last stable tempo.
                              //MusEGlobal::tempomap.setTempo(0, newTempo, false);
                        //      MusEGlobal::tempomap.setExtTempo(newTempo);
                        //      }
                        
                        //double tempo0 = 24000000.0 * tdiff0;
                        //double tempo1 = 24000000.0 * tdiff1;
                        //int newTempo = int((tempo0 + tempo1) / 2.0);
                        int newTempo = int(averagetimediff * 24000000.0);
                        if(MusEGlobal::debugSync)
                          printf("setting new tempo %d = %f\n", newTempo, (float)((1000000.0 * 60.0)/newTempo));
                        MusEGlobal::tempomap.setExtTempo(newTempo);
                        }
                        
                  mclock2 = mclock1;
                  mclock1 = mclock0;
                  */
//END : Using external tempo map
                  
                  }
                  break;
            case ME_TICK:  // midi tick  (every 10 msec)
                  // FIXME: Unfinished? mcStartTick is uninitialized and Song::setPos doesn't set it either. Dangerous to allow this.
                  //if (mcStart) {
                  //      song->setPos(0, mcStartTick);
                  //      mcStart = false;
                  //      return;
                  //      }
                  break;
            case ME_START:  // start
                  // Re-transmit start to other devices if clock out turned on.
                  for(int p = 0; p < MIDI_PORTS; ++p)
                    //if(p != port && MusEGlobal::midiPorts[p].syncInfo().MCOut())
                    if(p != port && MusEGlobal::midiPorts[p].syncInfo().MRTOut())
                    {
                      // p3.3.31
                      // If we aren't rewinding on start, there's no point in re-sending start.
                      // Re-send continue instead, for consistency.
                      if(MusEGlobal::midiPorts[port].syncInfo().recRewOnStart())
                        MusEGlobal::midiPorts[p].sendStart();
                      else  
                        MusEGlobal::midiPorts[p].sendContinue();
                    }
                  if (MusEGlobal::debugSync)
                        printf("   start\n");
                  
                  //printf("midi start:%f\n", curTime());
                  
                  if (1 /* !MusEGlobal::audio->isPlaying()*/ /*state == IDLE*/) {
                        if (!MusEGlobal::checkAudioDevice()) return;
                        
                        // p3.3.31
                        // Rew on start option.
                        if(MusEGlobal::midiPorts[port].syncInfo().recRewOnStart())
                        {
                          MusEGlobal::curExtMidiSyncTick = 0;
                          MusEGlobal::lastExtMidiSyncTick = MusEGlobal::curExtMidiSyncTick;
                          //MusEGlobal::audioDevice->seekTransport(0);
                          MusEGlobal::audioDevice->seekTransport(Pos(0, false));
                        }  

                        //unsigned curFrame = MusEGlobal::audio->curFrame();
                        //if (MusEGlobal::debugSync)
                        //      printf("       curFrame=%d\n", curFrame);
                        
                        alignAllTicks();
                        //if (MusEGlobal::debugSync)
                        //      printf("   curFrame: %d curTick: %d tempo: %d\n", curFrame, recTick, MusEGlobal::tempomap.tempo(0));

                        storedtimediffs = 0;
                        for (int i=0; i<24; i++)
                              timediff[i] = 0.0;
                        
                        // p3.3.26 1/23/10
                        // Changed because msgPlay calls MusEGlobal::audioDevice->seekTransport(song->cPos())
                        //  and song->cPos() may not be changed to 0 yet, causing tranport not to go to 0.
                        //MusEGlobal::audio->msgPlay(true);
                        //MusEGlobal::audioDevice->startTransport();
                        // p3.3.28
                        MusEGlobal::playPendingFirstClock = true;
                        
                        MusEGlobal::midiExtSyncTicks = 0;
                        playStateExt = true;
                        }
                  break;
            case ME_CONTINUE:  // continue
                  // Re-transmit continue to other devices if clock out turned on.
                  for(int p = 0; p < MIDI_PORTS; ++p)
                    //if(p != port && MusEGlobal::midiPorts[p].syncInfo().MCOut())
                    if(p != port && MusEGlobal::midiPorts[p].syncInfo().MRTOut())
                      MusEGlobal::midiPorts[p].sendContinue();
                  
                  if (MusEGlobal::debugSync)
                        printf("realtimeSystemInput continue\n");
                  
                  //printf("continue:%f\n", curTime());
                  
                  if (1 /* !MusEGlobal::audio->isPlaying() */ /*state == IDLE */) {
                        //unsigned curFrame = MusEGlobal::audio->curFrame();
                        //recTick = MusEGlobal::tempomap.frame2tick(curFrame); // don't think this will work... (ml)
                        //alignAllTicks();
                        
                        // p3.3.28
                        //MusEGlobal::audio->msgPlay(true);
                        // p3.3.31
                        // Begin incrementing immediately upon first clock reception.
                        MusEGlobal::playPendingFirstClock = true;
                        
                        playStateExt = true;
                        }
                  break;
            case ME_STOP:  // stop
                  {
                    // p3.3.35
                    // Stop the increment right away.
                    MusEGlobal::midiExtSyncTicks = 0;
                    playStateExt = false;
                    MusEGlobal::playPendingFirstClock = false;
                    
                    // Re-transmit stop to other devices if clock out turned on.
                    for(int p = 0; p < MIDI_PORTS; ++p)
                      //if(p != port && MusEGlobal::midiPorts[p].syncInfo().MCOut())
                      if(p != port && MusEGlobal::midiPorts[p].syncInfo().MRTOut())
                        MusEGlobal::midiPorts[p].sendStop();
                    
                    //MusEGlobal::playPendingFirstClock = false;
                    
                    //lastStoppedBeat = (MusEGlobal::audio->tickPos() * 4) / MusEGlobal::config.division;
                    //MusEGlobal::curExtMidiSyncTick = (MusEGlobal::config.division * lastStoppedBeat) / 4;
                    
                    //printf("stop:%f\n", curTime());
                    
                    if (MusEGlobal::audio->isPlaying() /*state == PLAY*/) {
                          MusEGlobal::audio->msgPlay(false);
                          //playStateExt = false;
                          }
                    
                    if (MusEGlobal::debugSync)
                          printf("realtimeSystemInput stop\n");
                    
                    // Just in case the process still runs a cycle or two and causes the 
                    //  audio tick position to increment, reset the incrementer and force 
                    //  the transport position to what the hardware thinks is the current position.
                    //MusEGlobal::midiExtSyncTicks = 0;
                    //Pos pos((MusEGlobal::config.division * lastStoppedBeat) / 4, true);
                    //Pos pos(MusEGlobal::curExtMidiSyncTick, true);
                    //MusEGlobal::audioDevice->seekTransport(pos);
                  }
                  
                  break;
            //case 0xfd:  // unknown
            //case ME_SENSE:  // active sensing
            //case ME_META:  // system reset (reset is 0xff same enumeration as file meta event)
            default:
                  break;      
            }

      }

//---------------------------------------------------------
//   MusEGlobal::mtcSyncMsg
//    process received mtc Sync
//    seekFlag - first complete mtc frame received after
//                start
//---------------------------------------------------------

void MidiSeq::mtcSyncMsg(const MTC& mtc, int type, bool seekFlag)
      {
      double time = mtc.time();
      double stime = mtc.time(type);
      if (MusEGlobal::debugSync)
            printf("MidiSeq::MusEGlobal::mtcSyncMsg time:%lf stime:%lf seekFlag:%d\n", time, stime, seekFlag);

      if (seekFlag && MusEGlobal::audio->isRunning() /*state == START_PLAY*/) {
//            int tick = MusEGlobal::tempomap.time2tick(time);
            //state = PLAY;
            //write(sigFd, "1", 1);  // say PLAY to gui
            if (!MusEGlobal::checkAudioDevice()) return;
            if (MusEGlobal::debugSync)
              printf("MidiSeq::MusEGlobal::mtcSyncMsg starting transport.\n");
            MusEGlobal::audioDevice->startTransport();
            return;
            }

      /*if (tempoSN != MusEGlobal::tempomap.tempoSN()) {
            double cpos    = MusEGlobal::tempomap.tick2time(_midiTick, 0);
            samplePosStart = samplePos - lrint(cpos * MusEGlobal::sampleRate);
            rtcTickStart   = rtcTick - lrint(cpos * realRtcTicks);
            tempoSN        = MusEGlobal::tempomap.tempoSN();
            }*/

      //
      // diff is the time in sec MusE is out of sync
      //
      /*double diff = time - (double(samplePosStart)/double(MusEGlobal::sampleRate));
      if (MusEGlobal::debugSync)
            printf("   state %d diff %f\n", MusEGlobal::mtcState, diff);
      */
      }

} // namespace MusECore
