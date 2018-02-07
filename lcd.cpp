#include "lcd.h"
#include<iostream>
#include<cstring>
#include<cassert>
#include<fstream>
#include<string>

lcd::lcd() : cycle(144*114), next_line(0), control{.val=0x91}, bg_scroll_y(0), bg_scroll_x(0), 
             bgpal{{0,1,2,3}}, obj1pal{{0,1,2,3}}, obj2pal{{0,1,2,3}}, 
             win_scroll_y(0), win_scroll_x(0), active_cycle(0), frame(0), 
             lyc_next_cycle(0), m0_next_cycle(0), m1_next_cycle(0), m2_next_cycle(0), 
             cpu_control{.val=0x91}, cpu_status(0), cpu_bg_scroll_y(0), cpu_bg_scroll_x(0), cpu_lyc(0), cpu_dma_addr(0), 
             cpu_bgpal{{0,1,2,3}}, cpu_obj1pal{{0,1,2,3}}, cpu_obj2pal{{0,1,2,3}},
             cpu_win_scroll_y(0), cpu_win_scroll_x(0), cpu_active_cycle(0), 
             screen(NULL), renderer(NULL), buffer(NULL), texture(NULL), overlay(NULL), lps(NULL), hps(NULL), bg1(NULL), bg2(NULL) {
    vram.resize(0x2000);
    oam.resize(0xa0);
    cpu_vram.resize(0x2000);
    cpu_oam.resize(0xa0);

    /* Initialize the SDL library */
    screen = SDL_CreateWindow("KhedGB",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            320, 288,
            SDL_WINDOW_RESIZABLE);
    if ( screen == NULL ) {
        fprintf(stderr, "lcd::Couldn't set 320x240x8 video mode: %s\nStarting without video output.\n",
                SDL_GetError());
        //exit(1);
        return;
    }

    SDL_SetWindowMinimumSize(screen, 320, 288);
    cur_x_res=160;
    cur_y_res=144;

    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED/*|SDL_RENDERER_PRESENTVSYNC*/);
    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE|SDL_RENDERER_PRESENTVSYNC);
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE/*|SDL_RENDERER_PRESENTVSYNC*/);
    if(!renderer) {
        fprintf(stderr, "lcd::Couldn't create a renderer: %s\nStarting without video output.\n",
                SDL_GetError());
        SDL_DestroyWindow(screen);
        screen = NULL;
        //exit(1);
        return;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,160,144);
    if(!texture) {
        fprintf(stderr, "lcd::Couldn't create a texture: %s\nStarting without video output.\n",
                SDL_GetError());
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
        SDL_DestroyWindow(screen);
        screen = NULL;
        //exit(1);
        return;
    }

    buffer = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);

    overlay = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);
    lps = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);
    hps = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);

    bg1 = SDL_CreateRGBSurface(0,512,512,32,0,0,0,0);
    bg2 = SDL_CreateRGBSurface(0,512,512,32,0,0,0,0);

    if(buffer && bg1) {
        assert(buffer->pitch == 160*4);
        assert(bg1->pitch == 512 * 4);
    }

    //printf("lcd::Setting render draw color to black\n");
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    //printf("lcd::Clearing rendering output\n");
    SDL_RenderClear(renderer);
    //printf("lcd::Pushing video update to renderer\n");
    SDL_RenderPresent(renderer);
    //printf("lcd::constructor reached end\n");
}

