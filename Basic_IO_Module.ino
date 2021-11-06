// =============================================================================
// Filename         : Basic_IO_Module.ino
// Version          : 
//
// Original Author  : Peter Metz
// Date Created     : 02-Jun-2020
//
// Revision History : 02-Jul-2020: Initial version
//
// Purpose          : To implement a (very) simple multi-channel digital and
//                    analogue I/O module, with an SCPI-like command interface
//                     
// Licensing        : Copyright (C) 2020, Peter Metz
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more 
// details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------
// Notes:
//   - should probably change platform-dependent type declaration to fixed-
//     length ones
// =============================================================================
#include <EEPROM.h>
#include <StateMachine.h>
#include <Parser.h>


// define DEBUG to enable logging of state changes, etc., to the Serial port;
// note that this approximately triples the size of the resulting code
//#define ENABLE_DEBUGGING

#ifdef ENABLE_DEBUGGING
#define LogToSerial(level, message) __LogToSerial(level, message)
#else
#define LogToSerial(level, message)
#endif

enum LogLevel { FATAL, ERROR, WARNING, INFO, VERBOSE, DEBUG };

LogLevel LoggingLevel = DEBUG;

void __LogToSerial(LogLevel level, String message) {
  if (level <= LoggingLevel)
    Serial.println(message);
}




// =============================================================================
// Class            : BasicIOModuleController
//
// Purpose          : This class implements the overarching controller's State
//                    Machine
// =============================================================================
class BasicIOModuleController {
  public:
    // plain-old-enums used in preference to enum class'es to minimise casts
    enum { TRIGGER_IMMEDIATE, TRIGGER_EXTERNAL };

    // used to define defaults and for saving to, and recalling values from EEPROM
    struct SaveState {
      byte  DigitalIOMode[8];
      bool  DigitalOutput[8];
      byte  AnalogIOMode[4];
      float AnalogOutput[4];
      byte  TriggerMode;
    };


    // not sure why we can't declare /and/ initialise these arrays here using
    // the static constexpr const <T> arrayname[] construct, but apparently
    // we can't, hence the two-part declaration...
    static const byte AddrSelectPin[];
    static const byte MainframeGoodPin = A7;  // HIGH => good

    static const byte DigitalIOPin[];
    static const bool DigitalOutput[];
    static const byte AnalogInPin[];
    static const byte AnalogOutPin[];

    BasicIOModuleController();
    ~BasicIOModuleController();

    void         tick();

    void         Recall();
    void         Reset();
    void         Save();
    void         Trigger();
    inline byte  UnitID() { return p_unitID; }

    void         triggerMode(byte mode) { p_triggerMode = mode; }
    inline byte  triggerMode() { return p_triggerMode; }

    void         digitalMode(byte channel, byte mode) { DigitalIOMode[channel] = mode; pinMode(DigitalIOPin[channel], mode); }
    void         analogMode(byte channel, byte mode)  { AnalogIOMode[channel] = mode;  if (mode == INPUT) pinMode(AnalogOutPin[channel], INPUT); }
    void         digital(byte channel, bool val)      { if (DigitalIOMode[channel] == OUTPUT) digitalWrite(DigitalIOPin[channel], val); }
    void         analog(byte channel, float val)      { if (AnalogIOMode[channel] == OUTPUT)  analogWrite(AnalogOutPin[channel], 255 * val); }

    inline byte  digitalMode(byte channel) { return DigitalIOMode[channel]; }
    inline byte  analogMode(byte channel)  { return AnalogIOMode[channel]; }
    inline bool  digital(byte channel)     { return (DigitalIOMode[channel] != OUTPUT) ? digitalRead(DigitalIOPin[channel]) : false; }
    inline float analog(byte channel)      { return (AnalogIOMode[channel] == INPUT) ? (float) analogRead(AnalogInPin[channel]) / 1023.0 : 0.0; }

  protected:
    static const SaveState SystemDefaults;

    void initialise(SaveState s);

    byte p_unitID;
    byte p_triggerMode;

    byte DigitalIOMode[8];
    byte AnalogIOMode[4];
};


constexpr const byte BasicIOModuleController::AddrSelectPin[] = { 12, 11, 10 };  // reverse the order once the backplane has been fixed

