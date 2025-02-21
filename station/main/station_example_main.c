/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "math.h"

#include "lwip/err.h"
#include "lwip/errno.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/prot/dns.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/*=================== struct Rssidata ===================*/
struct Rssidata{
    int ap1[10];
    int ap2[10];
    int ap3[10];
    int length;
};

typedef struct Rssidata Rssidata;
/*=================== struct Rssidata ===================*/

static const char *TAG = "wifi station";

static int s_retry_num = 0;


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void my_csi_callback(void *ctx, wifi_csi_info_t *data){
    printf("MAC = %d\n", (data->len));
    // 在這裡處理更多 CSI 數據
}

char* int_to_char(Rssidata* data, int count)
{
    printf("datalen = %d\n", data->length);
    
    int num[3];
    num[0] = data->ap1[count];
    num[1] = data->ap2[count];
    num[2] = data->ap3[count];
    printf("num[2] = %d\n", data->ap3[count]);

    int total_len = 0;
    char temp_str[15]; // 足够存储一个int类型转换后的字符串，包括负号和结束符
    for(int i=0; i<3; i++)
    {
        // 计算每个RSSI值的字符串长度
        int len = sprintf(temp_str, "%d", num[i]);
        total_len += len;
    }
    printf("len = %d\n", total_len);

    // 分配足够的空间存储最终的字符串
    char* str = malloc((total_len + 1) * sizeof(char)); // 为结束符多分配一个字符
    if (str == NULL) {
        return NULL; // 检查malloc是否成功
    }

    str[0] = '\0'; // 初始化空字符串

    for(int i=0; i<3; i++)
    {
        // 将每个RSSI值的字符串拼接到结果字符串中
        sprintf(temp_str, "%d", num[i]);
        strcat(str, temp_str);
        printf("str = %s\n", str);
    }
    
    return str;
}

#define ROUTER_1_SSID "Kon peko"
#define ROUTER_2_SSID "92x81"
#define ROUTER_3_SSID "92377"

void get_rssi_for_three_routers(Rssidata* data, int collect_num) {
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };

    printf("test\n");
    esp_wifi_scan_start(&scan_config, true);  // true: block until scan done
    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    printf("Router number = %d\n", ap_num);

    wifi_ap_record_t ap_records[ap_num];
    esp_wifi_scan_get_ap_records(&ap_num, ap_records);

    int rssi_router_1 = -100;  // 初始化RSSI为低值
    int rssi_router_2 = -100;
    int rssi_router_3 = -100;

    for (int i = 0; i < ap_num; i++) {
        if (strcmp((char*)ap_records[i].ssid, ROUTER_1_SSID) == 0) {
            rssi_router_1 = ap_records[i].rssi;
        } else if (strcmp((char*)ap_records[i].ssid, ROUTER_2_SSID) == 0) {
            rssi_router_2 = ap_records[i].rssi;
        } else if (strcmp((char*)ap_records[i].ssid, ROUTER_3_SSID) == 0) {
            rssi_router_3 = ap_records[i].rssi;
        }
    }
    data->ap1[collect_num] = rssi_router_1;
    data->ap2[collect_num] = rssi_router_2;
    data->ap3[collect_num] = rssi_router_3;

    printf("RSSI for Router 1: %d\n", rssi_router_1);
    printf("RSSI for Router 2: %d\n", rssi_router_2);
    printf("RSSI for Router 3: %d\n", rssi_router_3);
}

void tcp_client_task(void *mydata)
{
    Rssidata *data = mydata;
    printf("start!\n");
    /*============================ TCP IP =============================*/
    /*===================== 新建socket ======================*/
    int addr_family = 0;
    int ip_protocol = 0;

    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if(sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        close(sock);
    }
    /*================== 配置server訊息 ======================*/
    #define TCP_SERVER_ADRESS "192.168.82.216"
    #define TCP_PORT 3333
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(TCP_SERVER_ADRESS);
    dest_addr.sin_port = htons(TCP_PORT);

    // char host_ip[] = TCP_SERVER_ADRESS;
    /*===================== 連接sever ========================*/
    int connect_err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if(connect_err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        close(sock);
    }
    ESP_LOGI(TAG, "Successfully connected");

    /*===================== 發送 / 接收數據 ====================*/
    static const char *payload;
    // char rx_buffer[128];

    // int rssival, pastRssival;
    // bool first = true;
    wifi_csi_config_t my_config = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = true,
        .manu_scale        = false,
        .shift             = false,
    };

    /*=========================== CSI ==============================*/
        // 註冊CSI回調，不需要上下文
    // esp_err_t err = esp_wifi_set_csi_rx_cb(my_csi_callback, NULL);
    // esp_wifi_set_csi(true);  // 啟用CSI功能
    // esp_wifi_set_csi_config(&my_config);  // 設定CSI參數
    // if (err == ESP_OK) {
    //     printf("Callback registered successfully.\n");
    // } else {
    //     printf("Failed to register callback.\n");
    // }

    int count = 0;
    while(count < data->length)
    {
        /*=========================== RSSI ==============================*/
        // esp_wifi_sta_get_rssi(&rssival);
        // printf("RSSI value = %d\n", rssival);
        
        payload = int_to_char(data, count);
        count++;
        printf("%d\n", strlen(payload));

        // if(first == true)
        // {
        //     pastRssival = rssival;
        //     first = false;
        // }
        // else if(abs(rssival - pastRssival) >= 10)
        // {
        //     printf("The location has changed!\n");
        // }
        // pastRssival = rssival;

        int send_err = send(sock, payload, strlen(payload), 0);
        if(send_err < 0)
        {
            ESP_LOGE(TAG, "Error occur during sending: errno %d", errno);
            break;
        }

        // 使用非const指针来释放内存
        char* tempPayload = (char*)payload;
        free(tempPayload);

        // int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        // if(len < 0)
        // {
        //     ESP_LOGE(TAG, "receive failed: errno %d", errno);
        //     break;
        // }
        // else
        // {
        //     rx_buffer[len] = 0;
        //     ESP_LOGI(TAG, "Recived %d bytes from %s: ", len, host_ip);
        //     ESP_LOGI(TAG, "%s", rx_buffer);
        // }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    if(sock != -1)
    {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    Rssidata* mydata = malloc(sizeof(Rssidata));
    int collect_num = 0;
    while(collect_num < 10)
    {
        get_rssi_for_three_routers(mydata, collect_num);
        collect_num++;
    }
    mydata->length = collect_num;

    xTaskCreate(tcp_client_task, "tcp_client", 4096, mydata, 5, NULL);
}
