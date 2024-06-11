/**
 * ESP32 Shy Guy main file
 *
 * @author    Florian Staeblein
 * @date      2024/04/12
 * @copyright © 2024 Florian Staeblein
 * 
 * ==============================================================
 * 
 * Configuration Wemos S2 Mini:
 * - Board: "ESP32S2 Dev Module"
 * - CPU Frequency: "240MHz (WiFi)"
 * - USB CDC On Boot: "Enabled"   <------------ Important!
 * - USB DFU On Boot: "Disabled"
 * - USB Firmware MSC On Boot: "Disabled"
 * - Flash Size: "4Mb (32Mb)"
 * - Partition Scheme: "No OTA (2MB APP/2MB SPIFFS)"
 * - PSRAM: "Enabled"
 * - Upload Mode: "Internal USB"
 * - Upload Speed: "921600"
 * 
 * -> Leave everything else on default!
 * 
 * Important notice:
 * If the Wemos S2 Mini is programmed via the Arduino IDE, the
 * "USB CDC On Boot" flag must be set at all times. This flag
 * causes the Wemos S2 Mini to report as a COM interface immediately
 * after booting via USB. This means that the microcontroller can
 * be programmed via the Arduino Ide WITHOUT having to press the 
 * "BOOT" and "RESET" buttons again. (This allows the housing of
 * the Aperoliker/Hugoliker to be closed).
 * 
 * ==============================================================
 */

//===============================================================
// Includes
//===============================================================
#include <Arduino.h>
#include <USB.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Wire.h>
#include "ADXL345.h"
#include "SystemHelper.h"
#include "Servo.h"
#include "Face.h"
#include "XT_DAC_Audio.h"
#include "sounddata_Hey.h"
#include "sounddata_GoAway.h"


//===============================================================
// Defines
//===============================================================
// Servo pin defines
#define PIN_SERVO               1     // GPIO 1  -> Servo PWM

// Accelerometer pin defines
#define PIN_ACC_SDA             4     // GPIO 4  -> Accelerometer serial data input/output
#define PIN_ACC_SCL             5     // GPIO 5  -> Accelerometer serial clock

// Display pin defines
#define PIN_TFT_DC              37    // GPIO 37 -> TFT data/command
#define PIN_TFT_RST             38    // GPIO 38 -> TFT reset
#define PIN_TFT_CS              34    // GPIO 34 -> TFT chip select (Not connected! ST7789 is always selected)
#define PIN_TFT_SDA             35    // GPIO 35 -> TFT serial data input/output
#define PIN_TFT_SCL             36    // PGIO 36 -> TFT serial clock

// Sound output pin defines
#define PIN_DAC                 17    // GPIO 17 -> Sound output

// Display definitons
#define SCREEN_WIDTH            240
#define SCREEN_HEIGHT           240

// Angle definitons (Adjust for your setup)
#define ANGLE_OPEN              32
#define ANGLE_CLOSED            130

// Tap recognition
#define TAPS_MAX                10
#define TAPS_WINDOW_MS          2000


//===============================================================
// Global definitions
//===============================================================
typedef enum : int
{
  eClosed,
  eOpening,
  eOpen,
  eClosing
} State;


//===============================================================
// Global variables
//===============================================================
Adafruit_ST7789* tft = NULL;
ADXL345* accelerometer = NULL;
Face* face = NULL;
Servo* servo = NULL;

// Sound output variables, use GPIO 17, one of the 2 DAC pins (DAC1)
XT_Wav_Class* wavFileHey = NULL;
XT_Wav_Class* wavFileGoAway = NULL;
XT_DAC_Audio_Class* dacAudio = NULL;
int16_t activeSound = -1;

// State machine state
State shyGuyState = eClosed;

// Accelerometer tap variables
uint32_t taps[TAPS_MAX];
int16_t tapPointer = 0;

// Timer variables for alive counter
uint32_t aliveTimestamp = 0;
const uint32_t AliveTime_ms = 2000;

// Timer variables for open counter
uint32_t openTimestamp = 0;
uint32_t openTime_ms = 5000;
bool withSound = false;
bool withGoAway = false;