constexpr const byte BasicIOModuleController::DigitalIOPin[]  = { 2, 4, 7, 8, A4, A5, A6, A7 };
constexpr const byte BasicIOModuleController::AnalogInPin[]   = { A0, A1, A2, A3 };
constexpr const byte BasicIOModuleController::AnalogOutPin[]  = { 3, 5, 6, 9 };


const BasicIOModuleController::SaveState BasicIOModuleController::SystemDefaults = {
  { INPUT, INPUT, INPUT, INPUT, INPUT, INPUT, INPUT, INPUT },  // DigitalIOMode[8]
  { LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW },                  // DigitalOutput[8]
  { INPUT, INPUT, INPUT, INPUT },                              // AnalogIOMode[4]
  { 0.0, 0.0, 0.0, 0.0 }                                       // AnalogOutput[4]
};




// =============================================================================
// Method           : BasicIOModuleController::BasicIOModuleController()
//
// Purpose          : 
//
// Inputs           : (none)
// =============================================================================
BasicIOModuleController::BasicIOModuleController() {
  // Set the I/O pin-modes
  pinMode(AddrSelectPin[0], INPUT_PULLUP);
  pinMode(AddrSelectPin[1], INPUT_PULLUP);
  pinMode(AddrSelectPin[2], INPUT_PULLUP);
  pinMode(MainframeGoodPin, INPUT);

  // Determine our UnitID
  p_unitID = (digitalRead(AddrSelectPin[0])) | (digitalRead(AddrSelectPin[1]) << 1) | (digitalRead(AddrSelectPin[2]) << 2);

  //
  initialise(SystemDefaults);
}


// =============================================================================
// Method           : BasicIOModuleController::~BasicIOModuleController
//
// Purpose          : To manage the clean-shutdown of the physical module
// =============================================================================
BasicIOModuleController::~BasicIOModuleController() {
}


// =============================================================================
// Method           : BasicIOModuleController::initialise
//
// Purpose          : 
//
// Inputs           : [s]
// =============================================================================
void BasicIOModuleController::initialise(SaveState s) {
  byte i;

  for (i = 0; i < 8; i++) {
    digitalMode(i, SystemDefaults.DigitalIOMode[i]);
    digital(i, SystemDefaults.DigitalOutput[i]);
  }

  for (i = 0; i < 4; i++) {
    analogMode(i, SystemDefaults.AnalogIOMode[i]);
    analog(i, SystemDefaults.AnalogOutput[i]);
  }
}


// =============================================================================
// Method           : BasicIOModuleController::Save
//
// Purpose          : 
// =============================================================================
void BasicIOModuleController::Save() {
  byte i;
  SaveState s;

  for (i = 0; i < 8; i++) {
    s.DigitalIOMode[i] = digitalMode(i);
    s.DigitalOutput[i] = digital(i);  // this might be problematic...
  }

  for (i = 0; i < 4; i++) {
    s.AnalogIOMode[i] = analogMode(i);
    s.AnalogOutput[i] = analog(i);
  }

  EEPROM.put(0, s);
}


// =============================================================================
// Method           : BasicIOModuleController::Save
//
// Purpose          : 
// =============================================================================
void BasicIOModuleController::Recall() {
  SaveState s;

  EEPROM.get(0, s);
  initialise(s);
}


// =============================================================================
// Method           : BasicIOModuleController::Save
//
// Purpose          : 
// =============================================================================
void BasicIOModuleController::Reset() {
  initialise(SystemDefaults);
}


// =============================================================================
// Method           : TrackingGeneratorModuleController::Trigger
//
// Purpose          : 
// =============================================================================
void BasicIOModuleController::Trigger() {
}


// =============================================================================
// Method           : BasicIOModuleController::tick
//
// Purpose          : To update the State Machine (currently this just affects /
//                    relates to our sweep-mode
// =============================================================================
void BasicIOModuleController::tick() {
}






// =============================================================================
// Class            : BasicIOModuleParser
//
// Purpose          : This class implements the overarching controller's command
//                    parser
// =============================================================================
class BasicIOModuleParser : public Parser {
  public:
    BasicIOModuleParser(BasicIOModuleController *controller) {
      this->controller = controller;
      Parser::CommandHandlers = (Parser::Handler *)&(Handlers);

      if (controller->UnitID() == 7)
        p_active = true;
      else
        p_active = false;
    }
    inline bool active() { return p_active; }

  protected:
    BasicIOModuleController *controller;

