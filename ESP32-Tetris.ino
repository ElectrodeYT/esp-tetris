#include <splash.h>
#include <Adafruit_SSD1306.h>


Adafruit_SSD1306 disp(128, 64, &Wire, -1);

/// Buttons
const int left_button = 18;
const int right_button = 23;
const int rot_button = 17;
const int drop_button = 16;

const int sleep_button = 27;

// Button software debounce
const int button_delay = 100;

int left_button_delay = button_delay;
int right_button_delay = button_delay;
int rot_button_delay = button_delay;
int drop_button_delay = button_delay;
int sleep_button_delay = button_delay;

bool left_button_state = false;
bool right_button_state = false;
bool rot_button_state = false;
bool drop_button_state = false;
bool sleep_button_state = false;


/// Playfield
unsigned short playfield[21];
const int playfield_x = 2;
const int playfield_y = 8;
const int playfield_width = 10;
const int playfield_height = 20;

const int block_size = 6;

// Scores
int score = 0;
int level = 0;

/// Falling blocks
int curr_block;
int curr_block_x;
int curr_block_y;

const int initial_delay_time = 1000;
const int lowest_delay_time = 400;
int curr_block_delay_curr = 0;
int curr_block_delay_time = initial_delay_time;

// Deep sleep stuff
const int deep_sleep_push_time = 2000;
int deep_sleep_cur_time = 0;

// 0: normal
// 1: right
// 2: upside down
// 3: left
int curr_block_rot = 0;

// 0: square  (O)
// 1: line    (I)
// 2: z left  (Z)
// 3: z right (S)
// 4: L left  (L)
// 5: L right (J)
// 6: triangle(T)

// because we hate flash space, we hardcode this lol
// TODO: handle rotation around the central point correctly

const int falling_blocks[7][4][4] = {
  // O piece
  {
    { 0b11, 0b11, 0, 0 },
    { 0b11, 0b11, 0, 0 },
    { 0b11, 0b11, 0, 0 },
    { 0b11, 0b11, 0, 0 }
  },
  // I piece
  {
    { 0b1111, 0, 0, 0 },
    { 1, 1, 1, 1 },
    { 0b1111, 0, 0, 0 },
    { 1, 1, 1, 1 }
  },
  // Z piece
  {
    { 0b110, 0b011, 0, 0 },
    { 0b01, 0b11, 0b10, 0 },
    { 0b110, 0b011, 0, 0 },
    { 0b01, 0b11, 0b10, 0 }
  },
  // S piece
  {
    { 0b011, 0b110, 0, 0 },
    { 0b10, 0b11, 0b01, 0 },
    { 0b011, 0b110, 0, 0 },
    { 0b10, 0b11, 0b01, 0 }
  },
  // L piece
  {
    { 0b001, 0b111, 0, 0 },
    { 0b10, 0b10, 0b11, 0 },
    { 0b111, 0b100, 0, 0 },
    { 0b11, 0b01, 0b01, 0 }
  },
  // J piece
  {
    { 0b100, 0b111, 0, 0 },
    { 0b11, 0b10, 0b10, 0 },
    { 0b111, 0b001, 0, 0 },
    { 0b01, 0b01, 0b11, 0 }
  },
  // T piece
  {
    { 0b010, 0b111, 0, 0 },
    { 0b10, 0b11, 0b10, 0 },
    { 0b111, 0b010, 0, 0 },
    { 0b01, 0b11, 0b01 }
  }
};

const int falling_blocks_widths[7][4] = {
  { 2, 2, 2, 2 }, // O piece
  { 4, 1, 4, 1 }, // I piece
  { 3, 2, 3, 2 }, // Z piece
  { 3, 2, 3, 2 }, // S piece
  { 3, 2, 3, 2 }, // L piece
  { 3, 2, 3, 2 }, // J piece
  { 3, 2, 3, 2 }  // T piece
};

const int falling_blocks_heights[7][4] = {
  { 2, 2, 2, 2 }, // O piece
  { 1, 4, 1, 4 }, // I piece
  { 2, 3, 2, 3 }, // Z piece
  { 2, 3, 2, 3 }, // S piece
  { 2, 3, 2, 3 }, // L piece
  { 2, 3, 2, 3 }, // J piece
  { 2, 3, 2, 3 }  // T piece
};

unsigned long old_time = 0;


