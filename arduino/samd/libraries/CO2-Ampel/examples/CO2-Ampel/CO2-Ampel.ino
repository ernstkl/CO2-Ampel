/*
  CO2-Ampel

  Serielle Ausgabe
    9600 Baud 8N1

  Serielle Befehle
    R=0      - Remote/Fernsteuerung aus
    R=1      - Remote/Fernsteuerung an
    S=1      - Save/Speichern
    L=RRGGBB - LED-Farbe (000000-FFFFFF)
    H=X      - LED-Helligkeit (0-FF)
    B=1      - Buzzer an 500ms
    T=X      - Temperaturoffset in °C (0-20)
    A=X      - Altitude/Hoehe ueber dem Meeresspiegel (0-3000)
    P=X      - Pressure/Luftdruck in hPa (0 oder 700-1400)
    C=1      - Calibration/Kalibrierung auf 400ppm (mind. 2min Betrieb an Frischluft vor Befehl)
    1=X      - Range/Bereich 1 (400-40000)
    2=X      - Range/Bereich 2 (400-40000)
    3=X      - Range/Bereich 3 (400-40000)
    4=X      - Range/Bereich 4 (400-40000)
    5=X      - Range/Bereich 5 (400-40000)

  Testmodus
    1. Den Switch-Taster beim Einschalten gedrueckt halten.
    2. Buzzer-Test
    3. LED-Test: rot, gruen, blau
    4. Sensor-Test: LED 1 = Licht, LED 2 = CO2, LED 3 = Temperatur, LED 4 = Luftfeuchtigkeit

  Kalibrierung
    1. Die Ampel bei Frischluft mind. 1 Minute betreiben (im Freien oder am offenen Fenster, aber windgeschützt).
    2. Den Testmodus starten.
    3. Nach dem LED-Test (blaue LEDs) den Switch-Taster waehrend des Sensor-Tests kurz drücken (Buzzer ertoent).
    4. Die Kalibrierung wird nach dem Sensor-Test ausgeführt und dauert mindestens 2 Minuten.
       Die LEDs zeigen dabei den aktuellen CO2-Wert an: gruen bis 499ppm, gelb bis 699ppm, rot ab 700ppm
    5. Nach erfolgreicher Kalibrierung leuchten die LEDs kurz blau und der Buzzer ertoent.

  Beim Flashen einer neuen Firmware gehen alle Einstellungen verloren (z.B. Hoehenkorrektur).
  Die Kalibrierung des Sensors bleibt jedoch erhalten.

*/

#define VERSION "8"

//--- CO2-Werte ---
//Covid Praevention: https://www.umwelt-campus.de/forschung/projekte/iot-werkstatt/ideen-zur-corona-krise
/*
#define START_GRUEN         600 //>=600ppm, darunter blau
#define START_GELB          800 //>= 800ppm
#define START_ROT          1000 //>=1000ppm
#define START_ROT_BLINKEN  1200 //>=1200ppm
#define START_BUZZER       1400 //>=1400ppm
*/

//Ermuedung
#define START_GRUEN         600 //>=600ppm, darunter blau
#define START_GELB         1000 //>=1000ppm
#define START_ROT          1200 //>=1200ppm
#define START_ROT_BLINKEN  1400 //>=1400ppm
#define START_BUZZER       1600 //>=1600ppm

//--- Messintervall ---
#define INTERVALL            5 //2-1800s (normal)
#define INTERVALL_DUNKEL    30 //2-1800s (bei Dunkelheit)

//--- Ampelhelligkeit (LEDs) ---
#define HELLIGKEIT         180 //1-255 (255=100%, 179=70%)
#define HELLIGKEIT_DUNKEL  20  //1-255 (255=100%, 25=10%)
#define NUM_LEDS           4   //Anzahl der LEDs

//--- Lichtsensor ---
#define LICHT_DUNKEL       20   //<20 -> dunkel
#define LICHT_INTERVALL    3600 //1-60000s (Sensorpruefung)

//--- WiFi/WLAN ---
#define WIFI_SSID          "" //WiFi SSID
#define WIFI_CODE          "" //WiFi Passwort

//--- Allgemein ---
#define ALTITUDE           0  //Meter ueber dem Meeresspiegel
#define TEMP_OFFSET        4  //Temperaturoffset in °C (0-20)
#define TEMP_OFFSET_WIFI   10 //Temperaturoffset in °C (0-20)
#define AMPEL_DURCHSCHNITT 1 //1 = CO2 Durchschnitt fuer Ampel verwenden
#define AUTO_KALIBRIERUNG  0 //1 = automatische Kalibrierung an (erfordert 7 Tage Dauerbetrieb mit 1h Frischluft pro Tag)
#define DISPLAY_AUSGABE    0 //1 = Ausgabe auf Display aktivieren
#define BAUDRATE           9600 //9600 Baud

#include "my-defines.h"

