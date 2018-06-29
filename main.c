#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <eepromVariables.h>

// F_CPU = 1000000 Hz

// Global variables
volatile uint16_t millis = 0;	// this counter will roll over around 65 seconds
								// saves memory and is fine for this application
#define RUN 0
#define PROGRAM 1
uint8_t machineState = RUN;
uint16_t statusBlinkCounter = 0;	// counter for status LED
uint16_t statusBlinkDelay = 0;		// delay setting for status LED
uint16_t playbackDelay = 128;		// delay setting for playback speed
uint8_t playbackIndex = 0;			// current frame within pattern memory
uint8_t playbackConfig = 0;
uint8_t playbackPattern = 0;		// current pattern to play back (0 to 5)
uint8_t playbackMarkerPos;
uint8_t playbackDir = 0;
uint8_t outByte = 0;				// current output channel map

#define BTN_UPDATE_INTERVAL 5
#define BTN_HOLD_DELAY 2000
#define EEPROMSAVEINTERVAL 1000


// - Outpin pin mapping -
// Channel 1:	PA1
// Channel 2:	PA0
// Channel 3:	PD2
// Channel 4:	PD3
// Channel 5:	PD4
// Channel 6:	PD5
// Status LED:	PD6
#define STATUSLED PD6

// - Input pin mapping -
// Channel 1:	PB5
// Channe1 2:	PB4
// Channel 3:	PB3
// Channel 4:	PB2
// Channel 5:	PB1
// Channel 6:	PB0
// Control 1:	PB6
// Control 2:	PB7


// Configure timer 0 interrupt
void setupTimer(void) {
	TIMSK = _BV(OCIE0A);	// enable Timer 0
	TCNT0 = 0x00;			// clear Timer 0
	TCCR0A = _BV(WGM01);	// set Timer 0 to CTC mode
	TCCR0B = _BV(CS01);		// set Timer 0 prescaler to clk/8
	OCR0A = 124;			// 124 = 125 counts. 1 MHz / 8 = 125kHz, 125kHz / 125 = 1kHz interrupt
	sei();					// enable interrupts
}


// Setup input and output ports
void initIO(void) {
	
	DDRD = 0b01111100;
	DDRA = 0b00000011;
	DDRB = 0b00000000;
	PORTB = 0b11111111; // set pullups active
	
}


// Define structure for button processing
typedef struct {
	uint8_t edgeDetect;
	uint16_t pressTimer;
	uint8_t statusFlags;
} button;

button Buttons[8];


#define EDGE_RISE	0b01111111
#define EDGE_FALL	0b11111110
#define STABLE_POS	0b11111111
#define STABLE_NEG	0b00000000

#define STATUS_PRESSED	0b00000001
#define STATUS_RELEASED 0b00000010
#define STATUS_ACTIVE	0b00000100
#define STATUS_INACTIVE 0b00001000
#define STATUS_HELD		0b00010000

// Update button inputs
void updateButtons(void) {
	uint8_t i;
	uint8_t inputByte;
	
	inputByte = ~PINB; // invert active-low inputs
	
	for (i = 0; i < 8; i++) {
		
		// load the current button level into the debounce array
		Buttons[i].edgeDetect <<= 1; // shift debounce array left
		if (inputByte & (1 << i)) Buttons[i].edgeDetect |= 1; // if button is active, write 1 to LSB
		
		// Determine button status and update status flags
		switch(Buttons[i].edgeDetect) {
			case EDGE_RISE:
				Buttons[i].statusFlags |= STATUS_PRESSED;
				Buttons[i].statusFlags &= ~(STATUS_RELEASED | STATUS_HELD);
				Buttons[i].pressTimer = 7*BTN_UPDATE_INTERVAL; // preset to 35ms
				break;
				
			case EDGE_FALL:
				Buttons[i].statusFlags |= STATUS_RELEASED;
				Buttons[i].pressTimer = 0; // clear timer
				break;
				
			case STABLE_POS:
				if (Buttons[i].statusFlags & STATUS_PRESSED) {
					Buttons[i].statusFlags |= STATUS_ACTIVE;
					Buttons[i].statusFlags &= ~STATUS_INACTIVE;
					Buttons[i].pressTimer += BTN_UPDATE_INTERVAL; // increment 5ms
					if (Buttons[i].pressTimer > BTN_HOLD_DELAY) Buttons[i].statusFlags |= STATUS_HELD;
					if (Buttons[i].pressTimer > 10000) Buttons[i].pressTimer = 10000; // max 10 seconds
				}
				break;
				
			case STABLE_NEG:
				Buttons[i].statusFlags |= STATUS_INACTIVE;
				Buttons[i].statusFlags &= ~STATUS_ACTIVE;
				Buttons[i].pressTimer = 0;
				break;
		}
		
		
		
	}
}