void getButtons(bool* left, bool* right, bool* rot, bool* drop, bool* hard_drop, unsigned long delta_time) {
  bool new_left = digitalRead(left_button);
  bool new_right = digitalRead(right_button);
  bool new_rot = digitalRead(rot_button);
  bool new_drop = digitalRead(drop_button);
  bool new_sleep = digitalRead(sleep_button);
  *left = new_left;
  *right = new_right;
  *rot = new_rot;
  *drop = new_drop;
  *hard_drop = new_sleep;


  // Detect if we should go into deepsleep
  if(new_sleep) {
    deep_sleep_cur_time += delta_time;
    if(deep_sleep_cur_time > deep_sleep_push_time) {
      // Display sleep entering screen
      disp.clearDisplay();
      disp.setCursor(0, 0);
      disp.print("ENTERING\nSLEEP\nMODE");
      disp.display();
      old_time = millis();
      int current_delay = 0;
      while(true) {
        unsigned long new_time = millis();
        unsigned long d = new_time - old_time;
        old_time = new_time;
        current_delay += d;
        if(!digitalRead(sleep_button) && (current_delay > 1000)) {
          break;
        }
      }
      disp.clearDisplay();
      disp.display();
      esp_deep_sleep_start();
    }
  } else {
    deep_sleep_cur_time = 0;
  }

  // Check if the cooldown happened
  if (*left && (left_button_delay < button_delay) && !left_button_state) {
    new_left = false;
    *left = false;
  }
  if (*right && (right_button_delay < button_delay) && !right_button_state) {
    new_right = false;
    *right = false;
  }
  if (*rot && (rot_button_delay < button_delay) && !rot_button_state) {
    new_rot = false;
    *rot = false;
  }
  if (*drop && (drop_button_delay < button_delay) && !drop_button_state) {
    new_drop = false;
    *drop = false;
  }
  if (*hard_drop && (sleep_button_delay < button_delay) && !sleep_button_state) {
    new_sleep = false;
    *hard_drop = false;
  }

  

  // Verify that these button presses actually just happened
  if (*left && left_button_state) {
    *left = false;
  }
  if (*right && right_button_state) {
    *right = false;
  }
  if (*rot && rot_button_state) {
    *rot = false;
  }
  if (*drop && drop_button_state) {
    *drop = false;
  }
  if (*hard_drop && sleep_button_state) {
    *hard_drop = false;
  }

  left_button_state = new_left;
  right_button_state = new_right;
  rot_button_state = new_rot;
  drop_button_state = new_drop;
  sleep_button_state = new_sleep;

  // If the button is pushed, reset the delay
  if (new_left) {
    left_button_delay = 0;
  }
  if (new_right) {
    right_button_delay = 0;
  }
  if (new_rot) {
    rot_button_delay = 0;
  }
  if (new_drop) {
    drop_button_delay = 0;
  }
  if (new_sleep) {
    sleep_button_delay = 0;
  }


  // Advance the cooldown
  if ((left_button_delay < button_delay) && !new_left) {
    left_button_delay += delta_time;
  }
  if ((right_button_delay < button_delay) && !new_right) {
    right_button_delay += delta_time;
  }
  if ((rot_button_delay < button_delay) && !new_rot) {
    rot_button_delay += delta_time;
  }
  if ((drop_button_delay < button_delay) && !new_drop) {
    drop_button_delay += delta_time;
  }
  if ((sleep_button_delay < button_delay) && !new_sleep) {
    sleep_button_delay += delta_time;
  }


}

inline int fallingBlockWidth() {
  return falling_blocks_widths[curr_block][curr_block_rot];
}

inline int fallingBlockHeight() {
  return falling_blocks_heights[curr_block][curr_block_rot];
}

inline bool getFieldBlock(int x, int y) {
  return playfield[y] & (1 << x);
}

inline void setFieldBlock(int x, int y, bool val) {
  if(val) {
    playfield[y] |= (1 << x);
  } else {
    playfield[y] &= ~(1 << y);
  }
}

inline bool getFallingBlock(int x, int y) {
  return falling_blocks[curr_block][curr_block_rot][y] & (1 << x);
}


void dumpField() {
  Serial.println("Playfield: ");
  for(int y = playfield_height - 1; y >= 0; y--) {
    Serial.print(y); Serial.print(": "); Serial.println(playfield[y], BIN);
  }
}

void drawField() {
  disp.setCursor(0, 0);
  disp.print("Scr: ");
  disp.print(score);
  // Draw the rectangle
  disp.drawRect(playfield_x - 1, playfield_y - 1, (playfield_width * block_size) + 1, (playfield_height * block_size) + 1, SSD1306_WHITE);

  for(int y = 0; y < playfield_height; y++) {
    for(int x = 0; x < playfield_width; x++) {
      if(getFieldBlock(x, y)) {
        int block_pix_x = playfield_x + (x * block_size);
        int block_pix_y = playfield_y + ((playfield_height - 1 - y) * block_size);
        disp.fillRect(block_pix_x, block_pix_y, block_size, block_size, SSD1306_WHITE);
      }
    }
  }
  
}

