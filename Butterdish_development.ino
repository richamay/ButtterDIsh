/******************************************************************************************

   Butter Dish Development Application

*******************************************************************************************
  This application helps develop the final version of the butterdish software

*/

#define TEST_NAME "First test"

//#define TEST // comment this out to run in production mode.  Otherwise injects stubs here and there

//#define BLYNK_DEBUG // Optional, this enables lots of prints
//#define BLYNK_PRINT Serial


#ifdef ESP32
#include <BlynkSimpleEsp32.h>
#endif
#ifdef ESP8266
#include <BlynkSimpleEsp8266.h>
#endif
#include <WiFi.h>

// temperature declarations
//
#define DALLAS  //comment out if not using Dallas for anything
#define THERMISTOR  //comment out if not using thermistor for anything
#define INTERNAL_TEMP_THERMISTOR  // define this if using thermistor, otherwise choose DALLAS
//#define INTERNAL_TEMP_DALLAS
#define AMBIENT_TEMP_DALLAS
#ifdef DALLAS
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#ifdef THERMISTOR
#include "thermistor.h"
#define FULL_SCALE_CORR 0.3125 / 0.33333 //correction for full scale problem in ESP8266
#define MISC_CORR 14900.0 / 12755.0 //correction for other factors
#endif

#ifdef DALLAS
//// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
//OneWire oneWire(ONE_WIRE_BUS);
//// Pass our oneWire reference to Dallas Temperature.
//DallasTemperature sensors(&oneWire);
#endif

//network and Blynk credentials
#include "credentials.h"
#ifndef PROJECT_TOKEN
#define PROJECT_TOKEN ""
#endif
#ifndef MY_SSID
#define MY_SSID ""
#endif
#ifndef PASS
#define PASS ""
#endif
char auth[] = PROJECT_TOKEN;
// Your WiFi credentials.
char ssid[] = MY_SSID;
char pass[] = PASS;
WiFiClient client;


// sql server set up
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include "sql_server_info.h"
#ifndef SERVER_ADDR
#define SERVER_ADDR "0.0.0.0"
#endif
#ifndef USER
#define USER ""
#endif
#ifndef PASSWORD
#define PASSWORD ""
#endif
char user[] = USER;              // MySQL user login username
char password[] = PASSWORD;        // MySQL user login password
// Sample query
char INSERT_DATA[] = "INSERT INTO test_arduino.test_result (test_name, record_type, ambient_temp, internal_temp, power_setting, mSec, status, cover, message) VALUES ('%s','%s',%s,%s,%s,%d,'%s','%s','%s');";
char query[256];
char internal_temp[10];
char ambient_temp[10];
char power_setting[10];
IPAddress server_addr(SERVER_ADDR); // IP of the MySQL *server* here
MySQL_Connection conn((Client *)&client);


/***********************************************************


   Blynk declarations


 ************************************************************/
// Attach virtual serial terminal to Virtual Pin V1
WidgetTerminal terminal(V1);

// set up a timer to help with several functions
BlynkTimer timer;

static String last_command = ""; //for command parsing


/************************************************

  Node MCU pin declarations

************************************************/

// Ultrasonic Sensor
//const int trigPin = D3;
//const int echoPin = D5;

// onboard LED
#define LED_ESP D4  //blue led onboard the ESP8266
#define LED LED_ESP
//#define LED LED_BUILTIN  //Red led onbaoard the NodeMCU dev board

// Outputs to Relay
//const int doorPin = D0;
//const int lightPin = D7;
//byte switch_pins[] = { doorPin, lightPin }; // collective gpio to be used as switch/relay control

// Buzzer
//const int buzzerPin = D0;

// Photocell
//const int photocellPin = A0;


#ifdef THERMISTOR
#define THERMISTORPIN A0
#define FULL_SCALE_CORR 0.3125 / 0.33333 //correction for full scale problem in ESP8266
#define MISC_CORR 14900.0 / 12755.0 //correction for other factors
#endif

#ifdef DALLAS
// pin for one wire buss
#define ONE_WIRE_BUS D2
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
#endif


/************************************************

  Supporting functions and declarations

************************************************/

