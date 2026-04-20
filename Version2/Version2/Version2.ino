/*
 * ST7789 76×284 + BMP280  —  ESP32-C6  (rotation 1 = landscape 284×76)
 *
 * Display : SCL=20  SDA=19  RST=18  DC=15  CS=14  BL=9(PROG)
 * BMP280  : SDA=7   SCL=6
 */

#include "driver/gpio.h"
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ── Display pins ──────────────────────────────────────────────────────────
#define LCD_SCK  20
#define LCD_SDA  19
#define LCD_RST  18
#define LCD_DC   15
#define LCD_CS   14
#define LCD_BL    9

// ── BMP280 I2C pins ───────────────────────────────────────────────────────
#define BMP_SDA   7
#define BMP_SCL   6
#define BMP_ADDR  0x76   // try 0x77 if sensor not found

// ── Panel geometry — rotation 1 (landscape 284×76) ───────────────────────
// The 82-col GRAM offset becomes a Y offset after 90° rotation
#define XOFF    0
#define YOFF   82
#define D_W   284
#define D_H    76

// ── Colours ───────────────────────────────────────────────────────────────
#define BLACK    0x0000
#define WHITE    0xFFFF
#define RED      0xF800
#define GREEN    0x07E0
#define BLUE     0x001F
#define YELLOW   0xFFE0
#define CYAN     0x07FF
#define MAGENTA  0xF81F
#define ORANGE   0xFD20
#define DKGRAY   0x4208
#define LTBLUE   0x051F

Adafruit_ST7789 tft(LCD_CS, LCD_DC, LCD_SDA, LCD_SCK, LCD_RST);
TwoWire         I2C_BMP = TwoWire(0);
Adafruit_BMP280 bmp(&I2C_BMP);

bool bmpOK = false;

// ── Window helpers ────────────────────────────────────────────────────────
void wFill(uint16_t c)                         { tft.fillRect(XOFF,YOFF,D_W,D_H,c); }
void wRect(int x,int y,int w,int h,uint16_t c) { tft.fillRect(XOFF+x,YOFF+y,w,h,c); }
void wVLine(int x,uint16_t c)                  { tft.drawFastVLine(XOFF+x,YOFF,D_H,c); }
void wText(int x,int y,const char*s,uint16_t fg,uint16_t bg=BLACK,uint8_t sz=1){
    tft.setTextColor(fg,bg); tft.setTextSize(sz);
    tft.setCursor(XOFF+x,YOFF+y); tft.print(s);
}

// ── History (last 20 readings) ────────────────────────────────────────────
#define HIST 20
float tHist[HIST]={}, pHist[HIST]={}, aHist[HIST]={};
int   histIdx = 0;

void pushHistory(float t, float p, float a){
    tHist[histIdx]=t; pHist[histIdx]=p; aHist[histIdx]=a;
    histIdx=(histIdx+1)%HIST;
}

// mini graph: x,y = top-left corner, w/h = size, within window coords
void drawMiniGraph(int gx,int gy,int gw,int gh,float*buf,float lo,float hi,uint16_t col){
    wRect(gx,gy,gw,gh,BLACK);
    float range=hi-lo; if(range<0.01f)range=1;
    int bw=gw/HIST;
    for(int i=0;i<HIST;i++){
        int di=(histIdx+i)%HIST;
        if(buf[di]==0) continue;
        int bh=(int)((buf[di]-lo)/range*(gh-1));
        if(bh<1)bh=1; if(bh>gh-1)bh=gh-1;
        wRect(gx+i*bw, gy+(gh-1-bh), bw>1?bw-1:1, bh, col);
    }
}

