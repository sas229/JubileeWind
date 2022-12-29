#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/flash.h"
#include "json.hpp"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include <stdio.h>
#include <iostream>

#define UART1_TX_PIN 4
#define UART1_RX_PIN 5
#define BAUD_RATE    4800
#define DATA_BITS    8
#define STOP_BITS    1
#define PARITY       UART_PARITY_NONE

#define MAX_LENGTH   82
#define START        36

#define FLASH_TARGET_OFFSET (256*1024)

// Device setup.
uint8_t direction_offset = 180;
uint8_t* flash_target_contents = (uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);

// UDP settings.
const char hostname[] = "JubileeWind";
char server_ip[] = "192.168.1.100";
const int server_port = 4123;
const int buffer_size = 300;
const char ssid[] = "Jubilee";
const char pass[] = "mc%4Mv2b*AwxAx";
const double pi = 2*std::acos(0.0);

// Data objects.
struct udp_pcb* upcb;
struct udp_pcb* spcb;
char buffer[buffer_size];
char ch;
std::string sentence;
double direction, speed;

using json = nlohmann::json;

void set_direction_offset(uint8_t new_direction_offset) {
    printf("New direction offset: %i\n", new_direction_offset);
    direction_offset = new_direction_offset;
}

void send_udp(char* IP , int port, void* data, int data_size) {
    ip_addr_t destAddr;
    ip4addr_aton(IP, &destAddr);
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, data_size, PBUF_RAM);
    memcpy(p->payload, data, data_size);
    cyw43_arch_lwip_begin();
    udp_sendto(upcb, p, &destAddr, port);
    cyw43_arch_lwip_end();
    pbuf_free(p);
}

void receive_udp(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip4_addr* addr, u16_t port)
{
    char j[100];
    memcpy(j, (char*)p->payload, p->len);
    printf("Command: %s\n", j);
    json command = j;
    printf("Parsed command to json.\n");
    std::cout << command.dump() << std::endl;
    if (command.contains("direction_offset")) {
        if (command["direction_offset"].is_number()) {
            printf("Command valid.\n");
            int value = (int)command["direction_offset"];
            printf("Setting direction offset value to %i degrees.\n", value);
            set_direction_offset(value);
            printf("Direction offset set to %i degrees.\n", value);
        }
    } else {
        printf("Command not recognised.\n");
    }
}

bool is_valid(std::string &sentence)
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

void parse_A5120N_sentence(std::string &sentence, double direction_offset, double &direction, double &speed) {
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
    // Convert to radians in the interval of +/- pi with - pi indicating a port wind.
    direction = ((2.0*pi)/360.0)*(strtod(direction_str, &p)) - pi;
    // Add offset.
    direction += (((2.0*pi)/360.0)*direction_offset);
    // Adjust direction to account for offset.
    if (direction > pi) {
        direction = -pi + (direction - pi);
    } else if (direction < -pi) {
        direction = pi - (direction - pi);
    }
    // Round to 3 s.f. 
    direction = std::round(1000.0*direction)/1000.0;

    int start_speed = sentence.find("R,")+2;
    for (int i=0; i<=6; i++) {
        if (sentence[start_speed+i]==44) {
            break;
        }
        speed_str[i] = sentence[start_speed+i];
    }
    // Convert to m/s and round to 1 s.f.
    speed = std::round(10.0*0.5144444*strtod(speed_str, &p))/10.0;
}

int main() {
    stdio_init_all();

    // WiFi setup.
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
        printf("Wifi chip failed to initialise.\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    cyw43_arch_lwip_begin();

    // Set custom device name.
    struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
    netif_set_hostname(n, hostname);
    netif_set_up(n);
    cyw43_arch_lwip_end();
    
    // Connect to WiFi.
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("WiFi failed to connect to network.");
        return 1;
    }
    printf("Connected...\n");
    // Illuminate LED if connection is successful.
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    // Setup UDP interface.
    upcb = udp_new();   
    spcb = udp_new();
    err_t err = udp_bind(spcb, IP_ADDR_ANY, server_port);

    // Setup receive callback for UDP interface.
    udp_recv(spcb, receive_udp, NULL);

    // // static_assert(sizeof(double)==8);
    // flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    // flash_range_program(FLASH_TARGET_OFFSET, &direction_offset, 1);
    // void* m = flash_target_contents;
    // printf("Direction offset: %i\n", flash_target_contents[0]);
    // double d;
    // memcpy(&d, m, sizeof(d));
    // std::cout << d << std::endl;

    // UART setup.
    uart_init(uart1, BAUD_RATE);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(uart1, DATA_BITS, STOP_BITS, PARITY);

    // Setup json output structure.
    json out;
    out["updates"][0]["source"] = {{"label", "A5120N"}, {"type", "SignalK"}, {"src","wind"}};

    while (true) {
        while (uart_is_readable(uart1)) {
            ch = uart_getc(uart1);
            if (ch == START) {
                if (is_valid(sentence)) {
                    parse_A5120N_sentence(sentence, (double)direction_offset, direction, speed);
                    out["updates"][0]["values"][0] = {{"path", "environment.wind.angleApparent"}, {"value", direction}};
                    out["updates"][0]["values"][1] = {{"path", "environment.wind.speedApparent"}, {"value", speed}};
                    std::cout << out.dump() << std::endl;
                    std::string output_str = out.dump();
                    std::vector<char> v(output_str.begin(), output_str.end());
                    void* p = &v[0];
                    send_udp(server_ip, server_port, p, v.size());
                }
                sentence.clear();
                sentence += ch;
            }
            else {
                sentence += ch;
            }
        } 
        cyw43_arch_poll();
    }
    udp_remove(upcb);
    udp_remove(spcb);
    cyw43_arch_deinit();
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    return 0;
}