bool connected_to_server = false; //boolean to indicate connection
//bool heading_test_line; //boolean to indicate whether the heading of the test line has been output
bool update_terminal = false; //boolean indicates whether to dump garage status to terminal when updating blynk
int LED_timer; //reference to connected led timer
//int RSSI_timer; //reference to RSSI measurement timer
int scan_status_timer; //reference to check status timer
int update_blynk_timer; //reference to update blynk timer
//int test_timer; //reference to periodic test timer
//
#define LED_INTV_NO_CONNECT 500L  //interval (ms) between LED blinks when not connected
#define LED_INTV_CONNECT 5000L  //interval (ms) between LED blinks when connected
//#define RSSI_INTV 2000L //interval (ms) between RSSI checks
#define  SCAN_INTV_FAST 200L //interval (ms) between status checks in fast mode
#define  SCAN_INTV_SLOW 10000L //interval (ms) between status checks in slow mode
#define  UPDATE_INTV_FAST 1000L //interval (ms) between updates to blynk in fast mode
#define  UPDATE_INTV_SLOW 5000L //interval (ms) between updates to blynk in slow mode
//#define  TEST_INTV_FAST 201L //interval (ms) between test loops in fast mode
//#define  TEST_INTV_SLOW 256L //interval (ms) between test loops in slow mode

enum Status { heating, holding, cooling };
enum Cover_position { is_on, is_off, uncertain };
String status_string[] = { "Heating", "Holding", "Cooling"};
String cover_string[] = { "On", "Off", "Uncertain"};

struct Butter_Dish
{
  Status status;
  Cover_position cover;
  float internal_temp;
  float ambient_temp;
  float power_setting;
  String status_string;
};

Butter_Dish dish;

/*************************************
   Function prototypes
***********************************/

String dish_object_string();
void display_dish_object ();
void terminal_status_update();

/*************************************
   Functions
***********************************/

float get_internal_temp() {
  // get internal temp.  Store in Butter_Dish and return as float
#ifdef TEST
  dish.internal_temp = 555.55;
  return dish.internal_temp;
#endif

#ifdef INTERNAL_TEMP_THERMISTOR
  dish.internal_temp = get_raw_temp(THERMISTORPIN, THERMISTORNOMINAL, TEMPERATURENOMINAL, NUMSAMPLES, BCOEFFICIENT, SERIESRESISTOR, FULL_SCALE_CORR, MISC_CORR);
  return dish.internal_temp;
#else
  //TODO add a Dallas measure return here
  return 999.99;
#endif
}

float get_ambient_temp() {
  // get ambient temp.  Store in Butter_Dish and return as float
#ifdef TEST
  dish.ambient_temp = 666.66;
  return dish.ambient_temp;
#endif
#ifdef AMBIENT_TEMP_DALLAS
  sensors.requestTemperatures(); // Send the command to get temperatures
  dish.ambient_temp = sensors.getTempCByIndex(0);
//  Serial.println(sensors.getTempCByIndex(0));
  return dish.ambient_temp;
#else
  //TODO add a thermistor measure return here
  dish.ambient_temp = 999.99;
  return dish.ambient_temp;
#endif
}

void record_status_sql(char* record_type, int mSec, char* message = "")
{
  // record information into sql database
  // test_name, record_type, ambient, internal, power, mSec, status, cover, message
  if (conn.connect(server_addr, 3306, user, password)) {
//    Serial.println("db connected");
    // first reformat the floats
    dtostrf(dish.internal_temp, 1, 1, internal_temp);
    dtostrf(dish.ambient_temp, 1, 1, ambient_temp);
    dtostrf(dish.power_setting, 4, 1, power_setting);
    char coverstring[10];
    //convert the cover position string to char* for sprintf
    cover_string[dish.cover].toCharArray(coverstring, 10);
    char statusstring[10];
    //convert the cover position string to char* for sprintf
    dish.status_string.toCharArray(statusstring, 10);
    sprintf(query, INSERT_DATA, TEST_NAME, record_type, ambient_temp, internal_temp, power_setting, mSec, statusstring, coverstring, message);
    Serial.println(query);
    delay(1000);
    //delay(1000);
    // Initiate the query class instance
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    // Save
    // Execute the query
    cur_mem->execute(query);
    // Note: since there are no results, we do not need to read any data
    // Deleting the cursor also frees up memory used
    //    delete cur_mem;
//    Serial.println("Data recorded!");
  }
  else
  {
    Serial.println("Database connection failed.");
  }
  conn.close();
}

void set_power_setting() {
  // come here to change the power setting
  // for now, just return a random number and update status
#ifdef TEST
  Status temp[] = { heating, holding, cooling };
  randomSeed(analogRead(0));
  long randomnumber = random(0, 3);
  switch (randomnumber) {
    case 0:
      dish.status = heating;
      break;
    case 1:
      dish.status = holding;
      break;
    case 2:
      dish.status = cooling;
      break;

  }
  randomnumber = random(0, 1001);
  dish.power_setting = ((float) randomnumber /10);
#endif
}