#define FARBE_BLAU           0,  0,255
#define FARBE_GRUEN          0,255,  0
#define FARBE_GELB         255,127,  0
#define FARBE_ROT          255,  0,  0

#define STARTWERT          500 //500ppm, CO2-Startwert


#include <Wire.h>
#include <SPI.h>
#include <FlashStorage.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi101.h>
#if DISPLAY_AUSGABE > 0
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
#endif


typedef struct
{
  boolean valid;
  unsigned int brightness;
  unsigned int altitude;
  unsigned int temp_offset;
  unsigned int range[5];
  char wifi_ssid[40];
  char wifi_code[40];
} SETTINGS;

SETTINGS settings;
FlashStorage(flash_settings, SETTINGS);
SCD30 sensor;
Adafruit_NeoPixel ws2812 = Adafruit_NeoPixel(NUM_LEDS, PIN_WS2812, NEO_GRB + NEO_KHZ800);
#if DISPLAY_AUSGABE > 0
  Adafruit_SSD1306 display(128, 64); //128x64 Pixel
#endif
WiFiServer server(80); //Webserver Port 80 (Plus Version)

unsigned int plus_version=0, remote_on=0;
unsigned int co2=STARTWERT, co2_average=STARTWERT;
unsigned int light=1024;
float temp=0, humi=0;


unsigned int light_sensor(void) //Auslesen des Lichtsensors
{
  unsigned int i;
  uint32_t color = ws2812.getPixelColor(0); //aktuelle Farbe speichern

  //ws2812.setPixelColor(2, ws2812.Color(0,0,0)); //LED 3 aus
  ws2812.fill(ws2812.Color(0,0,0), 0, 4); //alle LEDs aus
  ws2812.show();

  digitalWrite(PIN_LSENSOR_PWR, HIGH); //Lichtsensor an
  delay(40); //40ms warten
  i = analogRead(PIN_LSENSOR); //0...1024
  delay(10); //10ms warten
  i += analogRead(PIN_LSENSOR); //0...1024
  i /= 2;
  digitalWrite(PIN_LSENSOR_PWR, LOW); //Lichtsensor aus

  //ws2812.setPixelColor(2, color); //LED 3 an
  ws2812.fill(color, 0, NUM_LEDS); //alle LEDs an
  ws2812.show();

  return i;
}


void show_data(void) //Daten anzeigen
{
  if(Serial)
  {
    Serial.print("c: ");     //CO2
    Serial.println(co2);     //Wert in ppm
    Serial.print("t: ");     //Temperatur
    Serial.println(temp, 1); //Wert in °C
    Serial.print("h: ");     //Humidity/Luftfeuchte
    Serial.println(humi, 1); //Wert in %
    Serial.print("l: ");     //Licht
    Serial.println(light);
    Serial.println();
  }

  #if DISPLAY_AUSGABE > 0
    display.clearDisplay();
    display.setTextSize(5);
    display.setCursor(5,5);
    display.println(co2);
    display.setTextSize(1);
    display.setCursor(5,56);
    display.println("CO2 Level in ppm");
    display.display();
  #endif

  return;
}


