# Introduction
FRDM Project for a PONG game using different inputs
# TO DO 
- [ ] IR remote test doesn't work. It send the first signal correct and then it craches receiveing only 0xFFFFFFF, fix it
- [ ] Create test for the display
- [x] Create test for the joystick
- [ ] Create test for the 9DOF
- [ ] Add pretty input getter for joystick and Ir

# Components Used
- FRDMKL25Z
- Joystick Module
- IR Remote
- 9DOF Module
- Buzzer for sound effects
- LEDs for visual effects
- ESP32 module with wifi
- TFT display

# Hes values for the remote
```
CH- -> BA45FF00
CH -> B946FF00
CH+ -> B847FF00
Prev -> BB44FF00
Next -> BF40FF00
Play/Pause -> BC43FF00
VOL- -> F807FF00
VOL+ -> EA15FF00
EQ -> F609FF00
0 -> E916FF00
100+ -> E619FF00
200+ -> F20DFF00
1 -> F30CFF00
2 -> E718FF00
3 -> A15EFF00
4 -> F708FF00
5 -> E31CFF00
6 -> A55AFF00
7 -> BD42FF00
8 -> AD52FF00
9 -> B54AFF00
```
