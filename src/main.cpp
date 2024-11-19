#include "BluetoothSerial.h"
#include "AudioTools.h"
#include "AudioLibs/A2DPStream.h"
#include "AudioLibs/AudioSourceSDFAT.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include <SPI.h>
#include <TFT_eSPI.h>

bool isRandom = true;  // if this value is true, it will run in random mode, if this value is false it will run in continous mode
long indexSize = 0;
int prevIndex = 0;
unsigned long timer = 0;

// Define the pins for your SD card connection
#define PIN_AUDIO_KIT_SD_CARD_CLK   18
#define PIN_AUDIO_KIT_SD_CARD_MISO  19
#define PIN_AUDIO_KIT_SD_CARD_MOSI  23
#define PIN_AUDIO_KIT_SD_CARD_CS    5

TFT_eSPI tft = TFT_eSPI();  // Invoke custom library setup (User_Setup.h)

BluetoothSerial SerialBT;

const char *startFilePath = "/";
const char* ext = "mp3";
AudioSourceSDFAT source(startFilePath, ext); 
A2DPStream out;
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);

unsigned long current_time = 0;
int ypos = 10;

bool deviceFound = false;
const char* targetDeviceName = "HUAWEI C51"; // Replace with your target device name
String device_list[10];
int found_device_number = 0;
void btAdvertisedDeviceFound(BTAdvertisedDevice* pDevice) {
    Serial.printf("Found a device: %s\n", pDevice->toString().c_str());
    device_list[found_device_number] = String(pDevice->getName().c_str());
    found_device_number = found_device_number + 1;

    if (String(pDevice->getName().c_str()) == targetDeviceName) {
        Serial.println("Target device found!");
        deviceFound = true;
        SerialBT.discoverAsyncStop();
    }

    if((millis()-current_time) > 15000){
         Serial.println("SCanning Time OUT ");
         SerialBT.discoverAsyncStop();
     }

}

void setup() {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);
     tft.init();
     tft.setRotation(1); // Rotate as needed (0-3 for four different orientations)
     tft.fillScreen(TFT_BLACK);
     tft.setTextColor(TFT_WHITE, TFT_BLACK);  // White text with black background
     tft.setTextDatum(MC_DATUM);  // Middle Center datum
     tft.drawString("Scanning", tft.width()/2, 10);

    // Start Bluetooth scanning
    current_time = millis();
    SerialBT.begin("ESP32Scanner");
    Serial.println("Starting Bluetooth scan...");
    
    if (SerialBT.discoverAsync(btAdvertisedDeviceFound)) {
        Serial.println("Scanning for devices...");
        while (!deviceFound) {
            //Serial.println((millis() - current_time));
            if((millis() - current_time) > 15000){
                SerialBT.discoverAsyncStop();
                break;
            }
            delay(100);
            Serial.print(".");
        }
    } else {
        Serial.println("Error starting Bluetooth scan");
    }

    // Clean up Bluetooth resources
    SerialBT.end();
    
    if(deviceFound == false){

        
        Serial.println("Total : " + String(found_device_number) + " found");
        for (int i=0; i<found_device_number; i++){
            Serial.println(String(i) + " : " + device_list[i] );
            ypos = ypos + 16;
            tft.drawString(String(i) + " : " + device_list[i], tft.width()/2, ypos);
        }
     Serial.println("Please enter device number to connect");
        
        while(1){

            if (Serial.available() > 0) {
            
                String inputString = Serial.readStringUntil('\n');
                int number = inputString.toInt();
                Serial.print("Received number: ");
                Serial.println(number);
                if(number<found_device_number){
                    const char* targetDeviceName2 = device_list[number].c_str();
                    targetDeviceName = targetDeviceName2;
                    //device_list[number].toCharArray(targetDeviceName, device_list[number].length() + 1);
                    Serial.print("Target Device : " );
                    Serial.println(targetDeviceName);
                    break;
                }
                else{
                    Serial.println("Wrong Entry");
                }
            }

        }

    }
    // Initialize SPI for SD card
    Serial.println("Start connecting to target device");

    digitalWrite(TFT_CS, HIGH);     //// disable TFT
    SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

    // Set up A2DP stream
    auto cfg = out.defaultConfig(TX_MODE);
    cfg.name = targetDeviceName;
    cfg.auto_reconnect = true;

    if (out.begin(cfg)) {
        Serial.println("Connected to A2DP device");
    } else {
        Serial.println("Failed to connect to A2DP device");
        return;
    }

    // Set up the audio player
    Serial.println("Setting up MP3 player...");
    player.setVolume(1.0);
    player.begin();
    Serial.println("MP3 player ready");
    indexSize = source.size() + 1;
    Serial.print("Total Songs: ");
    Serial.println(indexSize);
}

void loop() {
    // Continue playing the MP3 file
    player.copy();
    if(isRandom){
        int currentIndex = source.index();
        if((currentIndex != prevIndex) && ((millis() - timer) > 15000)){
            Serial.print("Current: ");
            Serial.print(currentIndex);
            Serial.print(", Previous: ");
            Serial.print(prevIndex);
            int newIndex = esp_random() % (indexSize);
            player.setIndex(newIndex);
            Serial.print(", New: ");
            Serial.println(newIndex);
            prevIndex = newIndex;
            timer = millis();
        }
    } else {
        if(!player.isActive()){
            Serial.println("Playlist Finished");
            player.begin();
        }
    }
}