void serial_service(void)
{
  int i, cmd, val;
  char tmp[32];

  if(!Serial)
  {
    return;
  }

  if(Serial.available() == 0)
  {
    return;
  }

  cmd = Serial.read(); //Befehl
  if((cmd != 'R') && (remote_on == 0))
  {
    return;
  }

  val = Serial.read(); //schreiben/lesen
  if(val == '=') //=
  {
    switch(toupper(cmd))
    {
      case 'R': //Fernsteuerung
        cmd = Serial.read();
        if(cmd == '1')
        {
          remote_on = 1;
          analogWrite(PIN_BUZZER, 0); //Buzzer aus
          ws2812.setBrightness(30); //0...255
          ws2812.fill(ws2812.Color(255,0,255), 0, NUM_LEDS); //LEDs violet
          ws2812.show();
        }
        else
        {
          remote_on = 0;
          ws2812.setBrightness(settings.brightness);
        }
        Serial.println("OK");
        break;

      case 'S': //Save/Speichern
        cmd = Serial.read();
        if(cmd == '1')
        {
          settings.valid = true;
          flash_settings.write(settings); //Einstellungen speichern
          Serial.println("OK");
        }
        break;

      case 'H': //LED Helligkeit
        i = Serial.readBytesUntil('\n', tmp, sizeof(tmp));
        if(i > 0)
        {
          tmp[i] = 0;
          sscanf(tmp, "%X", &val);
          if(val < 10)
          {
            val = 10;
          }
          else if(val > 255)
          {
            val = 255;
          }
          settings.brightness = val;
          ws2812.setBrightness(val);
          ws2812.show();
          Serial.println("OK");
        }
        break;

      case 'L': //LED Farbe
        i = Serial.readBytesUntil('\n', tmp, sizeof(tmp));
        if(i > 0)
        {
          tmp[i] = 0;
          sscanf(tmp, "%X", &val);
          ws2812.fill(val, 0, NUM_LEDS);
          ws2812.show();
          Serial.println("OK");
        }
        break;

      case 'B': //Buzzer
        cmd = Serial.read();
        if(cmd == '1')
        {
          analogWrite(PIN_BUZZER, 255/2); //Buzzer an
          delay(500); //500ms warten
          analogWrite(PIN_BUZZER, 0); //Buzzer aus
          Serial.println("OK");
        }
        break;

      case 'T': //Temperaturoffset
        i = Serial.readBytesUntil('\n', tmp, sizeof(tmp));
        if(i > 0)
        {
          tmp[i] = 0;
          sscanf(tmp, "%d", &val);
          if((val >= 0) && (val <= 20))
          {
            settings.temp_offset = val;
            sensor.setTemperatureOffset(settings.temp_offset); //Temperaturoffset
            flash_settings.write(settings); //Einstellungen speichern
            Serial.println("OK");
          }
        }
        break;

      case 'A': //Altitude/Hoehe ueber dem Meeresspiegel
        i = Serial.readBytesUntil('\n', tmp, sizeof(tmp));
        if(i > 0)
        {
          tmp[i] = 0;
          sscanf(tmp, "%d", &val);
          if((val >= 0) && (val <= 3000))
          {
            settings.altitude = val;
            sensor.setAltitudeCompensation(settings.altitude); //Meter ueber dem Meeresspiegel
            flash_settings.write(settings); //Einstellungen speichern
            Serial.println("OK");
          }
        }
        break;

      case 'P': //Pressure/Luftdruck in hPa
        i = Serial.readBytesUntil('\n', tmp, sizeof(tmp));
        if(i > 0)
        {
          tmp[i] = 0;
          sscanf(tmp, "%d", &val);
          if((val == 0) || ((val >= 700) && (val <= 1400)))
          {
            sensor.setAmbientPressure(val); //0 oder 700-1400, Luftdruck in hPa
            Serial.println("OK");
          }
        }
        break;
        
      case 'C': //Calibration/Kalibrierung
        cmd = Serial.read();
        if(cmd == '1')
        {
          sensor.setForcedRecalibrationFactor(400); //400ppm = Frischluft
          Serial.println("OK");
        }
        break;

      case '1': //Range/Bereich 1
      case '2': //Range/Bereich 2
      case '3': //Range/Bereich 3
      case '4': //Range/Bereich 4
      case '5': //Range/Bereich 5
        i = Serial.readBytesUntil('\n', tmp, sizeof(tmp));
        if(i > 0)
        {
          tmp[i] = 0;
          sscanf(tmp, "%d", &val);
          if((val >= 400) && (val <= 40000))
          {
            settings.range[cmd-'1'] = val;
            Serial.println("OK");
          }
        }
        break;
    }
  }
  else if(val == '?') //?
  {
    switch(toupper(cmd))
    {
      case 'H': //LED Helligkeit
        Serial.println(settings.brightness, HEX);
        break;
      case 'T': //Temperaturoffset
        val = sensor.getTemperatureOffset(); //settings.temp_offset
        Serial.println(val, DEC);
        break;
      case 'A': //Altitude/Hoehe ueber dem Meeresspiegel
        Serial.println(settings.altitude, DEC);
        break;
      case '1': //Range/Bereich 1
      case '2': //Range/Bereich 2
      case '3': //Range/Bereich 3
      case '4': //Range/Bereich 4
      case '5': //Range/Bereich 4
        Serial.println(settings.range[cmd-'1'], DEC);
        break;
    }
  }

  return;
}


void urldecode(char *src) //URL Parameter dekodieren
{
  char a, b, *dst = src;

  while(*src) 
  {
    if((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit((uint8_t)a) && isxdigit((uint8_t)b))) 
    {
      if (a >= 'a')
        a -= 'a'-'A';

      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';

      if (b >= 'a')
        b -= 'a'-'A';

      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';

      *dst++ = 16 * a + b;
      src += 3;
    } 
    else if (*src == '+') 
    {
      *dst++ = ' ';
      src++;
    } 
    else 
    {
      *dst++ = *src++;
    }
  }
  *dst++ = '\0';

  return;
}


