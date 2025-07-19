#ifndef _GENERIC_DISPLAY_
#define _GENERIC_DISPLAY_

class GenericDisplay
{
public:
    virtual void RunTestLeds(void);
    virtual void ShowDigits(int numToShow);
    virtual void ShowCONN(void);
    virtual void ChangeBrightness(int brightness);
    virtual void ChangeColor(CRGB color);
};

#endif // _GENERIC_DISPLAY_