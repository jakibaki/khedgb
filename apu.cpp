#include "apu.h"
#include<cstdio>

const uint8_t apu::or_values[] = {0x80, 0x3f, 0x00, 0xff, 0xbf, //0x10,0x11,0x12,0x13,0x14
                                  0xff, 0x3f, 0x00, 0xff, 0xbf, //0x15,0x16,0x17,0x18,0x19
                                  0x7f, 0xff, 0x9f, 0xff, 0xbf, //0x1a,0x1b,0x1c,0x1d,0x1e,
                                  0xff, 0xff, 0x00, 0x00, 0xbf, //0x1f,0x20,0x21,0x22,0x23,
                                  0x00, 0x00, 0x70, 0xff, 0xff, //0x24,0x25,0x26,0x27,0x28
                                  0xff, 0xff, 0xff, 0xff, 0xff, //0x29,0x2a,0x2b,0x2c,0x2d
                                  0xff, 0xff,                   //0x2e,0x2f
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //0x30-0x37
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //0x38-0x3f

const uint8_t apu::square_wave[4][8] = {{0,0,0,0,0,0,0,1},
                                        {1,0,0,0,0,0,0,1},
                                        {1,0,0,0,0,1,1,1},
                                        {0,1,1,1,1,1,1,0}};
const uint8_t apu::noise_divisors[] = {8, 16, 32, 48, 64, 80, 96, 112};

void apu::clear() {
    for(int i=0; i<0x30;i++) {
        written_values[i] = 0;
    }
}

void apu::init() {
    sequencer_phase = 7; //so that next step is 0
    chan1_duty_phase = 0;
    chan2_duty_phase = 0;
    chan3_duty_phase = 0;
}

apu::apu() : writes_enabled(false), cycle(0), devid(0), cur_chunk(0), audio_open(false)
{
    clear();
}

apu::~apu() {

}

void apu::write(uint16_t addr, uint8_t val, uint64_t cycle) {
    //printf("addr: %04x val: %02x ", addr, val);
    if(writes_enabled || addr >= 0xff30) {
        written_values[addr - 0xff10] = val;
        cmd_queue.emplace_back(util::cmd{cycle, addr, val, 0});
    }
    else if(addr == 0xff26) {
        written_values[addr - 0xff10] = (val & 0x80);
        if(!(val&0x80)) {
            clear();
            writes_enabled = false;
        }
        else {
            init();
            writes_enabled = true;
        }
        cmd_queue.emplace_back(util::cmd{cycle, addr, val, 0});
    }
}

uint8_t apu::read(uint16_t addr, uint64_t cycle) {
    if(addr == 0xff26) return (written_values[addr - 0xff10] & 0x80);
    return written_values[addr - 0xff10] | or_values[addr - 0xff10];
    //TODO: Calculate status for NR52
    //TODO: Block reads from wave RAM when it's in use (or, rather, return current wave value)
    //TODO: Block reads from most registers when power is off
}

//Gets called at 64Hz, every 16384 1MHz-cycles
void apu::run(uint64_t run_to) {
    //printf("apu:: %ld->%ld\n", cycle, run_to);

    //Generate audio between cycle and cmd.cycle
    util::cmd cur_cmd;
    if(cmd_queue.size() > 0) {
        cur_cmd = cmd_queue.front();
    }
    int cur_sample = 0;

    std::array<uint8_t, 690*2> out_buffer;
    int sample_count = 689;
    if(cur_chunk == 15) {
        sample_count++;
    }
    for(;cycle < run_to; cycle++) {

        //Clock the frame sequencer at 512Hz
        if(cycle%2048 == 0) {
            clock_sequencer();
        }

        //Clock the frequency counters for each channel
        clock_freqs();

        //If it's time, apply the current command, and grab the next one, if it exists
        if(cmd_queue.size() > 0 && cycle == cur_cmd.cycle) {
            apply(cur_cmd);
            cmd_queue.pop_front();
            if(cmd_queue.size() > 0) {
                cur_cmd = cmd_queue.front();
            }
        }

        //Figure out if it's time to generate a new sample, and put it into the output buffer if it is
        int sample = int(float(sample_count) * (float(cycle & 0x3fff) / float(run_to & 0x3fff)));
        if(sample > cur_sample) {
            apu::samples s;
            render(s);
            out_buffer[cur_sample*2] = s.l;
            out_buffer[cur_sample*2+1] = s.r;
            cur_sample++;
        }
    }

    //Push the sample to the output device.
    if(audio_open) {
        //SDL_QueueAudio(devid, out_buffer.data(), sample_count * CHANNELS * SAMPLE_SIZE);
    }

    //Every 16 chunks, we need to generate 690 samples, instead of 689.
    cur_chunk++;
    cur_chunk &= 0x0f; //mod by 16
}

void apu::render(apu::samples& s) {
    //printf("APU Render cycle %ld to %ld\n", cycle, render_to);
    return;
}

bool apu::sweep_check() {
    return false;
}

//Advance the frame sequencer. Clocks at 512Hz, with 7 phases.
// Phase 0: Clock Length
// Phase 1:
// Phase 2: Clock Length and Sweep
// Phase 3:
// Phase 4: Clock Length
// Phase 5:
// Phase 6: Clock Length and Sweep
// Phase 7: Clock Volume
void apu::clock_sequencer() {
    sequencer_phase++;
    sequencer_phase %= 8;
    if(sequencer_phase == 0 || sequencer_phase == 2 || sequencer_phase == 4 || sequencer_phase == 6) {
        //clock length for all the channels
    }
    if((sequencer_phase == 2 || sequencer_phase == 6) && chan1_sweep_active) {
        //clock sweep for channel 1 (only one that supports it)

    }
    if(sequencer_phase == 7) {
        //clock volume for 1, 2, and 4? I think chan3 has level, but not envelope.
    }
}

void apu::clock_freqs() {
    if(chan1_active) {
        chan1_freq_counter--;
        if(!chan1_freq_counter) {
            chan1_freq_counter = chan1_freq_shadow;
            chan1_duty_phase++;
            chan1_duty_phase %= square_wave_length;
        }
    }
    if(chan2_active) {
        chan2_freq_counter--;
        if(!chan2_freq_counter) {
            chan2_freq_counter = chan2_freq.freq;
            chan2_duty_phase++;
            chan2_duty_phase %= square_wave_length;
        }
    }
    if(chan3_active) {
        chan3_freq_counter--;
        if(!chan3_freq_counter) {
            chan3_freq_counter = chan3_freq.freq;
            chan3_duty_phase++;
            chan3_duty_phase %= wave_length;
        }
    }
    if(chan4_active) {
        chan4_freq_counter--;
        if(!chan4_freq_counter) {
            chan4_freq_counter = noise_divisors[chan4_freq.div_ratio]<<(chan4_freq.shift_clock);
            uint16_t next = (chan4_lfsr & BIT0) ^ (chan4_lfsr & BIT1);
            chan4_lfsr>>=1;
            chan4_lfsr |= (next<<14);
            if(chan4_freq.noise_type) {
                chan4_lfsr |= (next<<6);
            }
            lfsr_value = ((next)?0:1);
        }
    }
}

void apu::apply(util::cmd& c) {
    switch(c.addr) {
        //sound 1: rectangle with sweep + envelope
        case 0xff10: //sound 1 sweep
            chan1_sweep.val = c.val;
            //printf("apu: S1 sweep time: %d increase: %d shift: %d\n", (c.val&0x70)>>4, (c.val&8)>>3, (c.val&7));
            break;
        case 0xff11: //sound 1 length + duty cycle
            chan1_patlen.val = c.val;
            //printf("apu: S1 duty cycle: %d length: %d\n", c.val>>6, (c.val&0x3f));
            break;
        case 0xff12: //sound 1 envelope
            chan1_env.val = c.val;
            //printf("apu: S1 default envelope: %d up/down: %d step length: %d\n", c.val>>4, (c.val&8)>>3, (c.val&7));
            break;
        case 0xff13: //sound 1 low-order frequency
            chan1_freq.freq_low = c.val;
            //printf("apu: S1 freq-low: %d\n", c.val);
            break;
        case 0xff14: //sound 1 high-order frequency + start
            chan1_freq.freq_high = c.val;
            if(chan1_freq.initial) {
                chan1_freq_shadow = chan1_freq.freq;
                chan1_sweep_counter = 0;
                chan1_active = true;
                if(chan1_sweep.shift != 0 && !sweep_check()) {
                    chan1_active = false;
                }
                /*
                - freq copied to sweep shadow register
                - sweep timer is reloaded (timer count restarts from 0)
                - sweep's internal enabled flag is set if sweep period or shift are non-zero, cleared otherwise
                - if sweep shift is non-zero, freq calc and overflow check happen immediately
                  - freq calc performed as previously noted, using the value in the shadow register
                  - if resulting value > 2047, channel is disabled.
                - when sweep is clocked, enabled flag is on, sweep period is non-zero:
                *     - if overflow check passes, the new freq is written to the shadow reg, and NR13+NR14
                *     - overflow check is run AGAIN with the new value (and can disable the channel), but value isn't written back
                *   - if NR13+NR14 are changed during operation, the channel will switch back to the old freq when data is copied from shadow, on next sweep
                */
            }

            //printf("apu: S1 start: %d freq-high: %d\n", c.val>>7, (0x03&c.val));
            break;

        //sound 2: rectangle with envelope
        case 0xff16: //sound 2 length + duty cycle
            chan2_patlen.val = c.val;
            //printf("apu: S2 duty cycle: %d length: %d\n", c.val>>6, (c.val&0x3f));
            break;
        case 0xff17: //sound 2 envelope
            chan2_env.val = c.val;
            //printf("apu: S2 default envelope: %d up/down: %d step length: %d\n", c.val>>4, (c.val&8)>>3, (c.val&7));
            break;
        case 0xff18: //sound 2 low-order frequency
            chan2_freq.freq_low = c.val;
            //printf("apu: S2 freq-low: %d\n", c.val);
            break;
        case 0xff19: //sound 2 high-order frequency + start
            chan2_freq.freq_high = c.val;
            if(chan2_freq.initial) {
                chan2_freq_shadow = chan1_freq.freq;
                chan2_active = true;
            }
            //printf("apu: S2 start: %d freq-high: %d\n", c.val>>7, (0x03&c.val));
            break;

        //sound 3: arbitrary waveform
        case 0xff1a: //sound 3 start/stop
            //printf("apu: S3 enable: %d\n", c.val>>7);
            break;
        case 0xff1b: //sound 3 length
            //printf("apu: S3 length: %d\n", c.val);
            break;
        case 0xff1c: //sound 3 output level
            //printf("apu: S3 level: %d\n", (c.val&0x60)>>5);
            break;
        case 0xff1d: //sound 3 low-order frequency
            //printf("apu: S3 freq-low: %d\n", c.val);
            break;
        case 0xff1e: //sound 3 high-order frequency + start
            //printf("apu: S3 start: %d freq-high: %d\n", c.val>>7, (0x03&c.val));
            break;

        //sound 4: noise
        case 0xff20: //sound 4 length
            //printf("apu: S4 length: %d\n", c.val&0x3f);
            break;
        case 0xff21: //sound 4 envelope
            //printf("apu: S4 default envelope: %d up/down: %d step length: %d\n", c.val>>4, (c.val&0x08)>>3, (c.val&7));
            break;
        case 0xff22: //sound 4 frequency settings
            //printf("apu: S4 shift freq: %d counter steps: %d freq division ratio: %d\n", c.val>>4, (c.val&0x08)>>3, (c.val&7));
            break;
        case 0xff23: //sound 4 start
            //printf("apu: S4 start: %d counter/continuous: %d\n", c.val>>7, (c.val&0x40)>>6);
            break;

        //audio control
        case 0xff24: //V-in output levels+channels
            //printf("apu: Vin->SO2: %d, S02 level: %d, Vin->SO1: %d, S01 level: %d\n", c.val>>7, (c.val&0x70)>>4, (c.val&0x08)>>3, (c.val&0x07));
            break;
        case 0xff25: //Left+right channel selections
            //printf("apu: S1->SO2: %d S2->S02: %d S3->S02: %d S4->S02: %d S1->SO1: %d S2->S01: %d S3->S01: %d S4->S01: %d\n", c.val>>7, (c.val&0x40)>>6, (c.val&0x20)>>5, (c.val&0x10)>>4,
                                                                                                                             //(c.val&0x0f)>>3, (c.val&0x04)>>2, (c.val&0x02)>>1, (c.val&0x01));
            break;
        case 0xff26: //Sound activation
            if(!c.val>>7) {
                clear();
                writes_enabled = false;
            }
            else {
                writes_enabled = true;
            }
            //printf("apu: master: %d S4-on: %d S3-on: %d S2-on: %d S1-on: %d\n", c.val>>7, (c.val&0x08)>>3, (c.val&0x04)>>2, (c.val&0x02)>>1, (c.val&0x01));
            break;

        default:
            if(c.addr >= 0xff30 && c.addr < 0xff40) {
                //printf("apu: wave[%d] = %d, wave[%d] = %d\n", (addr-0xff30)*2, (c.val&0xf0)>>4, (addr-0xff30)*2+1, c.val&0x0f);
                //waveform RAM
            }
            else {
                //printf("apu: unknown, addr=0x%04x, c.val=0x%02x\n", addr, c.val);
            }
    }
}
