#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <SD.h>
#include <map>
#include <Preferences.h>
std::map<String, File> openSD;
std::map<String, File> openLFS;
unsigned long previousMillis = 0;
const long intervalHeart = 1500;
const long intervalSlow = 100;
bool ledState = LOW;
#define ESP32_TX_PIN 43
#define LED_PIN 16
int maxRetries = 3;
const unsigned long timeoutDuration = 5000;
SPIClass spi(HSPI);
const int chipSelect = 10;
const int sckPin = 14;
const int misoPin = 12;
const int mosiPin = 13;
const int sdCardDetectPin = 11;
bool sdCardInserted = false;
std::string txValue = "";
struct Parts;
int currentFilePosition = 0;
const int charLimit = 314;
File currentFile;
File file;
Preferences preferences;
int Dvalue = 0;
int SerialNo = 0;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 5.5 * 3600; 
const int   daylightOffset_sec = 0; 
//BLE********************************
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic;
BLECharacteristic* pRxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
std::vector<uint8_t> bleReceivedData;

void writeBLEDataToSerial(const std::vector<uint8_t>& data) {
 Serial.write(0x06);
  Serial.write(data.data(), data.size());
}class MyRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    // Get the raw data as an Arduino String object
    String rxValue = pCharacteristic->getValue();

    // Only proceed if there's data
    if (rxValue.length() > 0) {
      // Convert the String to a std::vector<uint8_t> and preserve binary data
      std::vector<uint8_t> rxValueVector(rxValue.length());
      
      // Copy each character (as byte) into the std::vector<uint8_t>
      for (size_t i = 0; i < rxValue.length(); ++i) {
        rxValueVector[i] = static_cast<uint8_t>(rxValue[i]);
      }

      // Append the received data to the buffer
      bleReceivedData.insert(bleReceivedData.end(), rxValueVector.begin(), rxValueVector.end());


    }
  }
};


class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};
void initializeBLE() {
  
  String deviceName = "Lactosure - Sl.No - " + String(SerialNo);

  // Initialize the BLE device with the constructed name
  BLEDevice::init(deviceName.c_str());
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyRxCallbacks());

  pService->start();

  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void printBLEAddress() {
 
  BLEAddress address = BLEDevice::getAddress();
  
  String addressStr = address.toString().c_str();
  addressStr.toUpperCase();
  


  Serial.println(addressStr);
}

//BLE***********************************

//SDCard********************************
void initializeSDCard() {
   Serial.begin(115200);
  spi.begin(sckPin, misoPin, mosiPin, chipSelect);
  if (!SD.begin(chipSelect, spi)) {
    Serial.println("SD,ERROR");
    return;
  }
  Serial.println("SD,OK");
}
void deinitializeSDCard() {
    SD.end(); 
    spi.end(); 
}

void SDlistFiles() {

  Serial.println("List");
  

  File root = SD.open("/");
  if (!root) {
    Serial.println("ERROR");
    return;
  }
  
  File file = root.openNextFile();
  while (file) {
    Serial.print(file.name());
    Serial.print(",");
    Serial.println(file.size());
    file.close();  // Close the current file before opening the next one
    file = root.openNextFile();
  }
  
  root.close();
}


void openFileSD(const String& filename, int mode) {

  if (openSD.find(filename) != openSD.end()) {
    Serial.println("OK");
    return;
  }

  File file;
  if (mode == 1) {  
    if (SD.exists(filename)) {
      Serial.println("ERROR,Filename already-exist");
      return;
    }

    file = SD.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("ERROR");
      return;
    }

  } else if (mode == 0) {  // Open an existing file
    if (!SD.exists(filename)) {
      Serial.println("ERROR");
      return;
    }

    file = SD.open(filename, FILE_READ);
    if (!file) {
      Serial.println("ERROR");
      return;
    }
  }

  openSD[filename] = file;  
  Serial.println("OK");
}
void ReadFileSD(const String& filename, int startNumberOfBytes, int endNumberOfBytes) {
  auto it = openSD.find(filename);
  if (it == openSD.end()) {
    Serial.println("ERROR: File not found.");
    return;
  }

  File file = it->second;

  // Adjust for 1-based indexing
  startNumberOfBytes--;
  endNumberOfBytes--;

  // Validate parameters
  if (startNumberOfBytes < 0 || endNumberOfBytes < startNumberOfBytes) {
    Serial.println("Pointer,ERROR: Invalid byte range.");
    return;
  }

  int numBytes = endNumberOfBytes - startNumberOfBytes + 1;

  // Seek to the start byte position
  if (!file.seek(startNumberOfBytes)) {
    Serial.println("ERROR: Seek failed.");
    return;
  }

  // Buffer for reading file data in chunks
  const int bufferSize = 512;  // You can adjust this value according to your memory constraints
  char buffer[bufferSize];
  int bytesRead = 0;

  // Read the file content in chunks
  while (bytesRead < numBytes && file.available()) {
    int toRead = min(bufferSize, numBytes - bytesRead);  // Calculate how much to read in this iteration
    int actualRead = file.readBytes(buffer, toRead);     // Read data into the buffer

    if (actualRead > 0) {
      Serial.write(buffer, actualRead);  // Print the buffer content directly
      bytesRead += actualRead;
    } else {
      break;  // Stop if no more data is available
    }
  }

  if (bytesRead < numBytes) {
    Serial.println("ERROR,BUFFER SIZE");
  }
}



