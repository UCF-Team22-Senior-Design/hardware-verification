#include <Arduino.h>

// Networking Libray
#include <painlessmesh.h>

#define MESH_PREFIX "hardwareVerificationPrefix"
#define MESH_PASSWORD "hardwareVerificationPass"
#define MESH_PORT 5555

Scheduler userScheduler;
painlessMesh mesh;

void statusReport();
Task taskStatusReport(TASK_SECOND * 1, TASK_FOREVER, &statusReport);


// Audio libraries
#include <HTTPClient.h>
#include <SD.h>
#include <AudioGeneratorAAC.h>
#include <AudioOutputI2S.h>
#include <AudioFileSourcePROGMEM.h>
#include "sampleaac.h"

AudioFileSourcePROGMEM *in;
AudioGeneratorAAC *aac;
AudioOutputI2S *out;

void audioUpdate();
Task taskAudioUpdate(TASK_MILLISECOND * 10, TASK_FOREVER, &audioUpdate);

void audioUpdate()
{
  if (aac->isRunning())
  {
    aac->loop();
  }
  else
  {
    aac->stop();
    taskAudioUpdate.restartDelayed(TASK_SECOND * 5);
  }
}

// Comment out this line to build this program in target mode
//#define CONTROLLER

#ifdef CONTROLLER
  #define PLATFORM_CONTROLLER
  #define PLATFORM_LABEL "Controller"
#else
  #define PLATFORM_TARGET
  #define PLATFORM_LABEL "Target"
#endif

#ifdef PLATFORM_TARGET

// Lighting Libraries
#include <Adafruit_NeoPixel.h>

#define LED_PIN 2
#define NUMPIXELS 30
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void ledsUpdate();
Task taskLEDUpdate(TASK_SECOND * 1, TASK_FOREVER, &ledsUpdate);

void ledsUpdate()
{
  // March a single LED across the entire strip
  static int lightPos = 0;

  pixels.clear();
  pixels.setPixelColor(lightPos++, pixels.Color(79, 175, 239));
  pixels.show();
}

#endif

// Graphics Display Libraries
#ifdef PLATFORM_CONTROLLER

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PIN_SDA 21
#define PIN_SCL 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3D
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void screenUpdate();
Task taskScreenUpdate(TASK_SECOND * 1, TASK_FOREVER, &screenUpdate);

void screenUpdate()
{
  // Just print out the current time to the center of the screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.printf("Node Time: %lu\n\n", (unsigned long) mesh.getNodeTime());
  display.println("Hardware Verification!");

  display.display();
}

#endif

#ifdef PLATFORM_CONTROLLER
// Controller buttons: Left/Right menu, trigger
#define PIN_LEFT    36
#define PIN_RIGHT   37
#define PIN_TRIGGER  4

// Controller laser is driven by three pins.
#define PIN_LASER1  12
#define PIN_LASER2  13
#define PIN_LASER3  15

void driveLaser();
Task taskDriveLaser(TASK_MILLISECOND * 17, TASK_FOREVER, &driveLaser);

void driveLaser()
{
  // Get the status of the trigger
  int laserState = digitalRead(PIN_TRIGGER);

  // Set the laser's state to be that of the trigger (which is active-low)
  digitalWrite(PIN_LASER1, !laserState);
  digitalWrite(PIN_LASER2, !laserState);
  digitalWrite(PIN_LASER3, !laserState);
}
#endif

#ifdef PLATFORM_TARGET
// Target inputs: 
#define PIN_PHOTOTRANSISTOR 18
#endif

void statusReport()
{
  // Print out information about the current state of inputs/things to serial
  Serial.printf("<%lu> Status Report: \n", (unsigned long)mesh.getNodeTime());

  // Print out status of wireless network
  Serial.printf("\t# Of Network Nodes: %d\n", mesh.getNodeList(true).size());

  // Print out audio status
  Serial.printf("\tAudio Playback Status: %s\n", aac->isRunning() ? "Running" : "Stopped");

  // Print out input statuses
  Serial.printf("\tInput States: \n");
#ifdef CONTROLLER
  Serial.printf("\t\tLeft Menu: %d\n", digitalRead(PIN_LEFT));
  Serial.printf("\t\tRight Menu: %d\n", digitalRead(PIN_RIGHT));
  Serial.printf("\t\tTrigger: %d\n", digitalRead(PIN_TRIGGER));
#else
  Serial.printf("\t\tPhototrans State: %d\n", digitalRead(PIN_PHOTOTRANSISTOR));
#endif
  Serial.printf("\n\n");
}

void messageReceived(uint32_t from, String &msg)
{
  // Print out any messages we recieve
  Serial.printf("[painlessMesh] Received message from %d mesg=%s\n", from, msg.c_str());
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.printf("\nHardware Verification Begun. Platform: %s\n", PLATFORM_LABEL);

  Serial.printf("Initializing wireless mesh...\n");
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&messageReceived);

  userScheduler.addTask(taskStatusReport);
  taskStatusReport.enable();

  Serial.printf("Wireless Mesh Setup Complete.\n\n");

  // Setup Audio
  Serial.printf("Initializing Audio Hardware.\n");
  in = new AudioFileSourcePROGMEM(sampleaac, sizeof(sampleaac));
  aac = new AudioGeneratorAAC();
  out = new AudioOutputI2S();
  out->SetGain(0.125);
  aac->begin(in, out);

  userScheduler.addTask(taskAudioUpdate);
  taskAudioUpdate.enable();

  Serial.printf("Audio setup complete.\n\n");

#ifdef PLATFORM_CONTROLLER
  // Setup display

  Serial.printf("Initializing Display Hardware.");
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.printf("SSD1306 allocation failed");
  } else 
  {
    Serial.printf("Allocation of display successfull.\n");
    display.display();
  }

  userScheduler.addTask(taskScreenUpdate);
  taskScreenUpdate.enable();

  Serial.printf("Display setup complete.\n\n");

  // Register general IO pins
  pinMode(PIN_LEFT, INPUT);
  pinMode(PIN_RIGHT, INPUT);
  pinMode(PIN_TRIGGER, INPUT);
  
  pinMode(PIN_LASER1, OUTPUT);
  pinMode(PIN_LASER2, OUTPUT);
  pinMode(PIN_LASER3, OUTPUT);

  userScheduler.addTask(taskDriveLaser);
  taskDriveLaser.enable();
#endif

#ifdef PLATFORM_TARGET
  // Setup LEDs
  Serial.printf("Initializing LEDs\n");

  pixels.begin();

  userScheduler.addTask(taskLEDUpdate);
  taskLEDUpdate.enable();

  pinMode(PIN_PHOTOTRANSISTOR, INPUT);
#endif

  Serial.println("Setup complete.\n");
}

void loop()
{
  mesh.update();
}