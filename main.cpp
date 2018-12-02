/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#if 1
#define FB_WIDTH 1280
#define FB_HEIGHT 1024
#else
#define FB_WIDTH 640
#define FB_HEIGHT 480
#endif

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <stdint.h>
#include <cmath>
#include <valarray>
#include <vector>
#include <thread>
#include <SDL2/SDL.h>

template<int WIDTH, int HEIGHT> class WireWorld {
  const int numThreads = std::thread::hardware_concurrency();

  enum class State : uint8_t {
    Empty = 0,
    Head,
    Tail,
    Conductor
  };

  State *wire, *wireNext;

  int reinitCount = 0;

#define WRAP_AT_EDGE 0
#define VON_NEUMANN_NEIGHBOURHOOD 0

  inline int countNearbyHeads(int x, int y)
  {
    int count = 0;
    // Scan neighbourhood
    for(int ny = -1; ny <= 1; ny++) {
      for(int nx = -1; nx <= 1; nx++) {
        if((nx == 0) && (ny == 0))
          continue;
        int ix = x + nx;
        int iy = y + ny;
#if VON_NEUMANN_NEIGHBOURHOOD
        if((nx == ny) || (nx == -ny))
          continue;
#endif
#if WRAP_AT_EDGE
        if(ix < 0)
          ix = ix + WIDTH;
        else if(ix >= WIDTH)
          ix = ix - WIDTH;
        if(iy < 0)
          iy = iy + HEIGHT;
        else if(iy >= HEIGHT)
          iy = iy - HEIGHT;
#else
        if((ix < 0) || (ix >= WIDTH))
          continue;
        if((iy < 0) || (iy >= HEIGHT))
          continue;
#endif

        if(wire[iy * WIDTH + ix] == State::Head)
          count++;
      }
    }
    return count;
  }

  bool alreadyInited = false;

  void addElectron()
  {
    wire[102 * WIDTH + 5] = State::Head;
    wire[102 * WIDTH + 4] = State::Tail;
    return;
  }

  void generate()
  {
    alreadyInited = false;
    if(!alreadyInited) {
      for(int y = 0; y < HEIGHT; y++)
        for(int x = 0; x < WIDTH; x++) {
          bool elem = ((x >> 0) ^ (y >> 0)) & 1;
          elem = (rand() & 0xff) > 150;
          wire[y * WIDTH + x] = elem ? State::Conductor : State::Empty;
        }
      alreadyInited = true;
    } else {
#if 0
      if(rand() & 1) {
        wire[rand() % 8][rand() % 8] = State::Empty;
      } else { 
        wire[rand() % 8][rand() % 8] = State::Conductor;
      }
#endif
      flatten();
    }

  //  addElectron();
  }

  void flatten()
  {
    for(int y = 0; y < HEIGHT; y++) {
      for(int x = 0; x < WIDTH; x++) {
        State *wirePtr = &wire[y * WIDTH + x];
        if(*wirePtr != State::Empty)
          *wirePtr = State::Conductor;
      }
    }

    //addElectron();
  }

  void mutate()
  {
    wire[(rand() % HEIGHT) * WIDTH + (rand() % WIDTH)] = State::Empty;
    wire[(rand() % HEIGHT) * WIDTH + (rand() % WIDTH)] = State::Conductor;
    flatten();
  }

  // Binary search for valid evolution
  public:
  void init()
  {
    bool done = false;

    generate();
    //addElectron();
  }

  uint32_t lfsr = 0x12345678;

  inline uint32_t lfsrGet()
  {
    lfsr = (lfsr << 1) | (((lfsr >> 31) & 1) ^ ((lfsr >> 28) & 1));

    return lfsr ^ 0x5a5a5a5a;
  }

  // TODO: Probability of being changed goes down as each pixel is used 
  // less...
 
  bool simulate(int threadNum = 0)
  {
    bool active = false;

    const int chunkHeight = HEIGHT / numThreads;
    const int yStart = threadNum * chunkHeight;
    const int yEnd = (threadNum + 1) * chunkHeight - 1;

    for(int y = yStart; y <= yEnd; y++) {
      for(int x = 0; x < WIDTH; x++) {
        State *wireNextPtr = &wireNext[y * WIDTH + x];
        State *wirePtr = &wire[y * WIDTH + x];
        bool modFunction;
        switch(*wirePtr) {
          case State::Empty: 
            {
            int count = countNearbyHeads(x, y);
            if(count == 0) {
              modFunction = (lfsrGet() & 0x1fff) == 0x1fff;
            } else {
              modFunction = false;
            }
            *wireNextPtr = modFunction ? State::Conductor : State::Empty; 
            }
            break;
          case State::Head: 
            *wireNextPtr = State::Tail; 
            active = true;
            break;
          case State::Tail: 
            *wireNextPtr = State::Conductor;
            active = true;
            break;
         case State::Conductor:
            {
              int count = countNearbyHeads(x, y);
              if((count == 1) || (count == 2)) {

                if(*wireNextPtr == State::Tail) {
                  if((lfsrGet() & 0xff) == 0xff) {
                    *wireNextPtr = State::Empty;
                    break;
                  }
                } else {
                  if((lfsrGet() & 0x7ff) == 0x7ff) {
                    *wireNextPtr = State::Empty;
                    break;
                  }
                }

                *wireNextPtr = State::Head;
              } else
                *wireNextPtr = State::Conductor;
            }
            break;
        }
      }
    }
   

    return active;
  }

  int boringCount = 0;

  void render(uint32_t *fb, int threadNum = 0)
  {
    const int chunkHeight = HEIGHT / numThreads;
    const int yStart = threadNum * chunkHeight;
    const int yEnd = (threadNum + 1) * chunkHeight - 1;

    for(int y = yStart; y<= yEnd; y++) {
      for(int x = 0; x < WIDTH; x++) {
        uint32_t colour;
        switch(wireNext[y * WIDTH + x]) {
          case State::Empty:     colour = 0x000000; break;
          case State::Head:      colour = 0xff0000; break;
          case State::Tail:      colour = 0x00ff00; break;
          case State::Conductor: colour = 0x0000ff; break;
        }
        fb[x + y * WIDTH] = colour;
      }
    }
  }

  static void callFromThread(WireWorld *ww, uint32_t *fb, int tid)
  {
    ww->simulate(tid);
    ww->render(fb, tid);
  }

  void run(uint32_t *fb)
  {
    if(numThreads == 1) {
      callFromThread(this, fb, 0);
    } else {
      std::thread *t = new std::thread[numThreads];
      for(int i = 0; i < numThreads; i++) 
        t[i] = std::thread(callFromThread, this, fb, i);

      for(int i =  0; i < numThreads; i++)
        t[i].join();

      delete[] t;
    }
    std::swap(wire, wireNext);
    wire[(HEIGHT / 2) * WIDTH + 0] = State::Head;
  }

  void mouseClick(int x, int y, int buttons)
  {
      if(buttons & 4) {
      const int radius = 32;
      for(int sy = -radius; sy <= radius; sy++) {
        for(int sx = -radius; sx <= radius; sx++) {
          int iy = y + sy;
          int ix = x + sx;
          if((iy < 0) || (iy >= HEIGHT))
            continue;
          if((ix < 0) || (ix >= WIDTH))
            continue;
          auto *wirePtr = &wire[iy * WIDTH + ix];

          if(buttons & 1) {
            *wirePtr = State::Conductor;
          } else if(*wirePtr != State::Empty)
            *wirePtr = State::Conductor;
        } 
      }
    } else if(buttons & 2) {
      flatten();
    } else if(buttons & 1) {
      auto *wirePtr = &wire[y * WIDTH + x];
      if(*wirePtr != State::Empty)
        *wirePtr = State::Head;
    }
 
  }

  int getWidth() const { return WIDTH; }
  int getHeight() const { return HEIGHT; }

  WireWorld() { 
    wire = new State[WIDTH * HEIGHT]; 
    wireNext = new State[WIDTH * HEIGHT]; 
  }

  ~WireWorld()
  {
    delete[] wire;
    delete[] wireNext;
  }
};

