/**
 * Pong -- Pong game
 * by Tirasz
 */

#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

#define	__AVR_ATmega128__	1
#include <avr/io.h>


// GENERAL INIT - USED BY ALMOST EVERYTHING ----------------------------------

static void port_init() {
	PORTA = 0b00011111;	DDRA = 0b01000000; // buttons & led
	PORTB = 0b00000000;	DDRB = 0b00000000;
	PORTC = 0b00000000;	DDRC = 0b11110111; // lcd
	PORTD = 0b11000000;	DDRD = 0b00001000;
	PORTE = 0b00100000;	DDRE = 0b00110000; // buzzer
	PORTF = 0b00000000;	DDRF = 0b00000000;
	PORTG = 0b00000000;	DDRG = 0b00000000;
}

// TIMER-BASED RANDOM NUMBER GENERATOR ---------------------------------------

static void rnd_init() {
	TCCR0 |= (1  << CS00);	// Timer 0 no prescaling (@FCPU)
	TCNT0 = 0; 				// init counter
}

// generate a value between 0 and max - 1
static int rnd_gen(int max) {
	return TCNT0 % max;
}


// BUTTON HANDLING -----------------------------------------------------------

#define BUTTON_NONE		0
#define BUTTON_CENTER	1
#define BUTTON_LEFT		2
#define BUTTON_RIGHT	3
#define BUTTON_UP		4
static int button_accept = 1;

static int button_pressed() {
	// right
	if (!(PINA & 0b00000001) & button_accept) { // check state of button 1 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_RIGHT;
	}

	// up
	if (!(PINA & 0b00000010) & button_accept) { // check state of button 2 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_UP;
	}

	// center
	if (!(PINA & 0b00000100) & button_accept) { // check state of button 3 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_CENTER;
	}

	// left
	if (!(PINA & 0b00010000) & button_accept) { // check state of button 5 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_LEFT;
	}

	return BUTTON_NONE;
}

static void button_unlock() {
	//check state of all buttons
	if (
		((PINA & 0b00000001)
		|(PINA & 0b00000010)
		|(PINA & 0b00000100)
		|(PINA & 0b00001000)
		|(PINA & 0b00010000)) == 31)
	button_accept = 1; //if all buttons are released button_accept gets value 1
}

// LCD HELPERS ---------------------------------------------------------------

#define		CLR_DISP	    0x00000001
#define		DISP_ON		    0x0000000C
#define		DISP_OFF	    0x00000008
#define		CUR_HOME      0x00000002
#define		CUR_OFF 	    0x0000000C
#define   CUR_ON_UNDER  0x0000000E
#define   CUR_ON_BLINK  0x0000000F
#define   CUR_LEFT      0x00000010
#define   CUR_RIGHT     0x00000014
#define   CG_RAM_ADDR		0x00000040
#define		DD_RAM_ADDR	  0x00000080
#define		DD_RAM_ADDR2	0x000000C0

//#define		ENTRY_INC	    0x00000007	//LCD increment
//#define		ENTRY_DEC	    0x00000005	//LCD decrement
//#define		SH_LCD_LEFT	  0x00000010	//LCD shift left
//#define		SH_LCD_RIGHT	0x00000014	//LCD shift right
//#define		MV_LCD_LEFT	  0x00000018	//LCD move left
//#define		MV_LCD_RIGHT	0x0000001C	//LCD move right

static void lcd_delay(unsigned int b) {
	volatile unsigned int a = b;
	while (a)
		a--;
}

static void lcd_pulse() {
	PORTC = PORTC | 0b00000100;	//set E to high
	lcd_delay(1400); 			//delay ~110ms
	PORTC = PORTC & 0b11111011;	//set E to low
}

static void lcd_send(int command, unsigned char a) {
	unsigned char data;

	data = 0b00001111 | a;					//get high 4 bits
	PORTC = (PORTC | 0b11110000) & data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set D4-D7 bits

	data = a<<4;							//get low 4 bits
	PORTC = (PORTC & 0b00001111) | data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set d4-d7 bits
}

static void lcd_send_command(unsigned char a) {
	lcd_send(1, a);
}

static void lcd_send_data(unsigned char a) {
	lcd_send(0, a);
}

