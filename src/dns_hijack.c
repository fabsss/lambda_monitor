#include "dns_hijack.h"
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

#define DNS_PORT 53
#define DNS_MAX_LEN 256
#define DNS_TARGET_IP "192.168.4.1"

#define OPCODE_MASK 0x7800
#define QR_FLAG (1 << 7)
#define QD_TYPE_A 0x0001
#define ANS_TTL_SEC 300

static const char *TAG = "dns_hijack";
static uint32_t s_target_ip; /* network byte order, set once in dns_hijack_start() */

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t class;
} dns_question_t;

typedef struct __attribute__((packed)) {
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
} dns_answer_t;

/* Parses one DNS-label-encoded name (e.g. "\x09gstatic\x03com\x00") into a
 * regular dotted string, and returns a pointer to the first byte after it
 * in the packet (needed to locate the question's type/class fields, and
 * the correct pointer offset for the compressed name in the reply). */
static char *parse_dns_name(char *raw_name, char *parsed_name, size_t parsed_name_max_len)
{
    char *label = raw_name;
    char *name_itr = parsed_name;
    int name_len = 0;

    do {
        int sub_name_len = *label;
        name_len += (sub_name_len + 1);
        if (name_len > (int)parsed_name_max_len) {
            return NULL;
        }
        memcpy(name_itr, label + 1, sub_name_len);
        name_itr[sub_name_len] = '.';
        name_itr += (sub_name_len + 1);
        label += sub_name_len + 1;
    } while (*label != 0);

    parsed_name[name_len - 1] = '\0';
    return label + 1;
}

/* Builds a reply that answers every A-record question in req with
 * s_target_ip, leaving any non-A questions unanswered. Returns the reply
 * length, 0 for a non-standard query (left unanswered), or -1 on a
 * malformed/oversized request. */
static int parse_dns_request(char *req, size_t req_len, char *dns_reply, size_t dns_reply_max_len)
{
    if (req_len > dns_reply_max_len || req_len < sizeof(dns_header_t)) {
        return -1;
    }

    memset(dns_reply, 0, dns_reply_max_len);
    memcpy(dns_reply, req, req_len);

    dns_header_t *header = (dns_header_t *)dns_reply;
    if ((header->flags & OPCODE_MASK) != 0) {
        return 0;
    }

    header->flags |= QR_FLAG;
    uint16_t qd_count = ntohs(header->qd_count);
    header->an_count = htons(qd_count);

    size_t reply_len = (size_t)qd_count * sizeof(dns_answer_t) + req_len;
    if (reply_len > dns_reply_max_len) {
        return -1;
    }

    char *cur_ans_ptr = dns_reply + req_len;
    char *cur_qd_ptr = dns_reply + sizeof(dns_header_t);
    char name[128];

    for (int qd_i = 0; qd_i < qd_count; qd_i++) {
        char *name_end_ptr = parse_dns_name(cur_qd_ptr, name, sizeof(name));
        if (name_end_ptr == NULL) {
            ESP_LOGW(TAG, "Failed to parse DNS question name");
            return -1;
        }

        dns_question_t *question = (dns_question_t *)name_end_ptr;
        uint16_t qd_type = ntohs(question->type);

        if (qd_type == QD_TYPE_A) {
            dns_answer_t *answer = (dns_answer_t *)cur_ans_ptr;
            answer->ptr_offset = htons(0xC000 | (uint16_t)(cur_qd_ptr - dns_reply));
            answer->type = htons(qd_type);
            answer->class = question->class;
            answer->ttl = htonl(ANS_TTL_SEC);
            answer->addr_len = htons(sizeof(answer->ip_addr));
            answer->ip_addr = s_target_ip;
            cur_ans_ptr += sizeof(dns_answer_t);
        }

        cur_qd_ptr = (char *)question + sizeof(dns_question_t);
    }

    return (int)reply_len;
}

static void dns_hijack_task(void *arg)
{
    (void)arg;
    char rx_buffer[128];
    char reply[DNS_MAX_LEN];

    while (1) {
        struct sockaddr_in dest_addr = {
            .sin_addr.s_addr = htonl(INADDR_ANY),
            .sin_family = AF_INET,
            .sin_port = htons(DNS_PORT),
        };

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        ESP_LOGI(TAG, "Listening on UDP port %d, answering all A queries with %s", DNS_PORT, DNS_TARGET_IP);

        while (1) {
            struct sockaddr_in source_addr;
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            rx_buffer[len] = 0;

            int reply_len = parse_dns_request(rx_buffer, len, reply, sizeof(reply));
            if (reply_len > 0) {
                sendto(sock, reply, reply_len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            }
        }

        close(sock);
    }
}

void dns_hijack_start(void)
{
    s_target_ip = inet_addr(DNS_TARGET_IP);
    xTaskCreate(dns_hijack_task, "dns_hijack", 4096, NULL, 5, NULL);
}
