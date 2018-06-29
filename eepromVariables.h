//
//  eepromVariables.h
//  ELbow 2.0
//
//  Created by Garrett Mace on 4/30/16.
//  Copyright © 2016 Garrett Mace. All rights reserved.
//

#ifndef eepromVariables_h
#define eepromVariables_h

// Pre-loaded EEPROM patterns
// Channels 1 and 2: max 8 frames
// Channels 3 and 4: max 16 frames
// Channels 5 and 6: max 32 frames
uint8_t patternData1[8] EEMEM = {0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b10100000, 0, 0};
uint8_t patternData2[8] EEMEM = {0b00000000, 0b00000001, 0b00000011, 0b00000111, 0b00001111, 0b00011111, 0b10111111, 0};
uint8_t patternData3[16] EEMEM = {0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b00010000, 0b00001000, 0b00000100, 0b10000010, 0, 0, 0, 0, 0, 0};
uint8_t patternData4[16] EEMEM = {0b00111111, 0b10000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint8_t patternData5[32] EEMEM = {0b00010101, 0b10101010, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint8_t patternData6[32] EEMEM = {0b00000111, 0b00111000, 0b00000111, 0b00111000, 0b00000001, 0b00100000, 0b00000001, 0b10100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


// Per-pattern speed and playback settings
// Upper 4 bits are used for playback direction/style
// Lower 12 bits are used for delay (in milliseconds)
uint16_t patternConfig[6] EEMEM = {128, 128, 128, 128, 128, 128};

// Last-selected pattern
uint8_t lastPlaybackPattern EEMEM = 0;

#endif /* eepromVariables_h */
