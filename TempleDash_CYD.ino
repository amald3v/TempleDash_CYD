#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// CYD ESP32-2432S024 (2.4-inch) with shared display/touch SPI bus.
// TFT_eSPI must be configured separately; see README.
TFT_eSPI tft;
TFT_eSprite canvas(&tft);
#define TOUCH_CS 33
// Polling the shared bus catches short taps without depending on IRQ wiring.
XPT2046_Touchscreen touch(TOUCH_CS);

// Calibrated from this panel's four-corner readings.
int16_t RAW_X_MIN = 907, RAW_X_MAX = 3307;
int16_t RAW_Y_MIN = 680, RAW_Y_MAX = 3565;
bool SWAP_XY = true;
bool INVERT_X = false;
bool INVERT_Y = false;

const int W = 320, H = 240;
const uint16_t SKY = 0x2D9F, SAND = 0xD5A6, DARK = 0x2104;
const uint16_t GOLD = 0xFE40, RED = 0xF800;
int lane = 1;
bool jumping = false;
uint32_t actionUntil = 0, lastFrame = 0, lastSpawn = 0;
int score = 0, coins = 0;
float speedPixelsPerSecond = 62.0f;
bool gameOver = false, started = false;

struct Thing { int lane; float y; uint8_t type; bool active; };
Thing things[8];

int laneX(int l) { return 160 + (l - 1) * 70; }

void drawTrack() {
  canvas.fillScreen(SKY);
  canvas.fillRect(0, 95, W, 145, SAND);
  canvas.fillTriangle(0, 95, 125, 95, 0, 240, 0x9D48);
  canvas.fillTriangle(125, 95, 70, 240, 0, 240, 0x9D48);
  canvas.fillTriangle(195, 95, 320, 95, 320, 240, 0x9D48);
  canvas.fillTriangle(195, 95, 250, 240, 320, 240, 0x9D48);
  canvas.drawLine(125, 95, 90, 240, 0xFFFF);
  canvas.drawLine(195, 95, 230, 240, 0xFFFF);
  canvas.drawLine(160, 95, 160, 240, 0xB596);
  canvas.fillRect(0, 0, W, 24, DARK);
  canvas.setTextColor(TFT_WHITE, DARK); canvas.setTextSize(2);
  canvas.setCursor(8, 4); canvas.printf("RUN %d   COINS %d", score, coins);
}

void drawRunner() {
  int x = laneX(lane), y = 190;
  if (jumping) y = 158;
  uint16_t c = TFT_BLUE;
  canvas.fillCircle(x, y - 16, 10, c);
  canvas.fillRoundRect(x - 9, y - 7, 18, 27, 5, c);
  canvas.drawLine(x - 5, y + 17, x - 10, y + 32, c);
  canvas.drawLine(x + 5, y + 17, x + 10, y + 32, c);
}

void drawThings() {
  for (auto &o : things) if (o.active) {
    int x = laneX(o.lane), y = o.y;
    if (o.type == 0) { // block
      canvas.fillRoundRect(x - 13, y - 16, 26, 30, 4, RED);
      canvas.drawRect(x - 13, y - 16, 26, 30, TFT_WHITE);
    } else if (o.type == 1) { // low barrier
      canvas.fillRect(x - 20, y - 18, 40, 9, DARK);
      canvas.fillRect(x - 17, y - 9, 5, 17, DARK); canvas.fillRect(x + 12, y - 9, 5, 17, DARK);
    } else { canvas.fillCircle(x, y, 8, GOLD); canvas.drawCircle(x, y, 8, TFT_WHITE); }
  }
}

void showStart() {
  drawTrack();
  canvas.setTextColor(TFT_WHITE, SKY); canvas.setTextSize(3);
  canvas.setCursor(45, 45); canvas.print("TEMPLE DASH");
  canvas.setTextSize(2); canvas.setCursor(31, 112); canvas.print("Tap to start");
  canvas.setCursor(18, 145); canvas.print("Left/right: lanes");
  canvas.setCursor(18, 170); canvas.print("Center: jump");
  canvas.pushSprite(0, 0);
}

void resetGame() {
  lane = 1; score = coins = 0; speedPixelsPerSecond = 62.0f; gameOver = false; started = true;
  jumping = false;
  for (auto &o : things) o.active = false;
  lastSpawn = millis(); // first obstacle waits for the full opening interval
  lastFrame = millis();
}

