#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#define WIFI_SSID "iPhone"
#define WIFI_PASSWORD "Mylena18"
#define PORT 80
#define DEBUG 1

typedef struct {
    char a[12];
    char b[12];
    char joy[12];
    char dir[12];
    int x;
    int y;
} JoystickData;

JoystickData data = {
    .a = "Solto", .b = "Solto", .joy = "Solto",
    .dir = "Centro", .x = 0, .y = 0
};

int16_t read_adc_centered(uint channel) {
    adc_select_input(channel - 26);
    return (int16_t)adc_read() - 2048;
}

static err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    #if DEBUG
    printf("Request: %.*s\n", p->len, (char*)p->payload);
    #endif

//leitura da temperatura
adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;

 // Verifica se o conteúdo contém "/data"
   if (strstr((char*)p->payload, "GET /data") != NULL) {
    char json[256];
    char body[128];
    snprintf(body, sizeof(body),
    "{\"a\":\"%s\",\"b\":\"%s\",\"joy\":\"%s\",\"dir\":\"%s\",\"x\":%d,\"y\":%d,\"temp\":%.2f}",
    data.a, data.b, data.joy, data.dir, data.x, data.y, temperature);

    char header[256];
    snprintf(header, sizeof(header),
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: %d\r\n"
    "Connection: close\r\n\r\n%s", strlen(body), body);

    tcp_write(tpcb, header, strlen(header), TCP_WRITE_FLAG_COPY);    
} 

   else if (strncmp((char*)p->payload, "GET / ", 6) == 0) {
   const char *html_full =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Connection: close\r\n\r\n"
    "<!DOCTYPE html><html lang='pt-BR'><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<title>Monitoramento</title><style>"
    "body{margin:0;padding:20px;background:#f0f0f0;color:#000;font-family:sans-serif;}"
    ".c{background:#fff;max-width:600px;margin:40px auto;padding:20px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
    "</style></head><body><div class='c'><h1>Monitoramento</h1><div id='d'></div>"
    "<script>""async function u(){""try{""const r=await fetch('/data'),j=await r.json();"
    "document.getElementById('d').innerHTML = ""`<p>Botão A: ${j.a}</p>"
    "<p>Botão B: ${j.b}</p>""<p>Botão Joy: ${j.joy}</p>""<p>Direção: ${j.dir}</p>"
    "<p>X: ${j.x}, Y: ${j.y}</p>""<p class='temperature'>Temperatura Interna: ${j.temp.toFixed(2)} &deg;C</p>`;"
    "}catch(e){""document.getElementById('d').innerHTML = ""`<p style='color:red'>Erro: ${e.message}</p>`;""}"
    "setTimeout(u,1000);""}""u();""</script></body></html>";
    tcp_write(tpcb, html_full, strlen(html_full), TCP_WRITE_FLAG_COPY);
} 

  else {
    const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
    tcp_write(tpcb, not_found, strlen(not_found), TCP_WRITE_FLAG_COPY);
}


    pbuf_free(p);
    tcp_output(tpcb);
    return ERR_OK;
}

static err_t tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err != ERR_OK || !newpcb) return ERR_VAL;
    
    tcp_arg(newpcb, NULL);
    tcp_recv(newpcb, tcp_recv_cb);
    tcp_err(newpcb, NULL);
    
    #if DEBUG
    printf("Nova conexão aceita\n");
    #endif
    
    return ERR_OK;
}

void init_server() {
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, PORT);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, tcp_accept_cb);
    
    printf("Servidor rodando na porta %d\n", PORT);
}

int main() {
    stdio_init_all();
    
    gpio_init(5); gpio_set_dir(5, GPIO_IN); gpio_pull_up(5);
    gpio_init(6); gpio_set_dir(6, GPIO_IN); gpio_pull_up(6);
    gpio_init(22); gpio_set_dir(22, GPIO_IN); gpio_pull_up(22);
    
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_gpio_init(26);
    adc_gpio_init(27);
    
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode(); 
    printf("Conectando a %s...\n", WIFI_SSID);
    
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha na conexão Wi-Fi\n");
        return 1;
    }
    printf("Conectado! IP: %s\n", ip4addr_ntoa(&netif_default->ip_addr));
    
    
    init_server();
    
    while (true) {
        cyw43_arch_poll();
        
        data.x = read_adc_centered(26);
        data.y = read_adc_centered(27);

         #define DEADZONE 50
         if (abs(data.x) < DEADZONE) data.x = 0;
         if (abs(data.y) < DEADZONE) data.y = 0;
        
        data.a[0] = gpio_get(5) ? 'S' : 'P'; data.a[1] = '\0';
        data.b[0] = gpio_get(6) ? 'S' : 'P'; data.b[1] = '\0';
        data.joy[0] = gpio_get(22) ? 'S' : 'P'; data.joy[1] = '\0';
        
        float x_norm = data.x / 2048.0f;
        float y_norm = data.y / 2048.0f;

        if (y_norm > 0.6f && x_norm > 0.6f)strcpy(data.dir, "Nordeste");
        else if (x_norm > 0.6f && y_norm < -0.6f)strcpy(data.dir, "Noroeste");
        else if (x_norm < -0.6f && y_norm > 0.6f)strcpy(data.dir, "Sudeste");
        else if (y_norm < -0.6f && x_norm < -0.6f)strcpy(data.dir, "Sudoeste");
        else if (x_norm > 0.6f)strcpy(data.dir, "Norte");
        else if (x_norm < -0.6f)strcpy(data.dir, "Sul");
        else if (y_norm > 0.6f)strcpy(data.dir, "Leste");
        else if (y_norm < -0.6f)strcpy(data.dir, "Oeste");
        else strcpy(data.dir, "Centro");  // quando o joystick está parado
        sleep_ms(10);
    }
    
    cyw43_arch_deinit();
    return 0;
}