uint64_t lcd::run(uint64_t run_to) {
    assert(cycle < run_to);
    uint64_t start_offset = cycle - active_cycle;              //Cycles since the screen was activated
    uint64_t start_frame_cycle = start_offset % 17556;         //Cycles into current frame
    uint64_t start_frame_base = cycle - start_frame_cycle;     //Cycle when current frame started
    uint64_t start_frame_line = start_frame_cycle / 114;       //Current line in current frame
    uint64_t start_line_cycle = start_frame_cycle % 114;       //Current cycle in current line
    uint64_t start_line = start_frame_line;

    printf("PPU: Start: %ld run_to: %ld\n", cycle, run_to);

    uint64_t render_cycle = 0;
    while(cmd_queue.size() > 0) {
        util::cmd current = cmd_queue.front();
        uint64_t offset = current.cycle - active_cycle - 1;       //Cycles between activation of the screen and that command
        uint64_t frame_cycle = (offset % 17556);          //Last cycle in this frame before command
        uint64_t frames_since_active = offset / 17556;        //Full frames since becoming active
        uint64_t frame_line = frame_cycle / 114;
        uint64_t line_cycle = frame_cycle % 114;

        uint64_t render_end_line = frame_line;
        if(get_mode(current.cycle, true) > 1) { //Haven't completed enough cycles to request frame_line to be rendered, as first calculated
            render_end_line--;                  //Shouldn't render the line that this command occurs on, yet
        }
        printf("PPU: Command at: %ld\n", current.cycle);
        //printf("Active: %ld offset: %ld frame_cycle: %ld frames_since_active: %ld frame_line: %ld line_cycle: %ld\n", active_cycle, offset, frame_cycle, frames_since_active, frame_line, line_cycle);

        bool frame_output = false;
        if(current.cycle >= cycle && render_end_line >= start_line) {
            printf("PPU: render Startline: %ld endline: %ld\n",start_line,frame_line);
            frame_output = render(frame, start_line, render_end_line);
        }
        start_line = ((render_end_line + 1) % 154); //Next line to render will be after current line, and next frame to render will be after current frame. If we crossed over, the next start_line needs to reflect the reset.

        if(frame_output) {
            printf("PPU: Frame %ld\n", frame);
            render_cycle = active_cycle + ((frames_since_active - 1) * 17556) + (114 * 143) + (20 + 43);
            frame++;
        }

        apply(current.addr, current.val, current.data_index, current.cycle);

        cmd_queue.pop_front();
        cycle = current.cycle;
        current = cmd_queue.front();
    }

    uint64_t end_offset = run_to - active_cycle - 1;
    uint64_t end_frame_cycle = end_offset % 17556;
    uint64_t end_frame_base = run_to - end_frame_cycle;
    uint64_t end_frame_line = end_frame_cycle / 114;
    uint64_t end_line_cycle = end_frame_cycle % 114;
    uint64_t end_line = end_frame_line;
    if(end_line_cycle < (20 + 43)) { //the line will be rendered at the next ::run call
        end_line--;
    }

    printf("PPU: render Startline: %ld endline: %ld\n",start_line,end_line);
    bool frame_output = render(frame, start_line, end_line);
    if(frame_output) {
        printf("PPU: Frame %ld\n", frame);
        render_cycle = active_cycle + (((end_offset / 17556) - 1) * 17556) + (114 * 143) + (20 + 43);
        assert(render_cycle < run_to);
        frame++;
    }

    for(size_t i=0;i<cmd_data.size();i++) {
        cmd_data[i].resize(0);
    }
    cmd_data.resize(0);
    cycle = run_to;
    return render_cycle; //0 means a frame hasn't been rendered during this timeslice
}

