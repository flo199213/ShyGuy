/**
 * Includes all audio functions
 *
 * @author    Florian Staeblein
 * @date      2024/04/12
 * @copyright © 2024 Florian Staeblein
 */


//===============================================================
// Includes
//===============================================================
#include "esp32-hal-timer.h"
#include "XT_DAC_Audio.h"
#include "HardwareSerial.h"


//===============================================================
// Global variables
//===============================================================
// These globals are used because the interrupt routine was a little unhappy and unpredictable
// using objects, with kernal panics etc. I think this is a compiler / memory tracking issue
// For now any vars inside the interrupt are kept as simple globals

uint32_t NextFillPos = 0;					// position in buffer of next byte to fill
int32_t NextPlayPos = 0;					// position in buffer of next byte to play
int32_t EndFillPos = BUFFER_SIZE;	// position in buffer of last byte+1 that can be filled
uint8_t LastDacValue;							// Next Idx pos in buffer to send to DAC
uint8_t Buffer[BUFFER_SIZE];			// The buffer to store the data that will be sent to the 
uint8_t _dacPin;                   // pin to send DAC data to, presumably one of the DAC pins!
XT_Wav_Class *playItem = 0;       // Play item to play
uint16_t BufferUsedCount = 0;			// how much buffer used since last buffer fill
bool _enabled = false;            // Only plays if enabled

// because of the difficulty in passing parameters to interrupt handlers we have a global
// object of this type that points to the object the user creates.
XT_DAC_Audio_Class *XTDacAudioClassGlobalObject;       

// Timer variable
hw_timer_t * timer = NULL;

//===============================================================
// The main interrupt routine called 50,000 times per second
//===============================================================
void IRAM_ATTR onTimer() 
{
  if (!_enabled)
  {
    return;
  }

	// Sound playing code, plays whatevers in the buffer.
  // If not up against the next fill position then valid data to play
	if (NextPlayPos != NextFillPos)
	{
    // Send value to DAC only of changed since last value else no need
		if (LastDacValue != Buffer[NextPlayPos])
		{
			// value to DAC has changed, send to actual hardware, else we just leave setting as is as it's not changed
			LastDacValue = Buffer[NextPlayPos];

      // Write out the data
			dacWrite(_dacPin, LastDacValue);
		}

    // Move play pos to next byte in buffer
		NextPlayPos++;

    // If gone past end of buffer, 
		if (NextPlayPos == BUFFER_SIZE)
		{
      // Set back to beginning
      NextPlayPos = 0;
    }

    // Move area where we can fill up to to NextPlayPos
		EndFillPos = NextPlayPos - 1;

    // Check if less than zero, if so then 1 less is 
		if (EndFillPos < 0)
    {
      // BUFFER_SIZE (we can fill up to one less than this)
      EndFillPos = BUFFER_SIZE;
    }
	}

  // sounds in Q
	if (playItem != 0)
	{
    BufferUsedCount++;
  }
}


//===============================================================
// Constructor
//===============================================================
XT_Wav_Class::XT_Wav_Class(unsigned char *WavData, uint32_t datasize)
{
  // Create a new wav class object
  SampleRate = (WavData[25] * 256) + WavData[24];
  DataSize = datasize; //(WavData[42] * 65536) + (WavData[41] * 256) + WavData[40] + 44;
  IncreaseBy = float(SampleRate) / 50000;
  Data = WavData;
  Count = 0;
  LastIntCount = 0;
  DataIdx = 44;
  Completed = true;
}

//===============================================================
// Returns next byte
//===============================================================
uint8_t IRAM_ATTR XT_Wav_Class::NextByte()
{
	// Returns the next byte to be played, note that this routine will return values suitable to 
	// be played back at 50,000Hz. Even if this sample is at a lesser rate than that it will be
	// padded out as required so that it will appear to have a 50Khz sample rate
	
	// Note it is up to the calling routine to check if this WAV file has NOT completed playing
	// before calling. If you call it and it has completed playing then it will always return
	// 0x7F (speaker mid point).
	uint16_t IntPartOfCount;
	uint8_t ReturnValue;
	
	if (Completed)
	{
    return 0x7f;
  }
	
	// increase the counter, if it goes to a new integer digit then write to DAC
	Count += IncreaseBy; 
	IntPartOfCount = floor(Count);
	ReturnValue = Data[DataIdx];				// by default we return previous value; 
	
	if (IntPartOfCount > LastIntCount)
	{   
		// gone to a new integer of count, we need to send a new value to the DAC 
		LastIntCount = IntPartOfCount; // crashes on this line with panic
		ReturnValue = Data[DataIdx];
		DataIdx++;

    // End of data, flag end
		if (DataIdx >= DataSize)
		{
			Count = 0;				// reset frequency counter
			DataIdx = 44;			// reset data pointer back to beginning of WAV data
			Completed = true; // mark as completed
		}
	}
	
	return ReturnValue;
}