void drawFallingBlock() {
  for(int y = 0; y < fallingBlockHeight(); y++) {
    for(int x = 0; x < fallingBlockWidth(); x++) {
      if(getFallingBlock(x, y)) {
        int block_pix_x = playfield_x + ((curr_block_x + x) * block_size);
        int block_pix_y = playfield_y + ((playfield_height - 1 -(curr_block_y + y)) * block_size);
        disp.fillRect(block_pix_x, block_pix_y, block_size, block_size, SSD1306_WHITE);
      }
    }
  }
}

bool checkFallingBlockPlacement() {
  // We basically just check if the current placement of the block is actually valid or not
  for(int y = 0; y < fallingBlockHeight(); y++) {
    for(int x = 0; x < fallingBlockWidth(); x++) {
      int real_x = curr_block_x + x;
      int real_y = curr_block_y + y;
      if(real_x < 0 || real_x >= playfield_width) { Serial.print("x check failed: "); Serial.print(real_x); Serial.print(", "); Serial.println(real_y); return false; }
      if(real_y < 0 || real_y >= playfield_height) { Serial.print("y check failed: "); Serial.print(real_x); Serial.print(", "); Serial.println(real_y); return false; }
      if(getFieldBlock(real_x, real_y) && getFallingBlock(x, y)) {
        Serial.print("field check failed: "); Serial.print(real_x); Serial.print(", "); Serial.println(real_y);
        Serial.print("\tblock relative: "); Serial.print(x); Serial.print(", "); Serial.println(y);
        Serial.print("\tcurr block, rot: "); Serial.print(curr_block); Serial.print(", "); Serial.println(curr_block_rot);
        return false;
       }
    }
  }
  return true;
}

void clearLines() {
  int cleared_lines = 0;
  for(int y = 0; y < playfield_height; y++) {
    int mask_math_val = playfield[y];
    int mask_math_mask = (1 << playfield_width) - 1;
    mask_math_val &= mask_math_mask;
    if((mask_math_val - 1) >= (mask_math_mask - 1)) {
      // This line is full, yeet it
      Serial.print("yeeting line "); Serial.println(y); 
      // Pull the lines above it down
      for(int copy_y = (y + 1); copy_y < playfield_height; copy_y++) {
        playfield[copy_y - 1] = playfield[copy_y];
      }
      // Clear the top line
      playfield[playfield_height - 1] = 0;
      cleared_lines++;
      y--; // Decrement Y because the above line is now the current line
    }
  }
  // Now add the score
  switch(cleared_lines) {
    case 0: break;
    case 1: score += 40 * (level + 1); break;
    case 2: score += 100 * (level + 1); break;
    case 3: score += 300 * (level + 1); break;
    case 4: score += 1200 * (level + 1); break;
    default: Serial.println("what the fuck? you can only clear 4 lines a piece"); break;
  }
}

void generateBlock() {
  // This follows the NES tetris algorithm
  // This weird doodling around means the likelyhood of the same block coming twice is lower
  int new_block = random(0, 8);
  if(new_block == curr_block || new_block == 7) {
    new_block = random(0, 7);
  }
  curr_block = new_block;
  curr_block_x = 6 - fallingBlockWidth();
  curr_block_y = playfield_height - fallingBlockHeight();
}

void completeBlock() {
  for(int y = 0; y < fallingBlockHeight(); y++) {
    for(int x = 0; x < fallingBlockWidth(); x++) {
      int real_x = curr_block_x + x;
      int real_y = curr_block_y + y;
      if(real_x < 0 || real_x >= playfield_width) { Serial.print("drop-complete x check failed: "); Serial.print(real_x); Serial.print(", "); Serial.println(real_y);}
      if(real_y < 0 || real_y >= playfield_height) { Serial.print("drop-complete y check failed: "); Serial.print(real_x); Serial.print(", "); Serial.println(real_y);}
      if(getFieldBlock(real_x, real_y) && getFallingBlock(x, y)) {
        Serial.print("drop-complete field check failed: "); Serial.print(real_x); Serial.print(", "); Serial.println(real_y);
        Serial.print("\tblock relative: "); Serial.print(x); Serial.print(", "); Serial.println(y);
        Serial.print("\tcurr block, rot: "); Serial.print(curr_block); Serial.print(", "); Serial.println(curr_block_rot);
      }
      if(getFallingBlock(x, y)) {
        setFieldBlock(real_x, real_y, true);
      }
    }
  }
  generateBlock();
  clearLines();
}

