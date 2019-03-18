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
int value[40] = {};
int bpm = 0;
int bpmList[10] = {};
int index12;
int timeOut;
int prev_average;
int next_average;

int maxVal = 0;
int minVal = INT_MAX;
int middle;

float heartbeat_time;

bool increase = true;
bool pattern_detected = false;
bool decrease = false;
bool state = false;
bool stable = false;

InterruptIn button(PTD5);
DigitalOut led(PTD4);

//------------------------------------------------------------------------------------------------
// Send Data to the display
void pattern_to_display()
{
    // For each of the hex code in patternactual 
    for (int idx = 0; idx < sizeof(pattern_actual)-1; idx++)
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


// time the time between every time the wave goes above the threshold(cuttof)
// the cuttof is 85% of the range * 1000 to ignore small changes in heartbeat
void timing(int range)
{
    float cutoff = range * 8500;
    // get the value as a difference to the median *10000 to once again ignore small changes
    float recorded = (middle - value[index12 % 40]) * 10000;
    // value is above the threshold
    if (recorded >= cutoff)
    {
        heartbeat_time = frequency.read();
        // only detect it once in case the wave stay above the throshod for more than one cycle.
        // if it does the timing bewteen the 2 would be really small and hence can be discarded
        // only calculate if the difference is bigger that 10^-2
        if (heartbeat_time * 100 != 0)
        {
            // turn on the led when its above and off when below
            led = 1;
            frequency.stop();
            frequency.reset();
            pattern_detected = true;
            pc.printf("HeartBeatTime : %f\n", heartbeat_time);
        }
        else
        {
            pattern_detected = false;
        }
    }
    else
    {
        pattern_detected = false;
        frequency.start();
        led = 0;
    }
}
// Do the calculation for the heartbeat if a pattern was detected
// if no pattern was detected for some time reset the BPM so a flat line can be displayed
void bpmHandler(int counter)
{
    if (pattern_detected)
    {
        // circular list of only size 2 to then average the bpm to remove some garbage reading
        int tmpBpm = 60 / (heartbeat_time) / 2;
        if (40<tmpBpm && tmpBpm<200) {
            bpmList[index12 % 10] = tmpBpm;
        }
        
        pc.printf("bpm : %d\n", bpmList[index12 % 10]);

        int sumBpm = 0;
        int value = 0;
        for (int i = 0; i < 10; i++)
        {
            if (bpmList[i] != 0)
            {
                sumBpm += bpmList[i];
                value++;
            }
        }
        // Alternate the Led every tme a pattern is detected
        led = !led;

        // Makes sure to not divides by 0!!!
        if (value != 0)
        {
            bpm = sumBpm / value;
        }
        else
        {
            bpm = 0;
        }
        if (bpm>=200) {
            bpm = 0;
        }
        
        pc.printf("BPM : %d\n", bpm);
    }
    else
    {   
        // reset the Led and the bpm counter
        if (counter >= 25)
        {
            led = 0;
            bpm = 0;
        }
    }
}

// Interupt function
// Store the recorded value as a 3.30 voltage with no floating point(hence the *1000) to make calculation faster in a circular list of 40 value 
void flip()
{
    index12++;
    value[index12 % 40] = Ain.read() * 33000;
    //Discard value that are too high or too low
    if (10000 < value[index12 % 40] && value[index12 % 40] < 20000)
    {
        // loop through the list to find the min and max value
        minVal = INT_MAX;
        maxVal = 0;
        for (int i = 0; i < 40; i++)
        {
            int tmp = value[i];
            if (tmp < minVal)
            {
                minVal = tmp;
            }
            else if (tmp > maxVal)
            {
                maxVal = tmp;
            }
        }
        //find the middle and the range
        middle = (maxVal + minVal) / 2;
        int range = (maxVal - minVal) / 2;
        // ignore small changes in value
        if (range > 20) {
            timing(range);
        }
        if (!pattern_detected)
        {
            //start/ increase the timeout
            timeOut++;
        }
        else
        {
            // reset the timeout
            timeOut = 0;
        }
        bpmHandler(timeOut);

    }
    else
    {
        // reset everything
        led = 0;
        bpm = 0;
        minVal = INT_MAX;
        maxVal = 0;
        pattern_detected = false;
    }
    
    
   
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
    timer.attach(&flip, 0.02);
    pc.printf("Program Initialized");
    button.fall(&function_change);

    while (1)
    {
        // record the changing of the button
        // switch depending on the state of the button
        if (state)
        {
            // if there is a beat print a pattern otherwise just print a flatline
            // Also dont print beat otther 200 we assume its never going to happen
            if (bpm != 0 && bpm<200)
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