// This function flashes the onboard LED light.  The period is long for when the link is up and short when it is not
void flashLED() {
  digitalWrite(LED, LOW);  // Turn the LED on (Note that LOW is the voltage level
  // but actually the LED is on; this is because
  // it is active low on the ESP-01)
  delay(50);
  digitalWrite(LED, HIGH);
  //Serial.println("flashLED");
}

void printRSSI() {
  // print the SSID of the network you're attached to:

  if (Blynk.connected())
  {
    //    terminal.print("SSID: ");
    //    terminal.println(WiFi.SSID());

    // print your WiFi IP address:
    //    IPAddress ip = WiFi.localIP();
    //    terminal.print("IP Address: ");
    //    terminal.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    terminal.print("signal strength (RSSI): ");
    terminal.print(rssi);
    terminal.println(" dBm");
    //    Serial.print(rssi);
    //    Serial.println(" dBm");
    //    Serial.println(Blynk.connected());
  }
  else
  {
    //terminal.println("Can't check RSSI, system not connected");
    Serial.println("Can't check RSSI, system not connected");
  }
  terminal.flush();
}

void scan_status()
{
  // update dish status.  (tbd) if a change in state, change update interval

//  Serial.println("scan_status");

  //  get temperatures
  get_internal_temp();
  get_ambient_temp();

  // decide what to do about them
  set_power_setting();
  //update status string
  dish.status_string = status_string[dish.status];

  // save the status
  Serial.println("record_status");
  record_status_sql("status", millis(), "regular update");

  //print it if it's available
  if (Serial) {
    display_dish_object();
  }
}

/************************************************

  Dish object

************************************************/

void display_dish_object ()
{
  // dump contents of dish object to screen
  Serial.print(dish_object_string());
}

String dish_object_string() {
  String temp;
  temp = "Dish information summary ===============\n";
  temp += "Status\tCover\n";
  temp += (status_string[dish.status]);
  temp += ("\t");
  temp += cover_string[dish.cover] + "\n";
  temp += "Internal temp\tAmbient temp\n";
  temp = temp + dish.internal_temp;
  temp += ("\t");
  temp = temp + dish.ambient_temp + "\n";
  temp += "Power setting:  ";
  temp = temp + dish.power_setting + "\n";
  return temp;
}
/************************************************

  Testing

************************************************/


//void change_test_interval(long new_interval)
//{
//  // change test interval to new_interval
//  timer.changeInterval(test_timer, new_interval);  // set interval to new_interval
//  //ardprintf("Changing test timer interval to: % l", new_interval);
//  timer.restartTimer(test_timer);
//}

/************************************************

  Blynk functions and declarations

  V0=
  V1=WidgetTerminal terminal
  V2=onboard LED ON/OFF
  V3=
  V4=application text box
  V5=internal temperature
  V6=ambient temperature
  V7=power setting
  V8=status
  V9=cover
  V10=
  V11=
  V12 =
  V13 =
  V14 =
  V15 =

************************************************/


BLYNK_WRITE(V1) {
  // if you type "Marco" into Terminal Widget - it will respond: "Polo: "
  
  String this_command = param.asStr();
  if (String(" = ") == this_command) {
    this_command = last_command;
  }
  if (String("Marco") == this_command) {
    terminal.println("You said: 'Marco'");
    terminal.println("I said: 'Polo'");
  }
  else if (String("status") == this_command || String("s") == this_command)
  {
    terminal_status_update();
  }
  else if (String("update") == this_command || String("u") == this_command)
  { terminal_status_update();
    update_terminal = true;
    terminal.println("Periodic update ON");
  }
  else if (String("noupdate") == this_command || String("n") == this_command)
  {
    update_terminal = false;
    terminal.println();
    terminal.println("Periodic update OFF");
  }
  else {
    // Send it back
    terminal.print("You said: ");
    terminal.write(param.getBuffer(), param.getLength());
    terminal.println();
  }

  // Ensure everything is sent
  terminal.flush();
  // save command in case it's repeated
  last_command = this_command;
}