void showGameOver() {
  drawTrack(); drawThings(); drawRunner();
  canvas.fillRoundRect(30, 65, 260, 105, 10, DARK);
  canvas.setTextColor(TFT_WHITE, DARK); canvas.setTextSize(3);
  canvas.setCursor(76, 78); canvas.print("GAME OVER");
  canvas.setTextSize(2); canvas.setCursor(80, 116); canvas.printf("Score: %d", score);
  canvas.setCursor(54, 143); canvas.print("Tap to run again");
}

bool readTouch(int &x, int &y) {
  if (!touch.touched()) return false;
  TS_Point p = touch.getPoint();
  // Reject open-bus readings while allowing panel readings slightly beyond calibration taps.
  if (p.z < 220 || p.z > 4000 || p.x < 400 || p.x > 3800 ||
      p.y < 400 || p.y > 3800) return false;
  int a, b;
  if (SWAP_XY) {
    // On this panel raw Y runs left-to-right; raw X runs top-to-bottom.
    a = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, W - 1);
    b = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, H - 1);
  } else {
    a = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, W - 1);
    b = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, H - 1);
  }
  if (INVERT_X) a = W - 1 - a;
  if (INVERT_Y) b = H - 1 - b;
  x = constrain(a, 0, W - 1); y = constrain(b, 0, H - 1);
  return true;
}

void handleTouch() {
  static bool wasDown = false;
  static bool handled = false;
  static uint32_t downSince = 0;
  static uint32_t lastAction = 0;
  static int32_t sumX = 0, sumY = 0;
  static uint8_t samples = 0;
  int x, y; bool down = readTouch(x, y);
  uint32_t now = millis();
  if (down) {
    if (!wasDown) {
      wasDown = true;
      handled = false;
      downSince = now;
      sumX = x; sumY = y; samples = 1;
    } else if (samples < 5) {
      sumX += x; sumY += y; samples++;
    }
    // Average a few controller samples before acting; one action per press.
    if (!handled && samples >= 2 && now - downSince >= 18 && now - lastAction >= 140) {
      x = sumX / samples; y = sumY / samples;
      if (!started || gameOver) resetGame();
      else if (x < 90) lane = max(0, lane - 1);
      else if (x > 230) lane = min(2, lane + 1);
      else { jumping = true; actionUntil = now + 850; }
      handled = true;
      lastAction = now;
    }
  } else {
    wasDown = false;
    handled = false;
    samples = 0;
  }
}

void spawnThing() {
  for (auto &o : things) if (!o.active) {
    o.active = true; o.lane = random(0, 3); o.y = 82;
    // Mostly coins; obstacles are blocks or barriers.
    int r = random(0, 10); o.type = (r < 5) ? 2 : (r < 8 ? 0 : 1);
    return;
  }
}

void setup() {
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  Serial.begin(115200);
  tft.init(); tft.setRotation(1);
  // The 2.4-inch board shares the TFT SPI bus with its touch controller.
  touch.begin(tft.getSPIinstance()); touch.setRotation(1);
  canvas.setColorDepth(8);
  if (!canvas.createSprite(W, H)) {
    Serial.println("ERROR: 8-bit frame buffer allocation failed");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 90);
    tft.print("Buffer memory error");
    while (true) delay(1000);
  }
  randomSeed(analogRead(34));
  showStart();
}

void loop() {
  handleTouch();
  if (!started || gameOver) { delay(1); return; }
  unsigned long now = millis();
  if (now >= actionUntil) jumping = false;
  if (now - lastFrame < 45) return;
  float dt = (now - lastFrame) * 0.001f;
  if (dt > 0.10f) dt = 0.10f;
  lastFrame = now;

  // Redraw scene, advance objects toward the player.
  drawTrack();
  for (auto &o : things) if (o.active) {
    o.y += speedPixelsPerSecond * dt;
    if (o.y > 245) {
      o.active = false;
      score++;
      if (score % 10 == 0) speedPixelsPerSecond = min(96.0f, speedPixelsPerSecond + 4.0f);
    }
    else if (o.y >= 174 && o.y <= 218 && o.lane == lane) {
      if (o.type == 2) { coins++; score += 2; o.active = false; }
      else if (!jumping) { gameOver = true; }
    }
  }
  uint32_t spawnInterval = (uint32_t)max(850, 1900 - score * 18);
  if (now - lastSpawn > spawnInterval) { spawnThing(); lastSpawn = now; }
  drawThings(); drawRunner();
  if (gameOver) showGameOver();
  canvas.pushSprite(0, 0);
}