uint8_t checkHeld(uint8_t buttonID) {
	if (Buttons[buttonID].statusFlags & STATUS_HELD) {
		Buttons[buttonID].statusFlags &= ~(STATUS_HELD | STATUS_PRESSED);
		return 1;
	} else {
		return 0;
	}
}

uint8_t checkClicked(uint8_t buttonID) {
	if ((Buttons[buttonID].statusFlags & (STATUS_RELEASED | STATUS_PRESSED)) == (STATUS_RELEASED | STATUS_PRESSED)) {
		Buttons[buttonID].statusFlags &= ~(STATUS_RELEASED | STATUS_PRESSED | STATUS_HELD);
		return 1;
	} else {
		return 0;
	}
}

uint8_t checkActive(uint8_t buttonID) {
	if ((Buttons[buttonID].statusFlags & STATUS_ACTIVE)) {
		return 1;
	} else {
		return 0;
	}
}

// Map input byte to output channels
// Channel 1 is bit 0, Channel 6 is bit 5
// Input byte is active-high, output ports are active-high
void writeChannels(uint8_t inByte) {
	inByte |= 0b11000000;
	
	PORTD = (PORTD & 0b11000011) | (inByte & 0b00111100);
	PORTA = (PORTA & 0b11111100) | ((inByte & 0b0000001) << 1) | ((inByte & 0b00000010) >> 1);
	
}


// Display information on the status LED
#define OFF			0
#define ON			1
#define SLOWBLINK	400
#define FASTBLINK	50
#define STUTTERBLINK 300
void setStatusLED(uint16_t status) {

	if (status == ON) {
		PORTD |= _BV(STATUSLED);
		statusBlinkDelay = 0;
	} else if (status == OFF) {
		PORTD &= ~(_BV(STATUSLED));
		statusBlinkDelay = 0;
	} else {
		statusBlinkDelay = status;
	}
}





// Timer 0 ISR (system tick)
// Increment millis value for system timing
ISR(TIMER0_COMPA_vect) {
	millis++;
}


uint8_t *mapEEPROM(uint8_t pattern, uint8_t index) {
	switch(pattern) {
		case 0:
			return &patternData1[index % 8];
			break;
			
		case 1:
			return &patternData2[index % 8];
			break;
			
		case 2:
			return &patternData3[index % 16];
			break;
			
		case 3:
			return &patternData4[index % 16];
			break;
			
		case 4:
			return &patternData5[index % 32];
			break;
			
		case 5:
			return &patternData6[index % 32];
			break;
			
		default:
			return 0;
			break;
	}
}


uint8_t getFrame(uint8_t pattern, uint8_t index) {
			return eeprom_read_byte(mapEEPROM(pattern, index));
}

void writeFrame(uint8_t pattern, uint8_t index, uint8_t data) {
	cli();	// disable interrupts while writing EEPROM
			// we may lose a millisecond or two, but not a big deal in this application!
	eeprom_update_byte(mapEEPROM(pattern, index), data); // save data to EEPROM
	eeprom_busy_wait(); // wait for EEPROM write to finish
	sei(); // enable interrupts after writing EEPROM
}