BLYNK_WRITE(V2) {
  // Turn the LED on or off
  int pinData = param.asInt();
  if (pinData) {
    terminal.println("LED on");
    digitalWrite(LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)

  }
  else {

    terminal.println("LED off");
    digitalWrite(LED, HIGH);   // Turn the LED off (Note that HIGH is the voltage level
    // but actually the LED is off; this is because
    // it is active low on the ESP-01)
  }

  // Ensure everything is sent
  terminal.flush();
}

void update_blynk() {
  /* update data to Blynk.  Period of update is set elsewhere
    V0=
    V1=WidgetTerminal terminal
    V2=onboard LED ON/OFF
    V3=
    V4=application text box
    V5=internal temperature
    V6=ambient temperature
    V7=power setting
    V8=status
    V9=cover
    V10=
    V11=
    V12 =
    V13 = RSSI
    V14 =
    V15 =
  */
//  Serial.println("Update to Blynk");
  Blynk.virtualWrite(V4, dish_object_string());
  Blynk.virtualWrite(V5, dish.internal_temp);
  Blynk.virtualWrite(V6, dish.ambient_temp);
  Blynk.virtualWrite(V7, dish.power_setting);
  Blynk.virtualWrite(V8, dish.status_string);
  Blynk.virtualWrite(V9, cover_string[dish.cover]);

  Blynk.virtualWrite(V13, WiFi.RSSI());

  if (update_terminal) {
    terminal_status_update();
  }

}

void terminal_status_update() {
  //  terminal.println("Incoming update != == == == == == == == == == == == == == == == == == == ");
  terminal.println(dish_object_string());
}


/********************************************

  SETUP

*********************************************/
void setup()
{
  // Debug console
  Serial.begin(115200);

  //    Serial.print("Setting Wifi to ");
  //    Serial.print(ssid);
  //    Serial.print(" with password ");
  //    Serial.println(pass);
  //    Serial.print(" Auth key: ");
  //    Serial.println(auth);

  Blynk.begin(auth, ssid, pass);
  // You can also specify server:
  //Blynk.begin(auth, ssid, pass, "blynk - cloud.com", 80);
  //Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8080);

  pinMode(LED, OUTPUT); // Sets LED >> Output

  // This will print Blynk Software version to the Terminal Widget when
  // your hardware gets connected to Blynk Server
  terminal.println(F("Blynk v" BLYNK_VERSION ": Device started"));
  terminal.println(F("------------ -"));
  terminal.println(F("Type 'Marco' and get a reply, or type"));
  terminal.println(F("anything else and get it printed back."));
  terminal.flush();

  // set up timers
  LED_timer = timer.setInterval(LED_INTV_NO_CONNECT, flashLED);  //a timer to flash the LED based on WiFi connectivity.  LED interval to short - default there is no link
  //  RSSI_timer = timer.setInterval(RSSI_INTV, printRSSI);  // print RSSI periodically
  scan_status_timer = timer.setInterval(SCAN_INTV_SLOW, scan_status);  // start scan system status in slow mode
  update_blynk_timer = timer.setInterval(UPDATE_INTV_SLOW, update_blynk);  // start update Blynk in slow mode
  //test_timer = timer.setInterval(TEST_INTV_SLOW, display_garage_data);  // start test loop in slow mode


    Serial.println("TESTING begins!");
    Serial.print("Timers initialized: ");
    Serial.println(timer.getNumTimers());
  //  Serial.print(" with password ");
  //  Serial.println(pass);
  //  Serial.print(" Auth key: ");

  sensors.begin(); //start up the sensors
  record_status_sql("Test Start", millis(), ""); //start the database record
}


/********************************************

  LOOP

*********************************************/
void loop()
{
  Blynk.run();
  timer.run(); // Initiates BlynkTimer
  //  readUltrasonic(); // Uploads ultrasonic data
  //  readUltrasonic_test(); // Uploads ultrasonic data
  //  Serial.println("Blynk running");

  if (Blynk.connected()) {
    // if Blynk connected and previously not connected to server then we have a transition from unconnected
    if (!(connected_to_server)) {
      timer.changeInterval(LED_timer, LED_INTV_CONNECT);  // set LED interval to long - there is a connection
      timer.restartTimer(LED_timer);
      connected_to_server = true;
    }
  }
  else {
    // if Blynk NOT connected and previously connected to server then we have a transition from connected
    if (connected_to_server) {
      timer.changeInterval(LED_timer, LED_INTV_NO_CONNECT);  // set LED interval to short - not connected
      timer.restartTimer(LED_timer);
      connected_to_server = false;
      //Serial.println("Blynk not connected");
    }
  }
}
