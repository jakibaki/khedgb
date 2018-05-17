#include "cpu.h"
#include "memmap.h"
#include "rom.h"
#include "util.h"
#include<SDL2/SDL.h>
#include<switch.h>

#include<iostream>
#include<fstream>
#include<cassert>
#include<png.h>

#ifdef DEBUG
#define ASSERT assert
#else
#define ASSERT //assert
#endif
#define SCREEN_W 1280
#define SCREEN_H 720

#define JOY_A     0
#define JOY_B     1
#define JOY_X     2
#define JOY_Y     3
#define JOY_MINUS 11
#define JOY_LEFT  12
#define JOY_UP    13
#define JOY_RIGHT 14
#define JOY_DOWN  15
#define JOY_LSTICK_LEFT 16
#define JOY_LSTICK_UP 17
#define JOY_LSTICK_RIGHT 18
#define JOY_LSTICK_DOWN 19
#define LIM 2000

namespace util {
#ifndef PPU_ONLY
bool process_events(cpu * proc, memmap * bus) {
    SDL_Event event;
    unsigned int newx,newy;

    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            
            
            case SDL_JOYBUTTONDOWN:  /* Handle a KEYDOWN event */
                //printf("util::Keydown\n");
                bus->keydown(event.jbutton.button);
                break;
            case SDL_JOYBUTTONUP: /* Handle a KEYUP event*/
                //printf("util::Keyup\n"); 
                bus->keyup(event.jbutton.button);
                break;
            case SDL_WINDOWEVENT:
                //printf("util::Window event: ");
                switch(event.window.event) {
                    case SDL_WINDOWEVENT_SHOWN:
                        //printf("shown\n");
                        break;
                    case SDL_WINDOWEVENT_HIDDEN:
                        //printf("hidden\n");
                        break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        //printf("resized\n");
                        newx=event.window.data1;
                        newy=event.window.data2;
                        if(newx > 0 && newy > 0) {
                            //changed=true;
                        }
                        //ppui.resize(newx,newy);
                        bus->win_resize(newx, newy);
                        break;
                    case SDL_WINDOWEVENT_EXPOSED:
                        //printf("exposed\n");
                        //ppui.resize(1,1);
                        break;
                    case SDL_WINDOWEVENT_CLOSE:
                        //printf("closed\n");
                        SDL_Quit();
                        return false;
                        break;
                    default:
                        //printf("something else (%d)\n", event.window.event);
                        break;
                }
                break;
            case SDL_QUIT:
                //printf("util::SDL Quit triggered\n");
                //SDL_PauseAudio(true);
                SDL_Quit();
                return false;
                break;
            case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEWHEEL:
                //printf("Mouse event\n");
                break;
            case SDL_TEXTINPUT:
                //printf("util::SDL TextInput\n");
                break;
            case SDL_KEYMAPCHANGED:
                //printf("util::SDL KeymapChanged triggered\n");
                break;
            case SDL_AUDIODEVICEADDED:
                //printf("util::SDL Audio Device Added\n");
                break;
            default: /* Report an unhandled event */
                //printf("util::I don't know what this event is (%d)! Flushing it.\n", event.type);
                SDL_FlushEvent(event.type);
                break;
        }
    }
    return true;
}
#endif


int read(const std::string& filename, Vect<uint8_t>& output, size_t min_size, size_t max_size) {
    std::ifstream in(filename.c_str());
    if(in.is_open()) {
        size_t size = 0;
        in.seekg(0, std::ios::end);
        size = in.tellg();
        if(size < min_size || size > max_size) {
            in.close();
            return 1;
        }
        in.seekg(0, std::ios::beg);
        //printf("Opened %s, found a file of %ld bytes.\n", filename.c_str(), size);
        output.resize(size);
        in.read(reinterpret_cast<char *>(&(output[0])), size);
        in.close();
    }
    else {
        fprintf(stderr, "Couldn't open %s.\n", filename.c_str());
        return 2;
    }
    return 0;
}

