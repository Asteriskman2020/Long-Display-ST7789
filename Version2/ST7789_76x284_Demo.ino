/*
 * ST7789 76×284 + BMP280  —  ESP32-C6
 *
 * Display : SCL=20  SDA=19  RST=18  DC=15  CS=14  BL=9(PROG)
 * BMP280  : SDA=6   SCL=7
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
#define BMP_SDA   6
#define BMP_SCL   7
#define BMP_ADDR  0x76   // try 0x77 if sensor not found

// ── Panel geometry ────────────────────────────────────────────────────────
#define XOFF   82
#define YOFF    0
#define D_W    76
#define D_H   284

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
void wLine(int x0,int y0,int x1,int y1,uint16_t c){ tft.drawLine(XOFF+x0,YOFF+y0,XOFF+x1,YOFF+y1,c); }
void wText(int x,int y,const char*s,uint16_t fg,uint16_t bg=BLACK,uint8_t sz=1){
    tft.setTextColor(fg,bg); tft.setTextSize(sz);
    tft.setCursor(XOFF+x,YOFF+y); tft.print(s);
}

// ── Draw a labelled value row ──────────────────────────────────────────────
//   icon block | label | value
void drawRow(int y, uint16_t iconCol, const char* label, const char* val, const char* unit){
    wRect(0, y, 8, 18, iconCol);               // colour tab
    wRect(8, y, D_W-8, 18, DKGRAY);           // row bg
    wText(10, y+2,  label, CYAN,   DKGRAY, 1);
    wText(10, y+10, val,   WHITE,  DKGRAY, 1);
    wText(10+strlen(val)*6, y+10, unit, YELLOW, DKGRAY, 1);
}

// ── History bar-graph (last 20 readings, scaled per metric) ───────────────
#define HIST 20
float tHist[HIST]={}, pHist[HIST]={}, aHist[HIST]={};
int   histIdx = 0;

void pushHistory(float t, float p, float a){
    tHist[histIdx] = t;
    pHist[histIdx] = p;
    aHist[histIdx] = a;
    histIdx = (histIdx+1) % HIST;
}

void drawGraph(int y, int h, float* buf, float lo, float hi, uint16_t col){
    wRect(0, y, D_W, h, BLACK);
    wLine(0, y+h-1, D_W-1, y+h-1, DKGRAY);
    float range = hi - lo;
    if(range < 0.01f) range = 1;
    int bw = D_W / HIST;
    for(int i=0;i<HIST;i++){
        int di = (histIdx + i) % HIST;
        if(buf[di] == 0) continue;
        int bh = (int)((buf[di]-lo)/range * (h-2));
        if(bh < 1) bh = 1;
        if(bh > h-2) bh = h-2;
        wRect(i*bw, y+(h-2-bh), bw-1, bh, col);
    }
}

// ── Full screen redraw ────────────────────────────────────────────────────
void drawScreen(float temp, float pres, float alt){
    char buf[20];

    // ── Header ──
    wRect(0, 0, D_W, 20, BLUE);
    wText(4, 2, "BMP280", WHITE, BLUE, 1);
    wText(40, 2, "ESP32-C6", CYAN, BLUE, 1);

    wLine(0,20,D_W-1,20,WHITE);

    // ── Sensor rows ──
    if(!bmpOK){
        wRect(0,24,D_W,50,BLACK);
        wText(4,36,"NO SENSOR",RED,BLACK,1);
        wText(4,48,"CHECK WIRING",YELLOW,BLACK,1);
    } else {
        // Temperature
        dtostrf(temp, 5, 1, buf);
        drawRow(24, RED,    "TEMP",     buf, " C");

        // Pressure
        dtostrf(pres/100.0f, 7, 2, buf);
        drawRow(46, ORANGE, "PRESSURE", buf, "hPa");

        // Altitude
        dtostrf(alt, 6, 1, buf);
        drawRow(68, GREEN,  "ALTITUDE", buf, " m");
    }

    wLine(0,90,D_W-1,90,WHITE);

    // ── Graphs ──
    wText(2, 92, "TEMP",  RED,   BLACK, 1);
    drawGraph(100, 40, tHist, 15, 45, RED);

    wText(2, 142, "PRES", ORANGE, BLACK, 1);
    drawGraph(150, 40, pHist, 98000, 103000, ORANGE);

    wText(2, 192, "ALT",  GREEN,  BLACK, 1);
    drawGraph(200, 40, aHist, 0, 200, GREEN);

    // ── Footer ──
    wRect(0, D_H-18, D_W, 18, DKGRAY);
    wText(2, D_H-13, "v2.0 BMP280", LTBLUE, DKGRAY, 1);
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

    blOn();

    tft.init(240, 320);
    tft.setRotation(0);
    tft.invertDisplay(true);
    tft.fillScreen(BLACK);
    delay(100);

    // BMP280 on dedicated I2C bus
    I2C_BMP.begin(BMP_SDA, BMP_SCL);
    bmpOK = bmp.begin(BMP_ADDR);
    if(!bmpOK){
        // retry alternate address
        bmpOK = bmp.begin(0x77);
    }

    if(bmpOK){
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_X16,
                        Adafruit_BMP280::FILTER_X4,
                        Adafruit_BMP280::STANDBY_MS_500);
        Serial.println("BMP280 OK");
    } else {
        Serial.println("BMP280 NOT FOUND — check SDA=6 SCL=7 and address");
    }

    wFill(BLACK);
    drawScreen(0,0,0);
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
