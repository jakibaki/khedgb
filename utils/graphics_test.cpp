#include<cstdio>
#include "../lcd.h"
#include<vector>
#include<iostream>
#include<fstream>

uint8_t tiles[] {
0xc6, 0xc6, 0xaa, 0xaa, 0xaa, 0xaa, 0x92, 0x92, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x00, 0x00, //M
0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x48, 0x48, 0x48, 0x48, 0x4c, 0x4c, 0x34, 0x34, 0x00, 0x00, //a
0x00, 0x00, 0x00, 0x00, 0x5c, 0x5c, 0x62, 0x62, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, //r
0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x48, 0x48, 0x70, 0x70, 0x48, 0x48, 0x48, 0x48, 0x00, 0x00, //k
0x10, 0x10, 0x10, 0x10, 0x7c, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x18, 0x18, 0x00, 0x00, //t
0x00, 0x00, 0x00, 0x00, 0x38, 0x38, 0x44, 0x44, 0x7c, 0x7c, 0x40, 0x40, 0x38, 0x38, 0x00, 0x00, //e
0x00, 0x00, 0x00, 0x00, 0x38, 0x38, 0x44, 0x44, 0x30, 0x30, 0x80, 0x80, 0x78, 0x78, 0x00, 0x00, //s
0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x3c, 0x3c, 0x44, 0x44, 0x4c, 0x4c, 0x34, 0x34, 0x00, 0x00, //d
0x03, 0x03, 0x0f, 0x0c, 0x1f, 0x10, 0x3f, 0x20, 0x7f, 0x40, 0x7f, 0x4c, 0xff, 0x8c, 0xff, 0x80, //smiley1
0xff, 0x80, 0xff, 0x80, 0x7f, 0x48, 0x7f, 0x44, 0x3f, 0x23, 0x1f, 0x10, 0xff, 0x0c, 0x03, 0x03, //smiley2
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //space
};
 
uint8_t bg[] {0, 1, 2, 3, 6, 0xa, 4, 5, 6, 4, 0xa, 7, 1, 4, 1}; //= marks test data

uint8_t lcdc = 0x97;

uint8_t oam[] {0x20, 0x20, 0x08, 0x00,
               0x20, 0x28, 0x08, 0x20};

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    lcd ppu;

    uint8_t status=0; //ff41
    uint8_t ly=0;     //ff44
    uint8_t lyc=12;   //ff45

    if(argc == 1) {
        for(int i=0;i<11*16;i++) {
            ppu.write(0x8000+i, &(tiles[i]), 1, 0);
        }
        for(int i=0;i<15;i++) {
            ppu.write(0x9800+i, &(bg[i]), 1, 0);
        }
        for(int i=0;i<8;i++) {
            ppu.write(0xfe00+i, &(oam[i]), 1, 0);
        }
        ppu.write(0xff45, &lyc, 1, 0);
        ppu.write(0xff40, &lcdc, 1, 0);
        for(int i=0; i<114*154; i++) {
            ppu.read(0xff41, &status, 1, i);
            ppu.read(0xff44, &ly, 1, i);
            uint64_t frame = ppu.get_frame();
            uint8_t mode = ppu.get_mode(i);
            uint8_t stat_mode = status&0x03;
            uint8_t lyc_flag = (status&4)>>2;
            printf("Cycle: %d (Frame: %d, line: %d, line_cycle: %d) ly(%d)==12: %d mode %d (stat_mode: %d)\n", i, frame, (i%17556)/114, i%114, ly, lyc_flag, mode, stat_mode);
            ppu.run(i+1);
        }
        SDL_Delay(10000);
        SDL_Quit();
    }

    else {
        lcdc = 0x81;
        uint8_t bgx = 16;
        ppu.write(0xff40, &lcdc, 1, 0);

        ppu.write(0xff42, &bgx, 1, 0);
        ppu.write(0xff43, &bgx, 1, 0);
        std::ifstream in(argv[1]);
        std::vector<uint8_t> vram(0x2000,0);
        if(in.is_open()) {
            in.read(reinterpret_cast<char *>(&vram[0]), 0x2000);
        }
        for(int i=0;i<0x2000;i++) {
            ppu.write(0x8000+i, &vram[i], 1, 1);
        }
        for(int i=1;i<154;i++) ppu.run(114*i);
        SDL_Delay(10000);
        SDL_Quit();
    }
    return 0;
}
