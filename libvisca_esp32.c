#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "libvisca.h"

#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h> // struct addrinfo
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"

/* implemented in libvisca.c
 */
void _VISCA_append_byte(VISCAPacket_t* packet, unsigned char byte);
void _VISCA_init_packet(VISCAPacket_t* packet);
unsigned int _VISCA_get_reply(VISCAInterface_t* iface, VISCACamera_t* camera);
unsigned int _VISCA_send_packet_with_reply(VISCAInterface_t* iface, VISCACamera_t* camera, VISCAPacket_t* packet);


#define TAG "ViscaEsp32"
#define RX_BUF_SIZE 1024
#define DEBUG

#define BYTE unsigned char
uint32_t
_VISCA_write_packet_data(VISCAInterface_t* iface, VISCACamera_t* camera, VISCAPacket_t* packet)
{
  /*
  int i;
  printf("writing packet %d bytes ", (int)packet->length);
  for (i = 0; i < packet->length; ++i)
  {
    printf("%02x ", packet->bytes[i]);
  }
  printf("\n");
  */
  iface->write_bytes(iface, packet->bytes, packet->length);
  // printf("wrote  packet %d bytes\n", packet->length);
  return VISCA_SUCCESS;
}

/** Report 0 if no VISCA transaction is in progress
 *
 * Returns 0 (OK) if neither the TX or RX semaphore are in use.
 */
uint32_t _VISCA_tx_ready(VISCAInterface_t* iface)
{
  esp_err_t err = 0;

  if (iface->if_type == SERIAL_IFT) {
    err = uart_wait_tx_done(iface->ser_device, 0);

    char c;
    err = iface->read_bytes(iface, &c, 0, 0);
  }
  // printf("*READY:%d", err);
  return err;
}

uint32_t
_VISCA_get_byte(VISCAInterface_t* iface, unsigned char* byte) {
  int bytes = iface->read_bytes(iface, byte, 1, 500 / portTICK_PERIOD_MS);
  if (bytes < 0) {
    return VISCA_FAILURE;
  }
  else {
    return VISCA_SUCCESS;
  }
}


/***********************************/
/*       TRANSPORT BINDING         */
/***********************************/

int serial_write_bytes(struct _VISCA_interface* device, const void* src, size_t size)
{
  return uart_write_bytes(device->ser_device, src, size);
}

int serial_read_bytes(struct _VISCA_interface* device, void* buf, uint32_t length, TickType_t ticks_to_wait)
{
  return uart_read_bytes(device->ser_device, buf, length, ticks_to_wait);
}

void _tcp_disconnect(VISCAInterface_t* device)
{
  if (device->socket >= 0) {
    ESP_LOGE(TAG, "Shutting down socket and restarting...");
    shutdown(device->socket, 0);
    device->connected = false;
    close(device->socket);
  }
}

bool _tcp_connect(VISCAInterface_t* device)
{
  int addr_family = 0;
  int ip_protocol = 0;

  struct sockaddr_in dest_addr;
  inet_pton(AF_INET, device->ip, &dest_addr.sin_addr);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(device->ip_port);
  addr_family = AF_INET;
  ip_protocol = IPPROTO_IP;

  device->connected = false;

  device->socket = socket(addr_family, SOCK_STREAM, ip_protocol);
  if (device->socket < 0) {
    ESP_LOGE(TAG, "Unable to create socket: (error %d) %s", errno, esp_err_to_name(errno));
    return false;
  }
  ESP_LOGI(TAG, "Socket created, connecting to %s:%d", device->ip, device->ip_port);
  struct timeval to;

  to.tv_sec = 1;
  to.tv_usec = 0;
  if (setsockopt(device->socket, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)) < 0) {
    ESP_LOGE(TAG, "Unable to set read timeout on socket!");
    //return false;
  }
  int err = connect(device->socket, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
  if (err != 0) {
    ESP_LOGE(TAG, "Socket unable to connect: (error %d) %s", errno, esp_err_to_name(errno));
    return false;
  }
  ESP_LOGI(TAG, "Successfully connected (socket %d)", device->socket);
  device->connected = true;
  return true;
}

int _tcp_write_bytes(struct _VISCA_interface* device, const void* src, size_t size)
{
  if (!device->connected && device->autoConnect) {
    _tcp_connect(device);
  }
  if (device->connected) {
    int err = send(device->socket, src, size, 0);
    if (err < 0) {
      ESP_LOGE(TAG, "Error occurred during sending: (error %d) %s", errno, esp_err_to_name(errno));
      _tcp_disconnect(device);
      _tcp_connect(device);
      return 0;
    }
  }
  else {
    ESP_LOGE(TAG, "TCP Not Connected");
  }

  return size;
}

int _tcp_read_bytes(struct _VISCA_interface* device, void* buf, uint32_t length, TickType_t ticks_to_wait)
{
  int len = recv(device->socket, buf, length, 0);
  // Error occurred during receiving
  if (len < 0) {
    ESP_LOGE(TAG, "recv failed: (error %d) %s", errno, esp_err_to_name(errno));
    // _tcp_disconnect(device);
    // _tcp_connect(device);
    // len = 0;
  }
  else {
    //ESP_LOGI(TAG, "Received %d bytes from %s: 0x%02x", len, device->ip, *(char *)buf);
  }
  return len;
}

uint32_t
VISCA_configure_tcp(VISCAInterface_t* device, const char* ip, uint16_t port)
{
  if (!device || port == 0xffff) {
#ifdef DEBUG
    printf("VISCA_open_tcp: bad parms\n");
#endif
    return VISCA_FAILURE;
  }
  device->write_bytes = _tcp_write_bytes;
  device->read_bytes = _tcp_read_bytes;

  device->if_type = TCP_IFT;
  device->ip = ip;
  device->ip_port = port;
  device->address = 0;

  return VISCA_SUCCESS;
}
uint32_t
VISCA_configure_serial(VISCAInterface_t* device, uart_port_t port, int rxpin, int txpin)
{
  if (!device) {
#ifdef DEBUG
    printf("VISCA_open_serial: bad parms\n");
#endif
    device->connected = false;
    return VISCA_FAILURE;
  }

  device->if_type = SERIAL_IFT;
  device->address = 0;
  device->ser_device = port;
  device->rxpin = rxpin;
  device->txpin = txpin;
  device->write_bytes = serial_write_bytes;
  device->read_bytes = serial_read_bytes;

  return VISCA_SUCCESS;
}

uint32_t
VISCA_close_serial(VISCAInterface_t* device)
{
  if (!device) {
#ifdef DEBUG
    printf("_VISCA_close_serial: bad header parms\n");
#endif
    return VISCA_FAILURE;
  }
  uart_driver_delete(device->ser_device);
  return VISCA_SUCCESS;
}

uint32_t VISCA_usleep(uint32_t useconds)
{
  vTaskDelay(pdMS_TO_TICKS(useconds / 1000));
  return 0;
}

uint32_t
VISCA_open_interface(VISCAInterface_t* device)
{
  if (device->if_type == TCP_IFT) {
    if (_tcp_connect(device)) {
      return VISCA_SUCCESS;
    }
    else {
      device->connected = false;
      return VISCA_FAILURE;
    }
  }
  else {
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_APB,
    };

    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif
    ESP_ERROR_CHECK(uart_driver_install(device->ser_device, RX_BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(device->ser_device, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(device->ser_device, device->txpin, device->rxpin, 0, 0));
    device->address = 0;

    device->connected = true;
    return VISCA_SUCCESS;
  }
}
