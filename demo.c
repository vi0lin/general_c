// IMGUI
// GLFW
// WINDOW POSITION
// RELATIME FIREFOX DOM
// FEED WITH COMMANDS
// TIMED EXECUTION SCHEDULER
//   DRAG AND DROP / EXECUTE / DELETE
// DIFFERENT MAINS

static const int end_x=17;
static const int end_y=12;
// static const int end_x=2;
// static const int end_y=2;

static const int start_x=0;
static const int start_y=0;

// Time Check / Date Time / Scheduler
// Self Updater / Restart
// Persist Felder Erntezeit / On Restart / Check Dom Or Unharvest By File / Auto Harvest after Restart
// Switch Garden 1, 2, 3, 4
// Auto Weg Klicken / Get Dom Query Information
// Auto Re Login / IdleTimeout
//
//
// remote add next item slot 2 count 5
// quest optimizing
// loop / save last time
// self updater / on new exe.update found in folder / autoclose itself / autorestart
// stop abarbeiten / pickup / fortfahren where left
// moving mouse stops the interaction

#include <stdio.h> // for sleep
#include <stdlib.h> // for atoi

#include "input.h"
#include "timeout.h"
#include "do_imgui.h"
#include "do_vulkan.h"
#include "do_glfw.h"
#include "firefox_dom.h"
#include <math.h>
#include <stdint.h>
#include "updater.h"
#include <windows.h>

int x_tiles=17;
int y_tiles=12;
int first_tile_x=760;
int first_tile_y=320;
int first_position_x=1210;
int first_position_y=315;
int last_position_x=1853;
int last_position_y=756;
int width=40;
int ernten_x=1490;
int ernten_y=219;
int bew_x=1543;
int bew_y=228;
int alles_ernten_x=1691;
int alles_ernten_y=813;
int alles_bew_x=1642;
int alles_bew_y=816;

static const int sec=1;
static const int min=60;
static const int std=60*60;
static const int day=60*60*24;

void move_click(int x, int y)
{
  mouse_move(x,y);
  mouse_click_left(true);
  timeout_ms(100);
  mouse_click_left(false);
  timeout_ms(300);
}

typedef struct {
  int x;
  int y;
} Vector2;

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
  char* text;
  int lvl;
  int space;
  char* duration_str;
  int duration;
  int points_per_harvest;
  int yield_per_harvest;
} SaatSpecifics;

typedef struct{
  SaatSpecifics specs;
  Vector2 field;
} SpecificField;


void Field(){
  for(int ix=0; ix<17; ix++){
    for(int iy=0; iy<13; iy++){
    }
  }
}

