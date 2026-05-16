#include <unistd.h> // for sleep
#include <stdio.h> // for sleep
#include <string.h>

#include <format>
#include <string>
// #include <format>
// #include <iostream>

#include "input.h"
#include "timeout.h"

typedef enum {
  Undefined,
  Karotte,
  Salat,
  Gurke,
  Radieschen,
  Erdbeere,
  Tomate,
  Zwiebel,
  Spinat,
  Ringelblume,
  Blumenkohl,
  Kartoffel,
  Knoblauch,
  Brokkoli,
  Paprika,
  Sonnenblume,
  Spargel,
  Aubergine,
  Zucchini,
  Heidelbeere,
  Himbeere,
  Johannisbeere,
  Brombeere,
  Kürbis,
  Rose,
  Traube,
  Mirabelle,
  Apfel,
  Kirsche,
  Birne,
  Pflaume,
  Kokosnuss,
  Gerbera,
  Lavendel,
  Zitrone,
  Orangen,
  NickendeDistel,
  GemeineWegwarte,
  SibirischeSchwertlilie,
  Moorlilie,
  Heidenelke,
  Leberblümchen,
  Schwanenblume,
  Teufelsabbiss,
  Schlüsselblume,
  Klatschmohn,
  Walnuss,
  GelbeTeichrose,
  Wassersellerie
} Saat;

typedef struct {
  int id;
  Saat name;
  const char* text;
  int lvl;
  int space;
  const char* duration_str;
  int duration;
  int points_per_harvest;
  int yield_per_harvest;
} SaatSpecifics;

// #	Sorte	L	F	t	t	P/Ha	Yi/Ha"
SaatSpecifics saatspecs[]={
{ 1, Karotte, "Karotte", 1, 1, "00:10 h", 9, 1, 2},
{ 2, Salat, "Salat", 1, 1, "00:14 h", 14, 1, 2},
{ 3, Gurke, "Gurke", 2, 1, "00:40 h", 40, 4, 4},
{ 4, Radieschen, "Radieschen", 2, 1, "00:50 h", 50, 5, 3},
{ 5, Erdbeere, "Erdbeere", 3, 1, "02:00 h", 120, 12, 4},
{ 6, Tomate, "Tomate", 3, 1, "02:20 h", 140, 14, 4},
{ 7, Zwiebel, "Zwiebel", 4, 1, "08:00 h", 480, 48, 4},
{ 8, Spinat, "Spinat", 4, 1, "09:20 h", 560, 56, 4},
{ 9, Ringelblume, "Ringelblume", 5, 1, "06:00 h", 360, 36, 4},
{ 10, Blumenkohl, "Blumenkohl", 5, 1, "16:40 h", 1000, 100, 4},
{ 11, Kartoffel, "Kartoffel", 6, 1, "16:00 h", 960, 96, 4},
{ 12, Knoblauch, "Knoblauch", 6, 1, "22:00 h", 1320, 132, 4},
{ 13, Brokkoli, "Brokkoli", 7, 1, "20:00 h", 1200, 120, 4},
{ 14, Paprika, "Paprika", 7, 1, "40:00 h", 2400, 240, 5},
{ 15, Sonnenblume, "Sonnenblume", 8, 1, "05:00 h", 300, 30, 3},
{ 16, Spargel, "Spargel", 8, 2, "42:00 h", 2520, 504, 5},
{ 17, Aubergine, "Aubergine", 9, 1, "48:00 h", 2880, 288, 5},
{ 18, Zucchini, "Zucchini", 9, 1, "48:00 h", 2880, 288, 4},
{ 19, Heidelbeere, "Heidelbeere", 10, 1, "32:00 h", 1920, 192, 6},
{ 20, Himbeere, "Himbeere", 10, 1, "48:00 h", 2880, 288, 5},
{ 21, Johannisbeere, "Johannisbeere", 11, 1, "36:00 h", 2160, 216, 6},
{ 22, Brombeere, "Brombeere", 11, 1, "48:00 h", 2880, 288, 5},
{ 23, Kürbis, "Kürbis", 12, 1, "24:00 h", 1440, 144, 6},
{ 24, Rose, "Rose", 13, 1, "12:00 h", 720, 72, 4},
{ 25, Traube, "Traube", 14, 4, "48:00 h", 2880, 1152, 12},
{ 26, Mirabelle, "Mirabelle", 15, 4, "144:00 h", 8640, 3456, 16},
{ 27, Apfel, "Apfel", 16, 4, "120:00 h", 7200, 2880, 18},
{ 28, Kirsche, "Kirsche", 17, 4, "120:00 h", 7200, 2880, 18},
{ 29, Birne, "Birne", 18, 4, "120:00 h", 7200, 2880, 18},
{ 30, Pflaume, "Pflaume", 19, 4, "144:00 h", 8640, 3456, 16},
{ 31, Kokosnuss, "Kokosnuss", 20, 4, "35:00 h", 2100, 840, 1 },
{ 32, Gerbera, "Gerbera", 21, 1, "09:20 h", 560, 56, 4},
{ 33, Lavendel, "Lavendel", 21, 2, "120:00 h", 7200, 1440, 8},
{ 34, Zitrone, "Zitrone", 22, 4, "16:00 h", 960, 384, 4},
{ 35, Orangen, "Orangen", 23, 4, "96:00 h", 5760, 2304, 9},
{ 36, NickendeDistel, "Nickende Distel", 22, 1, "18:00 h", 1080, 108, 4},
{ 37, GemeineWegwarte, "Gemeine Wegwarte", 23, 1, "68:00 h", 4080, 408, 5},
{ 38, SibirischeSchwertlilie, "Sibirische Schwertlilie", 24, 1, "48:00 h", 2880, 288, 5},
{ 39, Moorlilie, "Moorlilie", 25, 1, "80:00 h", 4800, 480, 6},
{ 40, Heidenelke, "Heidenelke", 26, 1, "24:00 h", 1440, 144, 4},
{ 41, Leberblümchen, "Leberblümchen", 27, 1, "12:00 h", 720, 72, 4},
{ 42, Schwanenblume, "Schwanenblume", 28, 1, "60:00 h", 3600, 360, 5},
{ 43, Teufelsabbiss, "Teufelsabbiss", 29, 1, "30:00 h", 1800, 180, 4},
{ 44, Schlüsselblume, "Schlüsselblume", 30, 1, "16:00 h", 960, 96, 4},
{ 45, Klatschmohn, "Klatschmohn", 31, 1, "120:00 h", 7200, 720, 6},
{ 46, Walnuss, "Walnuss", 32, 4, "336:00 h", 20160, 8064, 19},
{ 48, GelbeTeichrose, "Gelbe Teichrose (Wasser)", 19, 1, "10:00 h", 600, 60, 3},
{ 49, Wassersellerie, "Wassersellerie (Wasser)", 20, 4, "13:20 h", 800, 320, 4}
};

