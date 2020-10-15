/*

*/

#define SD_CS 10
#define JOY_VERT  A9 // should connect A9 to pin VRx
#define JOY_HORIZ A8 // should connect A8 to pin VRy
#define JOY_SEL   53

#include <Arduino.h>

// core graphics library (written by Adafruit)
#include <Adafruit_GFX.h>

// Hardware-specific graphics library for MCU Friend 3.5" TFT LCD shield
#include <MCUFRIEND_kbv.h>

// LCD and SD card will communicate using the Serial Peripheral Interface (SPI)
// e.g., SPI is used to display images stored on the SD card
#include <SPI.h>

// needed for reading/writing to SD card
#include <SD.h>

#include "lcd_image.h"

#include <TouchScreen.h>

#include <Fonts/FreeSansBold9pt7b.h>

// touch screen pins, obtained from the documentaion
#define YP A3 // must be an analog pin, use "An" notation!
#define XM A2 // must be an analog pin, use "An" notation!
#define YM 9  // can be a digital pin
#define XP 8  // can be a digital pin

#define REST_START_BLOCK 4000000
#define NUM_RESTAURANTS 1066

// calibration data for the touch screen, obtained from documentation
// the minimum/maximum possible readings from the touch point
#define TS_MINX 100
#define TS_MINY 120
#define TS_MAXX 940
#define TS_MAXY 920

// thresholds to determine if there was a touch
#define MINPRESSURE   10
#define MAXPRESSURE 1000

// a multimeter reading says there are 300 ohms of resistance across the plate,
// so initialize with this to get more accurate readings
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// different than SD
Sd2Card card;

struct  restaurant {
	int32_t  lat; //  Stored  in 1/100 ,000 degrees
	int32_t  lon; //  Stored  in 1/100 ,000 degrees
	uint8_t rating; // 0-10: [2 = 1 star , 10 = 5 stars]
	char  name [55];  //  already  null  terminated  on the SD card
};

struct  RestDist {
	uint16_t  index; // index  of  restaurant  from 0 to  NUM_RESTAURANTS -1
	uint16_t  dist;   //  Manhatten  distance  to  cursor  position
};

RestDist rest_dist[NUM_RESTAURANTS];

// to prevent re-reading of blocks in getRestaurantFast()
uint32_t oldBlockNum=0;
restaurant restBlock[8];

#define BLUE    0x001F
#define RADIUS 3
#define TEXTSIZE 15

MCUFRIEND_kbv tft;

#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320
#define YEG_SIZE 2048

lcd_image_t yegImage = { "yeg-big.lcd", YEG_SIZE, YEG_SIZE };

#define JOY_CENTER   512
#define JOY_DEADZONE 64

#define CURSOR_SIZE 9

//  These  constants  are  for  the  2048 by 2048  map.
#define  MAP_WIDTH  2048
#define  MAP_HEIGHT  2048
#define  LAT_NORTH  5361858l
#define  LAT_SOUTH  5340953l
#define  LON_WEST  -11368652l
#define  LON_EAST  -11333496l

//  These  functions  convert  between x/y map  position  and  lat/lon// (and  vice  versa.)
int32_t  x_to_lon(int16_t x) {return  map(x, 0, MAP_WIDTH , LON_WEST , LON_EAST);}
int32_t  y_to_lat(int16_t y) {return  map(y, 0, MAP_HEIGHT , LAT_NORTH , LAT_SOUTH);}
int16_t  lon_to_x(int32_t  lon) {return  map(lon , LON_WEST , LON_EAST , 0, MAP_WIDTH);}
int16_t  lat_to_y(int32_t  lat) {return  map(lat , LAT_NORTH , LAT_SOUTH , 0, MAP_HEIGHT);}

// the cursor position on the display
int cursorX, cursorY;

// current position on the YEG map
int yegX = YEG_SIZE/2 - (DISPLAY_WIDTH - 60)/2;
int yegY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;

// cuurent state to switch between map and displaying list of restaurants
int state = 0;

// used to store old state of cursor button press, to avoid double state changes
int OldButtonVal = LOW;

// forward declaration for redrawing the cursor
void redrawCursor(uint16_t colour);
void redrawMap(int oldX, int oldY);

