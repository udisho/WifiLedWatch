#ifndef _GENERIC_DISPLAY_
#define _GENERIC_DISPLAY_

class GenericDisplay
{
public:
    virtual void RunTestLeds(void);
    virtual void ShowDigits(int numToShow);
    virtual void ChangeColor(void);
};

#endif // _GENERIC_DISPLAY_