void checkMoveLeft() {
  if(curr_block_x == 0) { return; }
  curr_block_x--;
  if(!checkFallingBlockPlacement()) { curr_block_x++; }
}

void checkMoveRight() {
  if(curr_block_x == (playfield_width - fallingBlockWidth())) { return; }
  curr_block_x++;
  if(!checkFallingBlockPlacement()) { curr_block_x--; }
}

void checkRotate() {
  int old_rot = curr_block_rot;
  curr_block_rot++; if(curr_block_rot >= 4) { curr_block_rot = 0; }
  if(!checkFallingBlockPlacement()) { curr_block_rot = old_rot; Serial.println("rotate failed"); }
}

bool checkDrop() {
  if(curr_block_y == 0) { completeBlock(); return true; }
  curr_block_y--;
  if(!checkFallingBlockPlacement()) { curr_block_y++; completeBlock(); return true; }
  return false;
}

void gameOver() {
  int blink_time = 0;
  int stare_time = 0;
  bool draw_falling_block = true;
  while(true) {
    unsigned long new_time = millis();
    unsigned long delta_time = new_time - old_time;
    old_time = new_time;

    bool left;
    bool right;
    bool rot;
    bool drop;
    bool hard_drop;
    getButtons(&left, &right, &rot, &drop, &hard_drop, delta_time);

    if((stare_time > 1000) && (left || right || rot || drop || hard_drop)) {
      for(int y = 0; y < playfield_height; y++) { playfield[y] = 0; }
      generateBlock();
      curr_block_delay_time = initial_delay_time;
      score = 0;
      level = 0;
      return;
    }

    disp.clearDisplay();
    drawField();
    if(draw_falling_block) { drawFallingBlock(); }
    blink_time += delta_time;
    stare_time += delta_time;
    if(blink_time > 1000) { blink_time = 0; draw_falling_block = !draw_falling_block; }
    disp.setCursor(5, 120);
    disp.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    disp.print("GAME OVER");
    disp.display();
  }
}

void setup() {
  // put your setup code here, to run once:
  Wire.begin(22, 19);
  Serial.begin(115200);
  if (!disp.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  disp.setRotation(3);
  disp.setTextSize(1);
  disp.setTextColor(WHITE);
  
  disp.clearDisplay();
  disp.display();

  
  // Setup pulldowns
  pinMode(left_button, INPUT_PULLDOWN);
  pinMode(right_button, INPUT_PULLDOWN);
  pinMode(rot_button, INPUT_PULLDOWN);
  pinMode(drop_button, INPUT_PULLDOWN);
  pinMode(sleep_button, INPUT_PULLDOWN);

  // Setup deepsleep resume stuff
  esp_sleep_enable_ext0_wakeup((gpio_num_t)sleep_button, 1);

  // If we woke up from deepsleep, require that the sleep button is still pressed for a bit
  if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    old_time = millis();
    int button_press_time = 0;
    while(true) {
      unsigned long new_time = millis();
      unsigned long delta_time = new_time - old_time;
      old_time = new_time;
      if(!digitalRead(sleep_button)) {
        // Button got released, go back to sleep
        esp_deep_sleep_start();
      } else if(button_press_time > 500) {
        sleep_button_state = true;
        break;
      }
      button_press_time += delta_time;
    }
  }

  randomSeed(analogRead(35));

  
  dumpField();

  generateBlock();
  
  old_time = millis();

}

void loop() {
  unsigned long new_time = millis();
  unsigned long delta_time = new_time - old_time;
  old_time = new_time;

  curr_block_delay_curr += delta_time;

  disp.clearDisplay();

  if(!checkFallingBlockPlacement()) { gameOver(); }

  bool left;
  bool right;
  bool rot;
  bool drop;
  bool hard_drop;
  getButtons(&left, &right, &rot, &drop, &hard_drop, delta_time);

  if(left) { checkMoveLeft(); }
  if(right) { checkMoveRight(); }
  if(rot) { checkRotate(); }
  if(drop || (curr_block_delay_curr >= curr_block_delay_time)) { if(drop) { score += 20; } checkDrop(); curr_block_delay_curr = 0; }
  if(hard_drop) { while(!checkDrop()) { score += 40; } curr_block_delay_curr = 0; }
  
  // Draw field
  drawField();
  // Draw falling block
  drawFallingBlock();
  // Level
  //disp.setCursor(0, playfield_y + (playfield_height * block_size) + 4);
  //disp.print("Lvl: ");
  //disp.print(level);
  disp.display();
}
