//Adafruit MCP23017 Arduino Library を導入してください
//Arduino Micro または Leonard を使用してください

//簡単な説明
//マスコン5段、ブレーキ8段+EBです。それ以外は使用不可です。(今のところ)
//ノッチの移動量に応じて、各キーボードコマンドを打ち込んでいます。
//MC53側は真理値に基づいてノッチ(N,P1～P5)を指示します。レバーサも対応します。
//ME38側はポテンショの値を一旦角度換算し、ブレーキノッチ(N,B1～EB)を指示します。

//BVEゲーム開始時は、一旦ブレーキハンドルはB8に、マスコンノッチはNにします。

#include <Adafruit_MCP23X17.h>
#include <Keyboard.h>

#define PIN_MC_1 0
#define PIN_MC_2 1
#define PIN_MC_3 2
#define PIN_MC_4 3
#define PIN_MC_5 4
#define PIN_MC_DEC 5
#define PIN_MC_DIR_F 6
#define PIN_MC_DIR_B 7
#define SS2 5
#define CS_PIN SS

//ここをポテンショの値を入れて調整してください
//今後のアップデートで自動化予定
#define POT_N 93
#define POT_EB 783


//↓デバッグのコメント(//)を解除するとシリアルモニタでデバッグできます
//#define DEBUG

Adafruit_MCP23X17 mcp;
SPISettings settings = SPISettings( 1000000 , MSBFIRST , SPI_MODE0 );

int notch_mc = 0;
int notch_mc_latch= 0;
String notch_name = "";
int notch_brk = 0;
int notch_brk_latch = 0;
String notch_brk_name = "";
int iDir = 0;
int iDir_latch = 0;
String strDir = "";

void setup() {
  pinMode(SS2, OUTPUT);
  Serial.begin(115200);

  if (!mcp.begin_SPI(CS_PIN)) {
    Serial.println("Error.");
    while (1);
  }

  // マスコンスイッチを全てプルアップ
  mcp.pinMode(PIN_MC_1, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_2, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_3, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_4, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_5, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_DEC, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_DIR_F, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_DIR_B, INPUT_PULLUP);

  Keyboard.begin();
}

void loop() {
  // LOW = pressed, HIGH = not pressed

  read_MC();            //マスコンノッチ読込ルーチン
  read_Dir();           //マスコンレバーサ読込ルーチン
  read_Break();//ブレーキハンドル読込ルーチン
  read_Break_Setting(); //ブレーキハンドル読込ルーチン(未実装)
  keyboard_control();   //キーボード(HID)アウトプットルーチン

#ifdef DEBUG
  Serial.print(" ");
  Serial.print(notch_name);
  Serial.print(" ");
  Serial.print(notch_brk_name);
  Serial.print(" Dir:");
  Serial.print(strDir);
  Serial.println();
#endif

  //delay(100);
}

//MCP3008ADコンバータ読取
int adcRead(byte ch) { // 0 .. 7
  byte channelData = (ch + 8 ) << 4;
  // Serial.println(String(channelData,BIN));
  SPI.beginTransaction(settings);
  digitalWrite(SS2, LOW);
  SPI.transfer(0b00000001); // Start bit 1
  byte highByte = SPI.transfer(channelData); // singleEnd
  byte lowByte = SPI.transfer(0x00); // dummy
  digitalWrite(SS2, HIGH);
  SPI.endTransaction();
  return ((highByte & 0x03) << 8) + lowByte ;
}

//MCP23S17マスコンノッチ状態読込 (MC53抑速ブレーキ対応)
void read_MC(void) {
  Serial.print(!mcp.digitalRead(PIN_MC_1), DEC);
  Serial.print(!mcp.digitalRead(PIN_MC_2), DEC);
  Serial.print(!mcp.digitalRead(PIN_MC_3), DEC);
  Serial.print(!mcp.digitalRead(PIN_MC_4), DEC);
  Serial.print(!mcp.digitalRead(PIN_MC_5), DEC);
  Serial.print(!mcp.digitalRead(PIN_MC_DEC), DEC);
  Serial.print(!mcp.digitalRead(PIN_MC_DIR_F), DEC);
  Serial.print(!mcp.digitalRead(PIN_MC_DIR_B), DEC);

  if (mcp.digitalRead(PIN_MC_DEC)) {
    if (!mcp.digitalRead(PIN_MC_5)) {
      notch_mc = 14;
      notch_name = "P5";
    } else if (!mcp.digitalRead(PIN_MC_4)) {
      notch_mc = 13;
      notch_name = "P4";
    } else if (!mcp.digitalRead(PIN_MC_3)) {
      notch_mc = 12;
      notch_name = "P3";
    } else if (!mcp.digitalRead(PIN_MC_2)) {
      notch_mc = 11;
      notch_name = "P2";
    } else if (!mcp.digitalRead(PIN_MC_1)) {
      notch_mc = 10;
      notch_name = "P1";
    } else {
      notch_mc = 9;
      notch_name = "N ";
    }
#ifdef DEBUG
    Serial.print(" mc:");
    Serial.print(notch_mc);
#endif
  } else {
    if (!mcp.digitalRead(PIN_MC_5)) {
      notch_mc = 21;
      notch_name = "D1";
    } else if (!mcp.digitalRead(PIN_MC_3) && mcp.digitalRead(PIN_MC_4)) {
      notch_mc = 22;
      notch_name = "D2";
    } else if (!mcp.digitalRead(PIN_MC_2) && mcp.digitalRead(PIN_MC_4)) {
      notch_mc = 23;
      notch_name = "D3";
    } else if (!mcp.digitalRead(PIN_MC_2) && !mcp.digitalRead(PIN_MC_4)) {
      notch_mc = 24;
      notch_name = "D4";
    } else if (!mcp.digitalRead(PIN_MC_3) && !mcp.digitalRead(PIN_MC_4)) {
      notch_mc = 25;
      notch_name = "D5";
    }
  }
}