// ── One metric column: label + value + unit + mini graph ─────────────────
// cx = column x start, cw = column width
void drawCol(int cx, int cw, uint16_t accent,
             const char* label, const char* val, const char* unit,
             float* hist, float lo, float hi){
    // accent top bar
    wRect(cx, 12, cw, 3, accent);
    // label
    wText(cx+2, 16, label, accent, BLACK, 1);
    // value (size 2 = 12px tall chars)
    wRect(cx, 24, cw, 16, BLACK);
    wText(cx+2, 24, val,  WHITE, BLACK, 2);
    int vw = strlen(val)*12;
    wText(cx+2+vw, 28, unit, YELLOW, BLACK, 1);
    // mini graph
    drawMiniGraph(cx, 41, cw-1, D_H-42, hist, lo, hi, accent);
}

// ── Full screen ───────────────────────────────────────────────────────────
void drawScreen(float temp, float pres, float alt){
    char tBuf[12], pBuf[12], aBuf[12];

    // header
    wRect(0, 0, D_W, 12, BLUE);
    wText(3, 2, "BMP280", WHITE, BLUE, 1);
    wText(52, 2, "|", DKGRAY, BLUE, 1);
    wText(60, 2, "ESP32-C6", CYAN, BLUE, 1);
    wText(130, 2, "|", DKGRAY, BLUE, 1);
    wText(138, 2, "TEMP / PRES / ALT", LTBLUE, BLUE, 1);

    if(!bmpOK){
        wRect(0,12,D_W,D_H-12,BLACK);
        wText(4, 24, "BMP280 NOT FOUND", RED,   BLACK, 1);
        wText(4, 36, "Check SDA=7 SCL=6", YELLOW, BLACK, 1);
        return;
    }

    dtostrf(temp,       5, 1, tBuf);
    dtostrf(pres/100.0f,7, 1, pBuf);
    dtostrf(alt,        5, 1, aBuf);

    int cw = D_W / 3;   // ~94px each
    drawCol(0,        cw,   RED,    "TEMP",  tBuf, "C",   tHist, 15,  45);
    wVLine(cw,   DKGRAY);
    drawCol(cw,       cw,   ORANGE, "PRES",  pBuf, "hPa", pHist, 980, 1030);
    wVLine(cw*2, DKGRAY);
    drawCol(cw*2, D_W-cw*2, GREEN,  "ALT",   aBuf, "m",   aHist, 0,   200);
}

// ── Backlight (GPIO9 strapping pin) ───────────────────────────────────────
static void blOn(){
    gpio_reset_pin(GPIO_NUM_9);
    gpio_set_direction(GPIO_NUM_9, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(GPIO_NUM_9, GPIO_FLOATING);
    gpio_set_drive_capability(GPIO_NUM_9, GPIO_DRIVE_CAP_3);
    gpio_set_level(GPIO_NUM_9, 0);
}

// ─────────────────────────────────────────────────────────────────────────
void setup(){
    Serial.begin(115200);
    delay(300);
    Serial.printf("ST7789 landscape 284x76 — XOFF=%d YOFF=%d\n", XOFF, YOFF);

    blOn();

    tft.init(240, 320);
    tft.setRotation(1);          // 90° CW → landscape 284×76
    tft.invertDisplay(true);
    tft.fillScreen(BLACK);
    delay(100);

    I2C_BMP.begin(BMP_SDA, BMP_SCL);
    bmpOK = bmp.begin(BMP_ADDR);
    if(!bmpOK) bmpOK = bmp.begin(0x77);

    if(bmpOK){
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_X16,
                        Adafruit_BMP280::FILTER_X4,
                        Adafruit_BMP280::STANDBY_MS_500);
        Serial.println("BMP280 OK");
    } else {
        Serial.println("BMP280 NOT FOUND — check SDA=7 SCL=6 and address");
    }

    wFill(BLACK);
    drawScreen(0, 0, 0);
}

void loop(){
    float temp=0, pres=0, alt=0;
    if(bmpOK){
        temp = bmp.readTemperature();
        pres = bmp.readPressure();
        alt  = bmp.readAltitude(1013.25);
        Serial.printf("T=%.1f C  P=%.1f Pa  A=%.1f m\n", temp, pres, alt);
    }
    pushHistory(temp, pres, alt);
    drawScreen(temp, pres, alt);
    delay(2000);
}
