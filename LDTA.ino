#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Konfigurace pinů ---
const int ecgPin = A0;             
const int ledPin = 13;             
const int alarmPin = 12;           
const int filterSwitchPin = 2;     


int rawValue = 0;
int filteredValue = 0;
float maxAmp = 512.0;              
float threshold = 350.0;           
bool wasAboveThreshold = false;
const float referenceVoltage = 5.0;


byte graphBuffer[SCREEN_WIDTH];
bool beatBuffer[SCREEN_WIDTH]; // Pamatuje si, kde na displeji bliknout čáru tepu
int graphIndex = 0;
bool beatMarker = false;


unsigned long lastPeakTime = 0;
unsigned long lastMaxDecayTime = 0;
unsigned long lastGraphSample = 0;
unsigned long lastOledDraw = 0;
const int alarmTimeout = 2500;     


const int numBeats = 4;                 
unsigned long rrIntervals[numBeats] = {1000, 1000, 1000, 1000}; 
byte rrIndex = 0;
int bpm = 0;

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(alarmPin, OUTPUT);
  pinMode(filterSwitchPin, INPUT_PULLUP);
  
  Serial.begin(115200);

  // Inicializace OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Chyba OLED!"));
    while(true); 
  }
  
  
  Wire.setClock(400000);
  

  for(int i = 0; i < SCREEN_WIDTH; i++) {
    graphBuffer[i] = 31; 
    beatBuffer[i] = false;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 10);
  display.print("BPC-LDTA EKG");
  display.display();
  delay(1500);
}

void loop() {
  unsigned long currentTime = millis();
  

  rawValue = analogRead(ecgPin);
  if (digitalRead(filterSwitchPin) == LOW) { 
    static float avg = 0;
    avg = (avg * 0.7) + (rawValue * 0.3); 
    filteredValue = (int)avg;
  } else {
    filteredValue = rawValue;
  }


  if (currentTime - lastMaxDecayTime > 2000) {
    maxAmp = maxAmp * 0.9; 
    threshold = maxAmp * 0.7; 
    lastMaxDecayTime = currentTime;
  }
  if (filteredValue > maxAmp) {
    maxAmp = filteredValue;
    threshold = maxAmp * 0.7;
  }

  float hysteresis = maxAmp * 0.05; 

  if (filteredValue > threshold) {
    if (!wasAboveThreshold) {
      digitalWrite(ledPin, HIGH); 
      beatMarker = true; // Značka pro graf!
      
      unsigned long rrInterval = currentTime - lastPeakTime;
      if (rrInterval > 270 && rrInterval < 2000) {
        rrIntervals[rrIndex] = rrInterval;
        rrIndex = (rrIndex + 1) % numBeats;
        
        unsigned long rrAverage = 0;
        for (int i = 0; i < numBeats; i++) {
          rrAverage += rrIntervals[i];
        }
        rrAverage = rrAverage / numBeats;
        bpm = 60000 / rrAverage;
      }
      lastPeakTime = currentTime;
    }
    wasAboveThreshold = true;
  } else if (filteredValue < (threshold - hysteresis)) {
    digitalWrite(ledPin, LOW);
    wasAboveThreshold = false;
  }

  
  bool isAlarm = false;
  if (currentTime - lastPeakTime > alarmTimeout) {
    digitalWrite(alarmPin, HIGH);
    bpm = 0; 
    isAlarm = true;
  } else {
    digitalWrite(alarmPin, LOW);
  }

  if (currentTime - lastGraphSample >= 25) {
 
    int y = map(filteredValue, 0, 1023, 31, 16); 
    graphBuffer[graphIndex] = constrain(y, 16, 31);
    
    // Uložení případného tepu do grafu
    beatBuffer[graphIndex] = beatMarker;
    beatMarker = false; 

    graphIndex = (graphIndex + 1) % SCREEN_WIDTH;
    lastGraphSample = currentTime;
  }


  if (currentTime - lastOledDraw >= 100) {
    display.clearDisplay();
    
    // PŘEPPOČET MAXIMA NA VOLTY
    float maxVolts = maxAmp * (referenceVoltage / 1023.0);
    float thrVolts = threshold * (referenceVoltage / 1023.0);


    display.setCursor(0, 0);
    display.print("BPM:");
    if (bpm == 0) display.print("ERR"); else display.print(bpm);
    display.print(" MAX:");
    display.print(maxVolts, 1); display.print("V");

  
    display.setCursor(0, 8);
    if (isAlarm) display.print("ALARM!"); else display.print("OK");
    display.print("   THR:");
    display.print(thrVolts, 1); display.print("V");

   
    for (int i = 0; i < SCREEN_WIDTH; i++) {

      int bufferPos = (graphIndex + i) % SCREEN_WIDTH;

      display.drawPixel(i, graphBuffer[bufferPos], SSD1306_WHITE);

      if (beatBuffer[bufferPos]) {
        display.drawLine(i, 16, i, 31, SSD1306_WHITE);
      }
    }
    
    display.display();
    lastOledDraw = currentTime;
  }

  delay(2); 
}