//===============================================================
// Constructor
//===============================================================
XT_DAC_Audio_Class::XT_DAC_Audio_Class(uint8_t dacPin)
{
  XTDacAudioClassGlobalObject = this; // Set variable to use in ISR
  _dacPin = dacPin;									  // Set dac pin to use
  LastDacValue = 0x7f;								// Set to mid point

  _enabled = false;
}

//===============================================================
// Begins music
//===============================================================
void XT_DAC_Audio_Class::Begin()
{                         
  // Using a prescaler of 80 gives a counting frequency of 1,000,000 (1MHz) and using
  // and calling the function every 20 counts of the freqency (the 20 below) means
  // that we will call our onTimer function 50,000 times a second  
  timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerStart(timer);
  timerAlarm(timer, 20, true, 0);

  // Allow system to settle, otherwise garbage can play for first second
  dacWrite(_dacPin, 0);
  pinMode(_dacPin, INPUT);
  delay(1);
}

//===============================================================
// Enables or disables DAC output
//===============================================================
void XT_DAC_Audio_Class::Enable(bool enable)
{
  if (enable && !_enabled)
  {
    // Set speaker to mid point, stops click at start of first sound
    pinMode(_dacPin, ANALOG);

    // Ramp up to last value
    uint8_t steps = 20;
    for (int index = 0; index < steps; index++)
    {
      dacWrite(_dacPin, LastDacValue * index / steps);
      delay(10);
    }
    dacWrite(_dacPin, LastDacValue);
  }
  else if (!enable && _enabled)
  {    
    // Ramp down to zero value
    uint8_t steps = 20;
    for (int index = 0; index < steps; index++)
    {
      dacWrite(_dacPin, LastDacValue - (LastDacValue * index / steps));
      delay(10);
    }
    dacWrite(_dacPin, 0);
    pinMode(_dacPin, INPUT);
  }
  _enabled = enable;
}

//===============================================================
// Fills buffer from loop
//===============================================================
void XT_DAC_Audio_Class::FillBuffer()
{
	// Fill buffer with the sound to output
	if (NextFillPos == BUFFER_SIZE && 
    EndFillPos != 0 &&
    NextPlayPos != 0)
	{
    NextFillPos = 0;
  }	
	
	// If there are items that need to be played & room for more in buffer
	while (playItem != 0 && NextFillPos != EndFillPos && NextFillPos != BUFFER_SIZE)
	{
    // Get the byte to play and put into buffer
	  uint8_t ByteToPlay = 0x7f;
    if (playItem != 0 &&
      playItem->Completed == false)
    {
      ByteToPlay = playItem->NextByte();
    }
		Buffer[NextFillPos] = ByteToPlay;
    
    // Move to next buffer position
		NextFillPos++;

		if (NextFillPos != EndFillPos)
		{
			if (NextFillPos == BUFFER_SIZE)
			{
        NextFillPos = 0;
      }
		}
	}		
} 

//===============================================================
// Plays wav file
//===============================================================
void XT_DAC_Audio_Class::Play(XT_Wav_Class *Wav)
{
  // Stop current sound
	Stop();

	// Set up this wav to play
	Wav->LastIntCount = 0;
	Wav->DataIdx = 44;
	Wav->Count = 0;
  
  // Will start it playing
  playItem = Wav;
	Wav->Completed = false;
}

//===============================================================
// Stops sound
//===============================================================
void XT_DAC_Audio_Class::Stop()
{
	if (playItem != 0)
	{
		playItem->Completed = true;
	}
}

//===============================================================
// Prints average buffer usage
//===============================================================
void XT_DAC_Audio_Class::AverageBufferUsage()
{
	// returns the average buffer usage over 50 iterations of your main loop. 
	// Call this routine in your main loop and after 50 calls to this
	// routine it will display the avg buffer memory used via the serial link, 
	// so ensure you have enabled serial comms.
	// This routine should only be used to check how much buffer is being used during your
	// code executing in order to optimise how much buffer you need to reserve.
	
	static bool ResultPrinted = false;
	static uint8_t LoopCount = 0;
	
	if ((LoopCount == 50) & (ResultPrinted == false))
	{
		Serial.print("Avg Buffer Usage : ");
		Serial.print(BufferUsedCount / 50);
		Serial.println(" bytes");
		ResultPrinted = true;
	}
	LoopCount++;
}