void webserver_service(void)
{
  if(!plus_version)
  {
    return;
  }
  
  if(WiFi.status() == WL_IDLE_STATUS)
  {
    return;
  }

  if((WiFi.status() == WL_CONNECT_FAILED) ||
     (WiFi.status() == WL_CONNECTION_LOST) || 
     (WiFi.status() == WL_DISCONNECTED))
  {
    wifi_start();
    return;
  }

  WiFiClient client = server.available();
  if(client) //Client verbunden
  {
    boolean currentLineIsBlank = true;
    while(client.connected())
    {
      if(client.available())
      {
        char c = client.read();
        if(c == '\n' && currentLineIsBlank)
        {
          //HTTP Post Daten verarbeiten
          if(client.available())
          {
            unsigned int req_data=0;
            char req[2][40];
            req[0][0] = 0; //SSID
            req[1][0] = 0; //Code
            for(unsigned int r=0, i=0, last_c=0; client.available();)
            {
              c = client.read();
              if(c == '&') //Aufbau: 1=xxx&2=yyy
              {
                r = 0;
              }
              else if((c == '=') && isdigit(last_c)) //1=xxx
              {
                r = last_c-'0';
                i = 0;
              }
              else if((r > 0) && (r < 3)) //1 bis 2
              {
                req[r-1][i++] = c;
                req[r-1][i] = 0;
                req_data = 1;
              }
              last_c = c;
            }
            if(req_data)
            {
              urldecode(req[0]);
              //Serial.println(req[0]);
              strcpy(settings.wifi_ssid, req[0]);
              urldecode(req[1]);
              //Serial.println(req[1]);
              strcpy(settings.wifi_code, req[1]);
              flash_settings.write(settings); //Einstellungen speichern
            }
          }
          //HTTP Header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          //HTML Daten
          client.println("<!DOCTYPE html>");
          client.println("<html><head><title>CO2-Ampel</title></head>");
          client.println("<body>");
          client.println("<br><span style='font-size:3em'>");
          client.print("CO2 (ppm): ");
          client.println(co2);
          client.print("<br>Temperatur (C): ");
          client.println(temp, 1);
          client.print("<br>Luftfeuchte (%): ");
          client.println(humi, 1);
          client.println("<br></span><br><hr><br>");
          client.print("<br><b>WiFi Login</b>");
          client.println("<form method=post>");
          client.print("SSID <input name=1 size=20 maxlength=32 placeholder=SSID value='");
          client.print(settings.wifi_ssid);
          client.println("'><br>");
          client.print("Code <input name=2 size=20 maxlength=32 placeholder=Password value='");
          client.println("'><br>");
          client.println("<input type=submit value=Speichern> (Neustart erforderlich)");
          client.println("</form>");
          client.println("</body></html>");
          break;
        }
        if(c == '\n')
        {
          currentLineIsBlank = true;
        }
        else if(c != '\r')
        {
          currentLineIsBlank = false;
        }
      }
    }
    delay(2); //2ms warten zum Senden
    client.stop();
  }

  return;
}


