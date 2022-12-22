#include "pico/stdlib.h"

#include <stdio.h>
#include <iostream>
#include <etl/string.h>

#define UART1_TX_PIN 4
#define UART1_RX_PIN 5
#define BAUD_RATE    4800
#define DATA_BITS    8
#define STOP_BITS    1
#define PARITY       UART_PARITY_NONE

#define MAX_LENGTH 82

char ch;
etl::string<MAX_LENGTH> sentence;
double direction, speed;

bool is_valid(etl::istring &sentence)
{
    // Use sentence chars to calculate checksum and compare to XOR'd value.
    int start_checksum = sentence.find("*");
    char checksum_chars[2];
    checksum_chars[0] = sentence[start_checksum+1]; 
    checksum_chars[1] = sentence[start_checksum+2];
    char hex_digits[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    int checksum_int = 0;
    int power = 0;
    for (int i = 1; i >= 0; i--) {
        for (int j = 0; j < 16; j++) {
            if (checksum_chars[i] == hex_digits[j]) {
                checksum_int += j*std::pow(16, power);
            }
        }
        power++;
    }
    
    // Compute checksum from sentence by XOR'ing byte-by-byte.
    int checksum_xor = 0;
    // Ignore the $ sign and stop XOR'ing at the *.
    for (int i = 1; i < sentence.size(); i++) {
        if (sentence[i] == 42){
            break;
        }
        checksum_xor ^= sentence[i];
    }

    // If valid return true, else return false.
    if (checksum_int == checksum_xor) {
        return true;
    } else {
        return false;
    }
}

void parse_sentence(etl::istring &sentence, double &direction, double &speed) {
    char *p;
    char direction_str[6];
    char speed_str[6];

    int start_direction = sentence.find("V,")+2;
    for (int i=0; i<=6; i++) {
        if (sentence[start_direction+i]==44) {
            break;
        }
        direction_str[i] = sentence[start_direction+i];
    }
    direction = std::round(10*strtod(direction_str, &p))/10.0;

    int start_speed = sentence.find("R,")+2;
    for (int i=0; i<=6; i++) {
        if (sentence[start_speed+i]==44) {
            break;
        }
        speed_str[i] = sentence[start_speed+i];
    }
    speed = std::round(10*strtod(speed_str, &p))/10.0;
    
}

int main() {
    stdio_init_all();

    // UART setup.
    uart_init(uart1, BAUD_RATE);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(uart1, DATA_BITS, STOP_BITS, PARITY);

    while (true) {
        while (uart_is_readable(uart1)) {
            ch = uart_getc(uart1);
            if (ch == 36) {
                if (is_valid(sentence)) {
                    parse_sentence(sentence, direction, speed);
                    printf("Direction: %.1f degrees; Speed: %.1f knots\n", direction, speed);
                }
                sentence.clear();
                sentence += ch;
            }
            else {
                sentence += ch;
            }
        } 
    }
}