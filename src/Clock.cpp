#include "Particle.h"

// ── Pins ──────────────────────────────────────────────────────
#define CLK_PIN  D1
#define DATA_PIN D2
#define CS_PIN   D0

// ── Display config ────────────────────────────────────────────
#define NUM_DEVICES 4               // 4 chained FC16_HW 8×8 modules
#define DISPLAY_COLS (NUM_DEVICES * 8)  // 32 total columns

// ── MAX7219 registers ─────────────────────────────────────────
#define REG_DIGIT0      0x01
#define REG_DECODE      0x09
#define REG_INTENSITY   0x0A
#define REG_SCANLIMIT   0x0B
#define REG_SHUTDOWN    0x0C
#define REG_DISPLAYTEST 0x0F

// ── Font ──────────────────────────────────────────────────────
// Each digit is 4 columns. Each byte is one column; bit 0 = top row.
// Taken directly from fonts.h (ASCII '0'-'9').
static const uint8_t DIGIT_FONT[10][4] = {
    {0x7F, 0x41, 0x41, 0x7F}, // 0
    {0x00, 0x42, 0x7F, 0x40}, // 1
    {0x79, 0x49, 0x49, 0x4F}, // 2
    {0x63, 0x49, 0x49, 0x77}, // 3
    {0x0F, 0x08, 0x08, 0x7F}, // 4
    {0x4F, 0x49, 0x49, 0x79}, // 5
    {0x7F, 0x49, 0x49, 0x79}, // 6
    {0x03, 0x01, 0x01, 0x7F}, // 7
    {0x77, 0x49, 0x49, 0x77}, // 8
    {0x4F, 0x49, 0x49, 0x7F}, // 9
};
// Colon: single column, two dots at rows 2 and 4
static const uint8_t COLON_COL = 0x14;  // 0b00010100

// ── Frame buffer ─────────────────────────────────────────────
// fb[c] = 8-bit row bitmask for column c; bit 0 = top row.
// Column 0 is the leftmost column on the physical display.
static uint8_t fb[DISPLAY_COLS];

// ── SPI (bit-bang) ────────────────────────────────────────────
static void spiByte(uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        digitalWrite(DATA_PIN, (b >> i) & 1);
        digitalWrite(CLK_PIN, HIGH);
        digitalWrite(CLK_PIN, LOW);
    }
}

// Write the same register/value to all devices in the chain
static void maxSendAll(uint8_t reg, uint8_t val) {
    digitalWrite(CS_PIN, LOW);
    for (int i = 0; i < NUM_DEVICES; i++) {
        spiByte(reg);
        spiByte(val);
    }
    digitalWrite(CS_PIN, HIGH);
}

// ── MAX7219 initialisation ────────────────────────────────────
static void maxInit() {
    maxSendAll(REG_DISPLAYTEST, 0x00); // normal operation
    maxSendAll(REG_SCANLIMIT,   0x07); // scan all 8 rows
    maxSendAll(REG_DECODE,      0x00); // raw matrix mode (no BCD)
    maxSendAll(REG_INTENSITY,   0x08); // mid brightness (0x00-0x0F)
    maxSendAll(REG_SHUTDOWN,    0x01); // wake up
}

// ── Flush frame buffer to display ────────────────────────────
// FC16_HW chain: MCU DIN → M0 (rightmost) → M1 → M2 → M3 (leftmost).
// First packet clocked shifts furthest and lands in M3 (leftmost).
// So fb[0-7] (leftmost) is sent first, fb[24-31] (rightmost) last.
// Within each module bit c of the DIG register drives column c (0=left).
static void maxFlush() {
    for (uint8_t row = 0; row < 8; row++) {
        digitalWrite(CS_PIN, LOW);
        for (int m = 0; m < NUM_DEVICES; m++) {
            int base = m * 8;
            uint8_t rowByte = 0;
            for (int c = 0; c < 8; c++) {
                if (fb[base + c] & (1 << row)) {
                    rowByte |= (1 << c);
                }
            }
            spiByte(REG_DIGIT0 + row);
            spiByte(rowByte);
        }
        digitalWrite(CS_PIN, HIGH);
    }
}

// ── Text rendering ────────────────────────────────────────────
static int charWidth(char c) {
    if (c >= '0' && c <= '9') return 4;
    if (c == ':') return 1;
    return 1; // space or unknown
}

// Total pixel width including 1-column gaps between characters
static int textWidth(const char *s) {
    int w = 0;
    for (int i = 0; s[i]; i++) {
        if (i > 0) w++; // inter-character gap
        w += charWidth(s[i]);
    }
    return w;
}

static void drawChar(char c, int x) {
    if (c >= '0' && c <= '9') {
        for (int i = 0; i < 4; i++) {
            if (x + i >= 0 && x + i < DISPLAY_COLS)
                fb[x + i] = DIGIT_FONT[c - '0'][i];
        }
    } else if (c == ':') {
        if (x >= 0 && x < DISPLAY_COLS)
            fb[x] = COLON_COL;
    }
}

// Render a string centred on the 32-column display and flush
static void showString(const char *s) {
    memset(fb, 0, sizeof(fb));
    int x = (DISPLAY_COLS - textWidth(s)) / 2;
    for (int i = 0; s[i]; i++) {
        if (i > 0) x++;          // inter-character gap
        drawChar(s[i], x);
        x += charWidth(s[i]);
    }
    maxFlush();
}

// ── Particle state ────────────────────────────────────────────
String displayText = "Akshay";
int lastMinute = -1;

int setDisplayText(String newText) {
    displayText = newText;
    showString(newText.c_str());
    return 1;
}

void setup() {
    pinMode(CS_PIN,   OUTPUT);
    pinMode(CLK_PIN,  OUTPUT);
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(CS_PIN,  HIGH);
    digitalWrite(CLK_PIN, LOW);

    maxInit();
    Time.zone(5.5);

    Particle.variable("displayText", displayText);
    Particle.function("setText", setDisplayText);
}

void loop() {
    int currentMinute = Time.minute();
    if (currentMinute == lastMinute) return;

    lastMinute = currentMinute;
    char timeBuffer[6]; // "HH:MM"
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d",
             Time.hourFormat12(), Time.minute());
    displayText = String(timeBuffer);
    showString(timeBuffer);
}