void self_test(void) //Testprogramm
{
  unsigned int calibration=0, okay, sw, co2_last;
  unsigned int atecc, atwinc;

  sensor.setMeasurementInterval(2); //2s (kleinster Intervall)

  //Buzzer-Test
  analogWrite(PIN_BUZZER, 255/2); //Buzzer an
  delay(1000); //1s warten
  analogWrite(PIN_BUZZER, 0); //Buzzer aus

  //LED-Test
  ws2812.setBrightness(20); //0...255
  ws2812.fill(ws2812.Color(255,0,0), 0, NUM_LEDS); //LEDs rot
  ws2812.show();
  delay(1000); //1s warten
  ws2812.fill(ws2812.Color(0,255,0), 0, NUM_LEDS); //LEDs gruen
  ws2812.show();
  delay(1000); //1s warten
  ws2812.fill(ws2812.Color(0,0,255), 0, NUM_LEDS); //LEDs blau
  ws2812.show();
  delay(1000); //1s warten
  ws2812.fill(ws2812.Color(0,0,0), 0, NUM_LEDS); //LEDs aus

  //ATECC608+ATWINC1500 Test
  Wire1.beginTransmission(0x60); //Dummy Test
  Wire1.endTransmission();
  Wire1.beginTransmission(0x60); //0x60 = ATECC608
  delay(1); //1ms warten
  atecc  = Wire1.endTransmission(); //0 = ok
  atwinc = WiFi.status(); //ATWINC1500
  if((atecc == 0) || (atwinc != WL_NO_SHIELD))
  {
    ws2812.fill(ws2812.Color(255,255,255), 0, 4); //LEDs weiss
    ws2812.show();
    delay(1000); //1s warten
    if(atecc != 0) //ATECC608 Fehler
    {
      if(Serial)
      {
        Serial.println("Error: ATECC608");
      }
      while(1)
      {
        ws2812.fill(ws2812.Color(255,0,0), 0, 4); //LEDs rot
        ws2812.show();
        delay(500); //1s warten
        ws2812.fill(ws2812.Color(0,0,0), 0, 4); //LEDs aus
        ws2812.show();
        delay(500); //1s warten
      }
    }
    if(atwinc == WL_NO_SHIELD) //ATWINC1500 Fehler
    {
      if(Serial)
      {
        Serial.println("Error: ATWINC1500");
      }
      while(1)
      {
        ws2812.fill(ws2812.Color(255,0,0), 0, 4); //LEDs rot
        ws2812.show();
        delay(500); //1s warten
        ws2812.fill(ws2812.Color(255,255,0), 0, 4); //LEDs gelb
        ws2812.show();
        delay(500); //1s warten
      }
    }
  }

  //Sensor-Test
  ws2812.fill(ws2812.Color(0,0,0), 0, 4); //LEDs aus
  for(okay=0; okay < 15;)
  {
    if(digitalRead(PIN_SWITCH) == LOW) //Taster gedrueckt?
    {
      calibration = 1; //Kalibrierung ausfuehren
      analogWrite(PIN_BUZZER, 255/2); //Buzzer an
      delay(25); //25ms warten
      analogWrite(PIN_BUZZER, 0); //Buzzer aus
    }

    digitalWrite(PIN_LED, HIGH); //Status-LED an
    delay(100); //100ms warten
    digitalWrite(PIN_LED, LOW); //Status-LED aus
    delay(100); //100ms warten

    digitalWrite(PIN_LSENSOR_PWR, HIGH); //Lichtsensor an
    delay(50); //50ms warten
    light = analogRead(PIN_LSENSOR); //0...1024
    digitalWrite(PIN_LSENSOR_PWR, LOW); //Lichtsensor aus
    if((light >= 50) && (light <= 1000)) //50-1000
    {
      okay |= (1<<0);
      ws2812.setPixelColor(0, ws2812.Color(0,255,0));
    }
    else
    {
      okay &= ~(1<<0);
      ws2812.setPixelColor(0, ws2812.Color(0,0,0));
    }

    if(sensor.dataAvailable())
    {
      co2  = sensor.getCO2();
      temp = sensor.getTemperature();
      humi = sensor.getHumidity();

      if((co2 >= 100) && (co2 <= 1500)) //100-1500ppm
      {
        okay |= (1<<1);
        ws2812.setPixelColor(1, ws2812.Color(0,255,0));
      }
      else
      {
        okay &= ~(1<<1);
        ws2812.setPixelColor(1, ws2812.Color(0,0,0));
      }

      if((temp >= 5) && (temp <= 35)) //5-35°C
      {
        okay |= (1<<2);
        ws2812.setPixelColor(2, ws2812.Color(0,255,0));
      }
      else
      {
        okay &= ~(1<<2);
        ws2812.setPixelColor(2, ws2812.Color(0,0,0));
      }

      if((humi >= 20) && (humi <= 80)) //20-80%
      {
        okay |= (1<<3);
        ws2812.setPixelColor(3, ws2812.Color(0,255,0));
      }
      else
      {
        okay &= ~(1<<3);
        ws2812.setPixelColor(3, ws2812.Color(0,0,0));
      }

      show_data();
    }
    ws2812.show();
  }
  delay(2000); //2s warten

  if(calibration) //Kalibrierung starten?
  {
    //Altitude
    calibration = settings.altitude/250; //Meter ueber dem Meeresspiegel
    ws2812.fill(ws2812.Color(255,255,255), 0, 4); //LEDs weiss
    if(calibration > 0)
    {
      ws2812.fill(ws2812.Color(255,0,0), 0, calibration); //LEDs rot
    }
    ws2812.show();
    for(sw=0, okay=0; okay<1000; okay++) //10s Timeout
    {
      digitalWrite(PIN_LED, HIGH); //Status-LED an
      delay(5); //5ms warten
      digitalWrite(PIN_LED, LOW); //Status-LED aus
      delay(5); //5ms warten

      if(digitalRead(PIN_SWITCH) == LOW) //Taster gedrueckt
      {
        sw++;
        okay=0;
      }
      else //Taster losgelassen
      {
        if(sw > 200) //2s Tastendruck
        {
          break;
        }
        else if(sw > 15) //150ms Tastendruck
        {
          calibration++;
          if(calibration > 4)
          {
            calibration = 0;
          }
          ws2812.fill(ws2812.Color(255,255,255), 0, 4); //LEDs weiss
          if(calibration > 0)
          {
            ws2812.fill(ws2812.Color(255,0,0), 0, calibration); //LEDs rot
          }
          ws2812.show();
        }
        sw = 0;
      }
    }
    settings.altitude = 250*calibration;
    sensor.setAltitudeCompensation(settings.altitude); //Meter ueber dem Meeresspiegel
    flash_settings.write(settings); //Einstellungen speichern
    if(Serial)
    {
      Serial.print("Altitude: ");
      Serial.println(settings.altitude, DEC);
    }

    //Kalibrierung
    ws2812.fill(ws2812.Color(255,255,255), 0, 4); //LEDs weiss
    ws2812.show();
    co2_last = co2;
    for(okay=0; okay < 60;) //mindestens 60 Messungen (ca. 2 Minuten)
    {
      if(digitalRead(PIN_SWITCH) == 0) //Taster gedrueckt?
      {
        calibration = 0;
        break; //Abbruch
      }

      digitalWrite(PIN_LED, HIGH); //Status-LED an
      delay(100); //100ms warten
      digitalWrite(PIN_LED, LOW); //Status-LED an
      delay(100); //100ms warten

      if(sensor.dataAvailable()) //alle 2s
      {
        co2  = sensor.getCO2();
        temp = sensor.getTemperature();
        humi = sensor.getHumidity();

        if(co2 < 200) //Sensor falsch kalibriert
        {
          okay++;
        }
        else if((co2 >= 200) && (co2 <= 1500) && 
                (co2 >= (co2_last-30)) &&
                (co2 <= (co2_last+30))) //+/-30ppm Toleranz zum vorherigen Wert
        {
          okay++;
        }
        else
        {
          okay = 0;
        }
        co2_last = co2;

        if(co2 < 600)
        {
          ws2812.fill(ws2812.Color(0,255,0), 2, 3);   //gruen
        }
        else if(co2 < 1500)
        {
          ws2812.fill(ws2812.Color(255,255,0), 2, 2); //gelb
        }
        else //>=700
        {
          ws2812.fill(ws2812.Color(255,0,0), 2, 2);   //rot
        }
        ws2812.show();

        if(Serial)
        {
          Serial.print("ok: ");
          Serial.println(okay);
        }

        show_data();
      }
    }
    if(calibration && (okay >= 60))
    {
      sensor.setForcedRecalibrationFactor(400); //400ppm = Frischluft
      ws2812.fill(ws2812.Color(0,0,255), 0, 4); //LEDs blau
      ws2812.show();
      analogWrite(PIN_BUZZER, 255/2); //Buzzer an
      delay(500); //500ms warten
      analogWrite(PIN_BUZZER, 0); //Buzzer aus
      ws2812.fill(ws2812.Color(0,0,0), 0, 4);  //LEDs aus
      ws2812.show();
      if(Serial)
      {
        Serial.println("Calibration OK");
      }
    }
  }

  ws2812.setBrightness(settings.brightness); //0...255
  ws2812.fill(ws2812.Color(20,20,20), 0, NUM_LEDS); //LEDs weiss
  ws2812.show();
  co2 = STARTWERT;
  co2_average = STARTWERT;
  light = 1024;

  return;
}


