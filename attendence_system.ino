#include <SPI.h>
#include <MFRC522.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <Servo.h>

// definitions
#define CLASS_TIME_IN_MINUTE 15
#define ENTERANCE_DEADLINE_IN_MINUTE 10

// create servo object to control the door
Servo myservo;

// students' information variables
const int NUMBER_OF_STUDENTS = 2;
const String student_name[] = {"A. Alibakhshi", "N. Hajisobhani"};
const String student_tags[] = {"336ff31a", "4c661249"};
bool is_student_present[] = {false, false}; // [is_AMIRHOSSEIN_ALIBAKHSHI_present, is_NEGIN_HAJISOBHANI_present]

// global variables
bool is_instructor_in_class = false; // a boolean variable for saving the LED's state
unsigned long start_time;
unsigned long enterance_deadline;
unsigned long end_time;
bool is_class_started = false;
bool is_deadline_missed = false;
bool is_class_ended = false;

// pins
const int SERVO_PIN = 4; // assiging servo pin: D2
const int LDR_PIN = A0; // assiging LDR pin: A0
const int LED_PIN = 5;  // assiging LED pin: D1
const int BUZZER_PIN = 0; // assiging buzzer pin: D3
const int SS_PIN = D8; // assiging RFID SS pin: D8
const int RST_PIN = D0; // assiging RFID RST pin: D0

// initating RFID
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

// Init array that will store new NUID
byte nuidPICC[4];

// informations and instances for connecting to the internet
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
const char *ssid     = "<SSID>";
const char *password = "<PASSWORD>";

// function prototypes
void check_instructor_attendance(void);
void print_current_time(void);
void start_class(void);
void print_class_time_info (void);
void check_class_time(void);
void print_class_result(void);
void waiting_for_studants(void);
void print_tag_information(String);
void print_hex(byte*, byte);
void print_decimal(byte*, byte);
void get_time_after_x_sec(int, int, int, int);
void validate_attendance(String);
void open_door_for_students(String);
void buzz(void);
String get_student_tag(byte*, byte);
String get_student_name_by_tag(String);

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.begin(9600);
  SPI.begin(); // Init SPI bus

  myservo.attach(SERVO_PIN);  // attaching servo to the servo object
  myservo.write(0);

  rfid.PCD_Init(); // Init MFRC522
  Serial.println();
  Serial.print(F("Reader :"));
  rfid.PCD_DumpVersionToSerial();
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  WiFi.begin(ssid, password); // connecting to the internet for getting the time
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  timeClient.begin();
  timeClient.setTimeOffset(16200); // GMT+4:30 (4.5*3600)
  timeClient.update();
  Serial.println();
  print_current_time();
  Serial.println("Connected to the Internet!");
  Serial.println("Waiting for the instructor to start the class...");
}

void loop() {
  timeClient.update();
  check_instructor_attendance();
  check_class_time();
  waiting_for_studants();
}

void check_instructor_attendance() {
  bool is_button_pressed = analogRead(LDR_PIN) > 800;
  if (is_button_pressed) {
    is_instructor_in_class = !is_instructor_in_class;
    digitalWrite(LED_PIN, is_instructor_in_class ? HIGH : LOW);
    if (is_class_started && !is_instructor_in_class) {
      print_current_time();
      Serial.println("The instructor left the class!");
      is_class_started = false;
      is_deadline_missed = false;
      is_class_ended = false;
      for (int i = 0; i < NUMBER_OF_STUDENTS ; i++)
        is_student_present[i] = false;
    }
  }
  if (is_instructor_in_class && !is_class_started) {
    start_class();
  }
  delay(1000); // waiting for the LDR to be in a lighter place
}

void start_class() {
  is_class_started = true;
  start_time = timeClient.getEpochTime();
  // TODO: Convert 1 to 60
  enterance_deadline = start_time + 1.5 * ENTERANCE_DEADLINE_IN_MINUTE;
  end_time = start_time + 1.5 * CLASS_TIME_IN_MINUTE;
  print_class_time_info();
}