uint8_t getPatternMax(void) {
	switch(playbackPattern) {
		case 0:
		case 1:
			return 7;
			break;
			
		case 2:
		case 3:
			return 15;
			break;
			
		case 4:
		case 5:
			return 31;
			break;
			
		default:
			return 0;
			break;
	}
}

void loadPatternConfigs(uint8_t pattern) {
	
	playbackDelay = eeprom_read_word(&patternConfig[pattern]) & 0x0FFF;
	playbackConfig = eeprom_read_word(&patternConfig[pattern]) >> 12;
	playbackMarkerPos = getPatternMax();
	for (uint8_t i = 0; i < playbackMarkerPos; i++) {
		uint8_t tempByte = getFrame(playbackPattern, i);
		if (tempByte & 0b10000000) {
			playbackMarkerPos = i;
			break;
		}
	}
	
	
}


void fwdPattern(void) {
	if (playbackIndex < getPatternMax()) playbackIndex++;
}

void revPattern(void) {
	if (playbackIndex > 0) playbackIndex--;
}


void clearButtonFlags(uint8_t flags) {
	for (uint8_t i = 0; i < 8; i++) {
		Buttons[i].statusFlags &= ~(flags);
	}
}

void nextFrame(void) {
	outByte = getFrame(playbackPattern, playbackIndex);
	writeChannels(outByte);
	if (outByte & 0b10000000) {
		setStatusLED(FASTBLINK);
	} else if ((playbackIndex == getPatternMax()) || (playbackIndex == 0)) {
		setStatusLED(ON);
		setStatusLED(STUTTERBLINK);
	} else {
		setStatusLED(SLOWBLINK);
	}
}

void clearMarker(void) {
		uint8_t tempByte = getFrame(playbackPattern, playbackMarkerPos);
		writeFrame(playbackPattern, playbackMarkerPos, tempByte & 0b01111111);
}



#define PLAY_FORWARD 0
#define PLAY_REVERSE 1
#define PLAY_BOUNCE 2
void cyclePlaybackMode(void) {
	playbackConfig++;
	if (playbackConfig > 2) playbackConfig = 0;
}


#define BTN1 0
#define BTN2 1
#define BTN3 2
#define BTN4 3
#define BTN5 4
#define BTN6 5
#define BTNUP 6
#define BTNDN 7

void processButtons(void) {
	static uint16_t debounceEvent = 0;
	uint8_t i;

	
	if (millis - debounceEvent > (BTN_UPDATE_INTERVAL - 1)) {
		debounceEvent = millis;
		updateButtons();
		
		// Running state
		if (machineState == RUN) {
		
			for (i = 0; i < 6; i++) {
				
				if (checkHeld(5-i)) {
					setStatusLED(SLOWBLINK);
					machineState = PROGRAM;
					playbackIndex = 0;
					playbackPattern = i;
					clearButtonFlags(STATUS_HELD | STATUS_PRESSED | STATUS_RELEASED);
					nextFrame();
					loadPatternConfigs(i);
					break;
				}
				
				if (checkClicked(5-i)) {
					if (playbackPattern == i) {
						cyclePlaybackMode();
					} else {
						playbackPattern = i;
						playbackIndex = 0;
						loadPatternConfigs(i);
					}
				}
				
			}

			
			if (checkClicked(BTNUP)) {
				playbackDelay += (playbackDelay >> 3);
				if (playbackDelay > 2000) playbackDelay = 2000;
			}
			
			if (checkClicked(BTNDN)) {
				playbackDelay -= (playbackDelay >> 3);
				if (playbackDelay < 8) playbackDelay = 8;
			}

			
			
		
		// Programming state
		} else if (machineState == PROGRAM) {
			
			for (i = 0; i < 6; i++) {
				if (checkClicked(5-i)) {
					outByte ^= (1 << i);
					writeChannels(outByte);
				} else if (checkHeld(i)) { // exit programming mode if any channel button is held down
					setStatusLED(ON);
					clearButtonFlags(STATUS_HELD | STATUS_PRESSED | STATUS_RELEASED);
					machineState = RUN;
					break;
				}
			}
			
			if (checkClicked(BTNUP)) {
				writeFrame(playbackPattern, playbackIndex, outByte); // save last frame to EEPROM
				revPattern();
				nextFrame();
			}
			
			if (checkClicked(BTNDN)) {
				writeFrame(playbackPattern, playbackIndex, outByte); // save last frame to EEPROM
				fwdPattern();
				nextFrame();
			}
			
			if (checkHeld(BTNUP) || checkHeld(BTNDN)) {
				clearMarker();			// delete any existing end-sequence marker
				outByte |= 0b10000000;	// set end of pattern marker
				writeFrame(playbackPattern, playbackIndex, outByte); // save to EEPROM
				loadPatternConfigs(playbackPattern);
				nextFrame();
			}
			
		}
		
	}
	
	
}

