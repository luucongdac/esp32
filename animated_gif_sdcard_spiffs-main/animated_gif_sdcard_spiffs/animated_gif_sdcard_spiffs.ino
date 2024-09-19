#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>      // Install this library with the Arduino Library Manager
                           // Don't forget to configure the driver for the display!

#include <AnimatedGIF.h>   // Install this library with the Arduino Library Manager

#define SD_CS_PIN 12 // Chip Select Pin (CS) for SD card Reader

AnimatedGIF gif;
File gifFile;              // Global File object for the GIF file
TFT_eSPI tft = TFT_eSPI(); 

char *filename = "/dac.gif";   // Change to load other gif files in images/GIF

//-------------------------[web server]--------------------------------------
#include <WiFi.h>          // Thư viện WiFi để tạo Access Point
#include <WebServer.h>      // Thư viện WebServer để tạo web server

const char *ssid = "ESP32-Access-Point";  // Tên mạng WiFi mà ESP32 sẽ phát ra
const char *password = "12345678";        // Mật khẩu của mạng WiFi
//web 192.168.4.1
WebServer server(80);                     // Khởi tạo web server trên cổng 80

//-------------------------
void setup()
{
  // Khởi tạo WiFi AP trước tiên
  setupWiFiAP();
    
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(2); // Adjust Rotation of your screen (0-3)
  tft.fillScreen(TFT_BLACK);

  // Initialize SD card
  Serial.println("SD card initialization...");
  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("SD card initialization failed!");
    return;
  }

  // Initialize SPIFFS
  Serial.println("Initialize SPIFFS...");
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS initialization failed!");
  }

  //copyGifFromSDtoSPIFFS(filename);

  // Initialize the GIF
  Serial.println("Starting animation...");
  gif.begin(BIG_ENDIAN_PIXELS);

  // Cấu hình các đường dẫn của web server
  server.on("/", handleRoot);          // Trang chủ
  server.on("/upload", HTTP_POST, []() { server.send(200); }, handleFileUpload); // Xử lý upload file
  server.on("/get", handleGetFiles);   // Hiển thị danh sách file
  server.begin();                   // Bắt đầu WebServer
  Serial.println("Web server started");

}

void loop()
{
  server.handleClient(); // Xử lý các yêu cầu HTTP từ web server

  if (gif.open(filename, fileOpen, fileClose, fileRead, fileSeek, GIFDraw))
  {
    tft.startWrite(); // The TFT chip slect is locked low
    while (gif.playFrame(true, NULL))
    {
    }
    gif.close();
    tft.endWrite(); // Release TFT chip select for the SD Card Reader
  }
}
void copyGifFromSDtoSPIFFS(char *filename)
{
  // // Initialize SD card
  // Serial.println("SD card initialization...");
  // if (!SD.begin(SD_CS_PIN))
  // {
  //   Serial.println("SD card initialization failed!");
  //   return;
  // }

  // // Initialize SPIFFS
  // Serial.println("Initialize SPIFFS...");
  // if (!SPIFFS.begin(true))
  // {
  //   Serial.println("SPIFFS initialization failed!");
  // }

  // Reformmating the SPIFFS to have space if a large GIF is loaded
  // You could run out of SPIFFS storage space if you load more than one image or a large GIF
  // You can also increase the SPIFFS storage space by changing the partition of the ESP32
  //
  Serial.println("Formatting SPIFFS...");
  SPIFFS.format(); // This will erase all the files, change as needed, SPIFFs is non-volatile memory
  Serial.println("SPIFFS formatted successfully.");

  // Open GIF file from SD card
  Serial.println("Openning GIF file from SD card...");
  File sdFile = SD.open(filename);
  if (!sdFile)
  {
    Serial.println("Failed to open GIF file from SD card!");
    return;
  }

  // Create a file in SPIFFS to store the GIF
  File spiffsFile = SPIFFS.open(filename, FILE_WRITE, true);
  if (!spiffsFile)
  {
    Serial.println("Failed to copy GIF in SPIFFS!");
    return;
  }

  // Read the GIF from SD card and write to SPIFFS
  Serial.println("Copy GIF in SPIFFS...");
  byte buffer[512];
  while (sdFile.available())
  {
    int bytesRead = sdFile.read(buffer, sizeof(buffer));
    spiffsFile.write(buffer, bytesRead);
  }

  spiffsFile.close();
  sdFile.close();
  // Serial.println("SPIFFS usaged: " + (SPIFFS.usedBytes() - SPIFFS.totalBytes())/SPIFFS.totalBytes() *100);

}
// Callbacks for file operations for the Animated GIF Lobrary
void *fileOpen(const char *filename, int32_t *pFileSize)
{
  gifFile = SPIFFS.open(filename, FILE_READ);
  *pFileSize = gifFile.size();
  if (!gifFile)
  {
    Serial.println("Failed to open GIF file from SPIFFS!");
  }
  return &gifFile;
}

void fileClose(void *pHandle)
{
  gifFile.close();
}

int32_t fileRead(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
  int32_t iBytesRead;
  iBytesRead = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos;
  if (iBytesRead <= 0)
    return 0;

  gifFile.seek(pFile->iPos);
  int32_t bytesRead = gifFile.read(pBuf, iLen);
  pFile->iPos += iBytesRead;

  return bytesRead;
}

int32_t fileSeek(GIFFILE *pFile, int32_t iPosition)
{
  if (iPosition < 0)
    iPosition = 0;
  else if (iPosition >= pFile->iSize)
    iPosition = pFile->iSize - 1;
  pFile->iPos = iPosition;
  gifFile.seek(pFile->iPos);
  return iPosition;
}


//---------------[wifi]
void setupWiFiAP() {
  Serial.println("Setting AP (Access Point)...");
  WiFi.softAP(ssid, password);            // Tạo Access Point với tên và mật khẩu đã định

  IPAddress IP = WiFi.softAPIP();          // Lấy địa chỉ IP của ESP32 sau khi phát WiFi
  Serial.print("AP IP address: ");
  Serial.println(IP);
}
void handleRoot() {
  String html = "<html><body><h1>ESP32 File Management</h1>";

  // Form upload file
  html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
  html += "<input type='file' name='upload'>";
  html += "<input type='submit' value='Upload File'>";
  html += "</form><br>";

  // Nút xem danh sách file
  html += "<form action='/get' method='GET'>";
  html += "<button type='submit'>Show Files</button>";
  html += "</form>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}


void handleFileUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Upload File Start: %s\n", upload.filename.c_str());

    // Mở file trên thẻ SD để lưu dữ liệu
    File file = SD.open("/" + upload.filename, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing");
      server.send(500, "text/plain", "Failed to open file for writing");
      return;
    }
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Serial.printf("Uploading... Size: %u\n", upload.currentSize);
    File file = SD.open("/" + upload.filename, FILE_APPEND);
    if (file) {
      file.write(upload.buf, upload.currentSize);  // Ghi dữ liệu vào file
      file.close();
    } else {
      Serial.println("Failed to open file for appending");
      server.send(500, "text/plain", "Failed to open file for appending");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.printf("Upload complete. Size: %u\n", upload.totalSize);
    server.send(200, "text/plain", "File Uploaded Successfully");
  }
}



void handleGetFiles() {
  String fileList = "<html><body><h1>Files on SD Card</h1><ul>";

  File root = SD.open("/");
  File file = root.openNextFile();

  while (file) {
    fileList += "<li>";
    fileList += file.name();
    fileList += "</li>";
    file = root.openNextFile();
  }

  fileList += "</ul></body></html>";
  server.send(200, "text/html", fileList);
}


///-----------------