static void lcd_init() {
	//LCD initialization
	//step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	lcd_delay(10000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00100000;				//set D4 to 0, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)

	lcd_send_command(0x28); // function set: 4 bits interface, 2 display lines, 5x8 font
	lcd_send_command(DISP_OFF); // display off, cursor off, blinking off
	lcd_send_command(CLR_DISP); // clear display
	lcd_send_command(0x06); // entry mode set: cursor increments, display does not shift

	lcd_send_command(DISP_ON);		// Turn ON Display
	lcd_send_command(CLR_DISP);		// Clear Display
}

static void lcd_send_text(char *str) {
	while (*str)
		lcd_send_data(*str++);
}

static void lcd_send_line1(char *str) {
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_text(str);
}

static void lcd_send_line2(char *str) {
	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_text(str);
}


// GRAPHICS ------------------------------------------------------------------

#define CHAR_EMPTY_PATTERN			0
#define CHAR_LEFT_TOP				1
#define CHAR_LEFT_BOTTOM			2
#define CHAR_RIGHT_TOP				3
#define CHAR_RIGHT_BOTTOM			4
#define CHAR_PONG_BALL				5
#define CHAR_EMPTY_EMPTY			' '
#define CHAR_ERROR					'X'

#define CHARMAP_SIZE 6
// CHARMAP[character][row]
static unsigned char CHARMAP[CHARMAP_SIZE][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 },										// CHAR_EMPTY_PATTERN
	{ 0, 0, 0, 0, 0, 0, 0, 0 },										// CHAR_LEFT_TOP
	{ 0, 0, 0, 0, 0, 0, 0, 0 },										// CHAR_LEFT_BOTTOM
	{ 0, 0, 0, 0, 0, 0, 0, 0 },										// CHAR_RIGHT_TOP
	{ 0, 0, 0, 0, 0, 0, 0, 0 },										// CHAR_RIGHT_BOTTOM
	{ 0, 0, 0, 0, 0, 0, 0, 0 },								        // CHAR_PONG_BALL
};


static void chars_init() {
	for (int c = 0; c < CHARMAP_SIZE; ++c) {
		lcd_send_command(CG_RAM_ADDR + c*8);
		for (int r = 0; r < 8; ++r)
			lcd_send_data(CHARMAP[c][r]);
	}
}

static void players_init() {
	// Draw player characters (Their position on screen wont change)
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_data(CHAR_LEFT_TOP);

	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_data(CHAR_LEFT_BOTTOM);

	lcd_send_command(DD_RAM_ADDR + 15);
	lcd_send_data(CHAR_RIGHT_TOP);

	lcd_send_command(DD_RAM_ADDR2 + 15);
	lcd_send_data(CHAR_RIGHT_BOTTOM);
}
// GAME STATE ==================================================================

struct GameState {
	unsigned char ballPositionX; // 16 character wide screen, each character is 5 pixel wide -> Range: 0 - 79
	unsigned char ballPositionY; // 2 character tall screen, each character is 8 pixel tall --> Range: 0 - 15
	unsigned char leftPosition; // Position of left player (4 pixels tall) --> Range: 0 - 12; 
	unsigned char rightPosition; // Position of right player (4 pixels tall) --> Range: 0 - 12;
	unsigned char ballSpeedX;
	unsigned char ballSpeedY; 
	unsigned char gameSpeed;
};

static struct GameState gameState;

static void gameState_init() {
	gameState.ballPositionX = 40;
	gameState.ballPositionY = 8;
	gameState.rightPosition = 6;
	gameState.leftPosition = 6;
	gameState.ballSpeedX = rnd_gen(2) ? -1 : 1;
	gameState.ballSpeedY = rnd_gen(2) ? -1 : 1;
	gameState.gameSpeed = 20;
}

static void updateGamestate() {
	gameState.ballPositionX += gameState.ballSpeedX;
	gameState.ballPositionY += gameState.ballSpeedY;
}

// GRAPHICS ==================================================================