unsigned int wifi_start_ap(void)
{
  byte mac[6];
  char ssid[32];

  WiFi.macAddress(mac); //MAC-Adresse abfragen
  sprintf(ssid, "CO2AMPEL-%X-%X", mac[1], mac[0]);

  if((WiFi.status() == WL_CONNECTED) ||
     (WiFi.status() == WL_AP_CONNECTED))
  {
    WiFi.end(); //WiFi.disconnect();
  }

  WiFi.hostname(ssid); //Hostname setzen
  if(WiFi.beginAP(ssid) != WL_AP_LISTENING)
  {
    WiFi.end();
    return 1;
  }

  delay(5000);  //5s warten

  server.begin(); //starte Webserver

  return 0;
}


unsigned int wifi_start(void)
{
  byte mac[6];
  char name[32];

  if(settings.wifi_ssid[0] == 0) //keine Logindaten
  {
    return 1;
  }

  WiFi.macAddress(mac); //MAC-Adresse abfragen
  sprintf(name, "CO2AMPEL-%X-%X", mac[1], mac[0]);

  if((WiFi.status() == WL_CONNECTED) ||
     (WiFi.status() == WL_AP_CONNECTED))
  {
    WiFi.end(); //WiFi.disconnect();
  }

  WiFi.hostname(name); //Hostname setzen
  //WiFi.config(ip, dns, gateway, subnet);  //IP setzen
  WiFi.begin(settings.wifi_ssid, settings.wifi_code); //verbinde WiFi Netzwerk

  //auf Verbindung warten
  for(unsigned int t=0; WiFi.status() == WL_IDLE_STATUS; t++)
  {
    if(t >= 5) //5s
    {
      break;
    }
    digitalWrite(PIN_LED, HIGH); //Status-LED an
    delay(500); //500ms warten
    digitalWrite(PIN_LED, LOW); //Status-LED aus
    delay(500); //500ms warten
  }

  server.begin(); //starte Webserver

  return 0;
}


