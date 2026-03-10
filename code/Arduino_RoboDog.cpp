#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

#define SERVO0_PIN 11  //  前左腿
#define SERVO1_PIN 10  //  前右腿
#define SERVO2_PIN 9   //  後左腿
#define SERVO3_PIN 6   //  後右腿

#define Trig 13        //  超聲波Trig
#define Echo 12        //  超聲波Echo

//  距離閾值（cm）
#define MIN_DISTANCE 15
#define WARNING_DISTANCE 25
#define SAFE_DISTANCE 30

//  OLED顯示屏
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 4
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Servo servo0; //  前左
Servo servo1; //  前右
Servo servo2; //  後左
Servo servo3; //  後右

//  機器狗狀態
enum RobotState {
  STATE_IDLE,
  STATE_WALKING,
  STATE_TURNING,
  STATE_BACKING,
  STATE_STOPPED
};
RobotState robotState = STATE_IDLE;

//  超聲波相關變量
int obstacleDistance = 0;
bool obstacleDetected = false;
unsigned long lastObstacleCheck = 0;
const unsigned long OBSTACLE_CHECK_INTERVAL = 300;

//  步態控制
int gait_step = 0;
unsigned long lastGaitUpdate = 0;
const unsigned long GAIT_INTERVAL = 200;
bool turnDirection; //  true=左轉, false=右轉

//  眼睛動畫相關變量（來自第一段代碼）
int ref_eye_height = 40;
int ref_eye_width = 40;
int ref_space_between_eye = 10;
int ref_corner_radius = 10;

int left_eye_height = ref_eye_height;
int left_eye_width = ref_eye_width;
int left_eye_x = 32;
int left_eye_y = 32;
int right_eye_x = 32 + ref_eye_width + ref_space_between_eye;
int right_eye_y = 32;
int right_eye_height = ref_eye_height;
int right_eye_width = ref_eye_width;

static const int max_animation_index = 8;  //  動畫索引最大值

//  眼睛繪製函數
void draw_eyes(bool update = true) {
  display.clearDisplay();
  int x = int(left_eye_x - left_eye_width / 2);
  int y = int(left_eye_y - left_eye_height / 2);
  display.fillRoundRect(x, y, left_eye_width, left_eye_height, ref_corner_radius, SSD1306_WHITE);
  x = int(right_eye_x - right_eye_width / 2);
  y = int(right_eye_y - right_eye_height / 2);
  display.fillRoundRect(x, y, right_eye_width, right_eye_height, ref_corner_radius, SSD1306_WHITE);
  if (update) display.display();
}

//  眼睛置中
void center_eyes(bool update = true) {
  left_eye_height = ref_eye_height;
  left_eye_width = ref_eye_width;
  right_eye_height = ref_eye_height;
  right_eye_width = ref_eye_width;
  left_eye_x = SCREEN_WIDTH / 2 - ref_eye_width / 2 - ref_space_between_eye / 2;
  left_eye_y = SCREEN_HEIGHT / 2;
  right_eye_x = SCREEN_WIDTH / 2 + ref_eye_width / 2 + ref_space_between_eye / 2;
  right_eye_y = SCREEN_HEIGHT / 2;
  draw_eyes(update);
}

//  眨眼
void blink(int speed = 12) {
  draw_eyes();
  for (int i = 0; i < 3; i++) {
    left_eye_height -= speed;
    right_eye_height -= speed;
    draw_eyes();
    delay(1);
  }
  for (int i = 0; i < 3; i++) {
    left_eye_height += speed;
    right_eye_height += speed;
    draw_eyes();
    delay(1);
  }
}

//  睡眠（眼睛變細）
void sleep() {
  left_eye_height = 2;
  right_eye_height = 2;
  draw_eyes(true);
}

//  喚醒（眼睛由細變正常）
void wakeup() {
  sleep();
  for (int h = 0; h <= ref_eye_height; h += 2) {
    left_eye_height = h;
    right_eye_height = h;
    draw_eyes(true);
  }
}

//  開心眼睛（眼下出現三角形）
void happy_eye() {
  center_eyes(false);
  int offset = ref_eye_height / 2;
  for (int i = 0; i < 10; i++) {
    display.fillTriangle(left_eye_x - left_eye_width / 2 - 1, left_eye_y + offset,
                         left_eye_x + left_eye_width / 2 + 1, left_eye_y + 5 + offset,
                         left_eye_x - left_eye_width / 2 - 1, left_eye_y + left_eye_height + offset,
                         SSD1306_BLACK);
    display.fillTriangle(right_eye_x + right_eye_width / 2 + 1, right_eye_y + offset,
                         right_eye_x - left_eye_width / 2 - 1, right_eye_y + 5 + offset,
                         right_eye_x + right_eye_width / 2 + 1, right_eye_y + right_eye_height + offset,
                         SSD1306_BLACK);
    offset -= 2;
    display.display();
    delay(1);
  }
  display.display();
  delay(1000);
}

