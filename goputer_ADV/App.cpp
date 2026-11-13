#include "Bot.h"
#include "GoEngine.h"
#include "Storage.h"
#include <M5Cardputer.h>
#include <algorithm>
#include <string.h>
SET_LOOP_TASK_STACK_SIZE(16384);
Go::Game game;
Go::Bot bot;
M5Canvas frame(&M5Cardputer.Display);
bool spriteReady = false, dirty = true, inMenu = true, showHelp = false,
     finished = false;
bool zoomed = false, thinking = false, hasSave = false, storageReady = false,
     activeGame = false;
bool againstBot = true;
int dialog = 0, menuRow = 0, sizeIndex = 0, human = Go::Black, botLevel = 1;
int cursorX = 4, cursorY = 4, progressShown = -1;
uint32_t botStarted = 0, botElapsed = 0;
const int Sizes[] = {9, 13, 19};
const char *const BotNames[] = {"Easy", "Tactical", "Search 2-ply",
                                "Search 3-ply"};
char notice[24] = "Ready";
uint8_t loadBackup[Save::MaxBytes];
constexpr uint16_t Wood = 0xD5D0, Grid = 0x6206, Panel = 0x1083, Text = 0xEF7D;
constexpr uint16_t Muted = 0x9CD4, Accent = 0x4F3A, Red = 0xF986,
                   Cursor = 0x001F;
