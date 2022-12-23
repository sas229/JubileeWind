#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <iostream>
#include "json.hpp"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include <time.h>

#define UART1_TX_PIN 4
#define UART1_RX_PIN 5
#define BAUD_RATE    4800
#define DATA_BITS    8
#define STOP_BITS    1
#define PARITY       UART_PARITY_NONE

#define MAX_LENGTH   82
#define START        36

#define HOSTNAME     "JubileeWind"
#define RECV_IP      "192.168.1.100"
#define RECV_PORT    4123
#define BUF_SIZE     300
const char ssid[] = "Jubilee";
const char pass[] = "mc%4Mv2b*AwxAx";

unsigned int loop=0;
alarm_pool_t* alarmP;
repeating_timer_t  RptTimer;
struct udp_pcb* upcb;
struct udp_pcb* spcb;
bool TimerFlag=false;
char buffer[BUF_SIZE];

char ch;
std::string sentence;
double direction, speed;

using json = nlohmann::json;

void SendUDP(char* IP , int port, void* data, int data_size) {
    ip_addr_t destAddr;
    ip4addr_aton(IP, &destAddr);
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, data_size, PBUF_RAM);
    memcpy(p->payload, data, data_size);
    cyw43_arch_lwip_begin();
    udp_sendto(upcb, p, &destAddr, port);
    cyw43_arch_lwip_end();
    pbuf_free(p);
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

void parse_sentence(std::string &sentence, double &direction, double &speed) {
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

    // WiFi setup.
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
        printf("Failed to initialise...\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    cyw43_arch_lwip_begin();
    struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
    netif_set_hostname(n, HOSTNAME);
    netif_set_up(n);
    cyw43_arch_lwip_end();
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Failed to connect...");
        return 1;
    }
    printf("Connected...\n");

    upcb = udp_new();   
    spcb = udp_new();

    err_t err = udp_bind(spcb, IP_ADDR_ANY, RECV_PORT);

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
                    parse_sentence(sentence, direction, speed);
                    out["updates"][0]["values"][0] = {{"path", "environment.wind.angleApparent"}, {"value", direction}};
                    out["updates"][0]["values"][1] = {{"path", "environment.wind.speedApparent"}, {"value", speed}};
                    std::cout << out.dump(4) << "\n";
                    std::string output_str = out.dump();
                    std::vector<char> v(output_str.begin(), output_str.end());
                    void* p = &v[0];
                    SendUDP(RECV_IP, RECV_PORT, p, v.size());
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
    return 0;
}