typedef struct {
  int x;
  int y;
} Coordinate;

class Planted{
  public:
    SaatSpecifics seed;
    int datetime;
    Coordinate position;
    std::string toString() {
       return std::format("%s %d %d %d", seed.text, datetime, position.x, position.y);
    }
};

void move_click(int x, int y)
{
  // mouse_move(x,y);
  // mouse_click_left(true);
  timeout_ms(7000);
  // mouse_click_left(false);
}

int get(SaatSpecifics dict[], Saat name) {
  for (int i=0; i < 13; i++) { 
    if (dict[i].name == name) {
      return i;
    }
  }
  return -1;
}
class Field {
  private:
    int first_tile_x=760;
    int first_tile_y=320;
    int distance=40;
  public:
    int x=17;
    int y=12;
    Planted matrix[17][12];
    Field() {}
    void doPlant(int x, int y, Saat saatgut) {
      Planted p = Planted(saatspecs[get(saatspecs, saatgut)], 0, {x, y}); 
      matrix[x][y]=p;
    }
    void doBewaessern(int x, int y) {
    }
};

class ActionCueue {
  private:
    Field field;
  public:
    ActionCueue(Field field) {
      this->field=field;
      for(int ix=0; ix<17; ix++) {
        for(int iy=0; iy<12; iy++) {
          Planted n = {saatspecs[0], 1200, { ix, iy }};
          this->field.matrix[ix][iy]=n;
        }
      }
    }
    int GetActions() {
      for (int ix=0; ix<this->field.x; ix++) {
        for (int iy=0; iy<this->field.y; iy++) {
          Planted p=this->field.matrix[ix][iy];
          //printf("%s, %s, %s", p.seed.text, (char)p.datetime, (char)p.position.x+", "+(char)p.position.y);
          printf("%s, ", p.toString()); 
        }
      }
      // return p.seed.text;
      return -1;
    }
};

// int main() {
//   printf("test");
//   return -1;
//   // int deltatime=0;
//   // while(true) {
//   //   timeout_ms(1000);
//   //   printf("%d deltatime, %s", deltatime, "Test!");
//   //   deltatime+=1;
//   // }
//   // return -1;
//   // Field f=Field();
//   // for(int ix=0; ix<17; ix++) {
//   //   for(int iy=0; iy<12; iy++) {
//   //     f.doPlant(ix,iy,Ringelblume);
//   //   }
//   // }
//   // for(int ix=0; ix<17; ix++) {
//   //   for(int iy=0; iy<12; iy++) {
//   //     f.doBewaessern(ix,iy);
//   //   }
//   // }
// }

#include "Input.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

typedef struct{
  char name;
  Coordinate position;
}Position;

// Position pos[] = [
//   "anpflanzen": { 990, 223 },
//   "bewaessern": { 1106, 225 },
//   "ernten": { 1051, 214 },
//   "feld_ernten": { 1051, 214 },
// ];

void idle(Field field) {
  ActionCueue ac = ActionCueue(field);
  printf("%d", ac.GetActions());
}

volatile std::sig_atomic_t quitLoop{ false };

void handle(int signal) {
  quitLoop = true;
}

int main() {
    std::signal(SIGINT, handle);
    Position();
    auto& input = Input::getInstance();
    Field field=Field();
    // input.moveTo(800, 600);
    // input.doubleClickAt(100, 100);
    // input.type("Hello from C++ cross-platform input class!\n");
    // input.scroll(10);

    // std::this_thread::sleep_for(7s); // time to switch window
    //
    int x, y;
    while (true) {
      input.getPosition(x, y);

      std::cout << "Mouse is at: " << x << ", " << y << std::endl;
      idle(field);
      // timeout_ms(10000/10);
      std::this_thread::sleep_for(1000ms); // time to switch window
    }

    return 0;
}