enum Action {
  Up,
  Down,
  Left,
  Right,
  Place,
  Enter,
  Pass,
  Undo,
  NewGame,
  Help,
  Dead,
  Resume,
  Zoom,
  One,
  Two,
  Three,
  Yes,
  Reboot,
  SaveNow,
  Load,
  ActionCount
};
bool held[ActionCount] = {};
uint32_t repeatAt[4] = {};
lgfx::LGFXBase &surface() {
  if (spriteReady)
    return frame;
  return M5Cardputer.Display;
}
void label(int x, int y, const char *text, uint16_t color = Text) {
  auto &d = surface();
  d.setTextColor(color);
  d.setCursor(x, y);
  d.print(text);
}
void message(const char *text) {
  snprintf(notice, sizeof notice, "%s", text);
  dirty = true;
}
void cancelBot() {
  thinking = false;
  progressShown = -1;
  botElapsed = 0;
}
Save::Settings settings() {
  Save::Settings s;
  s.againstBot = againstBot;
  s.human = uint8_t(human);
  s.level = uint8_t(botLevel);
  s.finished = finished;
  s.zoomed = zoomed;
  s.cursorX = uint8_t(cursorX);
  s.cursorY = uint8_t(cursorY);
  return s;
}
bool persist() {
  if (!activeGame)
    return false;
  bool ok = Storage::save(game, settings());
  hasSave = Storage::available();
  message(ok ? "Saved" : "SAVE ERROR!");
  return ok;
}
bool botTurn() {
  return activeGame && !inMenu && !showHelp && !dialog && !finished &&
         !game.reviewing() && againstBot && game.position().turn != human;
}
void startGame() {
  game.reset(Sizes[sizeIndex]);
  cursorX = cursorY = game.size() / 2;
  inMenu = false;
  activeGame = true;
  showHelp = finished = zoomed = false;
  dialog = 0;
  cancelBot();
  persist();
}
void loadGame() {
  Save::Settings s;
  cancelBot();
  bool previous = activeGame;
  size_t backupSize = 0;
  if (previous)
    backupSize =
        Save::encode(game, settings(), 0, loadBackup, sizeof loadBackup);
  if (!Storage::load(game, s)) {
    if (previous && backupSize) {
      Save::Settings ignored;
      Save::decode(loadBackup, backupSize, game, ignored);
    }
    activeGame = previous;
    message("Load failed");
    return;
  }
  againstBot = s.againstBot;
  human = s.human;
  botLevel = s.level;
  finished = s.finished;
  zoomed = s.zoomed;
  cursorX = s.cursorX;
  cursorY = s.cursorY;
  for (int i = 0; i < 3; ++i)
    if (Sizes[i] == game.size())
      sizeIndex = i;
  activeGame = true;
  inMenu = showHelp = false;
  dialog = 0;
  message("Game restored");
}
void changeMenu(int delta) {
  if (menuRow == 0)
    sizeIndex = (sizeIndex + 3 + delta) % 3;
  if (menuRow == 1)
    againstBot = !againstBot;
  if (menuRow == 2)
    human = 3 - human;
  if (menuRow == 3)
    botLevel = (botLevel + 4 + delta) % 4;
  dirty = true;
}
void drawMenu() {
  auto &d = surface();
  char text[40];
  label(8, 4, "goputer(ADV) v2.1", Accent);
  for (int row = 0; row < 6; ++row) {
    int y = 22 + row * 15;
    if (row == menuRow)
      d.fillRect(5, y - 2, 230, 13, 0x2945);
    if (row == 0)
      snprintf(text, sizeof text, "Board    < %dx%d >", Sizes[sizeIndex],
               Sizes[sizeIndex]);
    if (row == 1)
      snprintf(text, sizeof text, "Opponent < %s >",
               againstBot ? "Bot" : "Local 2P");
    if (row == 2)
      snprintf(text, sizeof text, "Your side< %s >",
               human == Go::Black ? "Black" : "White");
    if (row == 3)
      snprintf(text, sizeof text, "Bot      < %s >", BotNames[botLevel]);
    if (row == 4)
      snprintf(text, sizeof text, "[ NEW GAME ]");
    if (row == 5)
      snprintf(text, sizeof text, "[ CONTINUE %s ]",
               hasSave ? "LAST GAME" : "(no save)");
    label(12, y, text, row == menuRow ? Accent : Text);
  }
  label(8, 113, "W/S:row A/D:change Enter:select", Muted);
  label(8, 125,
        storageReady ? "1-3:size L:continue H:help" : "FLASH SAVE UNAVAILABLE",
        storageReady ? Muted : Red);
}
void drawHelp() {
  label(5, 3, "goputer(ADV) / KEYS", Accent);
  label(5, 17, "EASD or ; . , / : cursor");
  label(5, 29, "Enter/Space:stone P:pass U:undo");
  label(5, 41, "Z:overview/zoom for 13/19");
  label(5, 53, "F:save L:load N:new Q:reboot");
  label(5, 67, "Autosave after every change", Accent);
  label(5, 79, "Two passes -> score review");
  label(5, 91, "X/Space:dead Enter:accept");
  label(5, 103, "R:resume Area +6.5 White");
  label(5, 115, "Search bots: limited 2/3 ply");
  label(5, 127, "H:back | save errors in red", Muted);
}
void drawBoard() {
  auto &d = surface();
  const auto &pos = game.position();
  int n = game.size();
  bool review = game.reviewing();
  Go::Score score;
  if (review)
    score = game.score();
  int visible = zoomed && n > 9 ? 9 : n, step = 110 / (visible - 1), ox = 18,
      oy = 19, vx = 0, vy = 0;
  if (visible < n) {
    vx = constrain(cursorX - visible / 2, 0, n - visible);
    vy = constrain(cursorY - visible / 2, 0, n - visible);
  }
  int end = step * (visible - 1), radius = constrain(step / 2 - 1, 2, 6);
  d.fillRect(0, 0, 136, 135, Wood);
  char text[40];
  for (int i = 0; i < visible; ++i) {
    int x = ox + i * step, y = oy + i * step;
    d.drawLine(ox, y, ox + end, y, Grid);
    d.drawLine(x, oy, x, oy + end, Grid);
    if (step >= 9 || i % 2 == 0) {
      int col = vx + i;
      char letter[2] = {char('A' + col + (col >= 8 ? 1 : 0)), 0};
      label(x - 2, 4, letter, Grid);
      snprintf(text, sizeof text, "%d", n - (vy + i));
      label(1, y - 3, text, Grid);
    }
  }
  for (int y = 0; y < visible; ++y)
    for (int x = 0; x < visible; ++x) {
      int bx = vx + x, by = vy + y, p = by * n + bx, sx = ox + x * step,
          sy = oy + y * step;
      // Global board coordinates, never viewport coordinates; hoshi are BLACK.
      if (Go::starPoint(n, bx, by))
        d.fillCircle(sx, sy, step >= 8 ? 2 : 1, 0x0000);
      if (pos.board[p]) {
        uint16_t color = pos.board[p] == Go::Black ? 0 : 0xffff;
        d.fillCircle(sx, sy, radius, color);
        if (pos.board[p] == Go::White)
          d.drawCircle(sx, sy, radius, 0x8410);
        if (game.dead(p)) {
          int r = std::min(radius, 3);
          d.drawLine(sx - r, sy - r, sx + r, sy + r, Red);
          d.drawLine(sx - r, sy + r, sx + r, sy - r, Red);
        } else if (!review && pos.last == p)
          d.fillCircle(sx, sy, 1, color ? 0 : 0xffff);
      } else if (review && score.owner[p])
        d.fillRect(sx - 1, sy - 1, 3, 3,
                   score.owner[p] == Go::Black ? 0 : 0xffff);
    }
  int cx = ox + (cursorX - vx) * step, cy = oy + (cursorY - vy) * step,
      cr = std::min(step / 2, 7);
  d.drawRect(cx - cr, cy - cr, cr * 2 + 1, cr * 2 + 1, Cursor);
  snprintf(text, sizeof text, "GO %dx%d %s", n, n, zoomed && n > 9 ? "Z" : "");
  label(140, 4, text, Accent);
  label(140, 17, againstBot ? BotNames[botLevel] : "LOCAL 2P", Muted);
  if (review) {
    label(140, 30, finished ? "FINAL SCORE" : "SCORE REVIEW", Accent);
    snprintf(text, sizeof text, "B %d.0", score.black);
    label(140, 44, text);
    snprintf(text, sizeof text, "W %d.5", score.white + 6);
    label(140, 56, text);
    int delta = score.black * 2 - score.white * 2 - 13, margin = abs(delta);
    snprintf(text, sizeof text, "%c +%d.%d", delta > 0 ? 'B' : 'W', margin / 2,
             margin % 2 * 5);
    label(140, 71, text, Accent);
    label(140, 88, finished ? "N:new game" : "X:dead group");
    label(140, 100, finished ? "U:undo" : "Enter:accept");
    label(140, 112, "R:resume F:save", Muted);
  } else {
    label(140, 30, pos.turn == Go::Black ? "BLACK TO PLAY" : "WHITE TO PLAY");
    snprintf(text, sizeof text, "Move %d", game.ply() + 1);
    label(140, 44, text, Muted);
    snprintf(text, sizeof text, "%c%d YOU:%c",
             'A' + cursorX + (cursorX >= 8 ? 1 : 0), n - cursorY,
             human == Go::Black ? 'B' : 'W');
    label(140, 56, text, Accent);
    if (thinking)
      snprintf(text, sizeof text, "Bot %dms", int(millis() - botStarted));
    else
      snprintf(text, sizeof text, "B:%u W:%u",
               unsigned(pos.captured[Go::Black]),
               unsigned(pos.captured[Go::White]));
    label(140, 71, text, Muted);
    label(140, 88, "Enter:stone");
    label(140, 100, "P:pass U:undo");
    label(140, 112, "Z:zoom F:save", Muted);
  }
  char shortText[17];
  snprintf(shortText, sizeof shortText, "%.16s", notice);
  label(140, 126, shortText, strstr(notice, "ERROR") ? Red : Accent);
}
void draw() {
  auto &d = surface();
  d.fillScreen(Panel);
  d.setTextFont(1);
  d.setTextSize(1);
  d.setTextWrap(false);
  if (showHelp)
    drawHelp();
  else if (inMenu)
    drawMenu();
  else
    drawBoard();
  if (dialog) {
    d.fillRect(8, 40, 224, 51, Panel);
    d.drawRect(8, 40, 224, 51, Accent);
    const char *question = dialog == 1   ? "Open new-game menu?"
                           : dialog == 2 ? "Reboot? Last game saved."
                           : dialog == 3 ? "Load last saved game?"
                                         : "Replace last save: new game?";
    label(16, 49, question);
    label(16, 68, "Y:yes  N/Del:cancel", Accent);
  }
  if (spriteReady)
    frame.pushSprite(0, 0);
  dirty = false;
}
void takeBack() {
  cancelBot();
  finished = false;
  if (game.reviewing())
    game.resume();
  else
    game.undo();
  if (againstBot && game.position().turn != human && game.ply() > 0)
    game.undo();
  persist();
}
void handle(Action a) {
  if (dialog) {
    if (a == Yes) {
      int choice = dialog;
      dialog = 0;
      if (choice == 1) {
        inMenu = true;
        cancelBot();
        menuRow = 4;
        dirty = true;
      }
      if (choice == 2) {
        if (activeGame && !inMenu && !persist())
          return;
        ESP.restart();
      }
      if (choice == 3)
        loadGame();
      if (choice == 4)
        startGame();
    } else if (a == NewGame || a == Undo) {
      dialog = 0;
      if (thinking)
        botStarted = millis();
      dirty = true;
    }
    return;
  }
  if (a == Reboot) {
    if (thinking)
      botElapsed += millis() - botStarted;
    dialog = 2;
    dirty = true;
    return;
  }
  if (a == Help) {
    showHelp = !showHelp;
    if (thinking) {
      if (showHelp)
        botElapsed += millis() - botStarted;
      else
        botStarted = millis();
    }
    dirty = true;
    return;
  }
  if (showHelp)
    return;
  if (a == Load) {
    if (hasSave) {
      if (inMenu)
        loadGame();
      else {
        if (thinking)
          botElapsed += millis() - botStarted;
        dialog = 3;
        dirty = true;
      }
    } else
      message("No saved game");
    return;
  }
  if (inMenu) {
    if (a == Up)
      menuRow = (menuRow + 5) % 6;
    if (a == Down)
      menuRow = (menuRow + 1) % 6;
    if (a == Left)
      changeMenu(-1);
    if (a == Right)
      changeMenu(1);
    if (a >= One && a <= Three) {
      sizeIndex = a - One;
      menuRow = 0;
    }
    if (a == Enter || a == Place) {
      if (menuRow == 4) {
        if (hasSave)
          dialog = 4;
        else
          startGame();
      } else if (menuRow == 5) {
        if (hasSave)
          loadGame();
      } else
        changeMenu(1);
    }
    dirty = true;
    return;
  }
  if (a == SaveNow) {
    persist();
    return;
  }
  if (a == NewGame) {
    if (thinking)
      botElapsed += millis() - botStarted;
    dialog = 1;
    dirty = true;
    return;
  }
  if (a == Undo) {
    takeBack();
    return;
  }
  if (a == Zoom && game.size() > 9) {
    zoomed = !zoomed;
    dirty = true;
    return;
  }
  if (a == Resume && game.reviewing()) {
    game.resume();
    finished = false;
    cancelBot();
    persist();
    return;
  }
  int n = game.size();
  if (a == Up)
    cursorY = (cursorY + n - 1) % n;
  if (a == Down)
    cursorY = (cursorY + 1) % n;
  if (a == Left)
    cursorX = (cursorX + n - 1) % n;
  if (a == Right)
    cursorX = (cursorX + 1) % n;
  if (a <= Right) {
    dirty = true;
    return;
  }
  if (finished)
    return;
  int p = cursorY * n + cursorX;
  if (game.reviewing()) {
    if (a == Dead || a == Place) {
      game.toggleDead(p);
      persist();
    }
    if (a == Enter) {
      finished = true;
      persist();
    }
    return;
  }
  if (againstBot && game.position().turn != human)
    return;
  Go::Error e = Go::Error::Ok;
  if (a == Place || a == Enter) {
    e = game.play(p);
    if (e == Go::Error::Ok)
      persist();
    else
      message(Go::errorText(e));
  }
  if (a == Pass) {
    e = game.pass();
    if (e == Go::Error::Ok)
      persist();
    else
      message(Go::errorText(e));
  }
}
void readKeys(uint32_t now) {
  auto &k = M5Cardputer.Keyboard;
  auto s = k.keysState();
  bool keys[ActionCount] = {};
  keys[Up] = k.isKeyPressed('e') || k.isKeyPressed(';');
  keys[Down] = k.isKeyPressed('s') || k.isKeyPressed('.');
  keys[Left] = k.isKeyPressed('a') || k.isKeyPressed(',');
  keys[Right] = k.isKeyPressed('d') || k.isKeyPressed('/');
  keys[Place] = k.isKeyPressed(' ');
  keys[Enter] = s.enter;
  keys[Pass] = k.isKeyPressed('p');
  keys[Undo] = k.isKeyPressed('u') || s.del;
  keys[NewGame] = k.isKeyPressed('n');
  keys[Help] = k.isKeyPressed('h');
  keys[Dead] = k.isKeyPressed('x');
  keys[Resume] = k.isKeyPressed('r');
  keys[Zoom] = k.isKeyPressed('z');
  keys[One] = k.isKeyPressed('1');
  keys[Two] = k.isKeyPressed('2');
  keys[Three] = k.isKeyPressed('3');
  keys[Yes] = k.isKeyPressed('y');
  keys[Reboot] = k.isKeyPressed('q');
  keys[SaveNow] = k.isKeyPressed('f');
  keys[Load] = k.isKeyPressed('l');
  for (int i = 0; i < ActionCount; ++i) {
    bool fire = keys[i] && !held[i];
    if (i < 4) {
      if (fire)
        repeatAt[i] = now + 330;
      else if (keys[i] && int32_t(now - repeatAt[i]) >= 0) {
        fire = true;
        repeatAt[i] = now + 100;
      }
    }
    held[i] = keys[i];
    if (fire)
      handle(static_cast<Action>(i));
  }
}
void appSetup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  // M5GFX 0.3.0: Cardputer ADV landscape is rotation 0.
  M5Cardputer.Display.setRotation(0);
  M5Cardputer.Display.setBrightness(150);
  frame.setColorDepth(8);
  spriteReady = frame.createSprite(240, 135) != nullptr;
  game.reset();
  storageReady = Storage::begin();
  hasSave = Storage::available();
  menuRow = hasSave ? 5 : 4;
  draw();
}
void appLoop() {
  M5Cardputer.update();
  readKeys(millis());
  if (botTurn()) {
    if (!thinking) {
      bot.begin(game, micros() | 1u, botLevel);
      thinking = true;
      botElapsed = 0;
      botStarted = millis();
      message("Thinking...");
    }
    uint32_t start = micros();
    bool done = false;
    uint32_t limit = botLevel == 3 ? 3000 : 2000;
    do {
      done = bot.step(game);
      if (!done && botLevel >= 2 &&
          botElapsed + uint32_t(millis() - botStarted) >= limit) {
        bot.finish();
        done = true;
      }
    } while (!done && uint32_t(micros() - start) < 2000);
    int progress = int((botElapsed + uint32_t(millis() - botStarted)) / 100);
    if (progress != progressShown) {
      progressShown = progress;
      dirty = true;
    }
    if (done) {
      int p = bot.move();
      Go::Error e = p < 0 ? game.pass() : game.play(p);
      cancelBot();
      if (e == Go::Error::Ok)
        persist();
      else
        message(Go::errorText(e));
    }
  }
  if (dirty)
    draw();
  delay(2);
}
