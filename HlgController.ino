//
// HlgController.ino, 201805, Aad Slingerland.
//
#define PIN_CE 52                         // Chip Enable  (VMA301)
#define PIN_IO 50                         // Input/Output (VMA301)
#define PIN_SC 48                         // Serial Clock (VMA301)
#define PIN_RELAIS 40                     // The relais (VMA406)
#define PIN_PWM 3                         // PWM enabled pin
#define DUTY_MIN 0                        // minimum value of duty_level
#define DUTY_MAX 255                      // maximum value of duty_level
//
#define SUNRISE2_HOUR 16                  // four in the afternoon
#define SUNSET2_HOUR  10                  // ten in the morning
#define SUNRISE3_HOUR 16                  // four in the afternoon
#define SUNSET3_HOUR  04                  // four in the morning
//
#define DELAY_BUTTON 500UL                // ReadLcdButtonTimed (Unsigned Long)
#define DELAY_P1 3000UL                   // Delay for P1 before switching on
//
const unsigned long countdown = 60 * 15;  // sunrise/sunset time in seconds
unsigned long step_time = 0;              // time in milliseconds
byte duty_level = DUTY_MIN;               // PWM duty level value
const String APP = "HlgController";
//
// For this DS1302 library see also:
// http://digitalab.org/2017/08/real-time-clock-ds1302 for the DS1302 library.
//
#include <DS1302.h>
DS1302 rtc (PIN_CE, PIN_IO, PIN_SC);
//
// Make an LCD object with the standard pins used on the VMA203.
//
#include <LiquidCrystal.h>
LiquidCrystal lcd (8, 9, 4, 5, 6, 7);
//
// Buttons defined.
//
enum Buttons : byte
{
  BTN_NONE,
  BTN_SELECT,
  BTN_LEFT,
  BTN_DOWN,
  BTN_UP,
  BTN_RIGHT
};
//
// Analogous values for buttons (VMA101 + VMA203).
//
enum AnalogousValues : int
{
  BTN_NONE_VAL   = 1023,
  BTN_SELECT_VAL = 640,
  BTN_LEFT_VAL   = 409,
  BTN_DOWN_VAL   = 254,
  BTN_UP_VAL     = 98,
  BTN_RIGHT_VAL  = 0,
  BTN_RANGE      = 50
};
//
// Determine which button has been pressed.
//
int ReadLcdButton ()
{
  int btn_val = analogRead (0);
  //
  // Serial.print   ("ReadLcdButton: btn_val=");
  // Serial.println (btn_val);
  //
  if (btn_val > (BTN_NONE_VAL   - BTN_RANGE)) return BTN_NONE;
  if (btn_val > (BTN_SELECT_VAL - BTN_RANGE)) return BTN_SELECT;
  if (btn_val > (BTN_LEFT_VAL   - BTN_RANGE)) return BTN_LEFT;
  if (btn_val > (BTN_DOWN_VAL   - BTN_RANGE)) return BTN_DOWN;
  if (btn_val > (BTN_UP_VAL     - BTN_RANGE)) return BTN_UP;
  if (btn_val > (BTN_RIGHT_VAL  - BTN_RANGE)) return BTN_RIGHT;
  //
  Serial.println ("LcdReadButton: this line should not occur.");
  //
  return BTN_NONE;
}
//
// This method prevents button presses to overflow the state machine
// by keeping the last button pressed noted for a short while.
//
byte ReadLcdButtonTimed ()
{
  static byte last_button = 0;
  static unsigned long last_button_time = 0;
  //
  byte button = ReadLcdButton ();
  //
  Serial.print   ("ReadLcdButtonTimed: button=");
  Serial.println (button);
  //
  // Only perform the timed delay for buttons other then BTN_NONE.
  //
  if (button != BTN_NONE)
  {
    if (button != last_button)
    {
      last_button = button;
      last_button_time = millis ();
      //
      Serial.print   ("ReadLcdButtonTimed: last_button_time=");
      Serial.println (last_button_time);
    }
    else
    {
      unsigned long timed = DELAY_BUTTON - (millis () - last_button_time);
      //
      Serial.print   ("ReadLcdButtonTimed: delay=");
      Serial.println (timed);
      //
      delay (timed);
      last_button = BTN_NONE;
      button = BTN_NONE;
    }
  }
  else
  {
    last_button = BTN_NONE;
  }
  return button;
}
//
// A small class to encapsulate the operation of a VMA405 relay.
//
class Relay
{
  public:
    Relay (byte pin)
    {
      _pin = pin;
    }
    void On ()
    {
      pinMode (_pin, OUTPUT);  
      digitalWrite (_pin, HIGH);  
    }
    void Off ()
    {
      pinMode (_pin, OUTPUT);  
      digitalWrite (_pin, LOW);  
    }
  private:
    byte _pin;
};
Relay rel (PIN_RELAIS);
// 
enum Main_states : byte
{
  STATE_P0 = 0,
  STATE_P1 = 1,
  STATE_P2 = 2,
  STATE_P3 = 3
};
byte current_main_state;
//
enum P1_states : byte
{
  P1_WAITING = 0,
  P1_ON      = 1
};
byte current_p1_state = P1_WAITING;
//
enum P2_states : byte
{
  P2_OFF     = 0,
  P2_SUNRISE = 1,
  P2_ON      = 2,
  P2_SUNSET  = 3
};
byte current_p2_state = P2_OFF;
boolean dirty_p2_state = true;
//
void setup ()
{
  Serial.begin   (9600);
  Serial.println (" ");
  Serial.println (APP);
  //
  lcd.begin (16, 2);
  lcd.clear ();
  //
  // The previous current_main_state is saved in clock module. Get it from there.
  //
  byte b0 = rtc.readRam (0);
  if (b0 >= STATE_P0 && b0 <= STATE_P3)
  {
    current_main_state = b0;
  }
  else
  {
    current_main_state = STATE_P0;
  }
  //
  Serial.print   ("setup: restored current_main_state=");
  Serial.println (current_main_state);
  //
  // How long takes each step (255 in total) to switch from fully on
  // (duty_level = 255) to fully off (duty_level = 0) or vice versa.
  //
  step_time = (countdown * 1000 / DUTY_MAX);
  //
  Serial.print   ("setup: step_time=");
  Serial.println (step_time);
}
//
void loop ()
{
  //
  // Small delay for debugging in serial monitor.
  // Comment this out before running in production.
  //
  // delay (100);
  //
  // The date and time on the first row.
  //
  lcd.setCursor (0, 0);
  Time t = rtc.time ();
  char str[17];
  snprintf (str, sizeof(str), "%04d%02d%02d  %02d%02d%02d",
            t.yr, t.mon, t.date, t.hr, t.min, t.sec);
  lcd.print (str);
  //
  Serial.print   ("loop: date_and_time=");
  Serial.println (str);
  //
  // The current main state (program) on the second row.
  //
  lcd.setCursor (7, 1);
  lcd.print ("P");
  lcd.print (current_main_state);
  //
  // The current sub-state (if applicable) just after that.
  //
  if (current_main_state == STATE_P1)
  {
    lcd.print (current_p1_state);
  }
  else if (current_main_state == STATE_P2 || current_main_state == STATE_P3)
  {
    lcd.print (current_p2_state);
  }
  else
  {
    lcd.print ("  ");
  }
  //
  // The duty_level expressed as a percentage on the second row.
  // Note that the map function is used to inverse the duty_level
  // values because the PWM signal amplifier does an inversion.
  //
  int p_num = map (duty_level, 0, 255, 100, 0);
  char p_str[6];
  snprintf (p_str, sizeof(p_str), "%3d%%", p_num);
  lcd.setCursor (12, 1);
  lcd.print (p_str);
  //
  // Set the cursor for the button text.
  //
  lcd.setCursor (0, 1);
  //
  byte button = ReadLcdButtonTimed ();
  //
  switch (button)
  {
    case BTN_NONE:
      {
        lcd.print ("NONE  ");
        break;
      }
    case BTN_SELECT:
      {
        lcd.print ("SELECT");
        set_state_select ();
        break;
      }
    case BTN_LEFT:
      {
        lcd.print ("LEFT  ");
        break;
      }
    case BTN_DOWN:
      {
        lcd.print ("DOWN  ");
        set_state_down ();
        break;
      }
    case BTN_UP:
      {
        lcd.print ("UP    ");
        set_state_up ();
        break;
      }
    case BTN_RIGHT:
      {
        lcd.print ("RIGHT ");
        break;
      }
  }
  //
  run_main ();
}
//
// Switch to a next main state (program) when the SELECT button is pressed.
//
void set_state_select ()
{
  switch (current_main_state)
  {
    case STATE_P0:
      current_main_state = STATE_P1;
      break;

    case STATE_P1:
      current_main_state = STATE_P2;
      break;

    case STATE_P2:
      current_main_state = STATE_P3;
      break;

    case STATE_P3:
      current_main_state = STATE_P0;
      break;
  }
  //
  // Save current main state for Setup ().
  //
  rtc.writeRam (0, current_main_state);
  //
  // Indicate that an initial substate for P2 P3 should be done.
  //
  dirty_p2_state = true;
}
//
// Switch to a higher main state (program) when the UP button is pressed.
//
void set_state_up ()
{
  switch (current_main_state)
  {
    case STATE_P0:
      break;

    case STATE_P1:
      current_main_state = STATE_P0;
      break;

    case STATE_P2:
      current_main_state = STATE_P1;
      break;

    case STATE_P3:
      current_main_state = STATE_P2;
      break;
  }
  //
  // Save current main state for Setup ().
  //
  rtc.writeRam (0, current_main_state);
  //
  // Indicate that an initial substate for P2 P3 should be done.
  //
  dirty_p2_state = true;
}
//
// Switch to a lower main state (program) when the DOWN button is pressed.
//
void set_state_down ()
{
  switch (current_main_state)
  {
    case STATE_P0:
      current_main_state = STATE_P1;
      break;

    case STATE_P1:
      current_main_state = STATE_P2;
      break;

    case STATE_P2:
      current_main_state = STATE_P3;
      break;

    case STATE_P3:
      break;
  }
  //
  // Save current main state for Setup ().
  //
  rtc.writeRam (0, current_main_state);
  //
  // Indicate that an initial substate for P2 P3 should be done.
  //
  dirty_p2_state = true;
}
//
// Run one of the main programs (state).
//
void run_main ()
{
  //
  // First the value of current_p1_state is set to P1_WAITING because the
  // run_P1 () function has no way to find out when to do so.
  //
  if (current_main_state != STATE_P1)
  {
    current_p1_state = P1_WAITING;
  }
  //
  switch (current_main_state)
  {
    case STATE_P0:
      run_P0 ();
      break;

    case STATE_P1:
      run_P1 ();
      break;

    case STATE_P2:
      run_P2 (SUNRISE2_HOUR, SUNSET2_HOUR);
      break;

    case STATE_P3:
      run_P2 (SUNRISE3_HOUR, SUNSET3_HOUR);
      break;
  }
}
//
// Program 0. Switch off immediate.
//
void run_P0 ()
{
  Serial.println ("run_P0");
  //
  rel.On ();
  duty_level = DUTY_MAX;
  analogWrite (PIN_PWM, duty_level);
}