//===============================================================
// Setup function
//===============================================================
void setup(void)
{
  // Start USB CDC, for the case "USB CDC On Boot" is missing in buid defines
  USBSerial.begin();
  USB.begin();

  // Initialize serial communication
  Serial.begin(115200);
  delay(2000);
  Serial.println("[SETUP] Shy Guy V1.0");
  Serial.println(GetSystemInfoString());
  Serial.println();

  // Print restart reason
  Serial.print("[SETUP] CPU0 reset reason: ");
  Serial.println(GetResetReasonString(0));
  Serial.println();

  // Initialize GPIOs
  Serial.println("[SETUP] Initialize GPIOs");
  pinMode(PIN_SERVO, OUTPUT);
  digitalWrite(PIN_SERVO, LOW);

	// Initialize servo pin
  Serial.println("[SETUP] Initialize servo pin");
  servo = new Servo(PIN_SERVO);
  
  // Set the servo position to opened
  servo->SetAngle(ANGLE_OPEN);
  
  // Initialize SPI
  Serial.println("[SETUP] Initialize SPI");
  SPIClass* spi = new SPIClass(HSPI);
  spi->begin(PIN_TFT_SCL, -1, PIN_TFT_SDA, PIN_TFT_CS);

  // Initialize display
  Serial.println("[SETUP] Initialize display");
  tft = new Adafruit_ST7789(spi, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

  // Fill Bootscreen
  tft->init(SCREEN_WIDTH, SCREEN_HEIGHT, SPI_MODE3);
  tft->invertDisplay(true);
  tft->setTextWrap(false);
  tft->setTextSize(2);
  tft->setTextColor(ST77XX_WHITE);
  tft->fillScreen(ST77XX_BLACK);
  tft->setCursor(0, 50);
  tft->println("Booting...");
  
  // Initialize a new face
  Serial.println("[SETUP] Initialize Face");
  tft->println("Init Face");
  face = new Face(tft, SCREEN_WIDTH, SCREEN_HEIGHT, 40);
  face->Expression.GoTo_Normal();

  // Create new face behavior
  face->Behavior.SetEmotion(eEmotions::Normal, 1.0);
  face->Behavior.SetEmotion(eEmotions::Angry, 1.5);
  face->RandomBehavior = true;
  face->RandomBlink = true;
  face->RandomLook = true;
  face->Blink.Timer.SetIntervalMillis(2000);

  // Allow face to settle
  for (int index = 0; index < 10; index++)
  {
    face->Update(ST77XX_WHITE, ST77XX_BLACK, false);
  }

	// Initialize accelerometer
  Serial.println("[SETUP] Initialize Accelerometer");
  tft->println("Init Acc");
  accelerometer = new ADXL345();
  if (!accelerometer->begin(PIN_ACC_SDA, PIN_ACC_SCL))
  {
    if (!accelerometer->begin(PIN_ACC_SDA, PIN_ACC_SCL))
    {
      Serial.println("[SETUP] Error: Could not find ADXL345!");
      tft->println("Error: No ADXL345!");
      while(true) { };
    }
  }

  // Set tap detection on Z-Axis, 5g, 0.5s
  accelerometer->setTapDetectionZ(1);
  accelerometer->setTapThreshold(5);
  accelerometer->setTapDuration(0.5);

  // Select INT 1 for get activities (Even if no INT1 is connected!)
  accelerometer->useInterrupt(ADXL345_INT1);

  // Initialize audio files
  Serial.println("[SETUP] Initialize Audio Files");
  tft->println("Init Files");
  wavFileHey = new XT_Wav_Class(sounddata_data_Hey, sounddata_length_Hey);
  wavFileGoAway = new XT_Wav_Class(sounddata_data_Go_away, sounddata_length_Go_away);

  // Allow interrupts
  sei();

  // Initialize audio ( DAC)
  Serial.println("[SETUP] Initialize Audio (DAC)");
  tft->println("Init Audio");
  dacAudio = new XT_DAC_Audio_Class(PIN_DAC);
  dacAudio->Begin();

  // Final output
  Serial.println("[SETUP] Finished");
  tft->println("Setup Finished");

  // Set the servo position to closed and reset screen
  delay(500);
  servo->SetAngle(ANGLE_CLOSED);
  tft->fillScreen(ST77XX_BLACK);
  dacAudio->Enable(false);
}

//===============================================================
// Loop function
//===============================================================
void loop()
{
  // Show debug alive message
  if ((millis() - aliveTimestamp) > AliveTime_ms)
  {
    aliveTimestamp = millis();

    // Print memory information
    Serial.print("[LOOP] ");
    Serial.println(GetMemoryInfoString());

    //dacAudio->PrintPlayitem();
    //Serial.print("[LOOP] DAC Bufferusage: ");
    //Serial.println(dacAudio->AverageBufferUsage());
  }

  // Read accelerometer values
  Activites activities = accelerometer->readActivites();

  // Check for tap and if last tap is more than 50ms ago (debounce)
  if (activities.isTap &&
    millis() - taps[(tapPointer - 1) % TAPS_MAX] > 50)
  {
    taps[tapPointer] = millis();
    tapPointer = (tapPointer + 1) % TAPS_MAX;
    Serial.println("[LOOP] Tap Detected");
  }

  // State machine
  switch (shyGuyState)
  {
    case eClosed:
      {
        // Check to start shy guy
        if (Is3xKnock() ||
          (Serial.available() &&
          Serial.readString().startsWith("1")))
        {
          // Debug output
          Serial.println("[LOOP] 3x Knock detected");

          // Init face for first time opening
          face->Update(ST77XX_WHITE, ST77XX_BLACK, false);

          // Set random open time (2s, 3s, 4s or 5s)
          openTime_ms = random(1, 5) * 1000;
          withSound = random(0, 100) > 30;
          withGoAway = openTime_ms > 2000;
        
          // Set the servo position to open
          servo->SetAngle(ANGLE_OPEN);
        
          // Start face with a blink
          face->DoBlink();
          face->Update(ST77XX_WHITE, ST77XX_BLACK, true);

          if (withSound)
          {
            // Start sound
            Serial.println("[LOOP] Start playing 'Hey'");
            dacAudio->Enable(true);
            dacAudio->Play(wavFileHey);
            activeSound = 1;
          }

          // Save open time and change state
          openTimestamp = millis();
          shyGuyState = eOpen;
          
          // Debug output
          Serial.println("[LOOP] Opened");
        }
      }
      break;
    case eOpen:
      {
        // Update face
        face->Update(ST77XX_WHITE, ST77XX_BLACK, true);
        
        // Fill audio buffer
        dacAudio->FillBuffer();

        // If sound play finished
        if (activeSound == 1 &&
          wavFileHey->Completed)
        {
          Serial.println("[LOOP] Playing 'Hey' finished");

          if (withGoAway)
          {
            Serial.println("[LOOP] Start playing 'Go away'");
            dacAudio->Play(wavFileGoAway);
            activeSound = 2;
          }
          else
          {
            activeSound = -1;
            dacAudio->Stop();
            dacAudio->Enable(false);
          }
        }
        if (activeSound == 2 &&
          wavFileGoAway->Completed)
        {
          Serial.println("[LOOP] Playing 'Go away' finished");
          activeSound = -1;
          dacAudio->Stop();
          dacAudio->Enable(false);
        }

        // Check for closing
        if (millis() - openTimestamp > openTime_ms)
        {
          // Debug output
          Serial.println("[LOOP] Open time finished -> close");

          shyGuyState = eClosing;
        }
      }
      break;
    case eClosing:
      {
        // Debug output
        Serial.println("[LOOP] Closing started");

        // Set the servo position to closed
        servo->SetAngle(ANGLE_CLOSED);
        
        // Reset screen
        tft->fillScreen(ST77XX_BLACK);

        // Stop sound
        dacAudio->Stop();
        dacAudio->Enable(false);
        Serial.println("[LOOP] Stop playing sound");
        
        // Reset shy guy state
        shyGuyState = eClosed;

        // Debug output
        Serial.println("[LOOP] Closed");
      }
      break;
    default:
      break;
  }
}

//===============================================================
// Returns true if the last three knocks are within the tap window 
//===============================================================
bool Is3xKnock()
{
  uint32_t knockTimeLast = taps[(tapPointer - 1) % TAPS_MAX];
  uint32_t knockTimeFirst = taps[(tapPointer - 3) % TAPS_MAX];

  // The last knock was less than 500ms ago and the three knocks are less than the set window apart
  return millis() - knockTimeLast < 500 &&
    knockTimeLast - knockTimeFirst < TAPS_WINDOW_MS;
}