void closeFileSD(const String& filename) {
  auto it = openSD.find(filename);
  if (it == openSD.end()) {
    Serial.println("ERROR");
    return;
  }

  File file = it->second;
  file.close();

  if (!file) {
    openSD.erase(it);  
    Serial.println("OK");
  } else {
    Serial.println("ERROR");
  }
}
void writeContinuousFileSD(const String& filename, const String& content, bool createNew, int startIdx, int endIdx, bool appendOnNewLine) {
    File file;

    // Check if the file is already open
    auto it = openSD.find(filename);
    if (it != openSD.end()) {
        file = it->second; // Use the already open file
    } else {
        if (createNew) {
            // Check if the file already exists
            if (SD.exists(filename)) {
                   Serial.print("ERROR,Filename already-exist");
                return;
            }
            // Create a new file and add data to it
            file = SD.open(filename, FILE_WRITE);
            if (!file) {
                Serial.print("ERROR");
                file.close();
                return;
            }
        } else {
            // Open existing file for reading
            if (!SD.exists(filename)) {
                Serial.print("ERROR");
                file.close();
                return;
            }
            file = SD.open(filename, FILE_READ); // Open for reading to preserve data
        }

        // Add the file to the map if not already open
        if (file) {
            openSD[filename] = file;
        }
    }

    if (!file) {
        Serial.print("ERROR");
        file.close();
        return;
    }

    // If creating a new file, start by writing the provided content
    if (createNew) {
        file.print(content);
        file.close();
        openSD.erase(filename);
        Serial.println("OK");
        return;
    }

    // Read the current content of the file
    String fileContent = "";
    while (file.available()) {
        fileContent += char(file.read());
    }

    // Handle the case where startIdx is the size of the file
    if (startIdx == fileContent.length() + 1) {
        // Check if endIdx is valid
        if (endIdx < content.length()) {
            Serial.print("ERROR");
            file.close();
            return;
        }

        // Prepare content based on appendOnNewLine flag
        String appendContent = appendOnNewLine ? "\n" + content : content;

        // Append the content to the existing file
        file = SD.open(filename, FILE_WRITE);
        if (file) {
            file.seek(fileContent.length()); // Move to the end of the file
            file.print(appendContent);
            file.close();

            // Remove from map if it was not already open
            if (it == openSD.end()) {
                openSD.erase(filename);
            }

            Serial.println("OK");
        } else {
            Serial.print("ERROR");
            file.close();
        }
        return;
    }

    // Ensure startIdx and endIdx are within the bounds
    if (startIdx < 1 || endIdx > fileContent.length() || startIdx > endIdx) {
        Serial.print("ERROR");
        file.close();
        return;
    }

    // Replace content between startIdx and endIdx
    String before = fileContent.substring(0, startIdx - 1); // Content before startIdx (1-based index)
    String after = fileContent.substring(endIdx);           // Content after endIdx
    String newContent = before + content + after;

    // Write the new content back to the file
    file = SD.open(filename, FILE_WRITE);
    if (file) {
        file.print(newContent);
        file.close();

        // Remove from map if it was not already open
        if (it == openSD.end()) {
            openSD.erase(filename);
        }

        Serial.println("OK");
    } else {
        Serial.print("ERROR");
        file.close();
    }
}

void deleteAllFilesSD() {
  // Open the root directory
  File root = SD.open("/");

  if (!root) {
    Serial.println("SD,ERROR");
    return;
  }

  bool allFilesDeleted = true;  // Flag to track if all files were successfully deleted

  // Iterate through all files in the root directory
  File file = root.openNextFile();
  while (file) {
    // Get the file name
    String fileName = file.name();

    // Check if the file name is "System Volume Information"
    if (fileName != "System Volume Information") {
      // Prepend a slash to the file name
      fileName = String("/") + fileName;

      // Attempt to delete the file
      if (!SD.remove(fileName.c_str())) {
        allFilesDeleted = false;  // If any file deletion fails, set the flag to false
      }
    }

    // Close the file
    file.close();

    // Get the next file
    file = root.openNextFile();
  }

  // Close the root directory
  root.close();

  // Print the result after all files have been processed
  if (allFilesDeleted) {
    Serial.println("OK");
  } else {
    Serial.println("Format,Incomplete");
  }
}

void deleteFileSD(const String& filename) {



  if (!SD.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  if (SD.remove(filename)) {
    Serial.println("OK");
  } else {
    Serial.println("ERROR");
  }
}
void copyFileToLittleFS(const char* filename) {

  String sdFilePath = "/";
  sdFilePath += filename;

  String littleFSFilePath = "/";
  littleFSFilePath += filename;

  File sdFile = SD.open(sdFilePath.c_str());
  if (!sdFile) {
    Serial.println("ERROR");
    return;
  }
  File littleFSFile = LittleFS.open(littleFSFilePath.c_str(), FILE_WRITE);
  if (!littleFSFile) {
    Serial.println("LFS,ERROR");
    sdFile.close();
    return;
  }
  uint8_t buffer[128];
  int bytesRead;
  while (sdFile.available()) {
    bytesRead = sdFile.read(buffer, sizeof(buffer));
    littleFSFile.write(buffer, bytesRead);
  }

  sdFile.close();
  littleFSFile.close();
  Serial.println("MOVE1,OK");
}


void copyFileToSDCard(const char* filename) {
  String sdFilePath = "/";
  sdFilePath += filename;
  String littleFSFilePath = "/";
  littleFSFilePath += filename;
  File littleFSFile = LittleFS.open(littleFSFilePath.c_str());
  if (!littleFSFile) {
    Serial.println("ERROR");
    return;
  }
  File sdFile = SD.open(sdFilePath.c_str(), FILE_WRITE);
  if (!sdFile) {
    Serial.println("SD,ERROR");
    littleFSFile.close();
    return;
  }
  uint8_t buffer[128];
  int bytesRead;
  while (littleFSFile.available()) {
    bytesRead = littleFSFile.read(buffer, sizeof(buffer));
    sdFile.write(buffer, bytesRead);
  }
  littleFSFile.close();
  sdFile.close();
  Serial.println("MOVE0,OK");
}


void sizeFileinput(const String& filename) {


 
  if (SD.exists(filename)) {
    File file = SD.open(filename, FILE_READ);
    if (file) {

      size_t fileSize = file.size();

      Serial.print(filename);
      Serial.print(",");
      Serial.print(fileSize);
      Serial.println();
      file.close();
    } else {
      Serial.println("ERROR");
    }
  } else {
    Serial.println("ERROR");
  }
}


void SDgetRateCFS(String filename, float clr, float fat, float snf) {
  if (!SD.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = SD.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }

    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }

    float clrValue = line.substring(0, comma1).toFloat();
    float fatValue = line.substring(comma1 + 1, comma2).toFloat();
    float snfValue = line.substring(comma2 + 1, comma3).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();

    if (clrValue == clr && fatValue == fat && snfValue == snf) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}

