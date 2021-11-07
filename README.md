# ate-basic-io-module

Implements a (very) simple multi-channel digital and analogue I/O module, with an SCPI-like command interface. In lieu of more complete documentation, the following excerpt defines the full set of supported commands; in (interactive) use, substitute the '0' in the multi-channel command definitions with the actual intended channel number.

```c++
const char BasicIOModuleParser::NOTHING[]    = "";
const char BasicIOModuleParser::QUERY_ONLY[] = "(QUERY ONLY)";

const Parser::Handler BasicIOModuleParser::Handlers[] = {
//    Command           Command handler                                        Multi-channel?  Allowed values
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
```

Currently tested just on the ATmega-based Arduino platform.