bool reinit_sdl_screen(SDL_Window ** screen, SDL_Renderer ** renderer, SDL_Texture ** texture, unsigned int xres, unsigned int yres) {
    if(*screen) {
        SDL_DestroyWindow(*screen);
        *screen = NULL;
    }
    if(*renderer) {
        SDL_DestroyRenderer(*renderer);
        *renderer = NULL;
    }
    if(*texture) {
        SDL_DestroyTexture(*texture);
        *texture = NULL;
    }

    /* Initialize the SDL library */
    //*screen = SDL_CreateWindow("KhedGB",
    //                           SDL_WINDOWPOS_CENTERED,
    //                           SDL_WINDOWPOS_CENTERED,
    //                           xres*2, yres*2,
    //                           SDL_WINDOW_RESIZABLE);
    //*screen = SDL_CreateWindow("sdl2+mixer+image demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    //*screen = SDL_CreateWindow("sdl2+mixer+image demo", SDL_WINDOWPOS_CENTERED, xres, yres, SDL_WINDOW_SHOWN, SDL_WINDOW_FULLSCREEN);
    //*screen = SDL_CreateWindow("sdl2+mixer+image demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, xres, yres, SDL_WINDOW_SHOWN);

    *screen = SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, xres*2, yres*2, SDL_WINDOW_FULLSCREEN);
    //*screen = SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_H/2, (SCREEN_H/2 * yres)/xres, SDL_WINDOW_FULLSCREEN);
    
    if ( *screen == NULL ) {
        return false;
    }

    //SDL_SetWindowMinimumSize(*screen, xres, yres);

    //*renderer = SDL_CreateRenderer(*screen, -1, SDL_RENDERER_ACCELERATED/*|SDL_RENDERER_PRESENTVSYNC*/);
    *renderer = SDL_CreateRenderer(*screen, 0, SDL_RENDERER_SOFTWARE);

    //*renderer = SDL_CreateRenderer(*screen, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    //*renderer = SDL_CreateRenderer(*screen, -1, SDL_RENDERER_SOFTWARE|SDL_RENDERER_PRESENTVSYNC);
    //*renderer = SDL_CreateRenderer(*screen, -1, SDL_RENDERER_SOFTWARE/*|SDL_RENDERER_PRESENTVSYNC*/);
    SDL_SetRenderDrawBlendMode(*renderer,SDL_BLENDMODE_BLEND);
    if(!(*renderer)) {
        fprintf(stderr, "lcd::Couldn't create a renderer: %s\nStarting without video output.\n", SDL_GetError());
        SDL_DestroyWindow(*screen);
        *screen = NULL;
        return false;
    }

    *texture = SDL_CreateTexture(*renderer, SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,xres,yres);
    if(!(*texture)) {
        fprintf(stderr, "lcd::Couldn't create a texture: %s\nStarting without video output.\n", SDL_GetError());
        SDL_DestroyRenderer(*renderer);
        *renderer = NULL;
        SDL_DestroyWindow(*screen);
        *screen = NULL;
        return false;
    }

    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_JoystickOpen(0);
    return true;
}

void destroy_sdl_screen() {

}
int clamp(int low, int val, int high) {
    if(val < low) val = low;
    if(val > high) val = high;
    return val;
}
std::string to_hex_string(uint32_t val, int length) { 
    ASSERT(length <= 8); 
    char buf[10]; 
    char fmt[10]; 
    std::snprintf(fmt,10,"%%0%dx",length); 
    std::snprintf(buf,10,fmt,val); 
    std::string out(buf);
    return out; 
}   

void setRGB(png_bytep r, uint8_t * i) {
    *r = ((i[0]<<6)|(i[1]<<4)|(i[2]<<2)|i[3]);
}

int output_png(std::string& filename, int width, int height, Vect<uint8_t>& image) {
    int code = 0;
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep row = NULL;

    // Open file for writing (binary mode)
    fp = fopen(filename.c_str(), "wb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file %s for writing\n", filename.c_str());
        code = 1;
        goto finalise;
    }

    // Initialize write structure
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fprintf(stderr, "Could not allocate write struct\n");
        code = 1;
        goto finalise;
    }

    // Initialize info structure
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fprintf(stderr, "Could not allocate info struct\n");
        code = 1;
        goto finalise;
    }

    // Setup Exception handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during png creation\n");
        code = 1;
        goto finalise;
    }

    png_init_io(png_ptr, fp);

    // Write header (8 bit colour depth)
    png_set_IHDR(png_ptr, info_ptr, width, height,
            2, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    // Allocate memory for one row (3 bytes per pixel - RGB)
    row = (png_bytep) malloc(width/4 * sizeof(png_byte));

    // Write image data
    int x, y;
    for (y=0 ; y<height ; y++) {
        for (x=0 ; x<width/4 ; x++) {
            setRGB(&(row[x]), &(image[y*width + x*4]));
        }
        png_write_row(png_ptr, row);
    }

    // End write
    png_write_end(png_ptr, NULL);

finalise:
    if (fp != NULL) fclose(fp);
    if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    if (row != NULL) free(row);

    return code;
}
}