void setup()
{
  int run_test=0;

  //setze Pins
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW); //LED aus
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_WS2812, OUTPUT);
  digitalWrite(PIN_WS2812, LOW);
  pinMode(PIN_LSENSOR_PWR, OUTPUT);
  digitalWrite(PIN_LSENSOR_PWR, LOW); //Lichtsensor aus
  pinMode(PIN_LSENSOR, INPUT);
  pinMode(PIN_SWITCH, INPUT_PULLUP);

  if(digitalRead(PIN_SWITCH) == LOW) //Taster gedrueckt
  {
    run_test = 1;
  }

  //WS2812
  ws2812.begin();
  ws2812.setBrightness(HELLIGKEIT); //0...255
  ws2812.fill(ws2812.Color(20,20,20), 0, NUM_LEDS); //LEDs weiss
  ws2812.show();

  //Wire/I2C
  Wire.begin();
  Wire.setClock(50000); //50kHz, empfohlen fuer SCD30
  Wire1.begin();
  Wire1.setClock(100000); //100kHz ATECC

  delay(250); //250ms warten

  //serielle Schnittstelle (USB)
  Serial.begin(BAUDRATE); //seriellen Port starten
  Serial.setTimeout(500); //500ms Timeout beim Lesen
  //while(!Serial); //warten auf USB-Verbindung
  if(Serial)
  {
    Serial.println("\nCO2 Ampel v" VERSION);
  }

  //ATECC608+ATWINC1500
  Wire1.beginTransmission(0x60); //Dummy Test
  Wire1.endTransmission();
  Wire1.beginTransmission(0x60); //0x60 = ATECC608
  delay(1); //1ms warten
  if(Wire1.endTransmission() == 0) //ATECC608 gefunden
  {
    if(WiFi.status() != WL_NO_SHIELD) //ATWINC1500 gefunden
    {
      plus_version = 1;
      if(Serial)
      {
        Serial.println("Plus Version");
      }
    }
    else
    {
      plus_version = 0;
      WiFi.end();
    }
  }

  #if DISPLAY_AUSGABE > 0
    delay(500); //500ms warten
    display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  #endif

  //SCD30
  while(sensor.begin(Wire, AUTO_KALIBRIERUNG) == false)
  {
    digitalWrite(PIN_LED, HIGH); //Status-LED an
    delay(500); //500ms warten
    digitalWrite(PIN_LED, LOW); //Status-LED aus
    delay(500); //500ms warten
    if(Serial)
    {
      Serial.println("Error: CO2 sensor not found");
    }
  }

  //Einstellungen
  settings = flash_settings.read(); //Einstellungen lesen
  if((settings.valid == false) || (settings.brightness > 255) || (settings.range[0] < 400))
  {
    settings.brightness   = HELLIGKEIT;
    settings.altitude     = ALTITUDE;
    if(plus_version)
    {
      settings.temp_offset = TEMP_OFFSET_WIFI;
    }
    else
    {
      settings.temp_offset = TEMP_OFFSET;
    }
    settings.range[0]     = START_GRUEN;
    settings.range[1]     = START_GELB;
    settings.range[2]     = START_ROT;
    settings.range[3]     = START_ROT_BLINKEN;
    settings.range[4]     = START_BUZZER;
    settings.wifi_ssid[0] = 0;
    strcpy(settings.wifi_ssid, WIFI_SSID);
    settings.wifi_code[0] = 0;
    strcpy(settings.wifi_code, WIFI_CODE);
    settings.valid        = true;
    flash_settings.write(settings);
    sensor.setAltitudeCompensation(settings.altitude); //Meter ueber dem Meeresspiegel
    sensor.setTemperatureOffset(settings.temp_offset); //Temperaturoffset
  }
  //sensor.setAmbientPressure(1000); //0 oder 700-1400, Luftdruck in hPa
  ws2812.setBrightness(settings.brightness); //0...255

  if(Serial && (settings.altitude > 0))
  {
    Serial.print("Altitude: ");
    Serial.println(settings.altitude, DEC);
  }

  #if DISPLAY_AUSGABE > 0
    display.clearDisplay();
    display.setTextColor(WHITE, BLACK);
    display.setTextSize(3);
    display.setCursor(40, 0);
    display.print("CO2");
    display.setCursor(23, 23);
    display.print("Ampel");
    display.setTextSize(1);
    display.setCursor(5, 48);
    display.print("Watterott electronic");
    display.setCursor(12, 56);
    display.print("www.watterott.com");
    display.display();
  #endif

  //Testmodus
  if(run_test && (digitalRead(PIN_SWITCH) == LOW)) //Taster beim Einschalten und aktuell noch gedrueckt
  {
    self_test(); //starte Testmodus
  }

  //Plus-Version
  if(plus_version)
  {
    if(wifi_start() != 0) //verbinde WiFi Netzwerk
    {
      if(wifi_start_ap() != 0) //starte AP
      {
        plus_version = 0;
      }
    }
    delay(2000); //2s warten
    if(Serial)
    {
      byte mac[6];
      WiFi.macAddress(mac);
      Serial.print("MAC: ");
      Serial.print(mac[5], HEX); Serial.print(":"); Serial.print(mac[4], HEX); Serial.print(":"); Serial.print(mac[3], HEX); Serial.print(":");
      Serial.print(mac[2], HEX); Serial.print(":"); Serial.print(mac[1], HEX); Serial.print(":"); Serial.print(mac[0], HEX); Serial.println("");
      IPAddress ip;
      ip = WiFi.localIP();
      Serial.print("IP: "); Serial.println(ip);
      ip = WiFi.subnetMask();
      Serial.print("NM: "); Serial.println(ip);
      ip = WiFi.gatewayIP();
      Serial.print("GW: "); Serial.println(ip);
      Serial.println("");
    }
  }

  //Messung starten
  sensor.setMeasurementInterval(INTERVALL); //setze Messinterval 
  delay(INTERVALL*1000UL); //Intervallsekunden warten

  return;
}


