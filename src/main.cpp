#include "mbed.h"
/*
example of driving maxim chip for Glasgow Uni Projects.

Dr J.J.Trinder 2013,14
jon.trinder@glasgow.ac.uk

 
*/

#define max7219_reg_noop 0x00
#define max7219_reg_digit0 0x01
#define max7219_reg_digit1 0x02
#define max7219_reg_digit2 0x03
#define max7219_reg_digit3 0x04
#define max7219_reg_digit4 0x05
#define max7219_reg_digit5 0x06
#define max7219_reg_digit6 0x07
#define max7219_reg_digit7 0x08
#define max7219_reg_decodeMode 0x09
#define max7219_reg_intensity 0x0a
#define max7219_reg_scanLimit 0x0b
#define max7219_reg_shutdown 0x0c
#define max7219_reg_displayTest 0x0f

#define LOW 0
#define HIGH 1

#define NUMBER_OF_SCREENS 1

Serial pc(USBTX, USBRX);

SPI max72_spi(PTD2, NC, PTD1);
DigitalOut load(PTD0); //will provide the load signal
DigitalOut toto(LED1);
AnalogIn ain(A0);

Ticker printer;
Ticker data;

bool button = false;

char pattern_actual[8 * NUMBER_OF_SCREENS+1] = {};
char pattern_queue[16] = {};
char pattern_diagonal[8] = {0x01, 0x2, 0x4, 0x08, 0x10, 0x20, 0x40, 0x80};
char pattern_square[8] = {0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff};
char pattern_star[8] = {0x04, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0x04, 0x00};
char pattern_letter_a[8] = {0x84, 0xC6, 0xA5, 0x84, 0x84, 0x84, 0x84, 0x84};
char pattern_dead_man[8] = {0xa5, 0x42, 0xa5, 0x00, 0x18, 0x24, 0x42, 0x00};
char pattern_all_on[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
char pattern_heart[8] = {0x04, 0x02, 0x01, 0xFF, 0x80, 0x40, 0x20, 0x10};

char pattern_paolo[8] = {0x18, 0x24, 0x3C, 0x24, 0x24, 0xE7, 0x81, 0xFF}; // Paolo create the most beautiful design made in a 8*8 display

//{0x18, 0x14, 0x12, 0x11, 0x10, 0x90, 0x50, 0x30};
char pattern_flat[8] = {0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08};

char pattern_number[2][10][8] = {{{0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x81, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x9F, 0x91, 0xF1}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x99, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x10, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xF1, 0x91, 0x9F}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x91, 0x9F}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x99, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x00, 0xF1, 0x91, 0xFF}}, {{0xFF, 0x81, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x20, 0x40, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x9F, 0x91, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x81, 0x99, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xF0, 0x10, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xF1, 0x91, 0x9F, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x91, 0x9F, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x80, 0x80, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xFF, 0x99, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}, {0xF1, 0x91, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}}};
/*
Write to the maxim via SPI
args register and the column data
*/
void write_to_max(int reg, int col)
{
    load = LOW;           // begin
    max72_spi.write(reg); // specify register
    max72_spi.write(col); // put data
    load = HIGH;          // make sure data is loaded (on rising edge of LOAD/CS)
}

void clear()
{
    for (int e = 1; e <= 8 * NUMBER_OF_SCREENS; e++)
    { // empty registers, turn all LEDs off
        write_to_max(e, 0);
    }
}

//writes 8 bytes to the display
void pattern_to_display()
{
    int cdata;
    for (int idx = 0; idx < 8 * NUMBER_OF_SCREENS; idx++)
    {
        cdata = pattern_actual[idx];
        write_to_max(idx + 1, cdata);
    }
}

void setup_dot_matrix()
{
    // initiation of the max 7219
    // SPI setup: 8 bits, mode 0
    max72_spi.format(8, 0);

    max72_spi.frequency(100000); //down to 100khx easier to scope ;-)

    write_to_max(max7219_reg_scanLimit, 0x07);
    write_to_max(max7219_reg_decodeMode, 0x00);  // using an led matrix (not digits)
    write_to_max(max7219_reg_shutdown, 0x01);    // not in shutdown mode
    write_to_max(max7219_reg_displayTest, 0x00); // no display test
    clear();                                     // clear the screen
    // maxAll(max7219_reg_intensity, 0x0f & 0x0f);    // the first 0x0f is the value you can set
    write_to_max(max7219_reg_intensity, 0x08);
}

void sum_num(char decimal[], char digit[])
{
    for (int i = 0; i < 8; i++)
    {
        pattern_actual[i] = decimal[i] + digit[i];
    }
}
void slider()
{
    for (int u = 0; u < 8; u++)
    {
        pattern_actual[u] = pattern_actual[u + 1];
    }
}
/*
Print the heartbeat pattern
TODO compress all 3 function into one with parameter
*/
void print_flat()
{
    for (int i = 0; i < 8; i++)
    {
        pattern_actual[8] = pattern_flat[i];
        slider();
        pattern_to_display();
        wait_ms(200);
    }
}
void print_signal()
{
    for (int i = 0; i < 8; i++)
    {
        pattern_actual[8] = pattern_heart[i];
        slider();
        //TODO : maybe add a queue to remove the wait command
        pattern_to_display();
        wait_ms(200);
    }
    print_flat();
}


/*
Print a number between 0-99
*/
void print_number(int number)
{
    if (number < 10)
    {
        sum_num(pattern_number[1][0], pattern_number[0][number]);
    }
    else if (number < 100)
    {
        int digit = number % 10;
        int decimal = (number - digit) / 10;
        sum_num(pattern_number[1][decimal], pattern_number[0][digit]);
    }
    else
    {
        //Implement For second screen.
    }
    pattern_to_display();
}

void test()
{
    for (int i = 0; i < 100; i++)
    {
        print_number(i);
        wait_ms(200);
    }
}

void data_taken()
{
    pc.printf("0x%04X \n", ain.read_u16());
}
int main()
{
    setup_dot_matrix(); /* setup matric */
    while (1)
    {
        toto = 1;
        print_signal();
        test();
        clear();
    }
}