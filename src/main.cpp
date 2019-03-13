#include <mbed.h>
#include "max7219.h"

Serial pc(USBTX, USBRX);
// Use of an external library https://os.mbed.com/teams/Maxim-Integrated/code/MAX7219/
Max7219 max7219(PTD2, NC, PTD1, PTD0);
// Pattern To be display that will be appended to pattern actual
// one more char than needed to allow a smooth transition of slider
char pattern_actual[17] = {};
char pattern_number[2][10][8] = {{{0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x81, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x40, 0x20}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xF1, 0x91, 0x9F}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x99, 0x81}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x10, 0xF0}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x9F, 0x91, 0xF1}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x9F, 0x91, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x80, 0x80}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x99, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x91, 0xF1}}, {{0xFF, 0x81, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x40, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xF1, 0x91, 0x9F, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x99, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x9F, 0x91, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x9F, 0x91, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x99, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x91, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x00}}};
char pattern_heart_deprecated[8] = {0x04, 0x02, 0x01, 0xFF, 0x80, 0x40, 0x20, 0x10};
char pattern_heart[6] = {0x38, 0xC0, 0x38, 0x06, 0x01, 0x06};
char pattern_flat[8] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
//Var related to the sensor
Ticker timer;
Timer frequency;
AnalogIn Ain(PTB1);
int waveform[20] = {};
int bpm = 0;
int bpmList[3] = {};
int index12;
int timeOut;
int prev_average;
int next_average;

float heartbeat_time;

bool increase = true;
bool pattern_detected = false;
bool decrease = false;
bool state = false;

InterruptIn button(PTD5);
DigitalOut led(PTD4);

//------------------------------------------------------------------------------------------------
// Send Data to the display
void pattern_to_display()
{
    // For each of the hex code in patternactual 
    for (int idx = 0; idx < sizeof(pattern_actual); idx++)
    {
        // print the first 8 to the first screen 
        if (idx < 8)
        {
            max7219.write_digit(2, idx + 1, pattern_actual[idx]);
        }
        //and the rest to the other one
        else
        {
            max7219.write_digit(1, (idx - 8) + 1, pattern_actual[idx]);
        }
    }
}

//--------------------------------------------------------------------------------------------
//Display a signal
//update the screen by displaying the element on its right for all element 
void slider()
{
    for (int u = 0; u < 16; u++)
    {
        pattern_actual[u] = pattern_actual[u + 1];
    }
}

//Print a flat line
void print_flat(int size)
{
    for (int i = 0; i < size; i++)
    {
        //only update the index 16 to not mess up the sliding since that element is never displayed
        pattern_actual[16] = pattern_flat[0];
        slider();
        pattern_to_display();
        //update the screen based on the speed of bpm 
        wait_ms((100 * 60) / bpm);
    }
}
// print the actual signal
void print_signal()
{
    for (int i = 0; i < sizeof(pattern_heart); i++)
    {
        //only update the index 16 to not mess up the sliding since that element is never displayed
        pattern_actual[16] = pattern_heart[i];
        slider();
        pattern_to_display();
        //update the screen based on the speed of bpm
        wait_ms((100 * 60) / bpm);
    }
    // fill in some flat line in between to make it more readable
    print_flat(4);
}