template<int WIDTH, int HEIGHT> class Framebuffer {
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  SDL_Texture *texture = NULL;

  void fini()
  {
    if(texture)
      SDL_DestroyTexture(texture);
    if(renderer)
      SDL_DestroyRenderer(renderer);
    if(window)
      SDL_DestroyWindow(window);
  }


  public:
  bool init()
  {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
    signal(SIGINT, SIG_DFL);

    window = SDL_CreateWindow("Framebuffer", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, 0);
    if(window == NULL) {
      fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
      return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | 
        SDL_RENDERER_PRESENTVSYNC);
    if(window == NULL) {
      fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
      return false;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32,
        SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if(texture == NULL) {
      fprintf(stderr, "Could not create texture: %s\n", SDL_GetError());
      return false;
    }

    return true;
  }

  int mouseX = 0, mouseY = 0, mouseButtons = 0;

  void getMouseInfo(int &mouseX, int &mouseY, int &mouseButtons)
  {
    mouseX = this->mouseX;
    mouseY = this->mouseY;
    mouseButtons = this->mouseButtons;
  }

  bool pollInput()
  {
    SDL_Event event;
    while(SDL_PollEvent(&event) != 0) {
      if(event.type == SDL_KEYDOWN) {
        if(event.key.keysym.sym == SDLK_ESCAPE)
          return false;
      } else if(event.type == SDL_QUIT) {
        return false;
      } else if(event.type == SDL_MOUSEMOTION) {
        if(event.motion.state) {
          mouseX = event.motion.x;
          mouseY = event.motion.y;
        }
      } else if(event.type == SDL_MOUSEBUTTONDOWN) {
          mouseX = event.button.x;
          mouseY = event.button.y;
          mouseButtons |= (1 << (event.button.button - 1));
      } else if(event.type == SDL_MOUSEBUTTONUP) {
          mouseX = event.button.x;
          mouseY = event.button.y;
          mouseButtons &= ~(1 << (event.button.button - 1));
      }
    }
    return true;
  }

  void * acquire()
  {
    void *pixels;
    int pitch;

    SDL_LockTexture(texture, NULL, &pixels, &pitch);

    return pixels;
  };

  bool paint()
  {
    SDL_UnlockTexture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    return true;
  }

  Framebuffer() { 
  }

  ~Framebuffer() {
    fini();
  }
};

int main(int argc, char **argv)
{
  Framebuffer<FB_WIDTH, FB_HEIGHT> fb;
  if(!fb.init())
    return 1;

  auto *ww = new WireWorld<FB_WIDTH, FB_HEIGHT>();

  ww->init();

  int frame = 0;
  while(fb.pollInput()) {
    int mouseX, mouseY, mouseButtons;
    fb.getMouseInfo(mouseX, mouseY, mouseButtons);
    ww->mouseClick(mouseX, mouseY, mouseButtons);

    ww->run(reinterpret_cast<uint32_t *>(fb.acquire()));
    fb.paint();
  }

  return 0;
}

