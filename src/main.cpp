#include<iostream>
#include<cstdint>
#include<string>
#include<fstream>
#include "memmap.h"
#include "cpu.h"
#include "lcd.h"
#include "util.h"
#include <dirent.h>
#include <switch.h>

void usage(char ** argv);


int getInd(char* curFile, int curIndex) {
    DIR* dir;
    struct dirent* ent;


    if(curIndex < 0)
        curIndex = 0;
    
    dir = opendir("/switch/roms/gbc");//Open current-working-directory.
    if(dir==NULL)
    {
        sprintf(curFile, "Failed to open dir!");
        return curIndex;
    }
    else
    {
        int i;
        for(i = 0; i <= curIndex; i++) {
            ent = readdir(dir);
        }
        if(ent)
            sprintf(curFile ,"/switch/roms/gbc/%s", ent->d_name);
        else
            curIndex--;
        closedir(dir);
    }
    return curIndex;
}

void getFile(char* curFile)
{
 
    gfxInitDefault();

    //Initialize console. Using NULL as the second argument tells the console library to use the internal console structure as current one.
    consoleInit(NULL);

    //Move the cursor to row 16 and column 20 and then prints "Hello World!"
    //To move the cursor you have to print "\x1b[r;cH", where r and c are respectively
    //the row and column where you want your cursor to move
    printf("\x1b[16;20HSelect a file using the up and down keys.");
    printf("\x1b[17;20HPress start to run the rom.");

    sprintf(curFile, "Couldn't find any files in that folder!");
    int curIndex = 0;

    curIndex = getInd(curFile, curIndex);
    printf("\x1b[18;20H%s", curFile);
    while(appletMainLoop())
    {
        //Scan all the inputs. This should be done once for each frame
        hidScanInput();

        //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_DOWN || kDown & KEY_DDOWN) {
            consoleClear();
            printf("\x1b[16;20HSelect a file using the up and down keys.");
            printf("\x1b[17;20HPress start to run the rom.");
            curIndex++;
            curIndex = getInd(curFile, curIndex);
            printf("\x1b[18;20H%s", curFile);
        }

        if (kDown & KEY_UP || kDown & KEY_DUP) {
            consoleClear();
            printf("\x1b[16;20HSelect a file using the up and down keys.");
            printf("\x1b[17;20HPress start to run the rom.");
            curIndex--;
            curIndex = getInd(curFile, curIndex);
            printf("\x1b[18;20H%s", curFile);
        }


        if (kDown & KEY_PLUS || kDown & KEY_A) {
            
            break;
        }  
        gfxFlushBuffers();
        gfxSwapBuffers();
        gfxWaitForVsync();
    }
    
    
    consoleClear();
    
    gfxExit();
}


int main(int argc, char *argv[]) {
    bool sgb = false;
    bool cgb = true;
    bool cpu_trace = false;
    bool headless = false;
    bool audio = true;
    char filename[100];
    getFile(filename);
    std::string romfile = filename;

    std::string fwfile  = "";
    Vect<std::string> args;

    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_AUDIO);

    //consoleInit(NULL);
    
    consoleDebugInit(debugDevice_NULL);

    stdout = stderr;


    if(romfile == "") {
        usage(argv);
        return 1;
    }

    //std::cout<<"Passing in rom file name of "<<romfile<<" and fw file name of "<<fwfile<<std::endl;


    romfsInit();


    memmap bus(romfile, fwfile);
    if(!bus.valid) {
        return 1;
    }

    if(sgb) {
        bool active = bus.set_sgb(true);
        if(active) {
            printf("Activating Super GameBoy mode\n");
        }
        else {
            printf("Cartridge reports that SGB mode is not supported. Disabling.\n");
        }
    }
    else {
        bus.set_sgb(false);
        if(bus.supports_sgb()) {
            printf("Super GameBoy mode wasn't selected, but the game supports it (--sgb to activate it)\n");
        }
    }

    if(cgb) {
        cgb = bus.set_color(); //This will fail, if a DMG/SGB firmware was provided
        //fprintf(stderr, "Sorry to get your hopes up, color is completely unsupported at this time.\n");
    }
    else {
        if(bus.needs_color()) {
            cgb = true;
            bus.set_color();
        }
    }

    if(cgb && sgb) {
        printf("SGB and GBC/CGB art mutually exclusive.\n");
        return 1;
    }

    cpu    proc(&bus, cgb, bus.has_firmware());
    lcd *  ppu = bus.get_lcd();

    if(cpu_trace) {
        proc.toggle_trace();
        ppu->toggle_trace();
    }

    uint64_t cycle = 0; //Cycles since the machine started running (the system starts in vblank)
    uint64_t tick_size = 70224/16; //Cycles to run in a batch
    bool continue_running = true;

    uint64_t cycle_offset = 0;
    uint64_t start = SDL_GetTicks();
    
    while(continue_running && appletMainLoop()) {

        //printf("Running CPU and PPU until cycle %ld (%ld for CPU)\n", cycle+tick_size, 2*(cycle+tick_size));
        continue_running = util::process_events(&proc, &bus);
        uint64_t count = proc.run(cycle + tick_size);
        if(!count) {
            continue_running = false;
            SDL_Quit();
            break;
        }

        //cur_output_cycle represents which cycle the ppu decided to render a frame at
        uint64_t cur_output_cycle = 0;
        if(!headless && continue_running) {
            cur_output_cycle = ppu->run(cycle + tick_size);
            //printf("Main got report of output at: %lld\n", cur_output_cycle);
        }

        //Frame delay
        if(cur_output_cycle != 0) {
            uint64_t now = SDL_GetTicks();
            uint64_t time_elapsed = now - start;
            uint64_t simulated_time = (1000 * (cycle + tick_size - cycle_offset)) / (1024 * 1024);
            if(time_elapsed < simulated_time) {
                if(simulated_time - time_elapsed < 17) {
                    SDL_Delay(simulated_time - time_elapsed);
                }
                else {
                    start = SDL_GetTicks();
                    cycle_offset = cycle + tick_size;
                }
            }
            else if(time_elapsed > simulated_time + 1000) {
                //printf("Seeing consistent slowdown.\n");
            }
        }
        cycle += tick_size;
    }
    return 0;
}

void usage(char ** argv) {
    printf("Usage: %s [options] romfile\n\n", argv[0]);
    printf("Options:\n");
    printf("\t--sgb: Set Super Gameboy mode, if possible.\n");
    printf("\t--cgb: Set Color Gameboy mode, if possible.\n");
    printf("\t--trace: Activate CPU instruction trace\n");
    printf("\t--headless: Headless mode (graphics disabled)\n");
    printf("\t--nosound: NoSound mode (audio disabled)\n");
    printf("\t--fw fw_filename: Boot using specified bootstrap firmware (expect 256 byte files for SGB and DMG, and 2,304 byte file for CGB)\n");
}
