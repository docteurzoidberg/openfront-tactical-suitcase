/**
 * @file dns_captive_portal.c
 * @brief Captive Portal DNS Server Implementation
 * 
 * Minimal DNS server that responds to all A record queries with the
 * device's AP IP address. This redirects all DNS lookups to the device,
 * enabling captive portal functionality for WiFi provisioning.
 * 
 * The server:
 * - Listens on UDP port 53
 * - Responds to all A record queries
 * - Returns the AP IP (192.168.4.1 by default)
 * - Runs in a dedicated FreeRTOS task
 */

#include "dns_captive_portal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/errno.h"
#include <string.h>

static const char *TAG = "OTS_DNS";

// Default ESP-IDF softAP IP is 192.168.4.1 unless explicitly changed
#define CAPTIVE_PORTAL_IP_A 192
#define CAPTIVE_PORTAL_IP_B 168
#define CAPTIVE_PORTAL_IP_C 4
#define CAPTIVE_PORTAL_IP_D 1
#define CAPTIVE_DNS_PORT 53
#define DNS_TASK_STACK_SIZE 4096
#define DNS_TASK_PRIORITY 4

static TaskHandle_t s_dns_task = NULL;
static int s_dns_sock = -1;
static volatile bool s_dns_running = false;

static void dns_task(void *arg);

esp_err_t dns_captive_portal_start(void) {
    if (s_dns_task) {
        ESP_LOGW(TAG, "Captive DNS already running");
        return ESP_OK;
    }
    
    s_dns_running = true;
    
    BaseType_t ok = xTaskCreate(dns_task, "captive_dns", DNS_TASK_STACK_SIZE, NULL, 
                                 DNS_TASK_PRIORITY, &s_dns_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create captive DNS task");
        s_dns_task = NULL;
        s_dns_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Captive DNS task created");
    return ESP_OK;
}

void dns_captive_portal_stop(void) {
    if (!s_dns_running) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping captive DNS server");
    s_dns_running = false;
    
    if (s_dns_sock >= 0) {
        shutdown(s_dns_sock, SHUT_RDWR);
        close(s_dns_sock);
        s_dns_sock = -1;
    }
    
    // Task will exit on its own
    if (s_dns_task) {
        s_dns_task = NULL;
    }
}

bool dns_captive_portal_is_running(void) {
    return s_dns_running && s_dns_task != NULL;
}

static void dns_task(void *arg) {
    (void)arg;
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket() failed: errno=%d", errno);
        s_dns_running = false;
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    s_dns_sock = sock;
    
    int reuse = 1;
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CAPTIVE_DNS_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "DNS bind() failed: errno=%d", errno);
        close(sock);
        s_dns_sock = -1;
        s_dns_running = false;
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Captive DNS started on UDP/%u", (unsigned)CAPTIVE_DNS_PORT);
    
    // Receive buffer large enough for typical DNS queries
    uint8_t rx[256];
    uint8_t tx[256];
    
    while (s_dns_running) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        int n = recvfrom(sock, rx, sizeof(rx), 0, (struct sockaddr *)&from, &from_len);
        if (n <= 0) {
            // Socket closed or error; exit if we are stopping
            if (!s_dns_running) {
                break;
            }
            continue;
        }
        
        // Minimal DNS parsing/response
        // Header: 12 bytes
        if (n < 12) {
            continue;
        }
        
        // Only handle standard queries with exactly 1 question
        uint16_t qdcount = (uint16_t)((rx[4] << 8) | rx[5]);
        if (qdcount != 1) {
            continue;
        }
        
        // Find end of QNAME
        int offset = 12;
        while (offset < n && rx[offset] != 0) {
            offset += (int)rx[offset] + 1;
        }
        if (offset + 5 >= n) {
            continue;
        }
        // offset points at 0 terminator
        offset += 1;
        
        // QTYPE/QCLASS are 4 bytes
        const int question_end = offset + 4;
        if (question_end > n) {
            continue;
        }
        
        // Build response: header + original question + single A answer
        memset(tx, 0, sizeof(tx));
        // Transaction ID
        tx[0] = rx[0];
        tx[1] = rx[1];
        // Flags: response, recursion not available, no error
        tx[2] = 0x81;
        tx[3] = 0x80;
        // QDCOUNT = 1
        tx[4] = 0x00;
        tx[5] = 0x01;
        // ANCOUNT = 1
        tx[6] = 0x00;
        tx[7] = 0x01;
        // NSCOUNT/ARCOUNT = 0
        tx[8] = 0x00;
        tx[9] = 0x00;
        tx[10] = 0x00;
        tx[11] = 0x00;
        
        int tx_len = 12;
        const int q_len = question_end - 12;
        if (tx_len + q_len + 16 > (int)sizeof(tx)) {
            continue;
        }
        memcpy(&tx[tx_len], &rx[12], (size_t)q_len);
        tx_len += q_len;
        
        // Answer section
        // NAME: pointer to 0x0c (start of QNAME)
        tx[tx_len++] = 0xC0;
        tx[tx_len++] = 0x0C;
        // TYPE: A
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x01;
        // CLASS: IN
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x01;
        // TTL: 0
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x00;
        // RDLENGTH: 4
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x04;
        // RDATA: AP IP
        tx[tx_len++] = CAPTIVE_PORTAL_IP_A;
        tx[tx_len++] = CAPTIVE_PORTAL_IP_B;
        tx[tx_len++] = CAPTIVE_PORTAL_IP_C;
        tx[tx_len++] = CAPTIVE_PORTAL_IP_D;
        
        // Send response
        (void)sendto(sock, tx, (size_t)tx_len, 0, (struct sockaddr *)&from, from_len);
    }
    
    ESP_LOGI(TAG, "Captive DNS task exiting");
    close(sock);
    s_dns_sock = -1;
    s_dns_task = NULL;
    vTaskDelete(NULL);
}
