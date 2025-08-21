#include "unihiker_k10.h"
#include "ei_wrapper.h"          // from your Edge Impulse Arduino library build
#include <SD.h>
#include "esp_camera.h"

// ---------- UniHiker objects ----------
UNIHIKER_K10 k10;
uint8_t screen_dir = 2;

// ---------- Buffers ----------
static uint8_t g_gray96[96 * 96];

// ---------- Forwards ----------
void onButtonAPressed();
void onButtonBPressed();

static void dumpStats(const uint8_t* buf, int n, const char* tag);
static void resizeRGB565_fitShortest_centerCrop_to96x96(
  const uint8_t* in565, int srcW, int srcH, uint8_t* outGray);

static bool saveNextImage96x96Bin(const uint8_t* gray96);
static bool loadLatestImage96x96Bin(uint8_t* gray96, String* latestNameOut);
static uint16_t findNextIndexOnSD();
static int16_t  findLatestIndexOnSD();

static void showLine(const String& s, uint8_t row, uint32_t color = 0x000000);

// If your generated library doesn't expose this macro for label count,
// feel free to hard-set it (e.g. 3). Usually EI defines it.
#ifndef EI_CLASSIFIER_LABEL_COUNT
#define EI_CLASSIFIER_LABEL_COUNT 3
#endif

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[UniHiker K10] Booting...");

  // Board & UI
  k10.begin();
  k10.initScreen(screen_dir);
  k10.initBgCamerImage();
  k10.setBgCamerImage(true);
  k10.creatCanvas();
  k10.setScreenBackground(0xFFFFFF);

  // Buttons
  k10.buttonA->setPressedCallback(onButtonAPressed);
  k10.buttonB->setPressedCallback(onButtonBPressed);

  // SD
  k10.initSDFile();
  if (!SD.begin()) {
    showLine("SD init failed!", 1, 0xFF0000);
    Serial.println("❌ SD begin() failed");
    while (true) delay(10);
  }

  // Intro text
  showLine("A: Capture", 1);
  showLine("B: Infer latest", 2);
  k10.canvas->updateCanvas();

  Serial.println("[UniHiker K10] Ready.");
}

void loop() {
  delay(50);
}

// ===================== Button A: Capture & Save =====================
void onButtonAPressed() {
  Serial.println("\n[A] Capture pressed");

  k10.rgb->write(-1, 0x00FF00); // green
  k10.setBgCamerImage(false);
  k10.setScreenBackground(0xFFFFFF);
  k10.creatCanvas();
  showLine("Capturing...", 1);
  k10.canvas->updateCanvas();

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Camera capture failed");
    showLine("Capture failed", 2, 0xFF0000);
    k10.canvas->updateCanvas();
    k10.setBgCamerImage(true);
    return;
  }

  // fb->format must be RGB565; if it's not, change your camera init to PIXFORMAT_RGB565
  const int srcW = fb->width;
  const int srcH = fb->height;

  resizeRGB565_fitShortest_centerCrop_to96x96(fb->buf, srcW, srcH, g_gray96);
  esp_camera_fb_return(fb);

  dumpStats(g_gray96, 96 * 96, "CAPTURED");

  if (saveNextImage96x96Bin(g_gray96)) {
    showLine("Saved OK", 2, 0x008800);
  } else {
    showLine("Save failed", 2, 0xFF0000);
  }

  k10.canvas->updateCanvas();
  delay(800);
  k10.setBgCamerImage(true);
  k10.rgb->write(-1, 0x000000);
}

// ===================== Button B: Load latest & Inference =====================
void onButtonBPressed() {
  Serial.println("\n[B] Infer pressed");

  k10.rgb->write(-1, 0x0000FF); // blue
  k10.setBgCamerImage(false);
  k10.setScreenBackground(0xFFFFFF);
  k10.creatCanvas();

  showLine("Finding latest...", 1);
  k10.canvas->updateCanvas();

  String latest;
  if (!loadLatestImage96x96Bin(g_gray96, &latest)) {
    showLine("No image found!", 2, 0xFF0000);
    k10.canvas->updateCanvas();
    k10.rgb->write(-1, 0x000000);
    delay(1200);
    k10.setBgCamerImage(true);
    return;
  }

  dumpStats(g_gray96, 96 * 96, "LOADED");

  showLine("Latest:", 2);
  showLine(latest.c_str(), 3, 0x0000FF);
  showLine("Running inference...", 4);
  k10.canvas->updateCanvas();

  ei_impulse_result_t result;
  bool ok = run_classifier_on_96x96_grayscale(g_gray96, &result);
  if (!ok) {
    showLine("Inference failed", 5, 0xFF0000);
    k10.canvas->updateCanvas();
    k10.rgb->write(-1, 0x000000);
    delay(1500);
    k10.setBgCamerImage(true);
    return;
  }

  // Build sortable top-3
  struct Item { int idx; float val; };
  Item items[EI_CLASSIFIER_LABEL_COUNT];
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    items[i].idx = i;
    items[i].val = result.classification[i].value;
  }
  // simple sort desc
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT - 1; i++) {
    for (int j = i + 1; j < EI_CLASSIFIER_LABEL_COUNT; j++) {
      if (items[j].val > items[i].val) {
        Item tmp = items[i]; items[i] = items[j]; items[j] = tmp;
      }
    }
  }

  // Show top-3
  for (int r = 0; r < min(3, EI_CLASSIFIER_LABEL_COUNT); r++) {
    int idx = items[r].idx;
    float p  = items[r].val * 100.0f;

    char line[96];
    snprintf(line, sizeof(line), "%d) %s  %.1f%%",
             r + 1, result.classification[idx].label, p);
    showLine(line, 6 + r, (r == 0) ? 0x000000 : 0x3366FF);
    Serial.println(line);
  }

  k10.canvas->updateCanvas();
  k10.rgb->write(-1, 0x000000);
  delay(2500);
  k10.setBgCamerImage(true);
}