void setup() {
  	init();

  	Serial.begin(9600);

	pinMode(JOY_SEL, INPUT_PULLUP);

	//    tft.reset();             // hardware reset
  	uint16_t ID = tft.readID();    // read ID from display
  	Serial.print("ID = 0x");
  	Serial.println(ID, HEX);
  	if (ID == 0xD3D3) ID = 0x9481; // write-only shield
  
  	// must come before SD.begin() ...
  	tft.begin(ID);                 // LCD gets ready to work

	Serial.print("Initializing SD card...");
	if (!SD.begin(SD_CS)) {
		Serial.println("failed! Is it inserted properly?");
		while (true) {}
	}
	Serial.println("OK!");

  	// SD card initialization for raw reads (for restaurant)
  	Serial.print("Initializing SPI communication for raw reads...");
  	if (!card.init(SPI_HALF_SPEED, SD_CS)) {
    	Serial.println("failed! Is the card inserted properly?");
    	while (true) {}
  	}
  	else {
    	Serial.println("OK!");
  	}


	tft.setRotation(1);

  	tft.fillScreen(TFT_BLACK);

  	// draws the centre of the Edmonton map, leaving the rightmost 60 columns black
	int yegMiddleX = YEG_SIZE/2 - (DISPLAY_WIDTH - 60)/2;
	int yegMiddleY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;
	lcd_image_draw(&yegImage, &tft, yegMiddleX, yegMiddleY,
                 0, 0, DISPLAY_WIDTH - 60, DISPLAY_HEIGHT);

  	// initial cursor position is the middle of the screen
  	cursorX = (DISPLAY_WIDTH - 60)/2;
  	cursorY = DISPLAY_HEIGHT/2;

  	redrawCursor(TFT_RED);
}


void getRestaurantFast(int restIndex, restaurant* restPtr) {

	uint32_t blockNum = REST_START_BLOCK + restIndex/8;

	// to prevent re-reading from same block
	if (blockNum != oldBlockNum){
	  	while (!card.readBlock(blockNum, (uint8_t*) restBlock))
	    	Serial.println("Read block failed, trying again.");
	    oldBlockNum = blockNum;
	}
  	*restPtr = restBlock[restIndex % 8];
}


// function to sort restaurants by distance using insertion sort
void isort(RestDist a[], int n){

	int i = 1;

	while (i<n){
		int j = i;
		while (j>0 && a[j-1].dist>a[j].dist){
			RestDist temp = a[j];
			a[j] = a[j-1];
			a[j-1] = temp;
			j--;
		}
		i++;
	}
}


// calculates dist of restaurants using Manhattan forumla
int calcDist(restaurant* restPtr){
	int x = lon_to_x(restPtr->lon);
	int y = lat_to_y(restPtr->lat);

	int dist = abs((yegX+cursorX)-x) + abs((yegY+cursorY)-y);
	return dist;
}


void displayRest(RestDist restDist[]){
	int selectedRest = 0; //  which  restaurant  is  selected?
	tft.fillScreen (0);
 	tft.setCursor(0, 0); //  where  the  characters  will be  displayed
 	tft.setTextWrap(false);
	tft.setTextSize(2);
 	for (int16_t i = 0; i < 21; i++) {
 		restaurant r;
 		getRestaurantFast(restDist[i].index , &r);
 		tft.setCursor(0, i*TEXTSIZE);
 		if (i !=  selectedRest) {
 		 // not  highlighted//  white  characters  on  black  background
 		 tft.setTextColor (0xFFFF , 0x0000);
 		}
 		else { 
 		 //  highlighted//  black  characters  on  white  background
 		 tft.setTextColor (0x0000 , 0xFFFF);
 		}
 		tft.print(r.name);

 		//tft.print("\n");
 	}
 	tft.print("\n");
}


void redrawCursor(uint16_t colour) {
  	tft.fillRect(cursorX - CURSOR_SIZE/2, cursorY - CURSOR_SIZE/2,
               CURSOR_SIZE, CURSOR_SIZE, colour);
}


void redrawMap(int oldX, int oldY) {

	int yegOldX = yegX + oldX  - CURSOR_SIZE/2;
	int yegOldY = yegY + oldY - CURSOR_SIZE/2;

	lcd_image_draw(&yegImage, &tft, yegOldX, yegOldY,
                 oldX - CURSOR_SIZE/2, oldY - CURSOR_SIZE/2, CURSOR_SIZE, CURSOR_SIZE);
}


