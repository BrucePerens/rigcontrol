// This implements the Improv WiFi protocol as documented at
// https://www.improv-wifi.com/serial/
// This version is an independent development, from the document above.
// I didn't look at their SDK while developing this.
//
// Copyright 2022 Algorithmic LLC, Licensed under AGPL 3. - Bruce Perens
//
// Improv is an Open Protocol, not a "standard" as they say on their web site,
// because there wasn't any formal standards process.
//
// The Improv protocol isn't directly compatible with running the REPL (console command
// processor) on the same serial connection. It really could be, if the magic number
// was "IMPROV " with a space, the data was some text representation like JSON, and the
// packet ended with a carriage-return character. I suggest that if there's ever a later
// version. I kludge around the incompatibility with the REPL by waiting
// for the Improv protocol for a few seconds, and then falling through to the REPL
// if I don't see any Improv packets.
//
// Important things that weren't said in the Improv Serial document:
// * The speed is 115200 baud.
// * Implementations actually send a newline character after the Improv packet,
//   although that is not in the spec. This may be _required_ because some
//   implementations use line-based I/O.
// * The flow for the Send WiFi Settings packet is not correctly explained, it leaves
//   out the current state packet sent while the state is Provisioning. The flow is:
//   * Set the WiFi information.
//   * Set the current state to Provisioning.
//   * Send the current state.
//   * Wait for WiFi to connect.
//   * Set the current state to Provisioned.
//   * Send the current state.
//   * Send the URL of the device as an RPC response.
//
//   * If WiFi doesn't connect:
//      * Set the current state back to Ready
//	* Send the current state.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include "generic_main.h"

const char	name[] = "Improv WiFi ";
const uint8_t	magic[6] = "IMPROV"; // Explicit size eliminates trailing '\0'.
const uint8_t	ImprovVersion = 1;

typedef enum improv_type
{
  CurrentState = 1,
  ErrorState = 2,
  RPC_Command = 3,
  RPC_Result = 4
} improv_type_t;

typedef enum improv_rpc_command {
  SendWiFiSettings = 1,
  RequestCurrentState = 2,
  RequestDeviceInformation = 3,
  RequestScannedWiFiNetworks = 4
} improv_rpc_command_t;

typedef enum improv_state {
  Ready = 2,		// Ready to accept credentials.
  Provisioning = 3,	// Credentials received, attempting to connect.
  Provisioned = 4	//
} improv_state_t;

typedef enum improv_error_state {
  NoError = 0,
  Invalid_RPC_Packet = 1,	// The command was malformed.
  Unknown_RPC_Command = 2,
  UnableToConnect = 3,		// The requested WiFi connection failed.
  UnknownError = 0xff
} improv_error_state_t;

static improv_state_t	improv_state = Ready;
struct sockaddr_in	address = {};
static bool		improv_received_a_valid_packet = false;
static esp_event_handler_instance_t handler_wifi_event_sta_got_ip4 = NULL;
static esp_event_handler_instance_t handler_wifi_event_sta_disconnected = NULL;

static void
improv_send(int fd, const uint8_t * data, improv_type_t type, unsigned int length)
{
  uint8_t buffer[256 + 11];
  unsigned int	checksum = 0;

  ; // gm_printf("Improv send type %d, length %d\n", type, length);
  memcpy(buffer, magic, sizeof(magic));
  buffer[6] = ImprovVersion;
  buffer[7] = (type & 0xff);
  buffer[8] = (length & 0xff);
  memcpy(&buffer[9], data, length);

  for ( unsigned int i = 0; i < 9 + length; i++ )
    checksum += buffer[i];

  ; // gm_printf("Checksum is 0x%x\n", checksum & 0xff);
  buffer[9 + length] = (checksum & 0xff);
  buffer[10 + length] = '\n';
  ; // gm_printf("Write: ");
  for ( int i = 0; i < 10 + length; i++ ) {
    uint8_t c = buffer[i];

    if ( c > ' ' && c <= '~' )
      ; // gm_printf("%c ", c);
    else
      ; // gm_printf("0x%x ", (int)c);
  }
  ; // gm_printf("\n");
  write(fd, buffer, 11 + length);
}

static int
improv_encode_string(const char * s, uint8_t * data)
{
  int	length = strlen(s);

  *data++ = (length & 0xff);

  memcpy(data, s, length);

  return length + 1;
}

