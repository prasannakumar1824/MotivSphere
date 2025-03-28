// =====================
// Integrated Code: Dual TFT Displays + Audio Playback from SD Card
// Updated: TFT2 Image Display - Horizontal Orientation (Landscape Mode)
// =====================

#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <TimeLib.h> 
#include <driver/i2s.h>

// ------------------------------
// Pin Definitions
// ------------------------------
#define SD_CS      15
#define SPI_MOSI   23
#define SPI_MISO   19
#define SPI_SCK    18

#define TFT1_CS    5
#define TFT1_DC    2
#define TFT1_RST   4

#define TFT2_CS    16
#define TFT2_DC    17
#define TFT2_RST   21

#define I2S_BCLK   27
#define I2S_LRC    26
#define I2S_DOUT   25

#define PREV_BUTTON_PIN 14
#define DEBOUNCE_DELAY 50
#define BUFFER_SIZE 4096

// ------------------------------
// TFT Instances
// ------------------------------
Adafruit_ST7735 tft1 = Adafruit_ST7735(TFT1_CS, TFT1_DC, TFT1_RST);  
Adafruit_ST7735 tft2 = Adafruit_ST7735(TFT2_CS, TFT2_DC, TFT2_RST);  

// ------------------------------
// Audio Variables
// ------------------------------
File audioFile;
const int totalSongs = 5;
String songList[totalSongs] = {
  "/songs/song1.wav",
  "/songs/song2.wav",
  "/songs/song3.wav",
  "/songs/song4.wav",
  "/songs/song5.wav"
};
int currentSong = 0;
unsigned long lastButtonPressTime = 0;

// ------------------------------
// Image Display Variables
// ------------------------------
const uint8_t totalImages = 4;
uint8_t currentImage = 1;
unsigned long previousImageMillis = 0;
const long imageInterval = 5000;

// ------------------------------
// Time Variables
// ------------------------------
unsigned long previousMillis = 0;
const long interval = 1000;

// ------------------------------
// Function Declarations
// ------------------------------
void setupI2S();
void playSong(const String &filename);
void displayDate();
void displayTime();
void displayTextFromFile(const char *filename);
void displayBMPOnTFT2(uint8_t imageNumber);
uint16_t read16(File &f);
uint32_t read32(File &f);

// ------------------------------
// Setup Function
// ------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PREV_BUTTON_PIN, INPUT_PULLUP);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }

  tft1.initR(INITR_BLACKTAB);
  tft1.setRotation(1);  // Horizontal orientation for TFT1
  tft1.fillScreen(ST77XX_BLACK);

  tft2.initR(INITR_BLACKTAB);
  tft2.setRotation(3);  // Horizontal (landscape) orientation for TFT2
  tft2.fillScreen(ST77XX_BLACK);

  setupI2S();
  setTime(03, 30, 0, 07, 03, 2025);

  displayDate();
  displayTime();
  displayTextFromFile("/text.txt");
  displayBMPOnTFT2(currentImage);
}

// ------------------------------
// Loop Function
// ------------------------------
void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    adjustTime(1);
    displayTime();
  }

  if (currentMillis - previousImageMillis >= imageInterval) {
    previousImageMillis = currentMillis;
    currentImage = (currentImage % totalImages) + 1;
    displayBMPOnTFT2(currentImage);
  }

  playSong(songList[currentSong]);

  if (digitalRead(PREV_BUTTON_PIN) == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
      lastButtonPressTime = currentTime;
      currentSong = (currentSong - 1 + totalSongs) % totalSongs;
    }
  } else {
    currentSong = (currentSong + 1) % totalSongs;
  }
}

// ------------------------------
// I2S Setup
// ------------------------------
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// ------------------------------
// Audio Playback
// ------------------------------
void playSong(const String &filename) {
  if (!SD.exists(filename)) return;

  audioFile = SD.open(filename);
  if (!audioFile) return;

  uint8_t buffer[BUFFER_SIZE];
  while (audioFile.available()) {
    size_t bytesRead = audioFile.read(buffer, BUFFER_SIZE);
    if (bytesRead % 2 != 0) bytesRead--;
    size_t bytesWritten;
    i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }
  audioFile.close();
}