//Apply the data extracted from the command queue, and apply it to the PPU view of the state
void lcd::apply(int addr, uint8_t val, uint64_t index, uint64_t cycle) {
    if(addr >= 0x8000 && addr < 0xa000) {
        vram[addr & 0x1fff] = val;
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        oam[addr-0xfe00] = val;
    }
    else {
        switch(addr) {
            case 0xff40:
                {
                    control_reg old_val{.val = control.val};
                    control.val = val;
                    if(control.display_enable && !old_val.display_enable) {
                        active_cycle = cycle;
                    }
                }
                break;
            case 0xff42:
                bg_scroll_y = val;
                break;
            case 0xff43:
                bg_scroll_x = val;
                break;
            case 0xff46://OAM DMA
                assert(cmd_data.size() > index);
                assert(cmd_data[index].size() == 0xa0);
                memcpy(&oam[0],&cmd_data[index][0],0xa0);
                break;
            case 0xff47:
                bgpal.pal[0] = (val & 0x03);
                bgpal.pal[1] = (val & 0x0c)>>2;
                bgpal.pal[2] = (val & 0x30)>>4;
                bgpal.pal[3] = (val & 0xc0)>>6;
                break;
            case 0xff48:
                obj1pal.pal[0] = (val & 0x03);
                obj1pal.pal[1] = (val & 0x0c)>>2;
                obj1pal.pal[2] = (val & 0x30)>>4;
                obj1pal.pal[3] = (val & 0xc0)>>6;
                break;
            case 0xff49:
                obj2pal.pal[0] = (val & 0x03);
                obj2pal.pal[1] = (val & 0x0c)>>2;
                obj2pal.pal[2] = (val & 0x30)>>4;
                obj2pal.pal[3] = (val & 0xc0)>>6;
                break;
            case 0xff4a:
                win_scroll_y = val;
                break;
            case 0xff4b:
                win_scroll_x = val;
                break;
            default:
                //Various data aren't necessary to render the screen, so we ignore anything like interrupts and status
                break;
        }
    }
    return;
}