//マスコンレバーサ読取
void read_Dir(void) {
  if (!mcp.digitalRead(PIN_MC_DIR_F)) {
    iDir = 1;
    strDir = "F";
  } else if (!mcp.digitalRead(PIN_MC_DIR_B)) {
    iDir = -1;
    strDir = "B";
  } else {
    iDir = 0;
    strDir = "N ";
  }
}

//ブレーキ角度読取
void read_Break(void) {

  int deg = map(adcRead(0), POT_N , POT_EB , 0, 165);
#ifdef DEBUG
  Serial.print(" Pot1:");
  Serial.print(10000 + adcRead(0));

  Serial.print(" Deg:");
  Serial.print(1000 + deg);
#endif

  if (deg < 5) {
    notch_brk = 9;
    notch_brk_name = "N ";
  } else if ( deg < 15 ) {
    notch_brk = 8;
    notch_brk_name = "B1";
  } else if ( deg < 25 ) {
    notch_brk = 7;
    notch_brk_name = "B2";
  } else if ( deg < 35 ) {
    notch_brk = 6;
    notch_brk_name = "B3";
  } else if ( deg < 45 ) {
    notch_brk = 5;
    notch_brk_name = "B4";
  } else if ( deg < 55 ) {
    notch_brk = 4;
    notch_brk_name = "B5";
  } else if ( deg < 65 ) {
    notch_brk = 3;
    notch_brk_name = "B6";
  } else if ( deg < 75 ) {
    notch_brk = 2;
    notch_brk_name = "B7";
  } else if ( deg < 150 ) {
    notch_brk = 1;
    notch_brk_name = "B8";
  } else {
    notch_brk = 0;
    notch_brk_name = "EB";
  }

#ifdef DEBUG
  bool sw = 0;
  if (adcRead(1) < 512)sw = 0; else sw = 1;
  Serial.print(" SW1:");
  Serial.print(sw);

  if (adcRead(2) < 512)sw = 0; else sw = 1;
  Serial.print(" SW2:");
  Serial.print(sw);

  if (adcRead(3) < 512)sw = 0; else sw = 1;
  Serial.print(" SW3:");
  Serial.print(sw);

  if (adcRead(4) < 512)sw = 0; else sw = 1;
  Serial.print(" SW4:");
  Serial.print(sw);
#endif
}

//キーボード(HID)出力
void keyboard_control(void) {
  //ノッチが前回と異なるとき
  if (notch_mc != notch_mc_latch ) {
    int d = abs(notch_mc - notch_mc_latch);
#ifdef DEBUG
    Serial.print(" notch_mc:");
    Serial.print(notch_mc);
    Serial.print(" notch_mc-notch_mc_latch:");
    Serial.print(d);
    Serial.print(" Key:");
#endif
    //力行ノッチ
    if ( notch_mc >= 9 && notch_mc_latch>= 9 && notch_mc <= 14 && notch_mc_latch<= 14 ) {
      //進段
      if ((notch_mc - notch_mc_latch) > 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x5A);//"/"
#ifdef DEBUG
          Serial.println("Z");
#endif
        }
      }
      //戻し
      if ((notch_mc - notch_mc_latch) < 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x41);//"/"
#ifdef DEBUG
          Serial.println("A");
#endif
        }
      }
    }
  }
  //ノッチが前回と異なるとき
  if (notch_brk != notch_brk_latch) {
    int d = abs(notch_brk - notch_brk_latch);
#ifdef DEBUG
    Serial.print(" notch_brk:");
    Serial.print(notch_brk);
    Serial.print(" notch_brk-notch_brk_latch:");
    Serial.print(d);
    Serial.print(" Key:");
#endif
    //ブレーキノッチ
    if ( notch_brk <= 9 && notch_brk_latch <= 9 && notch_brk > 0) {
      //戻し
      if ((notch_brk - notch_brk_latch) > 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x2C);//","
#ifdef DEBUG
          Serial.println(",");
#endif
        }
      }
      //ブレーキ
      if ((notch_brk - notch_brk_latch) < 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x2E);//"/"
#ifdef DEBUG
          Serial.println(".");
#endif
        }
      }
    }
    if (notch_brk == 0) {
      Keyboard.write(0x2F);//"/"
#ifdef DEBUG
      Serial.println("/");
#endif
    }
  }

  //レバーサが前回と異なるとき
  if (iDir != iDir_latch) {
    int d = abs(iDir - iDir_latch);
#ifdef DEBUG
    Serial.print(" iDir:");
    Serial.print(iDir);
#endif
    //前進
    if ((iDir - iDir_latch) > 0) {
      for (int i = 0; i < d ; i ++) {
        Keyboard.write(0xDA);//"↑"
#ifdef DEBUG
        Serial.println("↑");
#endif
      }
    }
    //後退
    if ((iDir - iDir_latch) < 0) {
      for (int i = 0; i < d ; i ++) {
        Keyboard.write(0xD9);//"↓"
#ifdef DEBUG
        Serial.println("↓");
#endif
      }
    }
  }

  notch_mc_latch= notch_mc;
  notch_brk_latch = notch_brk;
  iDir_latch = iDir;
}

//ブレーキ角度調整
void read_Break_Setting(void) {
  bool n = adcRead(5) < 512;
  bool eb = adcRead(6) < 512;
  Serial.println(n);
  Serial.println(eb);
}
