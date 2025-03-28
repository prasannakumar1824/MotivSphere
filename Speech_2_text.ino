// ------------------------------------------------------------------------------------------------------------------------------
// ------------------                      AUDIO RECORDING with ESP32 & I2S Microphone                       ------------------
// ----------------                                     Store as WAV on SD Card                              ------------------
// ----------------                          + Deepgram Speech-to-Text API Integration                       ------------------
// ----------------                                       Updated: February 21, 2025                         ------------------
// ------------------                                                                                      ------------------
// ------------------                    Features:                                                         ------------------
// ------------------                  ✅ Record 1-minute audio from I2S mic                                ------------------
// ------------------                  ✅ Save recording as a .wav file on SD card                          ------------------
// ------------------                  ✅ View real-time waveform in Arduino Serial Plotter                 ------------------
// ------------------                  ✅ Upload recording to Deepgram API for speech transcription         ------------------
// ------------------                                                                                      ------------------
// ------------------------------------------------------------------------------------------------------------------------------

/*
📡 **FULL CONNECTION SETUP:**

---
🗂️ **SD Card Module** → **ESP32 (VSPI Interface)**
- **GND** → GND  
- **VCC** → VIN (5V)  
- **MISO** → D19  
- **MOSI** → D23  
- **SCK** → D18  
- **CS** → D5  

---
🎙️ **I2S Microphone (e.g., INMP441, SPH0645)** → **ESP32**
- **GND** → GND  
- **VDD** → 3.3V  
- **SD (Data Out)** → D35  
- **SCK (Bit Clock)** → D33  
- **WS (Word Select/LRCK)** → D22  
- **L/R** → 3.3V *(Selects left channel for mono recording)*  

---
💡 **LED (Optional Status Indicator)** → **ESP32**
- **Anode (+)** → GPIO2  
- **Cathode (-)** → GND *(via a 220Ω resistor recommended)*  

---

🚀 **HOW TO USE:**
1. **Power the ESP32 and connect the modules as above.**  
2. **Insert your Deepgram API key** at the indicated line in the code.  
3. **Upload this code** → **Open Arduino IDE Serial Plotter (115200 baud)**  
4. **Press the reset button on ESP32** → Records for 1 minute and saves as `recorded_audio.wav`.  
5. **The code uploads the recording** → Receives and prints the transcription from Deepgram.  

✅ **Libraries Required:**
- ESP32 Board Support (via Arduino IDE Boards Manager)
- SD.h *(bundled with ESP32 core libraries)*
- I2S.h *(built into ESP32 core)*
- WiFi.h *(for internet connectivity)*
- HTTPClient.h *(for REST API requests)*

---

📂 **FILE STRUCTURE:**
- `recorded_audio.wav`: 1-minute audio recording in WAV format.
- **Transcription Output:** Printed in the Arduino Serial Monitor.

---
*/

#include <SD.h>
#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>

#define SAMPLE_RATE     16000
#define SAMPLE_BITS     16
#define CHANNELS        1
#define RECORD_TIME_SEC 60
#define I2S_PORT        I2S_NUM_0

#define SD_CS_PIN 5
#define I2S_WS     22
#define I2S_SCK    33
#define I2S_SD     35

// 🌐 Wi-Fi credentials
const char* ssid = "Plusone";  // 🔑 Insert your Wi-Fi SSID here
const char* password = "nanna369";  // 🔑 Insert your Wi-Fi password here

// 📝 Deepgram API Key (Insert your API key here)
const char* DEEPGRAM_API_KEY = "0aeb5cc5d797e42bc553e8e639c97e2d6790204d";

File audioFile;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  WiFi.begin(ssid, password);
  Serial.print("[INFO] Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi Connected.");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[ERROR] SD Card Mount Failed!");
    return;
  }
  Serial.println("[OK] SD Card Initialized.");

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);

  audioFile = SD.open("/recorded_audio.wav", FILE_WRITE);
  if (!audioFile) {
    Serial.println("[ERROR] Failed to create file.");
    return;
  }

  writeWavHeader(audioFile, SAMPLE_RATE, SAMPLE_BITS, CHANNELS);
  Serial.println("\n🔴 Recording started for 1 minute...");
  recordAudio(RECORD_TIME_SEC);
  finalizeWavHeader(audioFile);
  audioFile.close();
  Serial.println("✅ Recording saved as 'recorded_audio.wav'");

  uploadToDeepgram("/recorded_audio.wav");  // 🚀 Upload recording & get transcription
}