static void
improv_report_url(int fd)
{
  unsigned int	offset;
  uint8_t	data[256];
  char		ntop_buffer[INET_ADDRSTRLEN + 1];
  char		url_buffer[INET_ADDRSTRLEN + 10];

  data[0] = (SendWiFiSettings & 0xff);
  offset = 2;

  if ( address.sin_addr.s_addr != 0 ) {
    inet_ntop(AF_INET, &address.sin_addr, ntop_buffer, sizeof(ntop_buffer));
    snprintf(url_buffer, sizeof(url_buffer), "http://%s/", ntop_buffer);
    offset += improv_encode_string(url_buffer, &data[offset]);
  }
  data[1] = offset - 2;
  improv_send(fd, data, RPC_Result, offset);
}

static void
improv_send_current_state(int fd)
{
  const uint8_t	state = (improv_state & 0xff);
  improv_send(fd, &state, CurrentState, 1);
  if ( improv_state == Provisioned )
    improv_report_url(fd);
}

static void wifi_event_sta_got_ip4(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  int fd = (int)arg;
  ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
  
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
   IP_EVENT,
   IP_EVENT_STA_GOT_IP,
   &handler_wifi_event_sta_got_ip4));

  address.sin_addr.s_addr = event->ip_info.ip.addr;
  improv_state = Provisioned;
  improv_send_current_state(fd);
}

static void wifi_event_sta_disconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  improv_state = Ready;
}

static void
improv_send_error(int fd, improv_error_state_t error)
{
  const uint8_t	error_byte = (error & 0xff);
  improv_send(fd, &error_byte, ErrorState, 1);
}

static improv_error_state_t
improv_read (int fd, uint8_t * data, improv_type_t * r_type, uint8_t * r_length)
{
  unsigned int	index = 0;
  int			status;
  uint8_t		c;
  uint8_t		version;
  uint8_t		type = 0;
  uint8_t		length = 0;
  uint8_t		checksum;
  unsigned int		internal_checksum = 0;

  *r_type = 0;
  *r_length = 0;

  ; // gm_printf("improv_read\n");
  for ( ; ; ) {
    ; // gm_printf("Index = %d\n", index);
    if ( (status = read(fd, &c, 1)) == 1 ) {
      if ( c > ' ' && c <= '~' )
        ; // gm_printf("read %c\n", c);
      else
        ; // gm_printf("read 0x%x\n", (int)c);
    }
      
    else {
      ; // gm_printf("%sread failure: %s\n", name, strerror(errno));
      return Invalid_RPC_Packet;
    }


    // Keep throwing away data until I see "IMPROV".
    if ( index < sizeof(magic)) {
      if ( c != magic[index] ) {
        if ( c == 0xc0 ) {
          // This is probably a SLIP packet for the ROM bootloader.
	  // Reset the chip and hope it gets into bootloader mode.
          // There isn't a documented way to jump into it.
          esp_restart();
        }
        index = 0;
        continue;
      }
    }

    switch ( index ) {
    case 6:
      version = c;
      if ( version != 1 ) {
        ; // gm_printf("%sversion %d not implemented.\n", name, (int)c);
        improv_send_error(fd, Invalid_RPC_Packet);
        return Invalid_RPC_Packet;
      }
      break;
    case 7:
      type = c;
      ; // gm_printf("Got type %d\n", c);
      break;
    case 8:
      length = c;
      ; // gm_printf("Got length %d\n", c);
      break;
    }

    if ( index >= 9 && index < 9 + length )
      data[index - 9] = c;
    else if ( index == length + 9 ) {
      checksum = c;
      break;
    }
    internal_checksum += c;
    index++;
  }
 
  if ( checksum != (internal_checksum & 0xff) ) {
    ; // gm_printf("%schecksum doesn't match.\n", name);
    improv_send_error(fd, Invalid_RPC_Packet);
    return Invalid_RPC_Packet;
  }

  *r_type = (improv_type_t)type;
  *r_length = length;
  improv_received_a_valid_packet = true;
  return NoError;
}

static unsigned int
improv_decode_string(const uint8_t * data, uint8_t * s)
{
  uint8_t	length = *data++;
  memcpy(s, data, length);
  s[length] = '\0';
  return (unsigned int)length + 1;
}