//  快速眼動
void saccade(int direction_x, int direction_y) {
  int direction_x_movement_amplitude = 8;
  int direction_y_movement_amplitude = 6;
  int blink_amplitude = 8;
  for (int i = 0; i < 1; i++) {
    left_eye_x += direction_x_movement_amplitude * direction_x;
    right_eye_x += direction_x_movement_amplitude * direction_x;
    left_eye_y += direction_y_movement_amplitude * direction_y;
    right_eye_y += direction_y_movement_amplitude * direction_y;
    right_eye_height -= blink_amplitude;
    left_eye_height -= blink_amplitude;
    draw_eyes();
    delay(1);
  }
  for (int i = 0; i < 1; i++) {
    left_eye_x += direction_x_movement_amplitude * direction_x;
    right_eye_x += direction_x_movement_amplitude * direction_x;
    left_eye_y += direction_y_movement_amplitude * direction_y;
    right_eye_y += direction_y_movement_amplitude * direction_y;
    right_eye_height += blink_amplitude;
    left_eye_height += blink_amplitude;
    draw_eyes();
    delay(1);
  }
}

//  大眼睛移動
void move_big_eye(int direction) {
  int direction_oversize = 1;
  int direction_movement_amplitude = 2;
  int blink_amplitude = 5;
  for (int i = 0; i < 3; i++) {
    left_eye_x += direction_movement_amplitude * direction;
    right_eye_x += direction_movement_amplitude * direction;
    right_eye_height -= blink_amplitude;
    left_eye_height -= blink_amplitude;
    if (direction > 0) {
      right_eye_height += direction_oversize;
      right_eye_width += direction_oversize;
    } else {
      left_eye_height += direction_oversize;
      left_eye_width += direction_oversize;
    }
    draw_eyes();
    delay(1);
  }
  for (int i = 0; i < 3; i++) {
    left_eye_x += direction_movement_amplitude * direction;
    right_eye_x += direction_movement_amplitude * direction;
    right_eye_height += blink_amplitude;
    left_eye_height += blink_amplitude;
    if (direction > 0) {
      right_eye_height += direction_oversize;
      right_eye_width += direction_oversize;
    } else {
      left_eye_height += direction_oversize;
      left_eye_width += direction_oversize;
    }
    draw_eyes();
    delay(1);
  }
  delay(1000);
  center_eyes();
}

void move_right_big_eye() { move_big_eye(1); }
void move_left_big_eye()  { move_big_eye(-1); }

//  根據索引播放動畫
void launch_animation_with_index(int animation_index) {
  if (animation_index > max_animation_index) animation_index = max_animation_index;

  switch (animation_index) {
    case 0: wakeup(); break;
    case 1: center_eyes(true); break;
    case 2: move_right_big_eye(); break;
    case 3: move_left_big_eye(); break;
    case 4: blink(10); break;
    case 5: blink(20); break;
    case 6: happy_eye(); break;
    case 7: sleep(); break;
    case 8:
      center_eyes(true);
      for (int i = 0; i < 20; i++) {
        int dir_x = random(-1, 2);
        int dir_y = random(-1, 2);
        saccade(dir_x, dir_y);
        delay(1);
        saccade(-dir_x, -dir_y);
        delay(1);
      }
      break;
  }
}

//  步態動作（移植自第二段，非阻塞）
const int WALKING_STEPS = 6;
const int walkingSteps[WALKING_STEPS][4] = {
  {70, 90, 90, 70},
  {110, 90, 90, 110},
  {90, 90, 90, 90},
  {90, 70, 70, 90},
  {90, 110, 110, 90},
  {90, 90, 90, 90}
};

const int LEFT_TURN_STEPS = 3;
const int leftTurnSteps[LEFT_TURN_STEPS][4] = {
  {90, 70, 70, 90},
  {110, 90, 90, 110},
  {90, 90, 90, 90}
};

const int RIGHT_TURN_STEPS = 3;
const int rightTurnSteps[RIGHT_TURN_STEPS][4] = {
  {70, 90, 90, 70},
  {90, 110, 110, 90},
  {90, 90, 90, 90}
};

const int BACK_STEPS = 4;
const int backSteps[BACK_STEPS][4] = {
  {120, 60, 60, 120},
  {90, 90, 90, 90},
  {60, 120, 120, 60},
  {90, 90, 90, 90}
};

void setAllServos(int a0, int a1, int a2, int a3) {
  servo0.write(a0);
  servo1.write(a1);
  servo2.write(a2);
  servo3.write(a3);
}

