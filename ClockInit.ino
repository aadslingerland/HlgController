//
// ClockInit.ino, 201805, Aad Slingerland.
// http://digitalab.org/2017/08/real-time-clock-ds1302/
// Example sketch for interfacing with the DS1302 timekeeping chip.
//
#include <stdio.h>
#include <DS1302.h>
namespace 
{
  //  
  // Set the appropriate digital I/O pin connections. These are the pin
  // assignments for the Arduino as well for as the DS1302 chip.
  //
  const int CePin   = 52;  // Chip Enable
  const int IoPin   = 50;  // Input/Output
  const int SclkPin = 48;  // Serial Clock
  //
  // Create a DS1302 object.
  //
  DS1302 rtc (CePin, IoPin, SclkPin);
  String dayAsString (const Time::Day day)
  {
    switch (day)
    {
      case Time::kSunday:    return "Sunday";
      case Time::kMonday:    return "Monday";
      case Time::kTuesday:   return "Tuesday";
      case Time::kWednesday: return "Wednesday";
      case Time::kThursday:  return "Thursday";
      case Time::kFriday:    return "Friday";
      case Time::kSaturday:  return "Saturday";
    }
    return ("unknown day");
  }

  void printTime()
  {
    //
    // Get the current time and date from the chip.
    //
    Time t = rtc.time ();
    //
    // Name the day of the week.
    //
    const String day = dayAsString (t.day);
    //
    // Format the time and date and insert into the temporary buffer.
    //
    char buf[50];
    snprintf (buf, sizeof(buf), "%s %04d-%02d-%02d %02d:%02d:%02d",
      day.c_str (), t.yr, t.mon, t.date, t.hr, t.min, t.sec);
    //     
    // Print the formatted string to serial so we can see the time.
    //
    Serial.println (buf);
  }
} 

void setup() {
  Serial.begin(9600);
  Serial.println ("VMA101_ClockInit");
  //
  // Initialize a new chip by turning off write protection and clearing the
  // clock halt flag. These methods needn't always be called. See the DS1302
  // datasheet for details.
  // 
  rtc.writeProtect(false);
  rtc.halt(false);
  //
  // Make a new time object to set the date and time.
  // This need to be done first time to set the Date and time.
  // Thursday, July 6, 2017 at 22:58:50.
  //
  Time t(2018, 05, 06, 20, 28, 0, Time::kSunday);
  //
  //Set the time and date on the chip.
  //
  rtc.time(t);
}
//
// Loop and print the time every second.
//
void loop ()
{
  printTime();
  delay(1000);
}