static void
improv_set_wifi(int fd, const uint8_t * data, uint8_t length)
{
  uint8_t	ssid[257]; 
  uint8_t	password[257]; 
  uint8_t	ssid_length;

  ssid_length = improv_decode_string(data, ssid);
  improv_decode_string(&data[ssid_length], password);
  improv_state = Provisioning;
  improv_send_current_state(fd);

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
   IP_EVENT,
   IP_EVENT_STA_GOT_IP,
   &wifi_event_sta_got_ip4,
   (void *)fd,
   &handler_wifi_event_sta_got_ip4));


  gm_param_set("ssid", (const char *)ssid);
  gm_param_set("wifi_password", (const char *)password);

  ESP_ERROR_CHECK(esp_event_handler_register(
   WIFI_EVENT,
   WIFI_EVENT_STA_DISCONNECTED,
   &wifi_event_sta_disconnected,
   &handler_wifi_event_sta_disconnected));

  improv_state = Provisioning;
  improv_send_current_state(fd);
}

static void
improv_send_device_information(int fd)
{
  unsigned int	offset;
  uint8_t	data[256];
  data[0] = (RequestDeviceInformation & 0xff);
  offset = 2;
  offset += improv_encode_string("K6BP Rigcontrol", &data[offset]);
  offset += improv_encode_string(GM.build_version, &data[offset]);
  offset += improv_encode_string("ESP32 Audio Kit", &data[offset]);
  data[1] = offset - 2;
  improv_send(fd, data, RPC_Result, offset);
}

static void
improv_send_scanned_wifi_networks(int fd)
{
  uint8_t		data[256];
  wifi_scan_config_t	config = {};
  wifi_ap_record_t *	ap_records;
  uint16_t		number_of_access_points;
  char			rssi[5];

  config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  // WiFi scan times are in milliseconds per channel.
  config.scan_time.active.min = 120;
  config.scan_time.active.max = 120;
  config.scan_time.passive = 120;

  ESP_ERROR_CHECK(esp_wifi_scan_start(&config, true));

  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&number_of_access_points));

  ap_records = malloc(sizeof(*ap_records) * number_of_access_points);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number_of_access_points, ap_records));

  for ( unsigned int i = 0; i < number_of_access_points; i++ ) {
    unsigned int	offset = 0;

    data[0] = (RequestScannedWiFiNetworks & 0xff);
    offset = 2;
    offset += improv_encode_string((const char *)ap_records[i].ssid, &data[offset]);
    snprintf(rssi, sizeof(rssi), "%d", ap_records[i].rssi);
    offset += improv_encode_string(rssi, &data[offset]);
    const char * auth_required = ap_records[i].authmode != WIFI_AUTH_OPEN ? "YES" : "NO";
    offset += improv_encode_string(auth_required, &data[offset]);
    data[1] = offset - 2;
    improv_send(fd, data, RPC_Result, offset);
  }
  free(ap_records);
  data[0] = (RequestScannedWiFiNetworks & 0xff);
  data[1] = 0;
  improv_send(fd, data, RPC_Result, 2);
}

static improv_error_state_t
improv_process(int fd, const uint8_t * data, improv_type_t type, uint8_t length)
{
  uint8_t	command;
  uint8_t	command_length;

  switch ( type ) {
  case RPC_Command:
    command = data[0];
    command_length = data[1];

    switch ( command ) {
    case SendWiFiSettings:
      improv_set_wifi(fd, &data[2], command_length);
      break;
    case RequestCurrentState:
      improv_send_current_state(fd);
      break;
    case RequestDeviceInformation:
      improv_send_device_information(fd);
      break;
    case RequestScannedWiFiNetworks:
      improv_send_scanned_wifi_networks(fd);
      break;
    default:
      ; // gm_printf("%sBad command %d.\n", name, command);
      improv_send_error(fd, Invalid_RPC_Packet);
      return Invalid_RPC_Packet;
    }
    break;

  default:
    ; // gm_printf("%sBad type %d.\n", name, type);
    improv_send_error(fd, Unknown_RPC_Command);
    return Unknown_RPC_Command;
  }

  return NoError;
}

void
gm_improv_wifi(int fd)
{
  // Start the WiFi scan as soon as possible, so that the data is ready when the user
  // wishes to configure WiFi.
  uint8_t		data[256];
  improv_error_state_t	error;
  improv_type_t		type;
  uint8_t		length;

  ; // gm_printf("Waiting for the Web Updater.\n");
  error = improv_read(fd, data, &type, &length);

  if ( error == NoError ) {
    // If we get here, Improv started within the timeout. So keep it running on the
    // console rather than the REPL.
    improv_process(fd, data, type, length);
    for ( ; ; ) {
      error = improv_read(fd, data, &type, &length);
      if ( error == NoError ) {
        improv_process(fd, data, type, length);
      }
    }
  }
  else {
    ; // gm_printf("Not connected to the Web updater.\nStarting the command processor.\n");
    // Timed out without receiving an Improv packet.
    // Return so that the REPL can take over the console.
  }
}