// ===================== Helpers =====================
static void showLine(const String& s, uint8_t row, uint32_t color) {
  // row 1..N, each row ~24px tall using the UniHiker canvasText(row) API
  k10.canvas->canvasText(s.c_str(), row, color);
}

static void dumpStats(const uint8_t* buf, int n, const char* tag) {
  int mn = 255, mx = 0;
  long sum = 0;
  for (int i = 0; i < n; i++) {
    int v = buf[i];
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    sum += v;
  }
  float mean = (float)sum / n;
  Serial.printf("[%s] min=%d max=%d mean=%.1f\n", tag, mn, mx, mean);
}

// Fit-shortest-axis + center-crop → 96×96, output grayscale
static void resizeRGB565_fitShortest_centerCrop_to96x96(
  const uint8_t* in565, int srcW, int srcH, uint8_t* outGray) {

  const int D = 96;
  float scale = (float)D / (srcW < srcH ? srcW : srcH);
  int scaledW = (int)roundf(srcW * scale);
  int scaledH = (int)roundf(srcH * scale);

  int x0 = (scaledW - D) / 2;
  int y0 = (scaledH - D) / 2;

  for (int y = 0; y < D; y++) {
    float syf = (y + y0) / scale;
    int sy = (int)floorf(syf);
    if (sy < 0) sy = 0;
    if (sy >= srcH) sy = srcH - 1;

    for (int x = 0; x < D; x++) {
      float sxf = (x + x0) / scale;
      int sx = (int)floorf(sxf);
      if (sx < 0) sx = 0;
      if (sx >= srcW) sx = srcW - 1;

      int idx = (sy * srcW + sx) * 2;
      uint8_t b1 = in565[idx + 0];
      uint8_t b2 = in565[idx + 1];
      uint16_t p = (uint16_t(b2) << 8) | b1;

      uint8_t r = ((p >> 11) & 0x1F) << 3;
      uint8_t g = ((p >> 5)  & 0x3F) << 2;
      uint8_t b = ( p        & 0x1F) << 3;
      uint8_t gray = (uint8_t)((30*r + 59*g + 11*b) / 100);

      outGray[y * D + x] = gray;
    }
  }
}

// Save to next free /img_XXXX.bin
static bool saveNextImage96x96Bin(const uint8_t* gray96) {
  uint16_t idx = findNextIndexOnSD();
  char name[24];
  snprintf(name, sizeof(name), "/img_%04u.bin", idx);

  File f = SD.open(name, FILE_WRITE);
  if (!f) {
    Serial.printf("❌ open %s failed\n", name);
    return false;
  }

  size_t w = f.write(gray96, 96 * 96);
  f.close();
  Serial.printf("Saved %s (%u bytes)\n", name, (unsigned)w);
  return (w == 96 * 96);
}

static bool loadLatestImage96x96Bin(uint8_t* gray96, String* latestNameOut) {
  int16_t idx = findLatestIndexOnSD();
  if (idx < 0) {
    Serial.println("No /img_XXXX.bin files found");
    return false;
  }
  char name[24];
  snprintf(name, sizeof(name), "/img_%04d.bin", idx);

  File f = SD.open(name, FILE_READ);
  if (!f) {
    Serial.printf("❌ open %s failed\n", name);
    return false;
  }
  size_t r = f.read(gray96, 96 * 96);
  f.close();

  if (latestNameOut) *latestNameOut = String(name);
  Serial.printf("Loaded %s (%u bytes)\n", name, (unsigned)r);
  return (r == 96 * 96);
}

// Find next free index (1..9999)
static uint16_t findNextIndexOnSD() {
  for (uint16_t i = 1; i < 10000; i++) {
    char name[24];
    snprintf(name, sizeof(name), "/img_%04u.bin", i);
    if (!SD.exists(name)) return i;
  }
  return 9999;
}

// Find greatest existing index, or -1 if none
static int16_t findLatestIndexOnSD() {
  int16_t latest = -1;
  for (uint16_t i = 1; i < 10000; i++) {
    char name[24];
    snprintf(name, sizeof(name), "/img_%04u.bin", i);
    if (SD.exists(name)) latest = i;
  }
  return latest;
}
