#include<cstdint>
#include "memmap.h"
#include<iostream>
#include<fstream>
#include<cstring>

const uint8_t memmap::dmg_firmware[256] = {
    0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb, 0x21, 0x26, 0xff, 0x0e,
    0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3, 0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0,  
    0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,  
    0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x00, 0x06, 0x08, 0x1a, 0x13, 0x22, 0x23, 0x05, 0x20, 0xf9,  
    0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99, 0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20,  
    0xf9, 0x2e, 0x0f, 0x18, 0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,  
    0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0x0d, 0x20, 0xf7, 0x1d, 0x20, 0xf2,  
    0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62, 0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06,  
    0x7b, 0xe2, 0x0c, 0x3e, 0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,  
    0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04, 0xc5, 0xcb, 0x11, 0x17, 0xc1, 0xcb, 0x11, 0x17,  
    0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9, 0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b,  
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,  
    0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc,  
    0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c,  
    0x21, 0x04, 0x01, 0x11, 0xa8, 0x00, 0x1a, 0x13, 0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,  
    0xf5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xfb, 0x86, 0x20, 0xfe, 0x3e, 0x01, 0xe0, 0x50 }; 
                            

memmap::memmap(const std::string& rom_filename) : use_dmg(true), screen(), wram(0x2000), hram(0x7f) {
    std::ifstream in(rom_filename.c_str());
    if(in.is_open()) {
        size_t size = 0;
        in.seekg(0, std::ios::end);
        size = in.tellg();
        in.seekg(0, std::ios::beg);
        std::cout<<"Opened "<<rom_filename<<", found a file of "<<size<<" bytes."<<std::endl;
        rom.resize(size);
        in.read(reinterpret_cast<char *>(&(rom[0])), size);
        in.close();
    }
    else {
        std::cerr<<"Couldn;t open "<<rom_filename<<"."<<std::endl;
        rom.resize(0x8000);
    }
}

