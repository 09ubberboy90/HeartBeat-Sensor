#include <mbed.h>
#include "max7219.h"

Serial pc(USBTX, USBRX);

Max7219 max7219(PTD2, NC, PTD1, PTD0);
char pattern_actual[17] = {};
char pattern_number[2][10][8] = {{{0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x81, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x9F, 0x91, 0xF1}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x99, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x10, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xF1, 0x91, 0x9F}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x91, 0x9F}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x99, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xF1, 0x91, 0xFF}}, {{0xFF, 0x81, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x20, 0x40, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x9F, 0x91, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x81, 0x99, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xF0, 0x10, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xF1, 0x91, 0x9F, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x91, 0x9F, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x80, 0x80, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x99, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xF1, 0x91, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}}};
char pattern_heart_deprecated[8] = {0x04, 0x02, 0x01, 0xFF, 0x80, 0x40, 0x20, 0x10};
char pattern_heart[6] = {0x38, 0xC0, 0x38, 0x06, 0x01, 0x06};
char pattern_flat[8] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
int waveform[20] = {};
int bpm = 0;
int bpmList[3] = {}; 
Ticker timer;
AnalogIn Ain(PTB1);
DigitalOut led(PTD4);
int index12;
int timeOut;
Timer frequency;
bool increase = true;
bool pattern_detected = false;
bool decrease = false;
bool state = false;
float heartbeat_time;
InterruptIn button(PTD5);

//------------------------------------------------------------------------------------------------
void pattern_to_display()
{
    for (int idx = 0; idx < sizeof(pattern_actual); idx++)
    {
        if (idx < 8)
        {
            max7219.write_digit(2, idx + 1, pattern_actual[idx]);
        }
        else
        {
            max7219.write_digit(1, (idx - 8) + 1, pattern_actual[idx]);
        }
    }
}

//--------------------------------------------------------------------------------------------
//Display a signal
void slider()
{
    for (int u = 0; u < 16; u++)
    {
        pattern_actual[u] = pattern_actual[u + 1];
    }
}
/*
Print the heartbeat_time pattern
TODO compress all 3 function into one with parameter
*/
void print_flat(int size)
{
    for (int i = 0; i < size; i++)
    {
        pattern_actual[16] = pattern_flat[0];
        slider();
        pattern_to_display();
        wait_ms((100 * 60) / bpm);
    }
}

void print_signal()
{
    for (int i = 0; i < sizeof(pattern_heart); i++)
    {
        pattern_actual[16] = pattern_heart[i];
        slider();
        //TODO : maybe add a queue to remove the wait command
        pattern_to_display();
        wait_ms((100 * 60) / bpm);
    }
    print_flat(4);
}

//----------------------------------------------------------------------------------------
//Number Processing
void sum_num(int screen, char decimal[], char digit[])
{
    if (screen == 1)
    {
        for (int i = 0; i < 8; i++)
        {
            pattern_actual[i + 8] = decimal[i] + digit[i];
        }
    }
    else if (screen == 2)
    {

        for (int i = 0; i < 8; i++)
        {
            pattern_actual[i] = decimal[i] + digit[i];
        }
    }
}

void print_decimal(int screen, int number)
{
    if (number < 10)
    {
        sum_num(screen, pattern_number[1][0], pattern_number[0][number]);
    }
    else if (number < 100)
    {
        int digit = number % 10;
        int decimal = (number - digit) / 10;
        sum_num(screen, pattern_number[1][decimal], pattern_number[0][digit]);
    }
}

void print_number(int number)
{
    if (number < 100) //only one screen use so print 0 on the second screen
    {
        print_decimal(1, number);
        print_decimal(2, 0);
    }
    else
    {
        int digit = number % 100 % 10;
        int decimal = (number - digit) % 100 / 10;
        int centimal = (number - decimal) / 100;
        print_decimal(2, centimal);
        sum_num(1, pattern_number[1][decimal], pattern_number[0][digit]); //Print on second screen
    }
    pattern_to_display();
}
//----------------------------------------------------------------------------------------------


void timing()
{
    const int n = 1;
    int prev_average = 0;
    int next_average = 0;
    //Averages previous value if needed base on n;
    int value = 0;
    int counter = 2;
    while(value<n){
        int tmp = waveform[(index12 - counter) % 20];
        if (true) ///tmp !=0
        {
            value++;
            prev_average+= tmp;
        }
        counter++;
    }
    prev_average /= n;
    value = 0;
    counter = 0;
    while (value < n)
    {
        int tmp = waveform[(index12 - counter) % 20];
        if (true) //tmp !=0
        {
            value++;
            next_average += tmp;
        }
        counter++;
    }
    next_average/=n;

    //Check for a pattern
    if (next_average > prev_average && decrease)
    {
        heartbeat_time = frequency.read();
        frequency.stop();
        frequency.reset();
        pattern_detected = true;
        decrease = false;
        increase = true;
    }
    else if (next_average < prev_average && increase)
    {
        decrease = true;
        increase = false;
        frequency.start();
    }
    else
    {
        pattern_detected = false;
    }
    

}
void bpmHandler(int counter)
{
    if (pattern_detected)
    {
        pc.printf("Pulse : %f\n", 2 / heartbeat_time);
        bpmList[index12 % 2] = 60 / (heartbeat_time) / 2;
        // Averages the Bpm
        int sumBpm = 0;
        int value = 0;
        for (int i = 0; i < 2; i++)
        {
            if (bpmList[i] != 0)
            {
                sumBpm += bpmList[i];
                value++;
            }
        }
        led = !led;
        if (value != 0)
        {
            bpm = sumBpm / value;
        }
        else
        {
            bpm = 0;
        }
        pc.printf("BPM : %d\n", bpm);
    }
    else
    {
        if (counter >= 8) {
            led=0;
            bpm=0;
        }
    }
}
void flip()
{
    index12++;
    float tmp = Ain.read();
    waveform[index12%20] = tmp *330;
    if (!pattern_detected) {
        timeOut++;
    }
    else {
        timeOut=0;
    }
    
    bpmHandler(timeOut);
    timing();
    
}
void function_change()
{
    state = !state;
    pc.printf("State : ");
    pc.printf("%s\n", state ? "true" : "false");
}
int main()
{
    max7219.set_num_devices(2);
    max7219_configuration_t cfg_1 = {
        .device_number = 1,
        .decode_mode = 0,
        .intensity = Max7219::MAX7219_INTENSITY_8,
        .scan_limit = Max7219::MAX7219_SCAN_8};
    max7219_configuration_t cfg_2 = {
        .device_number = 2,
        .decode_mode = 0,
        .intensity = Max7219::MAX7219_INTENSITY_8,
        .scan_limit = Max7219::MAX7219_SCAN_8};

    max7219.init_device(cfg_1);
    max7219.init_device(cfg_2);
    max7219.enable_display();
    timer.attach(&flip, 0.1);
    pc.printf("Program Initialized");
    button.mode(PullUp);
    wait(.01);
    while (1)
    {
        // Use that to test button need to actually set the right pin since i dont know which on it's attached to
        button.fall(&function_change);
        if (state)
        {
            if (bpm != 0)
            {
                print_signal();
            }
            else
            {
                print_flat(5);
            }
        }
        else
        {
            print_number(bpm);
        }

        // USe that to print a counter of number

        // Use that to print the actual signal
    }

}