void ampel(unsigned int co2)
{
  static unsigned int blinken=0;

  //LEDs
  if(co2 < settings.range[0]) //blau
  {
    blinken = 0;
    ws2812.fill(ws2812.Color(FARBE_BLAU), 0, NUM_LEDS);
  }
  else if(co2 < settings.range[1]) //gruen
  {
    blinken = 0;
    ws2812.fill(ws2812.Color(FARBE_GRUEN), 0, NUM_LEDS);
  }
  else if(co2 < settings.range[2]) //gelb
  {
    blinken = 0;
    ws2812.fill(ws2812.Color(FARBE_GELB), 0, NUM_LEDS);
  }
  else if(co2 < settings.range[3]) //rot
  {
    blinken = 0;
    ws2812.fill(ws2812.Color(FARBE_ROT), 0, NUM_LEDS);
  }
  else //rot blinken
  {
    if(blinken == 0)
    {
      ws2812.fill(ws2812.Color(10,0,0), 0, NUM_LEDS);  //rot schwache Helligkeit
    }
    else
    {
      ws2812.fill(ws2812.Color(FARBE_ROT), 0, NUM_LEDS); //rot
    }
    blinken = 1-blinken; //invertieren
  }
  ws2812.show(); //zeige Farbe

  //Buzzer
  if(co2 < settings.range[4])
  {
    analogWrite(PIN_BUZZER, 0); //Buzzer aus
  }
  else
  {
    if(blinken == 0)
    {
      analogWrite(PIN_BUZZER, 255/2); //Buzzer an
    }
    else
    {
      analogWrite(PIN_BUZZER, 0); //Buzzer aus
    }
  }

  return;
}


void loop()
{
  static unsigned int dunkel=0, sw=0;
  static long long t_ampel=0, t_light=0, t_switch=0;
  unsigned int overwrite=0;

  //serielle Befehle verarbeiten
  serial_service();

  //WiFi-Daten verarbeiten
  webserver_service();

  //Taster pruefen
  if(digitalRead(PIN_SWITCH) == LOW) //Taster gedrueckt
  {
    if(sw == 0)
    {
      sw = 1;
      t_switch = millis(); //Zeit speichern
    }
  }
  else if(sw != 0) //Taster losgelassen
  {
    sw = 0;
    if((millis()-t_switch) > 3000) //3s Tastendruck
    {
      if(plus_version)
      {
        ws2812.fill(ws2812.Color(255,0,255), 0, 4); //LEDs violet
        ws2812.show();
        wifi_start_ap();
      }
    }
    else if((millis()-t_switch) > 100) //100ms Tastendruck
    {
      settings.brightness = settings.brightness/2; //Helligkeit halbieren
      if(settings.brightness < HELLIGKEIT_DUNKEL)
      {
        settings.brightness = HELLIGKEIT;
      }
      ws2812.setBrightness(settings.brightness);
      overwrite = 1;
    }
  }

  if((millis()-t_ampel) > 1000) //Ampelfunktion nur jede Sekunde ausfuehren
  {
    t_ampel = millis(); //Zeit speichern

    co2_average = (co2_average + co2) / 2; //Berechnung jede Sekunde

    digitalWrite(PIN_LED, HIGH); //Status-LED an
    delay(1); //1ms warten
    digitalWrite(PIN_LED, LOW); //Status-LED aus
  }
  else if(overwrite == 0)
  {
    return;
  }

  //Sensordaten auslesen
  if(sensor.dataAvailable())
  {
    co2  = sensor.getCO2();
    temp = sensor.getTemperature();
    humi = sensor.getHumidity();
    
    show_data();
  }

  //Ampel
  if(remote_on == 0)
  {
    #if AMPEL_DURCHSCHNITT > 0
      ampel(co2_average);
    #else
      ampel(co2);
    #endif
  }

  //Lichtsensor
  if(remote_on == 0)
  {
    if((millis()-t_light) > (LICHT_INTERVALL*1000UL))
    {
      t_light = millis(); //Zeit speichern

      light = light_sensor();
      if(light < LICHT_DUNKEL)
      {
        if(dunkel == 0)
        {
          dunkel = 1;
          sensor.setMeasurementInterval(INTERVALL_DUNKEL); 
          ws2812.setBrightness(HELLIGKEIT_DUNKEL); //0...255
        }
      }
      else
      {
        if(dunkel == 1)
        {
          dunkel = 0;
          sensor.setMeasurementInterval(INTERVALL); 
          ws2812.setBrightness(settings.brightness); //0...255
        }
      }
    }
  }

  return;
}
