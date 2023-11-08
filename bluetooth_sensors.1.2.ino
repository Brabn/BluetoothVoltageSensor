/* 
	Modified firmware for connecting ADS1115 ADC
	to the circuit with temperature sensor DS18B20 and transmission
	data via bluetooth
	version 1.1 dated 02.08.2017
	
*/

#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>	// Library for ADS1015 and ADS1115
#include <OneWire.h>

#define vInPin	A2
#define dsPin	4			// Temperature sensor pin
#define led 13

#define ADSAlarmPin 5		// The pin to which the signal about the start of measurement will be output
#define ADSPause 1000		// Delay before starting ADC measurements, ms
#define ADSCount 3 			// Number of ADC measurements
#define ADSDelay 300		// Pause between ADC measurements, ms

#define PowerVolageResisitor 1000	// Pause between input power voltage measurements, ms
#define PowerVolageRatio 2.5 		// Сoefficient on the lowering resistor. R2/(R1+R2)=4kOhm/(6kOhm+4kOhm)
#define TemperatureDelay 1000		// Pause between temperature measurements, ms


float  ADSGain =0.015625;	// ADC resolution, mV
Adafruit_ADS1115 ads;		// ADC type - ADS1115 
SoftwareSerial mySerial(2,3);
OneWire ds(dsPin);
float measureAverage=0;		// Average of all measurements
float measures[ADSCount];	// Measurement results
bool measureNow=false;		// Measurement mode
bool measureDelay=false;	// Delay  before measurement mode
bool measureFinish=false;	// Measurement completion mode
int measureCount=0;			// Measurement counter
short measureMode=0;		// Output type at the end of measurements
unsigned long measureTime=0;
unsigned long measureTimer=0;// Timer for delays


float temp,vIn,vInTemp;
int send,dsPresent=0;
long vtime,ttime;
byte data[12], addr[8], type_s,i;
byte name[8]={'A','I','A','0','0','0','0','1'};
int mas[10],masPtr;
void setup() {
  Serial.begin(9600);
mySerial.begin(9600);
ads.setGain(GAIN_EIGHT);    // set the ADC multiplier to 8x, range +/-0.512V 1 bit =0.25mV
							// !!  DO NOT EXCEED 0.5+0.3=0.8 volts at the ADC input - the chip will burn out!!!!
ads.begin();				// ADC initialization							

pinMode(ADSAlarmPin,OUTPUT);
digitalWrite(ADSAlarmPin,LOW);

pinMode(led,OUTPUT); digitalWrite(led,LOW);

delay(1000);
mySerial.println("AT"); delay(1000);
mySerial.print("AT+NAME");
mySerial.println("AIA00001"); 
temp=0;send=0;vtime=millis(); ttime=millis();
dsInit(); dsStart();  delay(700); 

}

