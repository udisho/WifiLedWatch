#ifndef SEVENSEGDISPLAY_H   // Check if SEVENSEGDISPLAY_H is not defined
#define SEVENSEGDISPLAY_H   // Define SEVENSEGDISPLAY_H

#include <FastLED.h>
#include <DS3231.h>
#include "GenericDisplay.h"

#define NUM_LEDS_IN_DIGIT 29
#define DEBUG_MODE true  // Set to true or false to enable/disable debug prints

#define DEBUG_PRINT(to_print)  \
    if (DEBUG_MODE) {          \
        Serial.println(to_print); \
    }
#define MAX_BRITHNESS 254
#define NUM_OF_LEDS(numOfDigits) (numOfDigits * NUM_LEDS_IN_DIGIT)



/* 11 10  9  8
  12          7
  13          6
  14          5
  15          4
16  0  1  2  3
  17          28
  18          27
  19          26
  20          25
    21 22 23 24 
*/

extern const unsigned int arrOfOneDigit[13][28];

extern const CRGB colors[];


template<int DATA_PIN, int NUM_OF_DIGITS>
class LedDigiDispaly : public GenericDisplay {
public:

  LedDigiDispaly();
  ~LedDigiDispaly();

  void ShowDigits(int numToShow) /*override*/;
  void ChangeColor()/*override*/ {}
  void RunTestLeds(void)/*override*/{}
  void ShowCONN(void);
  
private:
  const int m_dataPin;
  const int m_numOfLeds;
  CRGB m_leds[NUM_OF_LEDS(NUM_OF_DIGITS)];
  int m_currentColorIndex;
  CRGB m_currentColor;
  void clearLeds();
  //void showOneDigit(int startingLed);

  inline void showOneDigit(int startingLed, int number) {
    for (int i = 0; i < 28; i++) {
      this->m_leds[arrOfOneDigit[number][i] + startingLed] = m_currentColor;
    }
  }

};




//namespace LedDigiDispaly
//{

template<int DATA_PIN, int NUM_OF_DIGITS>  
LedDigiDispaly<DATA_PIN, NUM_OF_DIGITS>::LedDigiDispaly() :
/*GenericDisplay(),*/ m_dataPin(DATA_PIN), m_numOfLeds(NUM_OF_LEDS(NUM_OF_DIGITS)), m_currentColor(CRGB::LightGreen), m_currentColorIndex(0) 
{
   // m_leds = new CRGB[m_numOfLeds];
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(m_leds, NUM_OF_LEDS(NUM_LEDS_IN_DIGIT));
    FastLED.setBrightness(MAX_BRITHNESS);
}

template<int DATA_PIN, int NUM_OF_DIGITS>  
LedDigiDispaly<DATA_PIN, NUM_OF_DIGITS>::~LedDigiDispaly()
{
  //  delete[] m_leds;  
}


template<int DATA_PIN, int NUM_OF_DIGITS>    
void LedDigiDispaly<DATA_PIN, NUM_OF_DIGITS>::ShowDigits(int numToShow) 
{
    printf("printing digit: [%u]\n",numToShow );
     int i = NUM_OF_DIGITS;
     clearLeds();
    for (; i > 0; --i) {
      showOneDigit((i - 1) * NUM_LEDS_IN_DIGIT, numToShow % 10);
      numToShow /= 10;
    }
    FastLED.show();
}

template<int DATA_PIN, int NUM_OF_DIGITS>    
void LedDigiDispaly<DATA_PIN, NUM_OF_DIGITS>::ShowCONN(void) 
{
    printf("printing ShowCONN");
    static const int connWord[] = {11, 12, 13, 13};
    int i = NUM_OF_DIGITS;
    clearLeds();
    for (; i > 0; --i) {
      showOneDigit((i - 1) * NUM_LEDS_IN_DIGIT, connWord[i]);
    }
    FastLED.show();
}

template<int DATA_PIN, int NUM_OF_DIGITS>    
inline void LedDigiDispaly<DATA_PIN, NUM_OF_DIGITS>::clearLeds() {
    for (int i = 0; i < m_numOfLeds; ++i) {
      m_leds[i] = CRGB::Black;
    }
    
  }

//} // namespace LedDigiDispaly 

#endif  // End of header guard