    void              ADDRHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void               IDNHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void               SAVHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void               RCLHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void               RSTHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void               TRGHandler(const char *cmd, bool query, unsigned channel, const char *args);

    void              HELPHandler(const char *cmd, bool query, unsigned channel, const char *args);

    void        SystemAddrHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void     SystemTriggerHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void SystemTriggerModeHandler(const char *cmd, bool query, unsigned channel, const char *args);

    void         DigitalIOHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void     DigitalIOModeHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void          AnalogIOHandler(const char *cmd, bool query, unsigned channel, const char *args);
    void      AnalogIOModeHandler(const char *cmd, bool query, unsigned channel, const char *args);

    static const char NOTHING[];
    static const char QUERY_ONLY[];

    static const Handler Handlers[];

    bool p_active;
};


const char BasicIOModuleParser::NOTHING[]    = "";
const char BasicIOModuleParser::QUERY_ONLY[] = "(QUERY ONLY)";

const Parser::Handler BasicIOModuleParser::Handlers[] = {
    { "++ADDR",         (ParserHandler) &BasicIOModuleParser::ADDRHandler,              false, QUERY_ONLY },
    { "*IDN",           (ParserHandler) &BasicIOModuleParser::IDNHandler,               false, QUERY_ONLY },
    { "*SAV",           (ParserHandler) &BasicIOModuleParser::SAVHandler,               false, NOTHING },
    { "*RCL",           (ParserHandler) &BasicIOModuleParser::RCLHandler,               false, NOTHING },
    { "*RST",           (ParserHandler) &BasicIOModuleParser::RSTHandler,               false, NOTHING },
    { "*TRG",           (ParserHandler) &BasicIOModuleParser::TRGHandler,               false, NOTHING },
    { "HELP",           (ParserHandler) &BasicIOModuleParser::HELPHandler,              false, QUERY_ONLY },
    { "ID",             (ParserHandler) &BasicIOModuleParser::IDNHandler,               false, QUERY_ONLY },
    { "SYST:ADDR",      (ParserHandler) &BasicIOModuleParser::SystemAddrHandler,        false, QUERY_ONLY },
    { "SYST:TRIG",      (ParserHandler) &BasicIOModuleParser::SystemTriggerHandler,     false, QUERY_ONLY },
    { "SYST:TRIG:MODE", (ParserHandler) &BasicIOModuleParser::SystemTriggerModeHandler, false, QUERY_ONLY },
    { "DIO0",           (ParserHandler) &BasicIOModuleParser::DigitalIOHandler,         true,  "{ 0 | 1 | OFF | ON | LO | HI }" },
    { "DIO0:MODE",      (ParserHandler) &BasicIOModuleParser::DigitalIOModeHandler,     true,  "{ INPUT | INPUT_PULLUP | OUTPUT }" },
    { "AIO0",           (ParserHandler) &BasicIOModuleParser::AnalogIOHandler,          true,  "[ 0 .. 1 ]" },
    { "AIO0:MODE",      (ParserHandler) &BasicIOModuleParser::AnalogIOModeHandler,      true,  "{ INPUT | OUTPUT }" },
    END_OF_HANDLERS
};




void BasicIOModuleParser::ADDRHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (!query) {
    p_active = (controller->UnitID() == (byte) strtol(args, NULL, 10));

    if (p_active)
      process("*IDN?");
  }
}


void BasicIOModuleParser::IDNHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (p_active && query)
    Serial.println("ENGINUITY.DE,IOM-8-4,000000,0.2-20200706");
}


void BasicIOModuleParser::SAVHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (p_active && !query)
    controller->Save();
}


void BasicIOModuleParser::RCLHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (p_active && !query)
    controller->Recall();
}


void BasicIOModuleParser::RSTHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (p_active && !query)
    controller->Reset();
}


// Note: *TRG's are not address-specific!
void BasicIOModuleParser::TRGHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (!query)
    controller->Trigger();
}


void BasicIOModuleParser::SystemTriggerHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (p_active && !query)
    controller->Trigger();
}


void BasicIOModuleParser::HELPHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  byte i = 0;
  
  if (p_active && query)
    while (Handlers[i].cmd) {
      Serial.println(Handlers[i].cmd + String(" ") + Handlers[i].help);
      i++;
    }
}


void BasicIOModuleParser::SystemAddrHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (p_active)
    Serial.println(controller->UnitID());
}