void loop() {

if (measureDelay) 					// If we are in delay mode before measuring
{
	if ((millis() - measureTimer )>=ADSDelay)		//If the delay time has expired
	{
		measureDelay=false;	
		measureNow = true;			// Switch to measurement mode
		measureTimer=millis();		// Start the timer for measurement
		measureCount=0;				// Let's start with the first measurement
	}
}
if (measureNow)						// If we are in measurement mode
{
	if (measureCount<ADSCount)		// If not all measurements have been taken yet
	{
		if (( millis()-measureTimer)>=ADSPause)	// If the delay time between measurements has expired
		{
			measures[measureCount]=ADSGain * (float)ads.readADC_SingleEnded(0);	// We measure the voltage (taking into account the multiplier)
			measureAverage+=measures[measureCount];								// Add to the calculation of the average
			measureCount++;														// Next measurement
			measureTimer=millis();												// Start the timer for the next measurement
		}
	}
	else			// If all measurements have been completed
	{
		measureAverage=measureAverage/ADSCount;		// Calculate the average
		digitalWrite(ADSAlarmPin, LOW);				// Turn off the measurement alarm
		measureCount=0;								// Resetting the number of measurements
		measureNow=false;
		measureDelay=false;
		measureFinish=true;							//Switch to the measurement completion mode
	}
}
	
  if(mySerial.available()>0) { 
	  switch(mySerial.read()) {
		  case 's': send=1; break;
		  case 'S': send=1; break;
		  case 'n': send=2; break;		//Request to display all parameters
		  case 'N': send=2; break;
		  case 'u': send=3; break;
		  case 'U': send=3; break;
		  case 't': send=4; break;
		  case 'T': send=4; break;
		  case 'q': send=5; break;		// Request only voltage output	
		  case 'Q': send=5; break;

	  }
  }

 digitalWrite(led,HIGH);
switch(send) {
	case 1: 
		measureBegin();		// Let's start measuring
		measureMode=1;		// At the end of the measurement we will display all the data
	send=0; 
	break;
	case 2: mySerial.print("N:");
        for(i=0;i<sizeof(name);i++) {mySerial.write(name[i]);}
        mySerial.println();
        send=0;	break;
	case 3: mySerial.println(vIn); send=0; break;
	case 4: mySerial.println(temp); send=0; break;
	case 5: 

		measureBegin(); 	// Let's start measuring
		measureMode=2;		// At the end of the measurement, we will output only the voltage
		send=0; 
	
	break;
	case 6: mySerial.println("No DS18B20"); break;
}
if (measureFinish)		// If measurements are completed
		{
			if (measureMode==1)		// Display mode of all parameters
			{
			mySerial.print("U:");
			mySerial.print(vIn);
			mySerial.print(" T:");
			mySerial.print(temp);
			mySerial.print(" Q:");
			mySerial.println(measureAverage);
			
			}
			else if (measureMode==2)	// Voltage only output mode
			{
				mySerial.print("Q:");
				mySerial.println(measureAverage);
			}
			measureMode=0;			// reset mode
			measureFinish=false;	// Exiting the end of measurements mode
		}
		
digitalWrite(led,LOW);

if(millis()>vtime+PowerVolageDelay) { vInTemp=analogRead(vInPin); vIn=(((float)5/1023)*vInTemp*PowerVolageRatio);  vtime=millis();}	// Read cuurent power input voltage
if(millis()>ttime+TemperatureDelay) {temp=22;dsRead(); dsStart(); ttime=millis(); }									// Read cuurent temperature voltage
}
//---------------------------------------------------------------------
void measureBegin()		// Start of measurements
{
	if ((!measureDelay)&&(!measureNow)&&(!measureFinish))		//If we are not in the process of measuring
	{
	measureTimer=millis();		// Start the delay timer
	measureDelay=true;			// Switching to delay mode before measurements
	measureNow=false;
	measureFinish=false;
	measureAverage=0;			// Reset the previous result
	digitalWrite(ADSAlarmPin, HIGH);	// Turn on the measurement alarm
	}
}
void dsStart() {
	ds.reset(); delay(1); ds.skip(); delay(1); ds.write(0x44);}

void dsInit() {
if(!ds.search(addr)) {dsPresent=0;} else {dsPresent=1;}
ds.reset_search();
dsPresent=ds.reset(); ds.select(addr);
switch (addr[0]) {
case 0x10: // or older DS1820
type_s = 1; break;
case 0x28: // DS18B20
type_s = 0; break;
case 0x22: // DS1822
type_s = 0; break;
default: // Device is not a DS18x20 family device.
break;
}
}

void dsRead(){
dsPresent = ds.reset(); delay(10);
//ds.select(addr); delay(10);
ds.skip();
ds.write(0xBE); delay(100);
for ( i = 0; i < 9; i++) { data[i] = ds.read();}
int16_t raw = (data[1] << 8) | data[0];
if (type_s) {			//type_s
raw = raw << 3; 		// resolution 9 bits by default
if (data[7] == 0x10) {
raw = (raw & 0xFFF0) + 12 - data[6];
}
} else {
byte cfg = (data[4] & 0x60);
//for small values, small bits are not defined, let's reset them
if (cfg == 0x00) raw = raw & ~7; // resolution 9 bit, 93.75 ms
else if (cfg == 0x20) raw = raw & ~3; // resolution 10 bit, 187.5 ms
else if (cfg == 0x40) raw = raw & ~1; // resolution 11 bit, 375 ms
//// default resolution is 12 bits, conversion time is 750 ms
}
temp = (float)raw / 16.0;
};