void process() {

	bool do_once = false;

	int xVal = analogRead(JOY_HORIZ);
  	int yVal = analogRead(JOY_VERT);
  	int buttonVal = digitalRead(JOY_SEL);
  	
  	// update decides if anything needs to be redrawn
  	bool update = false;

  	// static members holding current cursor coordinates
  	static int oldX = (DISPLAY_WIDTH - 60)/2;
  	static int oldY = DISPLAY_HEIGHT/2;
  	static int selectedRest = 0;
  
  	// speed of cursor is a percent of max_speed
  	// percent is calculated based on how far joystick is moved
  	const int max_speed = 10;

  	// toggle program state between 0 and 1 based on cursor click
  	if (buttonVal == LOW && OldButtonVal == HIGH){
  		if (state == 0){
	  		Serial.println("Going to state 1");
	  		state = 1;
	  		do_once = true;
	  		selectedRest = 0;
	  	}
	  	else{
	  		Serial.println("Going to state 0");
	  		state = 0;
	  		restaurant temp;
	  		getRestaurantFast(rest_dist[selectedRest].index, &temp);
	  		cursorX = lon_to_x(temp.lon);
	  		cursorY = lat_to_y(temp.lat);
	  		yegX = cursorX - (DISPLAY_WIDTH-60)/2;
	  		yegY = cursorY - DISPLAY_HEIGHT/2;

	  		
	  		yegX = constrain(yegX, 0, YEG_SIZE - (DISPLAY_WIDTH-60));
	  		yegY = constrain(yegY, 0, YEG_SIZE - DISPLAY_HEIGHT);

	  		//order is imp
	  		cursorX = constrain(cursorX - yegX, 5, DISPLAY_WIDTH-60-5);
	  		cursorY = constrain(cursorY - yegY, 5, DISPLAY_HEIGHT-5);

	  		tft.fillScreen (0);

	  		lcd_image_draw(&yegImage, &tft, yegX, yegY,
	                 0, 0, DISPLAY_WIDTH - 60, DISPLAY_HEIGHT); 

	  		oldX = cursorX;
	  		oldY = cursorY;
	  		redrawCursor(TFT_RED);
	  	} 	
  	}

  	// state 0 is displaying map
	if(state == 0){

		TSPoint touch = ts.getPoint();

		// restore pinMode to output after reading the touch
	    // this is necessary to talk to tft display
		pinMode(YP, OUTPUT); 
		pinMode(XM, OUTPUT); 

		// if screen is touched on map, indicate all nearby restaurants by blue dot
		if (touch.z > MINPRESSURE && touch.z < MAXPRESSURE) {
			int16_t screen_x = map(touch.y, TS_MINX, TS_MAXX, DISPLAY_WIDTH-1, 0);
			int16_t screen_y = map(touch.x, TS_MINY, TS_MAXY, DISPLAY_HEIGHT-1, 0);

			if ( (0<=screen_x && screen_x<=DISPLAY_WIDTH-60) && (0<=screen_y && screen_y<DISPLAY_HEIGHT) ){
				restaurant temp;
				for (int i=0; i<NUM_RESTAURANTS; i++){
			 		getRestaurantFast(i, &temp);
			 		int x = lon_to_x(temp.lon);
		  			int y = lat_to_y(temp.lat);

		  			// condition to check if restaurant is visible on current section of map
		  			if ( (x>=yegX && x<=yegX+(DISPLAY_WIDTH-60)) && (y>=yegY && y<=yegY+DISPLAY_HEIGHT) ){
		  				x = constrain(x - yegX, 0, DISPLAY_WIDTH-60);
		  				y = constrain(y - yegY, 0, DISPLAY_HEIGHT);
		  				tft.fillCircle(x, y, RADIUS, BLUE);
		  			}
			 	}
			}
		}

		// check y-axis movement of cursor
		if (yVal < JOY_CENTER - JOY_DEADZONE && (4<cursorY || yegY>0)) {
		  	float percent = ((JOY_CENTER - JOY_DEADZONE - float(yVal))/(JOY_CENTER - JOY_DEADZONE));
		    int speed = max_speed * percent;
		    if (speed>0){
		    	cursorY -= speed; // decrease the y coordinate of the cursor
		    	update = true;
		    }
		    

		}
		else if (yVal > JOY_CENTER + JOY_DEADZONE && (cursorY<DISPLAY_HEIGHT-5 || yegY<YEG_SIZE - DISPLAY_HEIGHT) ) {
		  	float percent = ((float(yVal) - JOY_CENTER - JOY_DEADZONE)/(JOY_CENTER - JOY_DEADZONE));
			int speed = max_speed * percent;
		    if (speed>0){
		    	cursorY += speed;
		    	update = true;
		    }
		}

		// check c-axis movement of cursor
		if (xVal > JOY_CENTER + JOY_DEADZONE && (4<cursorX || yegX>0)) {
		  	float percent = ((float(xVal) - JOY_CENTER - JOY_DEADZONE)/(JOY_CENTER - JOY_DEADZONE));
		  	int speed = max_speed * percent;
		    if (speed>0){
		    	cursorX -= speed;
		    	update = true;
		    }
		}
		else if (xVal < JOY_CENTER - JOY_DEADZONE && (cursorX<DISPLAY_WIDTH-60-5) || yegX<YEG_SIZE - (DISPLAY_WIDTH - 60)) {
			float percent = ((JOY_CENTER - JOY_DEADZONE - float(xVal))/(JOY_CENTER - JOY_DEADZONE));
			int speed = max_speed * percent;
		    if (speed>0){
		    	cursorX += speed;
		    	update = true;
		    }
		}
 

		// draw a small patch of the Edmonton map at the old cursor position before
		// drawing a red rectangle at the new cursor position
		// if condition prevents flickering when not moving
		if (update == true){

			// blockUpdate is set to true if user tries to move beyond the screen edges
		  	bool blockUpdate = false;

		  	// going up
			if (cursorY<4 && yegY>0){
			  	blockUpdate = true;
			  	yegY -= DISPLAY_HEIGHT;
			  	cursorY = DISPLAY_HEIGHT-5;
			}
			// going down
			else if (cursorY>DISPLAY_HEIGHT-5 && yegY<YEG_SIZE - DISPLAY_HEIGHT){
			  	blockUpdate = true;
			  	yegY += DISPLAY_HEIGHT;
			  	cursorY = 4;
			}

		  	//going left
		  	if (cursorX<4 && yegX>0){
		  		blockUpdate = true;
		  		yegX -= DISPLAY_WIDTH-60;
		  		cursorX = DISPLAY_WIDTH-60-5;

		  	}
		  	//going right
		  	else if (cursorX>DISPLAY_WIDTH-60-5 && yegX<YEG_SIZE - (DISPLAY_WIDTH - 60)){
		  		blockUpdate = true;
		  		yegX += DISPLAY_WIDTH-60;
		  		cursorX = 4;
		  	}

		  	// scroll to new section of map
		  	if (blockUpdate){
				cursorX = (DISPLAY_WIDTH-60)/2;
				cursorY = DISPLAY_HEIGHT/2;

		  		yegY = constrain (yegY, 0, YEG_SIZE - DISPLAY_HEIGHT);
		  		yegX = constrain (yegX, 0, YEG_SIZE - (DISPLAY_WIDTH - 60));

		  		lcd_image_draw(&yegImage, &tft, yegX, yegY,
		                 0, 0, DISPLAY_WIDTH - 60, DISPLAY_HEIGHT);
		  	}

		  	else{
		  		// redraw map square and cursor square
		  		redrawMap(oldX, oldY);
		  	}

			// limit cursor so it does not leave map edges
			cursorY = constrain (cursorY, 4, DISPLAY_HEIGHT-5);
		  	cursorX = constrain (cursorX, 4, DISPLAY_WIDTH-60-5);

			// store current coordinates
		  	oldX = cursorX;
		  	oldY = cursorY;
		  	redrawCursor(TFT_RED);
		}
 	}

 	// state 1 is listing 21 nearest restaurants
	else{
	 	restaurant temp;

	 	int oldSelectedRest;

	 	// load restaurants fast using caching techniques
	 	if (do_once){
		 	for (int i=0; i<NUM_RESTAURANTS; i++){
		 		getRestaurantFast(i, &temp);
		 		rest_dist[i].index=i;
		 		rest_dist[i].dist = calcDist(&temp);
		 	}

			// sort and display restaurants according to distance from cursor
			isort(rest_dist, NUM_RESTAURANTS);
			displayRest(rest_dist);

		/*
		 	for (int i=0; i<NUM_RESTAURANTS; i++){
		 		Serial.print(rest_dist[i].index);
		 		Serial.print(" ");
		 		Serial.print(rest_dist[i].dist);
		 		Serial.println();
		 	}
		*/
	 		do_once = false;
	 	}
	 	
	 	// update_1 set to true if cursor moves vertically
		bool update_1 = false;

		// now move the selection up or down
		if (yVal < JOY_CENTER - JOY_DEADZONE && selectedRest>0) {
		    oldSelectedRest = selectedRest;
		   	selectedRest -= 1;
		    update_1 = true;
		}
		else if (yVal > JOY_CENTER + JOY_DEADZONE && selectedRest<20) {
			oldSelectedRest = selectedRest;
		    selectedRest += 1;
		    update_1 = true;
		}

		// update text highlighting and move to the next option
	    if (update_1){
	      	tft.setCursor(0,oldSelectedRest*TEXTSIZE);
	 		getRestaurantFast(rest_dist[oldSelectedRest].index , &temp);
	 		tft.setTextColor (0xFFFF , 0x0000);
	 		tft.print(temp.name);

	 		tft.setCursor(0,selectedRest*TEXTSIZE);
	 		getRestaurantFast(rest_dist[selectedRest].index , &temp);
	 		tft.setTextColor (0x0000 , 0xFFFF);
	 		tft.print(temp.name);
	    }

	}

  	OldButtonVal = buttonVal;
  	delay(20);
}


int main() {
	setup();

  	while (true) {
    	process();
  	}

	Serial.end();
	return 0;
}