void SDgetRateFat(String filename, float fat) {
  if (!SD.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = SD.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }

    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }

    float fatValue = line.substring(comma1 + 1, comma2).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();

    if (fatValue == fat) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}

void SDgetRateClr(String filename, float clr) {
  if (!SD.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = SD.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }

    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }

    float clrValue = line.substring(0, comma1).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();

    if (clrValue == clr) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}

void SDgetRateFatClr(String filename, float fat, float clr) {
  if (!SD.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = SD.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }

    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }

    float clrValue = line.substring(0, comma1).toFloat();
    float fatValue = line.substring(comma1 + 1, comma2).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();

    if (clrValue == clr && fatValue == fat) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}

void SDgetRateFatSnf(String filename, float fat, float snf) {
  if (!SD.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = SD.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }

    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }

    float fatValue = line.substring(comma1 + 1, comma2).toFloat();
    float snfValue = line.substring(comma2 + 1, comma3).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();

    if (fatValue == fat && snfValue == snf) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}

//SDCard********************************

//LFS***********************************
void initLittleFS() {
  if (!LittleFS.begin(false)) {
    Serial.println("LFS,Formatting");
    if (!LittleFS.begin(true)) {
      Serial.println("LFS,ERROR");
    }
    Serial.println("LFS,OK");
  } else {
    Serial.println("LFS,OK");
  }
}


void emptyLittleFS() {
  LittleFS.format();
}
void ClearLittleFS() {
  LittleFS.format();
  Serial.println("OK");
}

void printBinary(const String& str) {
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    for (int j = 7; j >= 0; j--) {
      Serial.print((c >> j) & 1);
    }
    Serial.print(' ');
  }
  Serial.println();
}

void listFiles() {

  Serial.println("List");
  
  File root = LittleFS.open("/");
  if (!root) {
    Serial.println("ERROR");
    return;
  }
  
  File file = root.openNextFile();
  while (file) {
    Serial.print(file.name());
    Serial.print(",");
    Serial.println(file.size());
    file = root.openNextFile();
  }
  
  root.close();
}

void sizeFileinputLFS(const String& filename) {

  if (LittleFS.exists(filename)) {
    File file = LittleFS.open(filename, "r");
    if (file) {

      size_t fileSize = file.size();

      Serial.print(filename);
      Serial.print(",");
      Serial.print(fileSize);
      Serial.println();
      file.close();
    } else {
      Serial.println("ERROR");
    }
  } else {
    Serial.println("ERROR");
  }
}

void openFileLFS(const String& filename, int mode) {
  if (!LittleFS.begin()) {
    Serial.println("LFS,ERROR");
    return;
  }

  if (openLFS.find(filename) != openLFS.end()) {
    Serial.println("OK");
    return;
  }

  File file;
  if (mode == 1) {  
    if (LittleFS.exists(filename)) {
       Serial.println("ERROR,Filename already-exist");
      return;
    }

    file = LittleFS.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("ERROR");
      return;
    }

  } else if (mode == 0) {  
    if (!LittleFS.exists(filename)) {
      Serial.println("ERROR");
      return;
    }

    file = LittleFS.open(filename, FILE_READ);
    if (!file) {
      Serial.println("ERROR");
      return;
    }
  }

  openLFS[filename] = file;  
  Serial.println("OK");
}
void ReadFileLFS(const String& filename, int startNumberOfBytes, int endNumberOfBytes) {
  auto it = openLFS.find(filename);
  if (it == openLFS.end()) {
    Serial.println("ERROR");
    return;
  }

  File file = it->second;

  // Adjust for 1-based indexing
  startNumberOfBytes--; 
  endNumberOfBytes--;

  // Validate parameters
  if (startNumberOfBytes < 0 || endNumberOfBytes < startNumberOfBytes) {
    Serial.println("Pointer,ERROR");
    return;
  }

  int numBytes = endNumberOfBytes - startNumberOfBytes + 1;

  // Seek to the start byte
  file.seek(startNumberOfBytes);

  // Buffer to store chunks of file content
  const int bufferSize = 512;  // Set this according to memory constraints, higher for more efficiency
  char buffer[bufferSize];
  int bytesRead = 0;

  while (bytesRead < numBytes && file.available()) {
    int toRead = min(bufferSize, numBytes - bytesRead);  // Calculate how much to read in this iteration
    int actualRead = file.readBytes(buffer, toRead);     // Read data into buffer

    if (actualRead > 0) {
      // Print the read content directly
      Serial.write(buffer, actualRead);
      bytesRead += actualRead;
    } else {
      break;  // Stop if no more data is available
    }
  }

  // Check if we reached the end of the requested byte range
  if (bytesRead < numBytes) {
    Serial.println("ERROR,BUFFER SIZE");
  }
}


void closeFileLFS(const String& filename) {
  auto it = openLFS.find(filename);
  if (it == openLFS.end()) {
    Serial.println("ERROR");
    return;
  }

  File file = it->second;
  file.close();

  if (!file) {
    openLFS.erase(it);  
    Serial.println("OK");
  } else {
    Serial.println("ERROR");
  }
}