void BasicIOModuleParser::DigitalIOHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (p_active && ((channel > 0) && (channel <= 8)))
    if (query) {
      Serial.println(controller->digital(channel - 1));

    } else {
      if (!strcasecmp(args, "0") || !strcasecmp(args, "OFF") || !strncasecmp(args, "LO", 2))
        controller->digital(channel - 1, LOW);
        
      if (!strcasecmp(args, "1") || !strcasecmp(args, "ON") || !strncasecmp(args, "HI", 2))
        controller->digital(channel - 1, HIGH);
    }
}


void BasicIOModuleParser::DigitalIOModeHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (p_active && ((channel > 0) && (channel <= 8)))
    if (query) {
      switch(controller->digitalMode(channel - 1)) {
        case OUTPUT:
        Serial.println("OUTPUT");
        break;

        case INPUT_PULLUP:
        Serial.println("INPUT_PULLUP");
        break;

        case INPUT:
        Serial.println("INPUT");
        break;

        default:
        Serial.println("***" + String(controller->digitalMode(channel - 1)));
        break;
      }

    } else {
      if (!strncasecmp(args, "OUT", 3))
        controller->digitalMode(channel - 1, OUTPUT);
        
      else if (!strncasecmp(args, "INPUT_", 6) || !strncasecmp(args, "PUL", 3))
        controller->digitalMode(channel - 1, INPUT_PULLUP);
        
      else if (!strncasecmp(args, "IN", 2))
        controller->digitalMode(channel - 1, INPUT);

    }
}


void BasicIOModuleParser::AnalogIOHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  double d;

  if (p_active && ((channel > 0) && (channel <= 4)))
    if (query) {
      Serial.println(controller->analog(channel - 1), 3);

    } else {
      d = strtod(args, NULL);
      if ((d >= 0) && (d <= 1.0))
        controller->analog(channel - 1, d);
    }
}


void BasicIOModuleParser::AnalogIOModeHandler(const char *cmd, bool query, unsigned channel, const char *args) {
  if (p_active && ((channel > 0) && (channel <= 4)))
    if (query) {
      switch(controller->analogMode(channel - 1)) {
        case OUTPUT:
        Serial.println("OUTPUT");
        break;

        case INPUT:
        Serial.println("INPUT");
        break;

        default:
        Serial.println("***" + String(controller->digitalMode(channel - 1)));
        break;
      }

    } else {
      if (!strncasecmp(args, "OUT", 3))
        controller->analogMode(channel - 1, OUTPUT);
        
      else if (!strncasecmp(args, "IN", 2))
        controller->analogMode(channel - 1, INPUT);
    }
}






// =============================================================================
// Arduino-specific code starts here
// =============================================================================
BasicIOModuleController *controller;
BasicIOModuleParser *parser;

char next;
String cmd;
bool cmdReady;

unsigned long lastTick, duration;
const unsigned long TickInterval = 50;  // in ms




// =============================================================================
// Function         : setup()
//
// Note             : This is the standard Arduino-defined setup function
// =============================================================================
void setup() {
  Serial.begin(38400);
  while (!Serial) {
  }

  cmd.reserve(100);
  cmdReady = false;
  
  controller = new BasicIOModuleController();
  parser = new BasicIOModuleParser(controller);
}


// =============================================================================
// Function         : serialEvent()
// =============================================================================
void serialEvent() {
  while (Serial.available()) {
    next = (char)Serial.read();
    if (next == '\b')                      // a backspace
      cmd.remove(cmd.length() - 1, 1);
    else
      cmd += next;
    
    if ((next == '\r') || (next == '\n'))  // a CR/LF
      cmdReady = true;
    else if (parser->active())
      Serial.write(next);                  // echo the received character
  }
}


// =============================================================================
// Function         : loop()
//
// Note             : This is the standard Arduino-defined loop function
// =============================================================================
void loop() {
  lastTick = millis();
  
  if (cmdReady) {
    cmd.trim();
    if (parser->active())
      Serial.println("");
      
    parser->process(cmd.c_str());
    
    cmd = "";
    cmdReady = false;  
  }

  controller->tick();

  // we need the intermediate [duration], as some serial-heavy outputs might
  // take longer than [TickInterval], and we don't want a negative (which would
  // actually turn out to be a _very_ large positive) delay
  duration = millis() - lastTick;
  if (duration < TickInterval)
    delay(TickInterval - duration);
}
