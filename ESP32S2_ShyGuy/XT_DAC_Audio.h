/**
 * Includes all audio functions
 *
 * @author    Florian Staeblein
 * @date      2024/04/12
 * @copyright © 2024 Florian Staeblein
 */


//===============================================================
// Defines
//===============================================================
#define BUFFER_SIZE 4000

//===============================================================
// The Main Wave class for sound samples
//===============================================================
class XT_Wav_Class
{
  public:
    // Constructor
    XT_Wav_Class(unsigned char *WavData, uint32_t datasize);

    uint16_t SampleRate;  
    volatile uint32_t DataSize = 0;     // The last integer part of count
    volatile uint32_t DataIdx = 44;
    volatile unsigned char *Data;  
    volatile float IncreaseBy = 0;      // The amount to increase the counter by per call to "onTimer"
    volatile float Count = 0;           // The counter counting up, we check this to see if we need to send
    volatile int32_t LastIntCount = -1; // The last integer part of count
    volatile bool Completed = true;
    volatile uint8_t LastValue;					// Last value returned from NextByte function
    
    // Returns next byte
    uint8_t NextByte();
};

//===============================================================
// the main class for using the DAC to play sounds
//===============================================================
class XT_DAC_Audio_Class
{
  
	public:
    // Constructor
		XT_DAC_Audio_Class(uint8_t dacPin);

    // Begins music
    void Begin();

    // Enables or disables DAC output
    void Enable(bool enable);

    // Fills buffer from loop
		void FillBuffer();

    // Plays wav file
		void Play(XT_Wav_Class *Wav);

    // Stops sound
		void Stop();
		
    // Returns average Buffer usage
		void AverageBufferUsage();
};

                                                          