void lcd::write(int addr, void * val, int size, uint64_t cycle) {
    assert(size==1||(addr==0xff46&&size==0xa0));
    printf("PPU: 0x%04X = 0x%02x @ %ld (mode %d)\n", addr, *((uint8_t *)val), cycle, get_mode(cycle));
    if(addr >= 0x8000 && addr < 0xa000) {
        //if(get_mode(cycle) != 3) {
            memcpy(&(cpu_vram[addr-0x8000]), val, size);
            cmd_queue.push_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
        //}
        //else {
        //    printf("PPU: Cycle %010ld Denied write during mode   3: 0x%04x = 0x%02x\n", cycle, addr, *((uint8_t *)val));
        //}
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        //if(get_mode(cycle) < 2) {
            memcpy(&(cpu_oam[addr - 0xfe00]), val, size);
            cmd_queue.push_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
        //}
        //else {
        //    printf("PPU: Cycle %010ld Denied write during mode 2/3: 0x%04x = 0x%02x\n", cycle, addr, *((uint8_t *)val));
        //}
    }
    else {
        switch(addr) {
            case 0xff40:
                {
                    control_reg old_val{.val = cpu_control.val};
                    cpu_control.val = *((uint8_t *)val);
                    if(cpu_control.display_enable && !old_val.display_enable) {
                        cpu_active_cycle = cycle;
                        update_estimates(cycle);
                    }
                    else if(old_val.display_enable != cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                std::cout<<"PPU: CTRL change"<<
                    " Priority: "<<cpu_control.priority<<
                    " sprites on : "<<cpu_control.sprite_enable<<
                    " sprite size: "<<cpu_control.sprite_size<<
                    " bg map: "<<cpu_control.bg_map<<
                    " tile addr mode: "<<cpu_control.tile_addr_mode<<
                    " window enable: "<<cpu_control.window_enable<<
                    " window map: "<<cpu_control.window_map<<
                    " display on: "<<cpu_control.display_enable<<std::endl;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff41: 
                {
                    uint8_t old_val = cpu_status; 
                    cpu_status = *((uint8_t *)val)&0xf8; 
                    if(cpu_status != old_val && cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                printf("LCD status set to %02X\n", cpu_status);
                break;
            case 0xff42:
                cpu_bg_scroll_y = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff43:
                cpu_bg_scroll_x = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff45:
                {
                    uint8_t old_val = cpu_lyc;
                    cpu_lyc = *((uint8_t *)val);
                    if(cpu_lyc > 153) { //Out of range to actually trigger
                        lyc_next_cycle = -1;
                    }
                    else if(old_val != cpu_lyc && cpu_control.display_enable) {
                        update_estimates(cycle);
                    }
                }
                break;
            case 0xff46://OAM DMA
                {
                    memcpy(&cpu_oam[0],val,0xa0);
                    uint64_t index = cmd_data.size();
                    cmd_data.resize(index + 1);
                    cmd_data[index].resize(0xa0);
                    for(int i=0;i<0xa0;i++) {
                        cmd_data[index][i] = cpu_oam[i];
                    }
                    cpu_dma_addr = *((uint8_t *)val);
                    cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), index});
                }
                break;
            case 0xff47:
                cpu_bgpal.pal[0] = *((uint8_t *)val) & 0x03;
                cpu_bgpal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                cpu_bgpal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                cpu_bgpal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff48:
                cpu_obj1pal.pal[0] = *((uint8_t *)val) & 0x03;
                cpu_obj1pal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                cpu_obj1pal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                cpu_obj1pal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff49:
                cpu_obj2pal.pal[0] = *((uint8_t *)val) & 0x03;
                cpu_obj2pal.pal[1] = (*((uint8_t *)val) & 0x0c)>>2;
                cpu_obj2pal.pal[2] = (*((uint8_t *)val) & 0x30)>>4;
                cpu_obj2pal.pal[3] = (*((uint8_t *)val) & 0xc0)>>6;
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff4a:
                cpu_win_scroll_y = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            case 0xff4b:
                cpu_win_scroll_x = *((uint8_t *)val);
                cmd_queue.emplace_back(util::cmd{cycle, addr, *((uint8_t *)val), 0});
                break;
            default:
                std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
    }
    return;
}

uint8_t lcd::get_mode(uint64_t cycle, bool ppu_view/*=false*/) {
    control_reg cnt = cpu_control;
    uint64_t active = cpu_active_cycle;
    if(ppu_view) {
        cnt = control;
        active = active_cycle;
    }

    if(!cnt.display_enable) {
        return 1;
    }
    int frame_cycle = (cycle - active) % 17556;
    int line = frame_cycle / 114;

    assert(line < 154);
    assert(line >= 0);

    int mode = 0; //hblank; largest amount of time per frame. May as well use it.
    if(line > 143) {
        mode = 1; //vblank
    }
    else {
        //1. oam access around 20 cycles (mode 2)
        //2. transfer to lcd for around 43 cycles (mode 3)
        //3. h-blank around 51 cycles (mode 0)
        //4. repeat 144 times, then vblank for 1140 cycles (mode 1)
        int line_cycle = frame_cycle % 114;
        if(line_cycle < 20) mode = 2; //OAM access
        else if (line_cycle < 20+43) mode = 3; //LCD transfer
    }
    return mode;
}

uint64_t lcd::get_active_cycle() {
    return cpu_active_cycle;
}

//Reads the CPU's view of the current state of the PPU
void lcd::read(int addr, void * val, int size, uint64_t cycle) {
    if(size > 1) return;
    //assert(size==1);
    if(addr >= 0x8000 && addr < 0xa000) {
        if(get_mode(cycle) != 3) {
            memcpy(val, &(cpu_vram[addr-0x8000]), size);
        }
        else *((uint8_t *)val) = 0xff;
    }
    else if(addr >= 0xfe00 && addr < 0xfea0) {
        if(get_mode(cycle) < 2) {
            memcpy(val, &(cpu_oam[addr-0xfe00]), size);
        }
        else *((uint8_t *)val) = 0xff;
    }
    else {
        switch(addr) {
            case 0xff40:
                *((uint8_t *)val) = cpu_control.val;
                break;
            case 0xff41: 
                if(!cpu_control.display_enable) {
                    *((uint8_t *)val) = cpu_status|1; //return current interrupt flags and v-blank mode, if screen is disabled.
                }
                else {
                    int mode = get_mode(cycle);
                    if(cpu_lyc == ((cycle - cpu_active_cycle) % 17556) / 114) {
                        mode |= 4;
                    }
                    *((uint8_t *)val) = mode | cpu_status;// | 0x80;
                }
                break;
            case 0xff42:
                *((uint8_t *)val) = cpu_bg_scroll_y;
                break;
            case 0xff43:
                *((uint8_t *)val) = cpu_bg_scroll_x;
                break;
            case 0xff44:
                if(!cpu_control.display_enable) {
                    *((uint8_t *)val) = 0;
                }
                else {
                    int frame_cycle = (cycle - cpu_active_cycle) % 17556;
                    int line = frame_cycle / 114;
                    *((uint8_t *)val) = line;
                }
                break;
            case 0xff45:
                *((uint8_t *)val) = cpu_lyc;
                break;
            case 0xff46:
                *((uint8_t *)val) = cpu_dma_addr;
                break;
            case 0xff47:
                *((uint8_t *)val) = cpu_bgpal.pal[0] | cpu_bgpal.pal[1]<<2 | cpu_bgpal.pal[2]<<4 | cpu_bgpal.pal[3]<<6;
                break;
            case 0xff48:
                *((uint8_t *)val) = cpu_obj1pal.pal[0] | cpu_obj1pal.pal[1]<<2 | cpu_obj1pal.pal[2]<<4 | cpu_obj1pal.pal[3]<<6;
                break;
            case 0xff49:
                *((uint8_t *)val) = cpu_obj2pal.pal[0] | cpu_obj2pal.pal[1]<<2 | cpu_obj2pal.pal[2]<<4 | cpu_obj2pal.pal[3]<<6;
                break;
            case 0xff4a:
                *((uint8_t *)val) = cpu_win_scroll_y;
                break;
            case 0xff4b:
                *((uint8_t *)val) = cpu_win_scroll_x;
                break;
            default:
                std::cout<<"PPU: Read from video hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
                *((uint8_t *)val) = 0xff;
        }
    }
    return;
}

void lcd::get_tile_row(int tilenum, int row, bool reverse, std::vector<uint8_t>& pixels) {
    assert(tilenum < 384); assert(row < 16); assert(pixels.size() == 8);
    int addr = tilenum * 16 + row * 2;
    int b1 = vram[addr];
    int b2 = vram[addr + 1];
    for(int x = 0; x < 8; x++) {
        int x_i = x;
        if(reverse) x_i = 7 - x;
        int shift = 128>>(x_i);
        pixels[x] = ((b1&shift)/shift + 2*((b2&shift)/shift));
    }
    return;
}

bool lcd::render(int frame, int start_line/*=0*/, int end_line/*=143*/) {
    assert(start_line >= 0);
    if(end_line < 0) return false;
    if(start_line > end_line) {
        end_line += 154;
        printf("Changing %d to %d\n", end_line-154, end_line);
    }
    assert(end_line - start_line < 154); //Expect not to ever receive over a frame of data
    fflush(stdin);
    bool output_sdl = true;
    bool output_image = false;

    if(!screen||!texture||!renderer) {
        printf("PPU: problem!\n");
        output_sdl = false;
    }

    for(int line=start_line;line <= end_line; line++) {
        int render_line = line % 154;
        if(render_line > 143) continue;

        //Draw the background
        if(control.priority) { //cpu_controls whether the background displays, in regular DMG mode
            if(output_sdl && start_line == 0) { //Draw the background color
                SDL_SetRenderDrawColor(renderer, 85*(3 - bgpal.pal[0]), 85*(3 - bgpal.pal[0]), 85*(3 - bgpal.pal[0]), 255);
                SDL_RenderClear(renderer);
            }

            uint32_t bgbase = 0x1800;
            if(control.bg_map) bgbase = 0x1c00;

            int y_in_pix = (render_line + bg_scroll_y) & 0xff;
            int y_tile = y_in_pix / 8;
            int y_tile_pix = y_in_pix % 8;

            bool unloaded = true;
            int tile_num = 0;
            std::vector<uint8_t> tile_pix(8,0);

            for(int x_out_pix = 0; x_out_pix < 160; x_out_pix++) {
                int x_in_pix = (x_out_pix + bg_scroll_x) & 0xff;
                int x_tile = x_in_pix / 8;
                int x_tile_pix = x_in_pix % 8;

                if(x_tile_pix == 0 || unloaded) {
                    tile_num = vram[bgbase+y_tile*32+x_tile];
                    if(!control.tile_addr_mode) {
                        tile_num = 256 + int8_t(tile_num);
                    }
                    get_tile_row(tile_num, y_tile_pix, false, tile_pix);
                    unloaded = false;
                }

                if(output_sdl /*&& c != 0*/) {
                    uint32_t color = SDL_MapRGB(this->buffer->format,85*(3-bgpal.pal[tile_pix[x_tile_pix]]),85*(3-bgpal.pal[tile_pix[x_tile_pix]]),85*(3-bgpal.pal[tile_pix[x_tile_pix]]));
                    ((uint32_t *)this->buffer->pixels)[render_line*160+x_out_pix] = color;
                }
            }
        }

        //Draw the window
        if(control.window_enable) {
            uint32_t winbase = 0x1800;
            if(control.window_map) winbase = 0x1c00;
            std::vector<uint8_t> line(8,0);

            int win_y = (render_line - win_scroll_y);
            if(win_y >= 0) {
                int tile_y = win_y / 8;
                int y_tile_pix = win_y % 8;
                for(int tile_x = 0; tile_x * 8 + win_scroll_x - 7 < 160; tile_x++) {
                    int tile_num = vram[winbase+tile_y*32+tile_x];
                    if(!control.tile_addr_mode) {
                        tile_num = 256+int8_t(tile_num);
                    }
                    get_tile_row(tile_num, y_tile_pix, false, line);
                    for(int x_tile_pix = 0; x_tile_pix < 8 && x_tile_pix + win_scroll_x + tile_x * 8 - 7 < 160; x_tile_pix++) {
                        int ycoord = tile_y * 8 + y_tile_pix;
                        int xcoord = tile_x * 8 + x_tile_pix;

                        if(!line[x_tile_pix]) continue;
                        if(output_sdl) {
                            uint8_t col = line[x_tile_pix];
                            uint32_t color = SDL_MapRGB(this->buffer->format,85*(3-bgpal.pal[col]),50*(3-bgpal.pal[col]),50*(3-bgpal.pal[col]));
                            ((uint32_t *)this->buffer->pixels)[ycoord*160+xcoord] = color;
                        }
                    }
                }
            }
        }

    
        //Draw the sprites
        if(control.sprite_enable) {
            std::vector<uint8_t> tile_line(8,0);
            for(int spr = 0; spr < 40; spr++) {
                oam_data sprite_dat;
                memcpy(&sprite_dat, &oam[spr*4], 4);
                sprite_dat.ypos -= 16;
                sprite_dat.xpos -= 8;
                uint8_t tile = oam[spr*4+2];

                int sprite_size = 8 + (control.sprite_size * 8);

                //If sprite isn't displayed on this line...
                if(sprite_dat.ypos > render_line || sprite_dat.ypos + sprite_size <= render_line) {
                    continue;
                }

                if(control.sprite_size) {
                    tile &= 0xfe;
                }

                int y_i = render_line - sprite_dat.ypos;
                if(sprite_dat.yflip == 1) {
                    y_i = sprite_size - (y_i + 1);
                }
                get_tile_row(tile, y_i, false, tile_line);

                for(int x=0;x!=8;x++) {
                    int x_i = x;
                    if(sprite_dat.xflip == 1) {
                        x_i = 7 - x;
                    }

                    uint8_t col = tile_line[x_i];
                    uint32_t color = 0;
                    if(!col) continue;
                    if(sprite_dat.palnum_dmg) {
                        color = SDL_MapRGB(this->buffer->format, 50 * ( 3-obj2pal.pal[col]), 50 * ( 3-obj2pal.pal[col]), 85 * ( 3-obj2pal.pal[col]));
                    }
                    else {
                        color = SDL_MapRGB(this->buffer->format, 50 * ( 3-obj1pal.pal[col]), 85 * ( 3-obj1pal.pal[col]), 50 * ( 3-obj1pal.pal[col]));
                    }

                    int xcoord = sprite_dat.xpos+x;
                    int ycoord = render_line;
                    ((uint32_t *)this->buffer->pixels)[ycoord*160+xcoord] = color;
                }
            }

        }

        if(output_sdl && render_line == 143) {
            if(texture) {
                //SDL_DestroyTexture(texture);
            }

            texture = SDL_CreateTextureFromSurface(renderer, this->buffer);

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer,texture,NULL,NULL);

            SDL_RenderPresent(renderer);
            output_image = true;
        }
    }

    return output_image;
}

void lcd::update_estimates(uint64_t cycle) {
    if(!cpu_control.display_enable) { //realistically shouldn't ever end up here
        lyc_next_cycle = -1;
        m0_next_cycle  = -1;
        m1_next_cycle  = -1;
        m2_next_cycle  = -1;
        return;
    }
    
    uint64_t frame_cycle = (cycle - cpu_active_cycle) % 17556;
    uint64_t line = frame_cycle / 114;
    uint64_t line_cycle = frame_cycle % 114;
    uint64_t frame_base_cycle = cycle - frame_cycle;

    //uint64_t mode = get_mode(cycle);

    if((cpu_status & LYC) > 0) {
        lyc_next_cycle = frame_base_cycle + cpu_lyc * 114;
        if(lyc_next_cycle <= cycle) { //We've passed it this frame; go to the next.
            lyc_next_cycle += 17556;
        }
    }
    else {
        lyc_next_cycle = -1;
    }

    if((cpu_status & M0) > 0) {
        m0_next_cycle = frame_base_cycle + (114 * line) + 63;
        if(line_cycle >= 20 + 43) {
            m0_next_cycle += 114;
        }
    }
    else {
        m0_next_cycle = -1;
    }

    if((cpu_status & M1) > 0) {
        m1_next_cycle = frame_base_cycle + (144 * 114);
        if(m1_next_cycle <= cycle) {
            m1_next_cycle += 17556;
        }
    }
    else {
        m1_next_cycle = -1;
    }

    if((cpu_status & M2) > 0) {
        m2_next_cycle = frame_base_cycle + (114 * (line + 1));
    }
    else {
        m2_next_cycle = -1;
    }

}

bool lcd::interrupt_triggered(uint64_t cycle) {
    if(!cpu_control.display_enable) return false;

    bool retval = false;

    if(lyc_next_cycle <= cycle) retval = true;
    if(m0_next_cycle <= cycle) retval = true;
    if(m1_next_cycle <= cycle) retval = true;
    if(m2_next_cycle <= cycle) retval = true;

    if(retval) {
        update_estimates(cycle);
    }

    return retval;
}

void lcd::dump_tiles() {
    std::ofstream vid(std::string("frame-dead.pgm").c_str());
    vid<<"P5\n192 128\n3\n";
    uint8_t buffer[128][192];
    for(int ytile=0;ytile<16;ytile++) {
        for(int xtile=0;xtile<24;xtile++) {
            int base = (xtile *16 + ytile)*16;
            for(int yp=0;yp<8;yp++) {
                int b1=vram[base+yp*2];
                int b2=vram[base+yp*2+1];
                int shift=128;
                for(int xp=0;xp<8;xp++) {
                    int c=(b1 & shift)/shift + 2*((b2 & shift)/shift);
                    assert(c==0||c==1||c==2||c==3);
                    buffer[ytile*8+yp][xtile*8+xp]=3-c;
                    shift/=2;
                }
            }
        }
    }
    for(int i=0;i<128;i++) {
        vid.write(reinterpret_cast<char *>(&buffer[i][0]),192);
    }
    vid.close();
}