void writeContinuousFileLFS(const String& filename, const String& content, bool createNew, int startIdx, int endIdx, bool appendOnNewLine) {
    File file;

    // Check if the file is already open
    auto it = openLFS.find(filename);
    if (it != openLFS.end()) {
        file = it->second; // Use the already open file
    } else {
        if (createNew) {
            // Check if the file already exists
            if (LittleFS.exists(filename)) {
               Serial.println("ERROR,Filename already-exist");
                return;
            }
            // Create a new file and add data to it
            file = LittleFS.open(filename, FILE_WRITE);
            if (!file) {
                Serial.println("ERROR");
                return;
            }
        } else {
            // Open existing file for reading
            if (!LittleFS.exists(filename)) {
                Serial.println("ERROR");
                return;
            }
            file = LittleFS.open(filename, FILE_READ); // Open for reading to preserve data
        }

        // Add the file to the map if not already open
        if (file) {
            openLFS[filename] = file;
        }
    }

    if (!file) {
        Serial.println("ERROR");
        return;
    }

    // If creating a new file, start by writing the provided content
    if (createNew) {
        file.print(content);
        Serial.println("OK");
        return;
    }

    // Handle the case where startIdx is the size of the file (append mode)
    if (startIdx == file.size() + 1) {
        // Check if endIdx is valid
        if (endIdx < content.length()) {
            Serial.println("ERROR");
            return;
        }

        // Append the content to the existing file
        file = LittleFS.open(filename, FILE_APPEND); // Reopen in append mode
        if (file) {
            if (appendOnNewLine) {
                file.println(); // Add a new line before appending if required
            }
            file.print(content);

            // Remove from map if it was not already open
            if (it == openLFS.end()) {
                openLFS.erase(filename);
            }

            Serial.println("OK");
        } else {
            Serial.println("ERROR");
        }
        return;
    }

    // Read the current content of the file
    String fileContent = "";
    while (file.available()) {
        fileContent += char(file.read());
    }

    // Ensure startIdx and endIdx are within the bounds
    if (startIdx < 1 || endIdx > fileContent.length() || startIdx > endIdx) {
        Serial.println("ERROR");
        return;
    }

    // Replace content between startIdx and endIdx
    String before = fileContent.substring(0, startIdx - 1); // Content before startIdx (1-based index)
    String after = fileContent.substring(endIdx);           // Content after endIdx
    String newContent = before + content + after;

    // Write the new content back to the file
    file = LittleFS.open(filename, FILE_WRITE);
    if (file) {
        file.print(newContent);

        // Remove from map if it was not already open
        if (it == openLFS.end()) {
            openLFS.erase(filename);
        }

        Serial.println("OK");
    } else {
        Serial.println("ERROR");
    }
}



void deleteFileLFS(const String& filename) {

  if (!LittleFS.exists(filename)) {
    Serial.println("ERROR");
    
    return;
  }


  if (LittleFS.remove(filename)) {
    Serial.println("OK");
  } else {
    Serial.println("ERROR");
  }
}
void getRateCFS(String filename, float clr, float fat, float snf) {
  if (!LittleFS.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }

    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }
    float clrValue = line.substring(0, comma1).toFloat();
    float fatValue = line.substring(comma1 + 1, comma2).toFloat();
    float snfValue = line.substring(comma2 + 1, comma3).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();
    if (clrValue == clr && fatValue == fat && snfValue == snf) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}

void getRateFat(String filename, float fat) {
  if (!LittleFS.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();


    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }


    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }


    float fatValue = line.substring(comma1 + 1, comma2).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();


    if (fatValue == fat) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}
void getRateClr(String filename, float clr) {
  if (!LittleFS.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }


  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }


    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }


    float clrValue = line.substring(0, comma1).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();
    if (clrValue == clr) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}
void getRateFatClr(String filename, float fat, float clr) {
  if (!LittleFS.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }


  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }

    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }

    float clrValue = line.substring(0, comma1).toFloat();
    float fatValue = line.substring(comma1 + 1, comma2).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();
    if (clrValue == clr && fatValue == fat) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}
void getRateFatSnf(String filename, float fat, float snf) {
  if (!LittleFS.exists(filename)) {
    Serial.println("ERROR");
    return;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("ERROR");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.indexOf(',') == -1) {
      continue;
    }

    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      continue;
    }

    float fatValue = line.substring(comma1 + 1, comma2).toFloat();
    float snfValue = line.substring(comma2 + 1, comma3).toFloat();
    float rateValue = line.substring(comma3 + 1).toFloat();

    if (fatValue == fat && snfValue == snf) {
      Serial.println(rateValue);
      file.close();
      return;
    }
  }

  Serial.println("0");
  file.close();
}

//LFS*********************************

//WIFI********************************


void scanAndConnectToWiFi() {
  int numNetworks = WiFi.scanNetworks();

  if (numNetworks == -1) {
    Serial.println("Wifi,ERROR");
    return;
  }
  
  if (numNetworks == 0) {
    Serial.println("Wifi,EMPTY");
    return;
  }

  // Create an array to hold network indices for sorting
  int indices[numNetworks];
  for (int i = 0; i < numNetworks; i++) {
    indices[i] = i;
  }

  // Sort the indices in descending order based on RSSI (signal strength)
  for (int i = 0; i < numNetworks - 1; i++) {
    for (int j = i + 1; j < numNetworks; j++) {
      if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
      }
    }
  }

  // Determine the limit (top 10 networks or fewer)
  int limit = (numNetworks < 15) ? numNetworks : 15;

  // Print the SSIDs in the same comma-separated format as before
  Serial.print("SSIDs,");
  for (int i = 0; i < limit; i++) {
    Serial.print(WiFi.SSID(indices[i]));
    if (i < limit - 1) {
      Serial.print(",");
    }
  }
  Serial.println();
}

void connectToWiFi(const String& ssid, const String& password) {

  WiFi.begin(ssid.c_str(), password.c_str());
  int retryCount = 0;
  const int maxRetries = 20;
  const int retryDelay = 500;
  while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
    delay(retryDelay);
    retryCount++;
  }
  if (WiFi.status() == WL_CONNECTED) {

  } else {
    Serial.println("ERROR");
  }
}