void memmap::read(int addr, void * val, int size, int cycle) {
    //std::cout<<"Cycle "<<std::dec<<cycle<<": ";
    if(addr >= 0 && addr < 0x8000) {
        if(use_dmg && addr < 0x100) {
/*          std::cout<<"Trying to copy "<<std::dec<<size<<" bytes from dmg_firmware["<<std::hex<<addr<<"] {";
            for(int i=0;i<size;i++) {
                std::cout<<int(dmg_firmware[addr+i])<<", ";
            }
            std::cout<<"}"<<std::endl;
*/
            memcpy(val, &(dmg_firmware[addr]), size);
        }
        else {
            /*
            std::cout<<"Trying to copy "<<std::dec<<size<<" bytes from rom["<<std::hex<<addr<<"] {";
            for(int i=0;i<size;i++) {
                std::cout<<int(rom[addr+i])<<", ";
            }
            std::cout<<"}"<<std::endl;
            */
            memcpy(val, &(rom[addr]), size);
        }
    }
    else if (addr >= 0x8000 && addr < 0xa000) {
        screen.read(addr, val, size, cycle);//memcpy(val, &(vram[addr-0x8000]), size);
    }
    else if (addr >= 0xa000 && addr < 0xc000) {
        std::cout<<"Read from external RAM: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
    }
    else if (addr >= 0xc000 && addr < 0xe000) {
        memcpy(val, &(wram[addr-0xc000]), size);
    }
    else if (addr >= 0xe000 && addr < 0xfe00) {
        std::cerr<<"Read from forbidden zone! 0x"<<std::hex<<addr<<" bzzzzzzt"<<std::endl;
    }
    else if (addr >= 0xfe00 && addr < 0xff00) {
        memcpy(val, &(oam[addr - 0xfe00]), size);
    }
    else if (addr >= 0xff00 && addr < 0xff80) {
        switch(addr) {
        case 0xff44:
            *((uint8_t *)val) = (cycle / 114);
            break;
        default:
            if(addr < 0xff03) {
                std::cout<<"Read from gamepad/link cable: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr > 0xff03 && addr < 0xff08) {
                std::cout<<"Read from timer hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr == 0xff0f) {
                std::cout<<"Read from interrupt hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr > 0xff0f && addr < 0xff3f) {
                std::cout<<"Read from audio hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr > 0xff3f && addr < 0xff4c) {
                screen.read(addr, val, size, cycle);//memcpy(val, &(vram[addr-0x8000]), size);
                //std::cout<<"Read from video hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else if(addr > 0xff4e && addr < 0xff6c) {
                std::cout<<"Read from CGB DMA/RAM: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
            else {
                std::cout<<"Read from unknown mem-mapped hardware: 0x"<<std::hex<<addr<<" (not implemented yet)"<<std::endl;
            }
        }
    }
    else if (addr >= 0xff80 && addr < 0xffff) {
        memcpy(val, &(hram[addr - 0xff80]), size);
    }
    else if (addr == 0xffff) {
        std::cerr<<"Attempted read of write-only register?"<<std::endl;
    }
    else {
        std::cerr<<"Water fog? Trying to read from 0x"<<std::hex<<addr<<" bzzzzzzt"<<std::endl;
    }

}

void memmap::write(int addr, void * val, int size, int cycle) {
    //std::cout<<"Cycle "<<std::dec<<cycle<<": ";
    if(addr >= 0 && addr < 0x8000) {
        std::cout<<"Write to ROM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (mappers not implemented yet)"<<std::endl;
    }
    else if (addr >= 0x8000 && addr < 0xa000) {
        screen.write(addr, val, size, cycle);
        //memcpy(&(vram[addr-0x8000]), val, size);
    }
    else if (addr >= 0xa000 && addr < 0xc000) {
        std::cout<<"Write to external RAM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
    }
    else if (addr >= 0xc000 && addr < 0xe000) {
        memcpy(&(wram[addr-0xc000]), val, size);
    }
    else if (addr >= 0xe000 && addr < 0xfe00) {
        std::cerr<<"Wrote to forbidden zone! 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" bzzzzzzt"<<std::endl;
    }
    else if (addr >= 0xfe00 && addr < 0xff00) {
        memcpy(&(oam[addr - 0xfe00]), val, size);
    }
    else if (addr >= 0xff00 && addr < 0xff80) {
        if(addr < 0xff03) {
            std::cout<<"Write to gamepad/link cable: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr > 0xff03 && addr < 0xff08) {
            std::cout<<"Write to timer hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr == 0xff0f) {
            std::cout<<"Write to interrupt hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr > 0xff0f && addr < 0xff3f) {
            std::cout<<"Write to audio hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr > 0xff3f && addr < 0xff4c) {
            screen.write(addr, val, size, cycle);
            //std::cout<<"Write to video hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
        else if(addr > 0xff4e && addr < 0xff6c) {
            if(addr == 0xff50) {
                use_dmg = false;
            }
            else {
                std::cout<<"Write to CGB DMA/RAM: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
            }
        }
        else {
            std::cout<<"Write to unknown mem-mapped hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
        }
    }
    else if (addr >= 0xff80 && addr < 0xffff) {
        memcpy(&(hram[addr - 0xff80]), val, size);
    }
    else if (addr == 0xffff) {
        std::cout<<"Write to interrupt hardware: 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" (not implemented yet)"<<std::endl;
    }
    else {
        std::cerr<<"Water fog? Write to 0x"<<std::hex<<addr<<" = 0x"<<int(*((uint8_t *)val))<<" bzzzzzzt"<<std::endl;
    }
}

void memmap::render(int frame) {
    screen.render(frame);
}
/*
0x0000-0x3FFF: Permanently-mapped ROM bank.
0x4000-0x7FFF: Area for switchable ROM banks.
0x8000-0x9FFF: Video RAM.
0xA000-0xBFFF: Area for switchable external RAM banks.
0xC000-0xCFFF: Game Boy’s working RAM bank 0 .
0xD000-0xDFFF: Game Boy’s working RAM bank 1.
0xFE00-0xFEFF: Sprite Attribute Table.
0xFF00-0xFF7F: Devices’ Mappings. Used to access I/O devices.
0xFF80-0xFFFE: High RAM Area.
0xFFFF: Interrupt Enable Register.
*/