// ------------------------------
// Display Functions
// ------------------------------
void displayDate() {
  tft1.fillRect(0, 0, 64, 10, ST77XX_BLACK);
  tft1.setTextColor(ST77XX_CYAN);
  tft1.setCursor(0, 0);
  tft1.printf("%02d/%02d/%04d", day(), month(), year());
}

void displayTime() {
  tft1.fillRect(64, 0, 64, 10, ST77XX_BLACK);
  tft1.setTextColor(ST77XX_YELLOW);
  tft1.setCursor(80, 0);
  tft1.printf("%02d:%02d:%02d", hour(), minute(), second());
}

void displayTextFromFile(const char *filename) {
  File textFile = SD.open(filename);
  if (!textFile) return;

  tft1.fillRect(0, 20, 128, 110, ST77XX_BLACK);
  int yPos = 20;
  String line = "";

  while (textFile.available()) {
    char c = textFile.read();
    if (c == '\n' || !textFile.available()) {
      if (!textFile.available() && c != '\n') line += c;
      tft1.setTextSize(line == "" ? 2 : 1);
      tft1.setTextColor(line == "" ? ST77XX_RED : ST77XX_WHITE);
      tft1.setCursor(0, yPos);
      tft1.println(line);
      yPos += (line == "" ? 20 : 10);
      line = "";
    } else {
      line += c;
    }
  }
  textFile.close();
}

// ------------------------------
// BMP Display (TFT2 - Horizontal Orientation)
// ------------------------------
void displayBMPOnTFT2(uint8_t imageNumber) {
  String filename = "/images/image" + String(imageNumber) + ".bmp";
  displayBMP(filename.c_str(), tft2, 0, 0);
}

uint16_t read16(File &f) { return f.read() | (f.read() << 8); }
uint32_t read32(File &f) { return f.read() | (f.read() << 8) | (f.read() << 16) | (f.read() << 24); }

void displayBMP(const char *filename, Adafruit_ST7735 &display, int16_t x, int16_t y) {
  File bmpFile = SD.open(filename);
  if (!bmpFile || read16(bmpFile) != 0x4D42) return;

  (void)read32(bmpFile); 
  (void)read32(bmpFile); 
  uint32_t bmpImageOffset = read32(bmpFile);
  (void)read32(bmpFile);

  int32_t bmpWidth = read32(bmpFile);
  int32_t bmpHeight = read32(bmpFile);
  (void)read16(bmpFile); 
  if (read16(bmpFile) != 24 || read32(bmpFile) != 0) return;

  uint32_t rowSize = (bmpWidth * 3 + 3) & ~3;
  bool flip = bmpHeight > 0;
  bmpHeight = abs(bmpHeight);

  display.setAddrWindow(x, y, x + bmpWidth - 1, y + bmpHeight - 1);
  uint8_t sdbuffer[3 * 20];
  uint8_t buffidx = sizeof(sdbuffer);

  for (int row = 0; row < bmpHeight; row++) {
    uint32_t pos = bmpImageOffset + (flip ? (bmpHeight - 1 - row) : row) * rowSize;
    if (bmpFile.position() != pos) bmpFile.seek(pos);

    for (int col = 0; col < bmpWidth; col++) {
      if (buffidx >= sizeof(sdbuffer)) {
        bmpFile.read(sdbuffer, sizeof(sdbuffer));
        buffidx = 0;
      }
      uint8_t b = sdbuffer[buffidx++];
      uint8_t g = sdbuffer[buffidx++];
      uint8_t r = sdbuffer[buffidx++];
      display.drawPixel(x + col, y + row, display.color565(r, g, b));
    }
  }
  bmpFile.close();
}