// #	Sorte	L	F	t	t	P/Ha	Yi/Ha"
SaatSpecifics SaatSpecs[]={
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

// 47 "Champignon (Pilz)	Var.	1"	Variabel	-	Variabel	4
// 50 "Echino (Kaktus)	Var.	4"	"151:"20 h+	9072+	9072+	Var.

typedef struct {
  int x,y;
  int index;
} Coordinate;

typedef struct {
  SaatSpecifics seed;
  int datetime;
  Coordinate coord;
} Planted;

typedef struct {
} GanzesFeld;

typedef struct {
  Saat key;
  Vector2 value;
} CupboardPosition;

// int get(CupboardPosition dict[], Saat name) {
//   for (int i=0; i < 13; i++) {
//     if (dict[i].name == name) {
//       return i;
//     }
//   }
//   return -1;
// }

// void select(Saat key) {
//   CupboardPosition data[] = {
//     { Salat, {520, 450}},
//     { Karotte, {560, 450}},
//     { Gurke, {600, 450}},
//     { Radieschen, {650, 450}},
//     { Erdbeere, {520, 510}},
//     { Tomate, {560, 510}},
//     { Zwiebel, {600, 510}},
//     { Spinat, {650, 510}},
//     { Kartoffel, {520, 590}},
//     { Blumenkohl, {560, 590}},
//     { Knoblauch, {600, 590}},
//     { Paprika, {650, 590}},
//     { Brokkoli, {520, 660}}
//   };
//   int index=get(data,key);
//   if (index >= 0) {
//     CupboardPosition found=data[index];
//     move_click(found.value.x, found.value.y);
//     timeout_ms(100);
//   }
// }

void select_anpflanzen() {
  move_click(990, 223);
  timeout_ms(500);
}

void select_bewaessern() {
  move_click(1106, 225);
  timeout_ms(500);
}

void nothing_to_harvest() {
  move_click(1077, 441);
  timeout_ms(500);
}

void select_slot(int slot) {
  int cols=4;
  int rows=5;
  int first_x=516;
  int first_y=454;
  int dist_x=40;
  int dist_y=70;
  move_click(first_x+(slot%cols)*dist_x, first_y+(int)(floor(slot/cols))%rows*dist_y);
  timeout_ms(500);
  mouse_move(1077, 441);
  timeout_ms(500);
}

void click_pos(int pos_x, int pos_y){
  int click_dur_ms=100;
  int x, y;
  get_mouse_pos(&x, &y);
  mouse_move(pos_x, pos_y);
  timeout_ms(click_dur_ms);
  mouse_click_left(true);
  timeout_ms(click_dur_ms);
  mouse_click_left(false);
  timeout_ms(click_dur_ms);
  mouse_move(x, y);
  timeout_ms(300);
}

void select_ernten() {
  click_pos(1051, 214);
  timeout_ms(500);
}

// void feld_ernten() {
//   move_click(1250, 810);
//   timeout_ms(5000); //5 Sekunden
//   move_click(1077,444);
//   timeout_ms(2000); //2 Sekunden
//   move_click(1200,446);
//   timeout_ms(2000); //2 Sekunden
// }

void ernte_helfer() {
  move_click(1247, 805);
  timeout(2*sec);
}

void ernte_helfer_prompt() {
  move_click(1079, 438);
  timeout_ms(500);
  move_click(1199, 438);
  timeout_ms(500);
}

void feld_abarbeiten(int s_x, int e_x, int s_y, int e_y) {
  // int max_tiles=x_tiles*y_tiles;
  int distance_x=(last_position_x-first_position_x)/x_tiles;
  int distance_y=(last_position_y-first_position_y)/y_tiles;
  distance_x=40;
  distance_y=40;
  int ix=s_x;
  int iy=s_y;
  while (ix<e_x) {
    iy=0;
    while (iy<e_y) {
      timeout_ms(800);
      mouse_move(first_tile_x+ix*distance_x, first_tile_y+iy*distance_y);
      timeout_ms(100);
      iy+=1;
      mouse_click_left(true);
      timeout_ms(700);
      mouse_click_left(false);
    }
    ix+=1;
  }
  mouse_move(990, 590);
  timeout_ms(500);
}

// for(int i=0; i<20; i++){
//   work[i] = malloc(sizeof(int) * 3);
// }
int work[20][3] = {
{ 0,  14*min,       14  },
{ 1,  10*min,       18  },
{ 2,  40*min,       11  },
{ 3,  50*min,       8   },
{ 4,  2*std,        3   },
{ 5,  2*std+20*min, 3   },
{ 6,  8*std,        1   },
{ 7,  9*std+20*min, 1   },
{ 8,  16*std,       1   },
{ 9, 16*std+40*min, 1   },
{ 10, 22*std,       1   },
{ 11, 40*std,       1   },
{ 12, 20*std,       1   },
{ 13, 6*std,        1   },
{ 14, 42*std,       1   },
{ 15, 5*std,        1   },
{ 16, 48*std,       1   },
{ 17, 48*std,       1   },
{ 18, 48*std,       1   },
{ 19, 32*std,       1   }
};

void belohnung_slot(int slot) {
  int cols=3;
  int rows=3;
  int first_x=948;
  int first_y=444;
  int dist_x=125;
  int dist_y=100;
  move_click(first_x+(slot%cols)*dist_x, first_y+floor(slot/rows)*dist_y);
  mouse_move(1077, 441);
  timeout_ms(300);
}
void belohnung_abholen() {
  move_click(1085,740);
  timeout_ms(300);
}
void belohnung_close() {
  move_click(1301,346);
  timeout_ms(300);
}
int size_of_array() {
  int arr[5] = { 1, 2, 3, 4, 5 };
  // Compute
  int n = sizeof(arr) / sizeof(arr[0]);
  printf("%d", n);
  // Pointer Arithmetic
  int o = *(&arr + 1) - arr;
  printf("%d", o);

  return 0;
}

// void slot_routine_n(int slot, int count) {
//   int click_slot=work[slot][0];
//   int time=work[slot][1];
//   int n=0;
//   while(n<count) {
//     select_slot(click_slot);
//     // write timestamp1
//     feld_abarbeiten(start_x,end_x,start_y,end_y);
//     select_bewaessern();
//     feld_abarbeiten(start_x,end_x,start_y,end_y);
//     // write timestamp2
//     // write timestamp2 - timestamp1
//     // time - computed feld_abarbeiten_time * 2
//     timeout_idle(time+5, 30);
//     ernte_helfer();
//     ernte_helfer_prompt();
//     n++;
//   }
// }
//

void select_feld(int feld) {
  int garten[4][2]={
    {1446, 422},
    {1446, 444},
    {1446, 466},
    {1446, 488}
  };
  click_pos(garten[feld-1][0], garten[feld-1][1]);
  timeout_ms(1500);
}

void slot_routine(int slot, int time) {
  timeout_ms(500);
  // write timestamp1
  select_feld(1);
  // select_ernten();
  // feld_abarbeiten(start_x,end_x,start_y,end_y);
  ernte_helfer();
  ernte_helfer_prompt();
  select_slot(slot);
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  select_bewaessern();
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  select_feld(2);
  // select_ernten();
  // feld_abarbeiten(start_x,end_x,start_y,end_y);
  ernte_helfer();
  ernte_helfer_prompt();
  select_slot(slot);
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  select_bewaessern();
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  // write timestamp2
  // write timestamp2 - timestamp1
  // time - computed feld_abarbeiten_time * 2
  timeout_idle(time+5*sec, 30);
  ernte_helfer();
  ernte_helfer_prompt();
}

void routine_fields(int* fields, int time) {
  int fields_len=*(&fields + 1) - fields;
  timeout_ms(500);
  // write timestamp1
  //
  for(int f=0; f<fields_len; f++){
    select_feld(fields[f]);
    ernte_helfer();
    ernte_helfer_prompt();
    feld_abarbeiten(start_x,end_x,start_y,end_y);
  }

  select_bewaessern();
  for(int f=0; f<=fields_len; f++){
    select_feld(fields[f]);
    feld_abarbeiten(start_x,end_x,start_y,end_y);
  }
  // write timestamp2
  // write timestamp2 - timestamp1
  // time - computed feld_abarbeiten_time * 2
  select_feld(fields[0]);
  timeout_idle(time+5*sec, 30);
  ernte_helfer();
  ernte_helfer_prompt();
}

void routine(int time) {
  timeout_ms(500);
  ernte_helfer();
  ernte_helfer_prompt();
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  select_bewaessern();
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  // write timestamp2
  // write timestamp2 - timestamp1
  // time - computed feld_abarbeiten_time * 2
  timeout_idle(time+5*sec, 30);
  ernte_helfer();
  ernte_helfer_prompt();
}


void routine_good(int time) {
  timeout_ms(500);
  // write timestamp1
  select_feld(1);
  // select_ernten();
  // feld_abarbeiten(start_x,end_x,start_y,end_y);
  ernte_helfer();
  ernte_helfer_prompt();
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  select_bewaessern();
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  select_feld(2);
  // select_ernten();
  // feld_abarbeiten(start_x,end_x,start_y,end_y);
  ernte_helfer();
  ernte_helfer_prompt();
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  select_bewaessern();
  feld_abarbeiten(start_x,end_x,start_y,end_y);
  // write timestamp2
  // write timestamp2 - timestamp1
  // time - computed feld_abarbeiten_time * 2
  timeout_idle(time+5*sec, 30);
  ernte_helfer();
  ernte_helfer_prompt();
}


void slot_routine_n(int slot, int count) {
  // todo make this better / positions in cupboard may vary
  // find carrot in cupboard
  // check carrots time
  int32_t time=work[slot][1];
  // int click_slot=work[slot][0];
  // int count=work[slot][2];
  int n=0;
  while(n<count) {
    slot_routine(slot, time);
    n++;
  }
}

void slot_routine_workload(int slot) {
  int click_slot=work[slot][0];
  int32_t time=work[slot][1];
  int count=work[slot][2];
  int n=0;
  while(n<count) {
    select_slot(click_slot);
    slot_routine(slot, time);
    n++;
  }
}

#include <signal.h>
static bool quitLoop=false;
static void sigintHandler(int sig) {
  printf("sig: %d", sig);
  quitLoop=true;
}

void Position() {
  int x, y;
  while(!quitLoop) {
    get_mouse_pos(&x, &y);
    printf("Mouse at: %d, %d\n", x, y);
    timeout_ms(30);
  }
}

int Hide() {
  HWND win = GetConsoleWindow();
  // ShowWindow(win, SW_HIDE);
  ShowWindow(win, SW_SHOWMINNOACTIVE);
  // SetWindowPos(win, NULL, -500, -500, 0, 0, SWP_NOSIZE);
  return 0;
}

int Hide2() {
  Do_Glfw();
}

int FirefoxDom() {
  do_firefox_dom();
}

int ClosePrompts() {
  // ernte_helfer();
  ernte_helfer_prompt();
}

int Logout() {
  click_pos(1417, 263);
  timeout_ms(500);
}

int Login() {
  click_pos(1241, 327);
  timeout(5);
  click_pos(1241, 327);
  timeout(5);
  // todo Check This!
  click_pos(1402, 304);
  timeout_ms(500);
}

int Imgui() {
  printf("Imgui");
  while(true) {
    timeout_ms(30);
    Do_Imgui();
  }
}

int Vulkan() {
  printf("Vulkan");
  while(true) {
    timeout_ms(30);
    Do_Vulkan();
  }
}

// int Harvest() {
// int Harvest(int argc, char* argv[]) {
int Harvest(int argc, char **argv) {
  signal(SIGINT, sigintHandler);
  printf("Harvest");
  timeout_ms(500);
  int rule[3][2]={
    { 5, 5 },
    { 7, 5 },
    { 8, 5 }
  };
  int x=0;
  while(true) {
    slot_routine_n(rule[x%2][0], rule[x%2][1]);
    x++;
  }
}

// int HarvestAsIsOnce(int argc, char **argv) {
int HarvestAllFieldsOnce(int argc, char* argv[]) {
  signal(SIGINT, sigintHandler);
  Hide();
  printf("Harvest All Fields Once");
  int pre_std;
  int pre_min;
  int pre_sec;
  int time_std;
  int time_min;
  if(argc==3||argc==5) {
    int pre_std=atoi(argv[0]);
    int pre_min=atoi(argv[1]);
    int pre_sec=atoi(argv[2]);
    if(argc==5) {
      int time_std=atoi(argv[3]);
      int time_min=atoi(argv[4]);
    }
  }
  else{
    int pre_std=0;
    int pre_min=0;
    int pre_sec=3;
    int time_std=0;
    int time_min=0;
  }
  timeout_ms(500);
  timeout_idle(pre_std*std+pre_min*min+pre_sec*sec,30);
  int fields[]={1, 2};
  routine_fields(fields, time_std*std+time_min*min);
}

int Display() {
  while(!quitLoop) {
    Do_Glfw();
    timeout_ms(100);
  }
}

// int HarvestAsIsOnce(int argc, char **argv) {
int HarvestThisFieldOnce(int argc, char* argv[]) {
  signal(SIGINT, sigintHandler);
  Hide();
  printf("Harvest This Field Once");
  int pre_std;
  int pre_min;
  int pre_sec;
  int time_std;
  int time_min;
  if(argc==3||argc==5) {
    int pre_std=atoi(argv[0]);
    int pre_min=atoi(argv[1]);
    int pre_sec=atoi(argv[2]);
    if(argc==5) {
      int time_std=atoi(argv[3]);
      int time_min=atoi(argv[4]);
    }
  }
  else{
    int pre_std=0;
    int pre_min=0;
    int pre_sec=3;
    int time_std=0;
    int time_min=0;
  }
  timeout_ms(500);
  timeout_idle(pre_std*std+pre_min*min+pre_sec*sec,30);
  routine(time_std*std+time_min*min);
}

int Timeout() {
  printf("Timeout");
  while(!quitLoop) {
    timeout_idle(100,30);
  }
}

int Markdown() {
  markdown();
}

int CheckSelectSlot() {
  timeout(1);
  select_slot(0);
  timeout(1);
  select_slot(4);
  timeout(1);
  select_slot(5);
  timeout(1);
  select_slot(19);
  timeout(1);
  select_slot(20);
  timeout(1);
  select_slot(24);
  timeout(1);
  select_slot(25);
  timeout(1);
  select_slot(39);
}

// int main(int argc, char* argv[]) {
  // Hide();
  // Position();
  // Updater();
  // CheckSelectSlot();
  // Harvest();
  // Timeout();
// }

// int main() {
//     int x, y;
//
//     // Move mouse
//     mouse_move(500, 300);
//
//     // Click
//     mouse_click_left(true);
//     usleep(50000);
//     mouse_click_left(false);
//
//     // Scroll
//     mouse_wheel(3);  // scroll up
//
//     // Type text
//     key_type("Hello from C!\n");
//
//     // Get position
//     get_mouse_pos(&x, &y);
//     printf("Mouse at: %d, %d\n", x, y);
//
//     return 0;
// }

    // if (strcmp(dict[i].key, key) == 0) {
      // printf("%s {%d, %d}", dict[i].key, dict[i].value.x, dict[i].value.y);
  // printf("%s - not found", key);
// CupboardPosition GetCupboardPosition(CupboardPosition data[], const char *key)
// {
//   CupboardPosition found=data[get(data,key)];
//   if (found != NULL) {
//     return found;
//   }
//   else {
//     return NULL;
//   }
// }

      // usleep(100000);
      //
    // select(Radischen);
  // move_click(1200,446);
    // move_click(x, y);
// #define NULL ((void*)0)
//int x, y;
    // usleep((ms/every)*1000-click_dur);
    // timeout_ms(1*1000*60*50+2000); //50 Minuten warten
    // select_ernten();
    // feld_abarbeiten(0,17,0,12);
  //   feld_ernten();
  // }
  // select("salat");
  // timeout_ms(1000);
  // select("karotte");
  // timeout_ms(1000);
  // select("gurke");
  // timeout_ms(1000);
  // select("radischien");
  // timeout_ms(1000);
  // select("erdbeere");
  // timeout_ms(1000);
  // select("tomate");
  // timeout_ms(1000);
  // select("zwiebel");
  // timeout_ms(1000);
  // select("spinat");
  // timeout_ms(1000);
  // select("kartoffel");
  // timeout_ms(1000);
  // select("blumenkohl");
  // timeout_ms(1000);
  // select("knoblauch");
  // timeout_ms(1000);
  // select("paprika");
  // timeout_ms(1000);
  // select("brokkoli");
  // timeout_ms(1000);
  // feld_abarbeiten(1,0,17,12);
  // select_bewaessern();
  // feld_abarbeiten(1,0,9,12);
  // feld_abarbeiten(10,0,17,12);
  // while(i<=max_tiles) {
  //   int real_x=first_position_x+i%x_tiles*distance_x;
  //   int real_y=first_position_y+i%y_tiles*distance_y;
  //   mouse_move(real_x, real_y);
  //   // mouse_click_left(true);
  //   usleep(50000);
  //   // mouse_click_left(true);
  //   usleep(300000);
  //   i+=1;
  // }
