#include <JeeLib.h>
#include "PololuLedStrip.h"

//RF part
#define RF69_COMPAT 0  // define this to use the RF69 driver i.s.o. RF12
#define myNodeID 18          //node ID of Rx (range 0-30)
#define network     42      //network group (can be in the range 1-250).
#define RF_freq RF12_433MHZ     //Freq of RF12B can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. Match freq to module

//LEDs part
// Create a buffer for holding the colors (3 bytes per color).
#define LED_CloakCount 13
#define LED_NeckCount 8
#define LED_COUNT 50

long previousMillisVoltageLED = 0;        // will store last time LED was updated

long VoltageOnTime = 150; //the time in ms in which the RCG LED it going to blink
long VoltageOffTime = 4850; //the time in ms in which the RCG LED it going to blink

// voltage reference for main power supply after voltage divider - LiPo 3S - 12.6v-9.6V -> 5v-0v
const int vHigh = 520; // = 10.8v
const int vMedium = 260; // = 9.6v
int pinVoltage = A3;

//comm vars
//data1 = red,
//data2 = green,
//data3 = blue,
//data4 = function - bit 0(flicker, solid), bit1(low, high), bit2(random color, different color),
//                   bit3(Patern1, Patern2), bit4(Patern3, Patern4)
typedef struct { int data1, data2, data3, data4; } PayloadTX;      // create structure - a neat way of packaging data for RF comms
PayloadTX emontx;
const int emonTx_NodeID=10;            //emonTx node ID - The Tx NodeID

// Create an ledStrip object and specify the pin it will use.
PololuLedStrip<6> ledStrip;

int pinBotton = 4;

// Variables will change:
int ledState = HIGH;         // the current state of the output pin
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin
int colorState = 0;

int statusPattern = 0;	//the status of the pattern for the leds

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 75;    // the debounce time; increase if the output flickers

long chaseDelay = 10; //the time each action is given
long ChaseTime;
long lastChaseTime;

static int iChase = 0;

long timeFader;
int periodeFader = 5000;
int valueFaderRed;
int valueFaderGreen;
int valueFaderBlue;

int cofR;
int cofG;
int cofB;

int oneShotFade;

rgb_color destColor;
rgb_color colors[LED_COUNT];
rgb_color colors_black[LED_COUNT];
int CloakLed[LED_CloakCount]={4,5,6,7,8,9,10,18,20,21,25,26,31};
int NeckLed[LED_NeckCount]={19,22,23,24,27,28,29,30};

void setup() {
	pinMode(pinBotton, INPUT);
	pinMode(pinVoltage,INPUT);
	digitalWrite (pinBotton, HIGH); //pull up
	
	rf12_initialize(myNodeID,RF_freq,network);   //Initialize RFM12 with settings defined above
	Serial.begin(9600);
	Serial.println("RF12B Cloak Receiver");
	
	Serial.print("Node: ");
	Serial.print(myNodeID);
	Serial.print(" Freq: ");
	if (RF_freq == RF12_433MHZ) Serial.print("433Mhz");
	if (RF_freq == RF12_868MHZ) Serial.print("868Mhz");
	if (RF_freq == RF12_915MHZ) Serial.print("915Mhz");
	Serial.print(" Network: ");
	Serial.println(network);
	for(uint16_t i = 0; i < LED_COUNT; i++)
	{
		colors[i] = (rgb_color){0, 0, 0 };
	}
	//"arm" the onshot
	oneShotFade = 0;
}
/*
void chaseLED()
{
	// in order to not use delay we use this mechanisem.. each light-action is given 10 ms to preform
	// after that we return to the main loop and check it the statusPattern or other variable changed.
	if (millis() - lastChaseTime > chaseDelay)
	{
		colors[iChase] = (rgb_color){};
		lastChaseTime = millis();
	}
	
}
*/