void loop() {}

void writeWavHeader(File &file, uint32_t sampleRate, uint16_t bitsPerSample, uint16_t channels) {
  uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;

  file.write((const uint8_t *)"RIFF", 4);
  file.write((const uint8_t *)"\0\0\0\0", 4); // Placeholder
  file.write((const uint8_t *)"WAVE", 4);
  file.write((const uint8_t *)"fmt ", 4);

  uint8_t fmt_chunk[] = {16, 0, 0, 0, 1, 0, (uint8_t)channels, 0};
  file.write(fmt_chunk, sizeof(fmt_chunk));

  uint8_t sample_rate_chunk[] = {
    (uint8_t)(sampleRate & 0xFF), (uint8_t)((sampleRate >> 8) & 0xFF),
    (uint8_t)((sampleRate >> 16) & 0xFF), (uint8_t)((sampleRate >> 24) & 0xFF)
  };
  file.write(sample_rate_chunk, sizeof(sample_rate_chunk));

  uint8_t byte_rate_chunk[] = {
    (uint8_t)(byteRate & 0xFF), (uint8_t)((byteRate >> 8) & 0xFF),
    (uint8_t)((byteRate >> 16) & 0xFF), (uint8_t)((byteRate >> 24) & 0xFF)
  };
  file.write(byte_rate_chunk, sizeof(byte_rate_chunk));

  uint8_t block_align_chunk[] = {(uint8_t)((channels * bitsPerSample) / 8), 0, (uint8_t)bitsPerSample, 0};
  file.write(block_align_chunk, sizeof(block_align_chunk));

  file.write((const uint8_t *)"data", 4);
  file.write((const uint8_t *)"\0\0\0\0", 4); // Data size placeholder
}

void finalizeWavHeader(File &file) {
  uint32_t fileSize = file.size();

  file.seek(4);
  uint8_t riff_size_chunk[] = {
    (uint8_t)((fileSize - 8) & 0xFF), (uint8_t)(((fileSize - 8) >> 8) & 0xFF),
    (uint8_t)(((fileSize - 8) >> 16) & 0xFF), (uint8_t)(((fileSize - 8) >> 24) & 0xFF)
  };
  file.write(riff_size_chunk, sizeof(riff_size_chunk));

  file.seek(40);
  uint32_t data_chunk_size = fileSize - 44;
  uint8_t data_size_chunk[] = {
    (uint8_t)(data_chunk_size & 0xFF), (uint8_t)((data_chunk_size >> 8) & 0xFF),
    (uint8_t)((data_chunk_size >> 16) & 0xFF), (uint8_t)((data_chunk_size >> 24) & 0xFF)
  };
  file.write(data_size_chunk, sizeof(data_size_chunk));
}

void recordAudio(uint32_t durationSec) {
  size_t bytesRead;
  int16_t buffer[512];
  uint32_t totalSamples = SAMPLE_RATE * durationSec;
  uint32_t samplesRecorded = 0;

  while (samplesRecorded < totalSamples) {
    i2s_read(I2S_PORT, &buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
    audioFile.write((uint8_t *)buffer, bytesRead);

    int16_t sample = buffer[0] >> 4;  // For Serial Plotter visualization
    Serial.println(sample);

    samplesRecorded += bytesRead / (SAMPLE_BITS / 8);
  }
}

void uploadToDeepgram(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println("[ERROR] Failed to open WAV file for uploading.");
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://api.deepgram.com/v1/listen");
    http.addHeader("Authorization", String("Token ") + DEEPGRAM_API_KEY);
    http.addHeader("Content-Type", "audio/wav");

    Serial.println("🚀 Uploading to Deepgram...");
    int httpResponseCode = http.sendRequest("POST", &file, file.size());

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("📝 Transcription:");
      Serial.println(response);
    } else {
      Serial.printf("[ERROR] Upload failed. Code: %d\n", httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("[ERROR] Wi-Fi disconnected during upload.");
  }

  file.close();
}