void standUp() {
  setAllServos(90, 90, 90, 90);
}

//  超聲波測距
int readUltrasonicDistance() {
  digitalWrite(Trig, LOW);
  delayMicroseconds(2);
  digitalWrite(Trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(Trig, LOW);
  long duration = pulseIn(Echo, HIGH);
  int distance = duration * 0.034 / 2;
  if (distance > 100) distance = 100;
  if (distance < 0) distance = 0;
  return distance;
}

void checkObstacle() {
  unsigned long now = millis();
  if (now - lastObstacleCheck >= OBSTACLE_CHECK_INTERVAL) {
    lastObstacleCheck = now;
    obstacleDistance = readUltrasonicDistance();

    if (obstacleDistance < MIN_DISTANCE) {
      obstacleDetected = true;
      if (robotState != STATE_BACKING && robotState != STATE_TURNING) {
        robotState = STATE_BACKING;
        gait_step = 0;
      }
    } else if (obstacleDistance < WARNING_DISTANCE) {
      obstacleDetected = true;
      if (robotState == STATE_WALKING) {
        robotState = STATE_TURNING;
        gait_step = 0;
        turnDirection = random(0, 2);
      }
    } else {
      obstacleDetected = false;
      if (robotState == STATE_IDLE || robotState == STATE_STOPPED) {
        robotState = STATE_WALKING;
        gait_step = 0;
      }
    }

    Serial.print("Dist: ");
    Serial.print(obstacleDistance);
    Serial.print(" cm, State: ");
    Serial.println(robotState);
  }
}

void updateGait() {
  unsigned long now = millis();
  if (now - lastGaitUpdate < GAIT_INTERVAL) return;
  lastGaitUpdate = now;

  switch (robotState) {
    case STATE_WALKING:
      {
        int step = gait_step % WALKING_STEPS;
        setAllServos(walkingSteps[step][0], walkingSteps[step][1],
                     walkingSteps[step][2], walkingSteps[step][3]);
        gait_step++;
      }
      break;

    case STATE_TURNING:
      {
        int maxSteps = turnDirection ? LEFT_TURN_STEPS : RIGHT_TURN_STEPS;
        const int (*steps)[4] = turnDirection ? leftTurnSteps : rightTurnSteps;
        if (gait_step < maxSteps) {
          setAllServos(steps[gait_step][0], steps[gait_step][1],
                       steps[gait_step][2], steps[gait_step][3]);
          gait_step++;
        } else {
          robotState = STATE_WALKING;
          gait_step = 0;
          standUp();
        }
      }
      break;

    case STATE_BACKING:
      {
        int step = gait_step % BACK_STEPS;
        setAllServos(backSteps[step][0], backSteps[step][1],
                     backSteps[step][2], backSteps[step][3]);
        gait_step++;
        if (gait_step >= BACK_STEPS * 2) {
          robotState = STATE_TURNING;
          gait_step = 0;
          turnDirection = random(0, 2);
        }
      }
      break;

    case STATE_IDLE:
    case STATE_STOPPED:
      standUp();
      break;
  }
}

//  初始化
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  //  OLED初始化
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  center_eyes(true); //  默認眼睛

  //  超聲波引腳
  pinMode(Trig, OUTPUT);
  pinMode(Echo, INPUT);

  //  舵機初始化
  servo0.attach(SERVO0_PIN);
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo3.attach(SERVO3_PIN);
  standUp();
  delay(500);

  Serial.println(F("Robot Dog Ready!"));
}

//  主循環
void loop() {
  //  檢查障礙物
  checkObstacle();

  //  執行步態
  updateGait();

  //  處理串口命令（可選調試）
  if (Serial.available()) {
    char cmd = Serial.read();
    switch (cmd) {
      case 'S': robotState = STATE_STOPPED; standUp(); break;
      case 'W': robotState = STATE_WALKING; gait_step = 0; break;
      case 'L': robotState = STATE_TURNING; gait_step = 0; turnDirection = true; break;
      case 'R': robotState = STATE_TURNING; gait_step = 0; turnDirection = false; break;
      case 'B': robotState = STATE_BACKING; gait_step = 0; break;
    }
  }

  //  臉部動畫（每8秒隨機播放一次）
  static unsigned long lastAnimTime = 0;
  const unsigned long ANIM_INTERVAL = 8000; //  8秒一次
  if (millis() - lastAnimTime >= ANIM_INTERVAL) {
    lastAnimTime = millis();
    int animIndex = random(0, max_animation_index + 1); //  隨機選擇0~8
    launch_animation_with_index(animIndex);
    //  確保動畫結束後眼睛回到中心（某些動畫可能改變了坐標）
    center_eyes(true);
  }

  delay(10);
}