void print_class_time_info () {
  print_current_time();
  Serial.println("Class Started!");
  int seconds = timeClient.getSeconds();
  int minutes = timeClient.getMinutes();
  int hours = timeClient.getHours();
  Serial.print("  - Start Time : ");
  Serial.println(timeClient.getFormattedTime());
  Serial.print("  - Deadline   : ");
  // TODO: convert 1 to 60
  get_time_after_x_sec(hours, minutes, seconds, 1.5 * ENTERANCE_DEADLINE_IN_MINUTE);
  Serial.print("  - End Time   : ");
  get_time_after_x_sec(hours, minutes, seconds, 1.5 * CLASS_TIME_IN_MINUTE);
}

void get_time_after_x_sec(int hour, int minute, int second, int offset_in_seconds) {
  second += offset_in_seconds;
  if (second >= 60) {
    minute += second / 60;
    second = second % 60;
  }
  if (minute >= 60) {
    hour += minute / 60;
    minute = minute % 60;
  }
  if (hour >= 24) {
    hour = hour % 24;
  }
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.println(second);
}

void check_class_time() {
  if (is_class_started) {
    unsigned long current_time = timeClient.getEpochTime();
    if (current_time >= end_time && !is_class_ended) {
      is_class_ended = true;
      print_current_time();
      Serial.println("The Class has been ended!");
      print_class_result();
    } else if (current_time >= enterance_deadline && !is_deadline_missed) {
      is_deadline_missed = true;
      print_current_time();
      Serial.println("No one can enter the class anymore!");
    }
  }
}

void print_class_result() {
  for (int i = 0; i < NUMBER_OF_STUDENTS ; i++) {
    Serial.print("  - ");
    Serial.print(student_name[i]);
    Serial.print(": \t");
    Serial.println(is_student_present[i] ? "PRESENT" : "ABSENT");
  }
}

void waiting_for_studants() {
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! rfid.PICC_IsNewCardPresent()) return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial()) return;

  print_current_time();
  Serial.println("A tag was detectad!");
  String student_tag = get_student_tag(rfid.uid.uidByte, rfid.uid.size);
  print_tag_information(student_tag);

  if (!is_class_started) {
    Serial.println("The class has not started yet!");
    delay(500);
    return;
  }
  
  validate_attendance(student_tag);

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

void print_tag_information(String student_tag) {
  Serial.print("  - PICC type       : ");
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));
  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

  // Store NUID into nuidPICC array
  for (byte i = 0; i < 4; i++) {
    nuidPICC[i] = rfid.uid.uidByte[i];
  }

  Serial.print("  - Tag Information :");
  print_hex(rfid.uid.uidByte, rfid.uid.size);
  Serial.print(" (");
  print_decimal(rfid.uid.uidByte, rfid.uid.size);
  Serial.println(")");

  Serial.print("  - Student Name    : ");
  Serial.println(get_student_name_by_tag(student_tag));
}

void print_hex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void print_decimal(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

String get_student_tag(byte *buffer, byte bufferSize) {
  String student_tag = "";
  for (byte i = 0; i < bufferSize; i++) {
    student_tag += String(buffer[i], HEX);
  }
  return student_tag;
}

String get_student_name_by_tag(String tag) {
  for (int i = 0; i < NUMBER_OF_STUDENTS; i++)
    if (student_tags[i] == tag)
      return student_name[i];
  return "Unknown Student";
}

void validate_attendance(String student_tag) {
  if (is_deadline_missed) {
    Serial.print("Sorry, you can not enter the class because ");
    Serial.println(is_class_ended ? "it is over!" : "you were too late!");
    buzz();
  } else {
    Serial.println("Welcome to the class!");
    open_door_for_students(student_tag);
  }
}

void open_door_for_students(String student_tag) {
  Serial.print("[");
  Serial.print(timeClient.getFormattedTime());
  Serial.println("] Openning the door...");
  myservo.write(180);

  for (int i = 0; i < NUMBER_OF_STUDENTS ; i++)
    if (student_tag == student_tags[i])
      is_student_present[i] = true;

  delay(3000); // leaving the door open for 3 seconds

  Serial.print("[");
  Serial.print(timeClient.getFormattedTime());
  Serial.println("] Closing the door...");
  myservo.write(0);
}

void buzz() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000); // active the buzzer for 1 second
  digitalWrite(BUZZER_PIN, LOW);
}

void print_current_time() {
  Serial.println("----------------------------------------------------------------------------------");
  Serial.print("[");
  Serial.print(timeClient.getFormattedTime());
  Serial.print("] ");
}