bool downloadFileSD(const char* url, const char* filename) {

  // Check if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi,ERROR");
    return false;
  }

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  // Check for any errors during the HTTP request
  if (httpCode <= 0) {
    http.end();
    return false;
  }

  // Check if the HTTP response is successful
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  // Open the file on the SD card for writing
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("SD,ERROR");
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[128] = { 0 };
  int bytesRead = 0;
  int totalBytes = 0;
  unsigned long lastTime = millis();
  const unsigned long timeoutDuration = 10000; // 10 seconds timeout

  // Read data from the stream and write it to the file
  while (http.connected() || stream->available()) {
    while (stream->available()) {
      bytesRead = stream->readBytes(buffer, sizeof(buffer));
      file.write(buffer, bytesRead);
      totalBytes += bytesRead;
      lastTime = millis();
    }
    if (millis() - lastTime >= timeoutDuration) {
      break;
    }
    delay(1);
  }

  // Close the file and end the HTTP request
  file.close();
  http.end();

  // Return true if the download was successful (data was written)
  return totalBytes > 0;
}

bool downloadFileLFS(const char* url, const char* filename) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi,ERROR");
    return false;
  }

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode <= 0) {
    http.end();
    return false;
  }

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  File file = LittleFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("LFS,ERROR");
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[128] = { 0 };
  int bytesRead = 0;
  int totalBytes = 0;
  unsigned long lastTime = millis();

  while (http.connected() || stream->available()) {
    while (stream->available()) {
      bytesRead = stream->readBytes(buffer, sizeof(buffer));
      file.write(buffer, bytesRead);
      totalBytes += bytesRead;
      lastTime = millis();
    }
    if (millis() - lastTime >= timeoutDuration) {
      break;
    }

    delay(1);
  }

  file.close();
  http.end();

  return totalBytes > 0;
}

void httpDownloadChart(const String& url) {
  String filename;

  // Check if the URL contains specific endpoints
  if (url.endsWith("COW")) {
    filename = "/COW.csv";
  } else if (url.endsWith("BUF")) {
    filename = "/BUF.csv";
  } else if (url.endsWith("MIX")) {
    filename = "/MIX.csv";
  } else if (url.indexOf("FarmerInfo/GetLatestFarmerInfo") != -1) {
    filename = "/FARMERS.csv";
  } else {
    // Extract filename from URL dynamically, considering the '?' character
    int lastSlashIndex = url.lastIndexOf('/');
    if (lastSlashIndex != -1) {
      int queryIndex = url.indexOf('?');
      if (queryIndex != -1) {
        filename = "/" + url.substring(lastSlashIndex + 1, queryIndex); // Get the part after the last '/' and before '?'
      } else {
        filename = "/" + url.substring(lastSlashIndex + 1); // Get the part after the last '/'
      }
    } else {
      Serial.println("File,Unsupported");
      return;
    }
  }

  int attempts = 0;
  bool success = false;

  while (attempts < maxRetries) {

    if(Dvalue == 0) {
      if (downloadFileLFS(url.c_str(), filename.c_str())) {
        success = true;
        break;
      } else {
        attempts++;
      }
    } else if(Dvalue == 1) {
      if (downloadFileSD(url.c_str(), filename.c_str())) {
        success = true;
        break;
      } else {
        attempts++;
      }
    }
  }

  if (success) {
    Serial.println("Downloading,SUCCESS");
  } else {
    Serial.println("Downloading,Incomplete");
  }
}


//WIFI********************************
//SYS*********************************
void blinkLEDHeart() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= intervalHeart) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}
void blinkLED() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= intervalSlow) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}
void handleWiFiLED() {
  if (WiFi.status() == WL_CONNECTED) {
    blinkLEDHeart();
  } else {
    blinkLED();
  }
}
String removeCommas(String input) {
  input.replace(",", "");
  return input;
}
void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("ERROR,No NTP");
    return;
  }
  
  // Format: NT,Date,Month,Year,Hr,Min,Sec
  Serial.printf("NT,%02d,%02d,%04d,%02d,%02d,%02d\n",
                timeinfo.tm_mday,
                timeinfo.tm_mon + 1,
                timeinfo.tm_year + 1900,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec);
}



//SYS********************************

void setup() {
  Serial.begin(115200);
  Serial.println("Poornasree ESP-32");
  Serial.println("V-3");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  pinMode(LED_PIN, OUTPUT);
  pinMode(sdCardDetectPin, INPUT_PULLUP);
  initLittleFS();
  Dvalue = preferences.getInt("Dvalue", 0);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}