// Blink status LED if needed
void processStatusLED(void) {
	static uint16_t statusBlinkCounter = 0;
	
	if (statusBlinkDelay) {
		if (millis - statusBlinkCounter > statusBlinkDelay) {
			statusBlinkCounter = millis;
			if (statusBlinkDelay == 100) {
				statusBlinkDelay = 300;
			} else if (statusBlinkDelay == 300) {
				statusBlinkDelay = 100;
			}
			
			PORTD ^= _BV(STATUSLED);
		}
	}
}


// Control output channels
void processOutputs(void) {
	static uint16_t playbackCounter = 0;

	
	if (machineState == RUN) {
		if (playbackConfig == PLAY_FORWARD) playbackDir = 0;
		else if (playbackConfig == PLAY_REVERSE) playbackDir = 1;
		
		if (millis - playbackCounter > playbackDelay) {
			playbackCounter = millis;
			if (playbackDir == 0) {
				if (playbackIndex < playbackMarkerPos) {
					playbackIndex++;
				} else {
					if (playbackConfig == PLAY_FORWARD) {
						playbackIndex = 0;
					} else if (playbackConfig == PLAY_BOUNCE){
						playbackDir = 1;
						playbackIndex--;
					}
				}
			} else if (playbackDir == 1) {
				if (playbackIndex > 0) {
					playbackIndex--;
				} else {
					if (playbackConfig == PLAY_REVERSE) {
						playbackIndex = playbackMarkerPos;
					} else if (playbackConfig == PLAY_BOUNCE) {
						playbackDir = 0;
						playbackIndex++;
					}
				}
			}
			
			outByte = getFrame(playbackPattern,playbackIndex);
			writeChannels(outByte);
		}
	}
}


// Save EEPROM values if needed
void processEEPROMsave(void) {
	static uint16_t EEPROMSaveCounter = 0;

	if (machineState == RUN) {
		if (millis - EEPROMSaveCounter > EEPROMSAVEINTERVAL) {
			EEPROMSaveCounter = millis;
			cli(); // disable interrupts
			eeprom_update_word(&patternConfig[playbackPattern], (playbackDelay & 0x0FFF) | (playbackConfig << 12));
			eeprom_update_byte(&lastPlaybackPattern, playbackPattern);
			eeprom_busy_wait();
			sei(); // enable interrupts
		}
	}
}

void main(void) __attribute__ ((noreturn));

void main(void) {

	// startup tasks
	initIO();
	setupTimer();

	playbackPattern = eeprom_read_byte(&lastPlaybackPattern);
	loadPatternConfigs(playbackPattern);
	
	machineState = RUN;	// start up in RUN mode
	setStatusLED(ON);	// steady on means RUN mode
	writeChannels(0);	// turn off all channels on startup
	
	// run this loop forever
	for (;;) {
		
		processButtons();		// read and react to button presses
		processStatusLED();		// control status LED
		processOutputs();		// play back patterns
		processEEPROMsave();	// save the EEPROM periodically
		
	}
}