static void drawBall() {
	
	static char prev_scr_row = 255;
	static char prev_scr_col = 255;

	// In character space (where is the ball within the custom character)
	unsigned const char char_row = (gameState.ballPositionY) % 8;
	unsigned const char char_col = (gameState.ballPositionX) % 5; 

	// In screen space (which character on the screen)
	unsigned const char scr_row = (gameState.ballPositionY) / 8; 
	unsigned const char scr_col = (gameState.ballPositionX) / 5;

	// Have to draw ball on player characters
	if(scr_col == 0 || scr_col == 15) {
		const char char_index = (CHAR_LEFT_TOP + scr_row) * (scr_col == 0) + (CHAR_RIGHT_TOP + scr_row) * (scr_col == 15);
		CHARMAP[char_index][char_row] = 0b10000 >> char_col;
		return;
	}

	// Update ball char
	lcd_send_command(CG_RAM_ADDR + CHAR_PONG_BALL*8);
	for (int r = 0; r < 8; r++) {
		lcd_send_data((0b10000 * (r == char_row)) >> char_col);
	}

	// Clear previous char before drawing new
	if(prev_scr_col != scr_col || prev_scr_row != scr_row) {
		unsigned char rowAddress = DD_RAM_ADDR;
		if(prev_scr_row > 0) {
			rowAddress = DD_RAM_ADDR2;
		}
		
		lcd_send_command(rowAddress + prev_scr_col);
		lcd_send_data(CHAR_EMPTY_PATTERN);
	}

	// Send ball char to correct location on screen
	unsigned char rowAddress = DD_RAM_ADDR;
	if(scr_row > 0) {
		rowAddress = DD_RAM_ADDR2;
	}
	
	lcd_send_command(rowAddress + scr_col);
	lcd_send_data(CHAR_PONG_BALL);

	prev_scr_col = scr_col;
	prev_scr_row = scr_row;
}

static void drawPlayers() {
	// For each character row on screen
	for (int scr_row = 0; scr_row < 2; scr_row++) {
		// Setting character addresses for each player based on row
		unsigned char p1_char_address = CHAR_LEFT_TOP * (scr_row == 0) + CHAR_LEFT_BOTTOM * (scr_row == 1);
		unsigned char p2_char_address = CHAR_RIGHT_TOP * (scr_row == 0) + CHAR_RIGHT_BOTTOM * (scr_row == 1);
		unsigned char shouldPaint = 0;

		// Setting special char for P1
		lcd_send_command(CG_RAM_ADDR + p1_char_address * 8);
		for (int i = 0; i < 8; i++) {
			shouldPaint = ( i + (scr_row * 8) >= gameState.leftPosition && i + (scr_row * 8) < gameState.leftPosition + 4 );
			lcd_send_data((0b10000 * shouldPaint) | CHARMAP[p1_char_address][i]);
			// Clear charmap hack
			CHARMAP[p1_char_address][i] = 0;
		}

		// Setting special char for P2
		lcd_send_command(CG_RAM_ADDR + p2_char_address * 8);
		for (int i = 0; i < 8; i++) {
			shouldPaint = ( i + (scr_row * 8) >= gameState.rightPosition && i + (scr_row * 8) < gameState.rightPosition + 4 );
			lcd_send_data((0b00001 * shouldPaint) | CHARMAP[p2_char_address][i]);
			// Clear charmap hack
			CHARMAP[p2_char_address][i] = 0;
		}
	}
}

static void drawGameState() {
	// Have to make sure to draw ball first because of charmap hack lol
	drawBall();
	drawPlayers();
}


// THE GAME ==================================================================

int main() {
	port_init();
	lcd_init();
	rnd_init();
	char frame = 0;
	// Loop of the whole program, always restarts game
	while (1) {
		// Splash screen
		lcd_send_line1("    Pong");
		lcd_send_line2("    by Tirasz");

		while (button_pressed() != BUTTON_CENTER){ // wait till start signal
			button_unlock(); // keep on clearing button_accept
		}

		// Clear display, init game graphics and state
		lcd_send_command(CLR_DISP); 
		players_init();											
		gameState_init();

		// loop of the game
		while (1) {
			int button = button_pressed();
			drawGameState();
			if(frame % gameState.gameSpeed == 0) {
				updateGamestate();
			}
			frame = (frame + 1) % 60;
			button_unlock();
			//if (button == BUTTON_LEFT)
				//gameState.ballPositionX -= 1;
			//if (button == BUTTON_RIGHT)
				//gameState.ballPositionX += 1;
			//drawBall(); 
		} // end of game-loop

		// playing some funeral tunes and displaying a game over screen
		lcd_send_line1("    GAME OVER   ");
		lcd_send_line2("Click to restart");

	} // end of program-loop, we never quit
}