void loop() {
  static bool connected = false;
  static bool waitForURLInput = false;
  static bool checkRSSI = false;

//SYS********************************
  
 if (WiFi.status() == WL_CONNECTED && !connected) {
    connected = true;
    checkRSSI = true;

  } 
    else if (WiFi.status() == WL_CONNECT_FAILED) {
    Serial.println("Wifi,ERROR");
  } else if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
  if (input.equals("AT")) {
      Serial.println("ACK");
    } 
    else if (input.startsWith("IP")) {
      Serial.println("ACK");
      if (WiFi.isConnected()) {
        Serial.println(WiFi.localIP());
      } 
      else {
        Serial.println("0.0.0.0");
      }
    } 
    else if (input.equals("RST")) {
      Serial.print("ACK");
      Serial.println();
      delay(500);
      ESP.restart();
    }
    
     else if (input.equals("NTime")) {
         Serial.println("ACK");
   if (WiFi.status() == WL_CONNECTED) {
    printLocalTime();
  } else {
    Serial.println("ERROR,No Wifi");
  }

}
    
    //SYS***********************************

    //SDcard********************************
    else if (input.startsWith("InitSD")) {
      Serial.println("ACK");
      if (sdCardInserted) {
        initializeSDCard();
      } else {
        Serial.println("SD,ERROR");
      }
    }

    else if (input.startsWith("ListSD")) {
      Serial.println("ACK");
      if (sdCardInserted) {
        SDlistFiles();
      } else {
        Serial.println("SD,ERROR");
      }
    }
    else if (input.startsWith("Open,SD,")) {
      Serial.println("ACK");
      if (sdCardInserted) {
        int commaIndex = input.lastIndexOf(',');
        String filename = input.substring(8, commaIndex);
        int mode = input.substring(commaIndex + 1).toInt();
        openFileSD(filename, mode);
      } else {
        Serial.println("SD,ERROR");
      }
    } 
    else if (input.startsWith("Read,SD,")) {
      Serial.println("ACK");
      if (sdCardInserted) {
        int openIndex = input.indexOf("Read,SD,");
        String inputPart = input.substring(openIndex + 8);
        inputPart.trim();
        int commaIndex1 = inputPart.indexOf(',');
        int commaIndex2 = inputPart.indexOf(',', commaIndex1 + 1);
        String filename = inputPart.substring(0, commaIndex1);
        filename.trim();
        int startPosition = inputPart.substring(commaIndex1 + 1, commaIndex2).toInt();
        int endPosition = inputPart.substring(commaIndex2 + 1).toInt();
        if (startPosition <= 0 || endPosition <= 0 || startPosition > endPosition) {
      Serial.println("Pointer,ERROR");
          return;
        }
        ReadFileSD(filename, startPosition, endPosition);
      } else {
        Serial.println("SD,ERROR");
      }
    }

    else if (input.startsWith("Close,SD,")) {
      Serial.println("ACK");
      if (sdCardInserted) {
        String filename = input.substring(9);
        filename.trim();
        closeFileSD(filename);
      } else {
        Serial.println("SD,ERROR");
      }
    }

    else if (input.startsWith("Size,SD,")) {
      Serial.println("ACK");
      if (sdCardInserted) {
        String filename = input.substring(8);
        filename.trim();
        sizeFileinput(filename);
      } else {
        Serial.println("SD,ERROR");
      }
    } 
    
    else if (input.startsWith("MOVtoLFS")) {
      Serial.println("ACK");

      if (sdCardInserted == true) {
        String filename = input.substring(9);
        filename.trim();

        copyFileToLittleFS(filename.c_str());
      }

      if (sdCardInserted == false) {
        Serial.println("SD,ERROR");
      }
    }


    else if (input.startsWith("MOVtoSD")) {
      Serial.println("ACK");


      if (sdCardInserted == true) {
        String filename = input.substring(9);
        filename.trim();

        copyFileToSDCard(filename.c_str());
      }

      if (sdCardInserted == false) {
        Serial.println("SD,ERROR");
      }
    }




else if (input.startsWith("Write,SD,")) {
    Serial.println("ACK");
    if (sdCardInserted == true) {
        int commaIndex1 = input.indexOf(',', 9);
        if (commaIndex1 == -1) {
            Serial.println("ERROR");
            return;
        }

        int commaIndex2 = input.indexOf(',', commaIndex1 + 1);
        if (commaIndex2 == -1) {
            Serial.println("ERROR");
            return;
        }

        int commaIndex3 = input.indexOf(',', commaIndex2 + 1);
        if (commaIndex3 == -1) {
            Serial.println("ERROR");
            return;
        }

        int commaIndex4 = input.indexOf(',', commaIndex3 + 1);
        if (commaIndex4 == -1) {
            Serial.println("ERROR");
            return;
        }

        String filename = input.substring(9, commaIndex1);
        filename.trim();

        int createNew = input.substring(commaIndex1 + 1, commaIndex2).toInt();
        if (createNew != 0 && createNew != 1) {
            Serial.println("ERROR");
            return;
        }

        int startIdx = input.substring(commaIndex2 + 1, commaIndex3).toInt();
        int endIdx = input.substring(commaIndex3 + 1, commaIndex4).toInt();

        int appendOnNewLine = input.substring(commaIndex4 + 1, input.indexOf(',', commaIndex4 + 1)).toInt();
        if (appendOnNewLine != 0 && appendOnNewLine != 1) {
            Serial.println("ERROR");
            return;
        }

        String content = input.substring(input.indexOf(',', commaIndex4 + 1) + 1);
        content.trim();

        if (startIdx < 1 || endIdx < startIdx) {
            Serial.println("Pointer,ERROR");
            return;
        }

        writeContinuousFileSD(filename, content, createNew == 1, startIdx, endIdx, appendOnNewLine == 1);
    } else {
        Serial.println("SD,ERROR");
    }
}

    
    else if (input.startsWith("Delete,SD,")) {
      Serial.println("ACK");
      if (sdCardInserted) {
        String filename = input.substring(10);
        filename.trim();
        deleteFileSD(filename);
      } else {
        Serial.println("SD,ERROR");
      }
    }


    else if (input.startsWith("FormatSD")) {
      Serial.println("ACK");
      
       deleteAllFilesSD();
    }

    //SDcard********************************

    //LFS***********************************

    else if (input.equals("InitLFS")) {
      Serial.println("ACK");
      initLittleFS();
    }


    else if (input.equals("ListLFS")) {
            Serial.println("ACK");
      listFiles();
    }


    else if (input.startsWith("FormatLFS")) {
      Serial.println("ACK");
      ClearLittleFS();

    } 
    else if (input.startsWith("Delete,LFS,")) {
      Serial.println("ACK");
      String filename = input.substring(11);
      filename.trim();
      deleteFileLFS(filename);
    } 
    else if (input.startsWith("Open,LFS,")) {
      Serial.println("ACK");
      // Extract the filename and mode from the command
      int commaIndex = input.lastIndexOf(',');
      String filename = input.substring(9, commaIndex);
      int mode = input.substring(commaIndex + 1).toInt();

      openFileLFS(filename, mode);
    } 
    else if (input.startsWith("Read,LFS,")) {
      Serial.println("ACK");



        int openIndex = input.indexOf("Read,LFS,");
        String inputPart = input.substring(openIndex + 9);
        inputPart.trim();
        int commaIndex1 = inputPart.indexOf(',');
        int commaIndex2 = inputPart.indexOf(',', commaIndex1 + 1);


        String filename = inputPart.substring(0, commaIndex1);
        filename.trim();

        int startPosition = inputPart.substring(commaIndex1 + 1, commaIndex2).toInt();
        int endPosition = inputPart.substring(commaIndex2 + 1).toInt();


        if (startPosition <= 0 || endPosition <= 0 || startPosition > endPosition) {
          Serial.println("Pointer,ERROR");
          return;
        }


        ReadFileLFS(filename, startPosition, endPosition);
      } 
    


    else if (input.startsWith("Close,LFS,")) {
      Serial.println("ACK");
      String filename = input.substring(10);
      filename.trim();
      closeFileLFS(filename);
    } 

   else if (input.startsWith("Write,LFS,")) {
    Serial.println("ACK");

    int commaIndex1 = input.indexOf(',', 10);
    if (commaIndex1 == -1) {
        Serial.println("ERROR");
        return;
    }

    int commaIndex2 = input.indexOf(',', commaIndex1 + 1);
    if (commaIndex2 == -1) {
        Serial.println("ERROR");
        return;
    }


    int commaIndex3 = input.indexOf(',', commaIndex2 + 1);
    if (commaIndex3 == -1) {
        Serial.println("ERROR");
        return;
    }

    int commaIndex4 = input.indexOf(',', commaIndex3 + 1);
    if (commaIndex4 == -1) {
        Serial.println("ERROR");
        return;
    }

    String filename = input.substring(10, commaIndex1);
    filename.trim();

    int createNew = input.substring(commaIndex1 + 1, commaIndex2).toInt();
    if (createNew != 0 && createNew != 1) {
        Serial.println("ERROR");
        return;
    }

    int startIdx = input.substring(commaIndex2 + 1, commaIndex3).toInt();
    int endIdx = input.substring(commaIndex3 + 1, commaIndex4).toInt();

    int appendOnNewLine = input.substring(commaIndex4 + 1, input.indexOf(',', commaIndex4 + 1)).toInt();

    String content = input.substring(input.indexOf(',', commaIndex4 + 1) + 1);
    content.trim();

    if (startIdx < 1 || endIdx < startIdx) {
        Serial.println("Pointer,ERROR");
        return;
    }

    writeContinuousFileLFS(filename, content, createNew == 1, startIdx, endIdx, appendOnNewLine == 1);
}

    else if (input.startsWith("Size,LFS,")) {
      Serial.println("ACK");
      String filename = input.substring(9);
      filename.trim();
      sizeFileinputLFS(filename);
    } 
    else if (input.startsWith("RateCFS,")) {
  Serial.println("ACK");
  int comma1 = input.indexOf(',');
  int comma2 = input.indexOf(',', comma1 + 1);
  int comma3 = input.indexOf(',', comma2 + 1);
  int comma4 = input.indexOf(',', comma3 + 1);
  int comma5 = input.indexOf(',', comma4 + 1);

  if (comma1 != -1 && comma2 != -1 && comma3 != -1 && comma4 != -1 && comma5 != -1) {
    String storage = input.substring(comma1 + 1, comma2);
    String filename = input.substring(comma2 + 1, comma3);
    float clr = input.substring(comma3 + 1, comma4).toFloat();
    float fat = input.substring(comma4 + 1, comma5).toFloat();
    float snf = input.substring(comma5 + 1).toFloat();
    if (storage == "LFS") {
      getRateCFS(filename, clr, fat, snf);
    }
    if (storage == "SD") {
      SDgetRateCFS(filename, clr, fat, snf);
    }
  }
} 
else if (input.startsWith("RateF,")) {
  Serial.println("ACK");
  int comma1 = input.indexOf(',');
  int comma2 = input.indexOf(',', comma1 + 1);
  int comma3 = input.indexOf(',', comma2 + 1);

  if (comma1 != -1 && comma2 != -1 && comma3 != -1) {
    String storage = input.substring(comma1 + 1, comma2);
    String filename = input.substring(comma2 + 1, comma3);
    float fat = input.substring(comma3 + 1).toFloat();
    if (storage == "LFS") {
      getRateFat(filename, fat);
    }
       if (storage == "SD") {
      SDgetRateFat(filename, fat);
    }
  }
} 
else if (input.startsWith("RateC,")) {
  Serial.println("ACK");
  int comma1 = input.indexOf(',');
  int comma2 = input.indexOf(',', comma1 + 1);
  int comma3 = input.indexOf(',', comma2 + 1);

  if (comma1 != -1 && comma2 != -1 && comma3 != -1) {
    String storage = input.substring(comma1 + 1, comma2);
    String filename = input.substring(comma2 + 1, comma3);
    float clr = input.substring(comma3 + 1).toFloat();
    if (storage == "LFS") {
      getRateClr(filename, clr);
    }
        if (storage == "SD") {
      SDgetRateClr(filename, clr);
    }
  }
} 
else if (input.startsWith("RateFC,")) {
  Serial.println("ACK");
  int comma1 = input.indexOf(',');
  int comma2 = input.indexOf(',', comma1 + 1);
  int comma3 = input.indexOf(',', comma2 + 1);
  int comma4 = input.indexOf(',', comma3 + 1);

  if (comma1 != -1 && comma2 != -1 && comma3 != -1 && comma4 != -1) {
    String storage = input.substring(comma1 + 1, comma2);
    String filename = input.substring(comma2 + 1, comma3);
    float fat = input.substring(comma3 + 1, comma4).toFloat();
    float clr = input.substring(comma4 + 1).toFloat();
    if (storage == "LFS") {
      getRateFatClr(filename, fat, clr);
    }
        if (storage == "SD") {
      SDgetRateFatClr(filename, fat, clr);
    }
  }
}

else if (input.startsWith("RateFS,")) {
  Serial.println("ACK");
  int comma1 = input.indexOf(',');
  int comma2 = input.indexOf(',', comma1 + 1);
  int comma3 = input.indexOf(',', comma2 + 1);
  int comma4 = input.indexOf(',', comma3 + 1);

  if (comma1 != -1 && comma2 != -1 && comma3 != -1 && comma4 != -1) {
    String storage = input.substring(comma1 + 1, comma2);
    String filename = input.substring(comma2 + 1, comma3);
    float fat = input.substring(comma3 + 1, comma4).toFloat();
    float snf = input.substring(comma4 + 1).toFloat();
    if (storage == "LFS") {
      getRateFatSnf(filename, fat, snf);
    }
       if (storage == "SD") {
      SDgetRateFatSnf(filename, fat, snf);
    }
  }
}

    //LFS********************************

    //BLE********************************

    else if (input.equals("STATUS_READ")) {
    Serial.print("ACK");
    Serial.println();
    delay(200);
    byte command[] = {0x40, 0x04, 0x05, 0x00, 0x00, 0x41, 0x0D, 0x0A};
    Serial.write(command, sizeof(command)); // Send the raw bytes as a single command
}


     else if (input[0] == 0x40 && input[2] == 0x05) {
    
    uint8_t inputLength = input[1];
    

    // Combine input[3] and input[4] into a 16-bit integer (big-endian)
    uint16_t combinedValue = (input[3] << 8) | input[4];
    SerialNo=combinedValue;
     if (SerialNo == 0) {
        Serial.println("SerialNo,ERROR");
    } else {
        Serial.println("SerialNo,OK");
    }
}


    else if (input.equals("Start_ble")) {
      Serial.println("ACK");
      initializeBLE();

    }

       else if (input.equals("BLE_Address")) {
      Serial.println("ACK");
      printBLEAddress();

    }

    else if (input.equals("BLE_Status")) {
      Serial.println("ACK");
      if (deviceConnected) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
      }
    }

    else if (input.equals("Version")) {
      Serial.println("ACK");
      Serial.println("2");
    }

    else if (input.equals("BLE_Read")) {
    Serial.println("ACK");
      if (!bleReceivedData.empty()) {

        writeBLEDataToSerial(bleReceivedData);
        bleReceivedData.clear();
      } else {
        Serial.println("No Data");
      }
    } 
    else if (input.startsWith("BLE_Send,")) {
      int firstComma = input.indexOf(',');
      int secondComma = input.indexOf(',', firstComma + 1);
      if (firstComma != -1 && secondComma != -1) {
        String part2 = input.substring(firstComma + 1, secondComma);
        String part3 = input.substring(secondComma + 1);
        if (deviceConnected) {
          part3 = removeCommas(part3);
          pTxCharacteristic->setValue((uint8_t*)part3.c_str(), part3.length());
          pTxCharacteristic->notify();

        } else {
          Serial.println("ERROR,No Connection");
        }
      }
    }
    //BLE*********************************

    //WIFI********************************
    
      else if (input.equals("Scan")) {
      Serial.println("ACK");
      scanAndConnectToWiFi();
    } 
      else if (input.startsWith("Connect,")) {
          Serial.println("ACK");
      int firstComma = input.indexOf(',');
      int secondComma = input.indexOf(',', firstComma + 1);
      if (firstComma != -1 && secondComma != -1) {
        String ssid = input.substring(firstComma + 1, secondComma);
        String password = input.substring(secondComma + 1);
        if (!WiFi.isConnected()) {
          connectToWiFi(ssid, password);
          
        } else {
          int32_t rssi = WiFi.RSSI();
          Serial.print("OK,");
          Serial.println(rssi);
        }
      } else {
        Serial.println("ERROR");
      }
    }

    else if (input.startsWith("RSSI")) {
      Serial.println("ACK");

      int32_t rssi = WiFi.RSSI();
      Serial.print("RSSI,");
      Serial.println(rssi);

    } 
    
    else if (input.equals("ConStatus")) {
      Serial.println("ACK");
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("YES");
      } else {
        Serial.println("NO");
      }
    }

 else if (input.equals("Download,SD")) {
      Serial.println("ACK");
     Dvalue = 1;
      preferences.putInt("Dvalue", Dvalue);
      Serial.println("Set-Download,SD");
    } 
     else if (input.equals("Download,LFS")) {
      Serial.println("ACK");
     Dvalue = 0;
      preferences.putInt("Dvalue", Dvalue);
      Serial.println("Set-Download,LFS");
    } 
   else if (input.startsWith("SendHttpGet,")) {
       Serial.println("ACK");
      String url = input.substring(12); 
      url.trim();
      if (!WiFi.isConnected()) {
        Serial.println("ERROR,No Wifi");
        return;
      }
      if (url.endsWith("COW") || url.endsWith("BUF") || url.endsWith("MIX")|| url.indexOf("FarmerInfo/GetLatestFarmerInfo") != -1) {

  // Remove the comma if it exists between "?" and "InputString"
  int commaIndex = url.indexOf(",InputString");
  int questionMarkIndex = url.indexOf('?');
  if (commaIndex == questionMarkIndex + 1) {
    url.remove(commaIndex, 1);  // Remove the comma
 
  } 
   httpDownloadChart(url);
      }
  
else {
  
    HTTPClient http;
    url = removeCommas(url);
    
  // Remove the comma if it exists between "?" and "InputString"
  int commaIndex = url.indexOf(",InputString");
  int questionMarkIndex = url.indexOf('?');
  if (commaIndex == questionMarkIndex + 1) {
    url.remove(commaIndex, 1);  // Remove the comma
 
  } 

    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.print(httpCode);
            Serial.println();
            Serial.print(payload);
            Serial.println("OK");
        } else if (httpCode == HTTP_CODE_NOT_FOUND) {
            String payload = http.getString();
            Serial.println(payload);
        } else {
            Serial.println("ERROR");
            Serial.print(httpCode);
            Serial.println();
        }
    } else {
        Serial.println("ERROR");
        Serial.print(httpCode);
    }
    http.end();
}
    }
  }
  if (checkRSSI) {
    if (WiFi.status() == WL_CONNECTED) {
      int32_t rssi = WiFi.RSSI();
      Serial.print("OK,");
      Serial.println(rssi);
          configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }
    checkRSSI = false;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  if (!deviceConnected && oldDeviceConnected) {
    pServer->startAdvertising();
    oldDeviceConnected = deviceConnected;
  }
  handleWiFiLED();
  bool cardPresent = (digitalRead(sdCardDetectPin) == LOW);
  if (cardPresent && !sdCardInserted) {
      sdCardInserted = true;
      Serial.println("SD,Inserted");
  initializeSDCard();
  } 
  if  (!cardPresent && sdCardInserted) {
    Serial.println("SD,Removed");
    sdCardInserted = false;
     deinitializeSDCard(); 
  }
}