//
// Program 1. Switch on but with a small delay.
//
void run_P1 ()
{
  Serial.println ("run_P1");
  //
  static unsigned long entry_time = 0;
  boolean rb = false;
  //
  switch (current_p1_state)
  {
    case P1_WAITING:
      if (entry_time == 0)
      {
        Serial.println ("run_P1: switching off.....");
        //
        rel.On ();
        duty_level = DUTY_MAX;
        analogWrite (PIN_PWM, duty_level);
        //
        entry_time = millis ();
        //
        Serial.print   ("run_P1: entry_time=");
        Serial.println (entry_time);
      }
      rb = HasTimedWaitPassed (entry_time, DELAY_P1);
      if (rb == true)
      {
        current_p1_state = P1_ON;
        entry_time = 0;                   // reset this for the next time
        //
        Serial.println ("run_P1: switching to P1_ON state");
      }
      break;

    case P1_ON:
      entry_time = 0;                     // reset this for the next time
      //
      rel.Off ();
      duty_level = DUTY_MIN;
      analogWrite (PIN_PWM, duty_level);
      break;
  }
}
//
// Program 2. Switch on the LEDS for a number of hours per day depending on
// the parameters passed as SUNRISE and SUNSET times. This routine is used
// for both P2 and P3 but with different parameter values.
//
void run_P2 (byte this_sunrise, byte this_sunset)
{
  Serial.print   ("run_P2: this_sunrise=");
  Serial.print   (this_sunrise);
  Serial.print   (", this_sunset=");
  Serial.println (this_sunset);
  //
  boolean rb;
  Time this_time = rtc.time();
  //
  Serial.print   ("run_P2: millis=");
  Serial.println (millis ());
  //
  // Are we in the first (half) second that this program is runing?
  // Or, was the main program state changed from 2 to 3 or vice versa?
  // If so, proceed with adjusting the initial current_p2_state.
  // Adjusting the initial state is important for the very first time this
  // program runs and, more important, in the case of a power outage.
  //
  if (dirty_p2_state == true)
  {
    dirty_p2_state = false;
    current_p2_state = P2_OFF;             // assume this, unless proven otherwise
    //
    if (this_time.hr == this_sunrise)      // inside the sunrise hour?
    {
      if (this_time.min < (countdown / 60))// inside the countdown?
      {
        current_p2_state = P2_SUNRISE;     // a new awakening
      }
      else
      {
        current_p2_state = P2_ON;           // sunrise already done
      }
    }
    else if (this_time.hr > this_sunrise)   // already between sunrise and midnight?
    {
      current_p2_state = P2_ON;
    }
    else if (this_time.hr < this_sunset)    // between midnight and sunset?
    {
      current_p2_state = P2_ON;
    }
    //
    Serial.print   ("run_P2: calculated initial state=");
    Serial.println (current_p2_state);
    //
  }
  //
  switch (current_p2_state)
  {
    case P2_OFF:
      rel.On ();
      duty_level = DUTY_MAX;
      analogWrite (PIN_PWM, duty_level);
      //
      if (this_time.hr == this_sunrise)
      {
        current_p2_state = P2_SUNRISE;
        //
        Serial.println ("run_P2: switching to P2_SUNRISE state");
      }
      break;

    case P2_SUNRISE:
      rel.Off ();
      rb = SunRise ();
      if (rb == true)
      {
        current_p2_state = P2_ON;
        //
        Serial.println ("run_P2: switching to P2_ON state");
      }
      break;

    case P2_ON:
      rel.Off ();
      duty_level = DUTY_MIN;
      analogWrite (PIN_PWM, duty_level);
      //
      if (this_time.hr == this_sunset)
      {
        current_p2_state = P2_SUNSET;
        //
        Serial.println ("run_P2: switching to P2_SUNSET state");
      }
      break;

    case P2_SUNSET:
      rb = SunSet ();
      if (rb == true)
      {
        current_p2_state = P2_OFF;
        //
        Serial.println ("run_P2: switching to P2_OFF state");
      }
      break;
  }
}
//
// Initiate or continue a SunRise event.
// Returns false when not yet done, true when done.
//
boolean SunRise ()
{
  Serial.println ("SunRise");
  //
  static unsigned long entry_time = 0;
  boolean rb = false;
  //
  Serial.print   ("SunRise: step_time=");
  Serial.print   (step_time);
  Serial.print   (". entry_time=");
  Serial.println (entry_time);
  //
  if (entry_time == 0)
  {
    entry_time = millis ();
  }
  //
  rb = HasTimedWaitPassed (entry_time, step_time);
  if (rb == true)
  {
    if (duty_level > DUTY_MIN)
    {
      duty_level--;
      analogWrite (PIN_PWM, duty_level);
      //
      Serial.print   ("SunRise: duty_level=");
      Serial.println (duty_level);
      //
      entry_time = millis ();
      //
      Serial.print   ("SunRise: new entry_time=");
      Serial.println (entry_time);
      //
      rb = false;                         // SunRise still in progress
    }
    else
    {
      rb = true;                          // SunRise has just completed
    }
  }
  //
  Serial.print   ("SunRise: return=");
  Serial.println (rb);
  Serial.println ("--------");
  //
  return rb;
}
//
// Initiate or continue a SunSet event.
// Returns false when not yet done, true when done.
//
boolean SunSet ()
{
  Serial.println ("SunSet");
  //
  static unsigned long entry_time = 0;
  boolean rb = false;
  //
  Serial.print   ("SunSet: step_time=");
  Serial.print   (step_time);
  Serial.print   (". entry_time=");
  Serial.println (entry_time);
  //
  if (entry_time == 0)
  {
    entry_time = millis ();
  }
  //
  rb = HasTimedWaitPassed (entry_time, step_time);
  if (rb == true)
  {
    if (duty_level < DUTY_MAX)
    {
      duty_level++;
      analogWrite (PIN_PWM, duty_level);
      //
      Serial.print   ("SunSet: duty_level=");
      Serial.println (duty_level);
      //
      entry_time = millis ();
      //
      Serial.print   ("SunSet: new entry_time=");
      Serial.println (entry_time);
      //
      rb = false;                         // SunSet still in progress
    }
    else
    {
      rb = true;                          // SunSet has just completed
    }
  }
  //
  Serial.print   ("SunSet: return=");
  Serial.println (rb);
  Serial.println ("-------");
  //
  return rb;
}
//
// Function to determine if a certain amount of milliseconds have passed since
// a specified entry_time. In case of an overflow of the result of the millis ()
// function (approximate each fifty days) this function will not fail but will
// return true so the program using this method will not stall.
//
boolean HasTimedWaitPassed (unsigned long entry_time, unsigned int wait)
{
  boolean rb = false;                     // assume not yet passed
  unsigned long this_time = millis ();
  //
  Serial.print   ("HasTimedWaitPassed: this_time=");
  Serial.println (this_time);
  //
  if (this_time >= entry_time)
  {
    if (this_time > (entry_time + wait))
    {
      rb = true;                          // wait has passed
    }
  }
  else
  {
    Serial.println ("HasTimedWaitPassed: overflow detected.");
    rb = true;                            // wait has not passed but...
  }
  //
  Serial.print   ("HasTimedWaitPassed: rb=");
  Serial.println (rb);
  //
  return rb;
}