void loop() {
	
	//if (analogRead(pinVoltage) < vMedium)
	//	statusPattern = 9;
	checkBotton();
	//statusPattern=1;
	//Serial.println(analogRead(pinVoltage));
	//statusPattern: 0= solid, no action, 1=flicker, 2=fader, 3=kill all lights, 4 = low voltage - flickering red
	switch (statusPattern)
	{
		case 0: //solid color
			changeColor(destColor.red,destColor.green,destColor.blue);
			ledStrip.write(colors, LED_COUNT);
			break;//do nothing for now... TBD
		case 1: //flicker
			//color_blakc is an array with 0s = black.
			ledStrip.write(colors_black, LED_COUNT);
			delay(20);
			break;
		case 2: //fader
			//Serial.println("statusPattern 2");
			//snapshot the current state in order to fade to those values
			if (!oneShotFade)
			{
				// 4 is our reference because we know for sure its part of the fabric
				cofR = destColor.red/2;
				cofG = destColor.green/2;
				cofB = destColor.blue/2;
				oneShotFade = 1; //disarm oneshot
				//Serial.println("statusPattern 2 oneshot");
				//Serial.println(cofR);
				//Serial.println(cofG);
				//Serial.println(cofB);
			}
			timeFader = millis();
			valueFaderRed = cofR+(cofR-1)*cos(2*PI/periodeFader*timeFader);
			valueFaderGreen = cofG+(cofG-1)*cos(2*PI/periodeFader*timeFader);
			valueFaderBlue = cofB+(cofB-1)*cos(2*PI/periodeFader*timeFader);
			changeColor(valueFaderRed,valueFaderGreen,valueFaderBlue);
			break;
		case 3: // kill all lights
			destColor.red = 0;
			destColor.green = 0;
			destColor.blue = 0;
			statusPattern = 0;
			break;
		//case 4:
		//	chaseLED();
		//case 9:
		//	changeColor(255,0,0);
		//	ledStrip.write(colors, LED_COUNT);
		//	delay(200);
		//	changeColor(0,0,0);
		//	ledStrip.write(colors, LED_COUNT);
		//	delay(1800);
		//	break;
		//default: //if all fails it means something is wrong....
		//	changeColor(emontx.data1,emontx.data2,emontx.data3);
		//	break;
	}
	
	if (statusPattern != 9)
	{
		ledStrip.write(colors, LED_COUNT);
		delay(20);	
	}
	
	if (rf12_recvDone()){
		if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)
		{
			
			int node_id = (rf12_hdr & 0x1F);		  //extract nodeID from payload
			//checks data is coming from node with the correct ID
			if (node_id == emonTx_NodeID)
			{
				emontx=*(PayloadTX*) rf12_data;            // Extract the data from the payload
				destColor.red=emontx.data1;
				destColor.green=emontx.data2;
				destColor.blue=emontx.data3;
				changeColor(emontx.data1,emontx.data2,emontx.data3);
				statusPattern = emontx.data4;
				if (statusPattern == 2)
				{
					oneShotFade = 0;
					//destColor.red=emontx.data1;
					//destColor.green=emontx.data2;
					//destColor.blue=emontx.data3;
				}
				Serial.print("data1: "); Serial.println(emontx.data1);
				Serial.print("data2: "); Serial.println(emontx.data2);
				Serial.print("data3: "); Serial.println(emontx.data3);
				Serial.print("data4: "); Serial.println(emontx.data4);
				Serial.println("  ");
			}
		}
	}
}
void checkBotton()
{
	// read the state of the switch into a local variable:
	int reading = digitalRead(pinBotton);
	
	// If the switch changed, due to noise or pressing:
	if (reading != lastButtonState) {
		// reset the debouncing timer
		lastDebounceTime = millis();
	}
	
	if ((millis() - lastDebounceTime) > debounceDelay) {
		// whatever the reading is at, it's been there for longer
		// than the debounce delay, so take it as the actual current state:

		// if the button state has changed:
		if (reading != buttonState) {
			buttonState = reading;
			// set the LED:
			if (buttonState == LOW)
			{
				oneShotFade =0;//rearm oneshot
				switch (colorState)
				{
					case 0:
						destColor.red = 255;
						destColor.green = 0;
						destColor.blue = 0;
						colorState++;
						break;
					case 1:
						destColor.red = 0;
						destColor.green = 255;
						destColor.blue = 0;
						colorState++;
						break;
					case 2:
						destColor.red = 0;
						destColor.green = 0;
						destColor.blue = 255;
						colorState++;
						break;
					case 3:
						random_color();
						colorState++;
						break;
					case 4:
						destColor.red = 0;
						destColor.green = 0;
						destColor.blue = 0;
						changeColor(destColor.red,destColor.green,destColor.blue);
						colorState=0;
						break;
				}
				// only toggle the LED if the new button state is HIGH
			}
			
		}
		
	}
	


	// save the reading.  Next time through the loop,
	// it'll be the lastButtonState:
	lastButtonState = reading;
}
void random_color()
{
	destColor.red = random(0, 255);
	destColor.green = random(0, 255);
	destColor.blue = random(0, 255);
	changeColor(destColor.red,destColor.green,destColor.blue);
}
void changeColor (int rColor, int gColor, int bColor)
{
	int j=0;
	int k=0;
	for (int i;i<=LED_COUNT;i++)
	{
		// brights color of the LED
		if (CloakLed[j] == i) //works only on cloak LEDs
		{
			colors[i] = (rgb_color){rColor, gColor, bColor };
			j++;
		}
		//for softer/low light on the neck
		if (NeckLed[k] == i) //works only on cloak LEDs
		{
			colors[i] = (rgb_color){(rColor/10), (gColor/10), (bColor/10) };
			k++;
		}
	}
}