//----------------------------------------------------------------------------------------
//Number Processing
// update the display based on which screen what should be displayed
// Sum the decimal and digit pattern into one
void sum_num(int screen, char decimal[], char digit[])
{
    if (screen == 1)
    {
        for (int i = 0; i < 8; i++)
        {
            pattern_actual[i] = decimal[i] + digit[i];
        }
    }
    else if (screen == 2)
    {
        // add and index of 8 to print the number
        // Since we do not want to slide the number we are allowed to write to all the index of pattern actual
        for (int i = 0; i < 8; i++)
        {
            pattern_actual[i + 8] = decimal[i] + digit[i];
        }
    }
}
// decide which pattern to use based on the number
// break the number (less than 100) into the decimal digit and unit digit
// only deal with number less than 100 since other function handle which screen which number should go
void print_decimal(int screen, int number)
{
    if (number < 10)
    {
        sum_num(screen, pattern_number[0][0], pattern_number[1][number]);
    }
    else if (number < 100)
    {
        int digit = number % 10;
        int decimal = (number - digit) / 10;
        sum_num(screen, pattern_number[0][decimal], pattern_number[1][digit]);
    }
}
// Main Function to call if there is a need to print a number
// Only print 0 on the second screen if the number is mess
void print_number(int number)
{
    if (number < 100) 
    {
        print_decimal(1, number);
        print_decimal(2, 0); //only one screen use so print 0 on the second screen
    }
    else
    {
        int digit = number % 100 % 10;
        int decimal = (number - digit) % 100 / 10;
        int centimal = (number - decimal) / 100;
        // Will not handle Thousand but no one will ever have a heartbeat in thousand so no need to handle
        print_decimal(2, centimal);
        // No need to call print decimal since the math was already done
        // Although the function flow should probably be rewritten if there is to be further development
        sum_num(1, pattern_number[0][decimal], pattern_number[1][digit]); 
    }
    //update the display
    pattern_to_display();
}

//----------------------------------------------------------------------------------------------
//Handler for the Heartbeat

// averages out the signal if there is ever a need to 
// takes the curent value-number and the current value and go down the history until n is reached
// then average number value and set the previous and next value as the result.
void averaged(int number)
{
    prev_average = 0;
    next_average = 0;
    //Averages previous value if needed base on number;
    int value = 0;
    int counter = number;
    while (value < number)
    {
        //circular loop using modulo
        int tmp = waveform[(index12 - counter) % 20];
        value++;
        prev_average += tmp;
        counter++;
    }
    prev_average /= number;

    //Averages the current value if needed base on number;
    value = 0;
    counter = 0;
    while (value < number)
    {
        int tmp = waveform[(index12 - counter) % 20];
        value++;
        next_average += tmp;
        counter++;
    }
    next_average /= number;
}

// Check for a pattern( a rising edge follow by a falling edge and nothing else)
void timing()
{
    //no need for average in this case so only 1
    averaged(1);
    // rising edge
    if (next_average > prev_average && decrease)
    {
        // set the timer base on the time since the falling edge
        heartbeat_time = frequency.read();
        frequency.stop();
        frequency.reset();
        //bool to handle the display and the edges switch
        pattern_detected = true;
        decrease = false;
        increase = true;
    }
    else if (next_average < prev_average && increase)
    {
        //start a timer
        decrease = true;
        increase = false;
        frequency.start();
    }
    else
    {
        pattern_detected = false;
    }
}
// Do the calculation for the heartbeat if a pattern was detected
// if no pattern was detected for some time reset the BPM so a flat line can be displayed
void bpmHandler(int counter)
{
    if (pattern_detected)
    {
        // circular list of only size 2 to then average the bpm to remove some garbage reading
        bpmList[index12 % 2] = 60 / (heartbeat_time) / 2;
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
        // Alternate the Led every tme a pattern is detected
        led = !led;
        //Makes sure to not divides by 0!!!
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
        // reset the Led and the bpm counter
        if (counter >= 10)
        {
            led = 0;
            bpm = 0;
        }
    }
}

// Interupt function
// Store the recorded value as a 3.30 voltage with no floating point to make calculation faster in a circular list of 20 value 
void flip()
{
    index12++;
    float tmp = Ain.read();
    waveform[index12 % 20] = tmp * 330;
    // start a timeout when no pattern is detected
    if (!pattern_detected)
    {
        timeOut++;
    }
    else
    {
        // reset the timeout
        timeOut = 0;
    }

    bpmHandler(timeOut);
    timing();
}

// button fuunction to change the state 
// called when the button is in a low state after debouncing
void function_change()
{
    state = !state;
    pc.printf("State : ");
    pc.printf("%s\n", state ? "true" : "false");
}

int main()
{
    // Set up the 2 screen 
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
    // attach the recorder in an interupt called every .1 seconds
    timer.attach(&flip, 0.1);
    pc.printf("Program Initialized");
    while (1)
    {
        // record the changing of the button
        button.fall(&function_change);
        // switch depending on the state of the button
        if (state)
        {
            // if there is a beat print a pattern otherwise just print a flatline
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
            // Print the heartbea as a number
            print_number(bpm);
        }
    }
}