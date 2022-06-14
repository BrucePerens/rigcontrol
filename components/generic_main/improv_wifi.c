// This implements the Improv WiFi protocol as documented at
// https://www.improv-wifi.com/serial/
// This version is an independent development, from the document above.
// I didn't look at their SDK while developing this.
//
// Copyright 2022 Algorithmic LLC, Licensed under AGPL 3. - Bruce Perens
//
// Improv is an Open Protocol, not a "standard" as they say on their web site,
// because there wasn't any formal standards process.
// The Improv protocol isn't directly compatible with running the REPL (console command
// processor) on the same serial connection. It really could be, if the magic number
// was "IMPROV " with a space, the data was some text representation like JSON, and the
// packet ended with a carriage-return character. I suggest that if there's ever a later
// version. I kludge around the incompatibility with the REPL by waiting
// for the Improv protocol for a few seconds, and then falling through to the REPL
// if I don't see any Improv packets.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <setjmp.h>
#include <signal.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include "generic_main.h"

const char	name[] = "Improv WiFi ";
const uint8_t	magic[] = "IMPROV";
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
static bool		improv_received_a_valid_packet = false;
static jmp_buf		jump;
static esp_event_handler_instance_t handler_wifi_event_sta_connected_to_ap = NULL;
static esp_event_handler_instance_t handler_wifi_event_sta_disconnected = NULL;

static void
improv_send(int fd, const uint8_t * data, improv_type_t type, unsigned int length)
{
  uint8_t buffer[256 + 11];
  unsigned int	checksum = 0;

  memcpy(buffer, magic, sizeof(magic));
  buffer[6] = ImprovVersion;
  buffer[7] = (type & 0xff);
  buffer[8] = (length & 0xff);
  memcpy(&buffer[9], data, length);

  for ( unsigned int i = 0; i < 9 + length; i++ )
    checksum += buffer[i];

  buffer[9 + length] = (checksum & 0xff);
  // gm_printf("Write: ");
  for ( int i = 0; i < length; i++ ) {
    // uint8_t c = buffer[i];

    // if ( c > ' ' && c <= '~' )
      // gm_printf("%c ", c);
    // else
      // gm_printf("0x%x ", (int)c);
  }
  // gm_printf("\n");
  write(fd, buffer, length + 10);
}

static void
improv_send_current_state(int fd)
{
  const uint8_t	state = (improv_state & 0xff);
  improv_send(fd, &state, CurrentState, 1);
}

static void wifi_event_sta_connected_to_ap(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  int fd = (int)event_data;
  
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
   WIFI_EVENT,
   WIFI_EVENT_STA_CONNECTED,
   &handler_wifi_event_sta_connected_to_ap));

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

  for ( ; ; ) {
    if ( (status = read(fd, &c, 1)) == 1 ) {
      // if ( c > ' ' && c <= '~' )
        // gm_printf("%s read %c\n", c);
      // else
        // gm_printf("%s read 0x%x\n", (int)c);
    }
      
    else {
      // gm_printf("%sread failure: %s\n", name, strerror(errno));
      return Invalid_RPC_Packet;
    }

    internal_checksum += c;

    // Keep throwing away data until I see "IMPROV".
    if ( index < sizeof(magic) ) {
      if ( c != magic[index] ) {
        index = 0;
        continue;
      }
    }

    switch ( index ) {
    case 6:
      version = c;
      if ( version != 1 ) {
        // gm_printf("%sversion %d not implemented.\n", name, (int)c);
        improv_send_error(fd, Invalid_RPC_Packet);
        return Invalid_RPC_Packet;
      }
      break;
    case 7:
      type = c;
      break;
    case 8:
      length = c;
      break;
    }

    int offset = (int)index - 9;

    if ( offset >= 0 && offset < length )
      data[offset] = c;
    else if ( offset == length ) {
      checksum = c;
      break;
    }

    index++;
  }
 
  if ( checksum != (internal_checksum & 0xff) ) {
    // gm_printf("%schecksum doesn't match.\n", name);
    improv_send_error(fd, Invalid_RPC_Packet);
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
  return (unsigned int)length;
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
improv_set_wifi(int fd, const uint8_t * data, uint8_t length)
{
  uint8_t	ssid[257]; 
  uint8_t	password[257]; 
  uint8_t	ssid_length;

  ssid_length = improv_decode_string(&data[11], ssid);
  improv_decode_string(&data[ssid_length + 12], password);
  improv_state = Provisioning;
  improv_send_current_state(fd);

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
   WIFI_EVENT,
   WIFI_EVENT_STA_CONNECTED,
   &wifi_event_sta_connected_to_ap,
   (void *)fd,
   &handler_wifi_event_sta_connected_to_ap));


  gm_param_set("ssid", (const char *)ssid);
  gm_param_set("wifi_password", (const char *)password);

  ESP_ERROR_CHECK(esp_event_handler_register(
   WIFI_EVENT,
   WIFI_EVENT_STA_DISCONNECTED,
   &wifi_event_sta_disconnected,
   &handler_wifi_event_sta_disconnected));
}

static void
improv_send_device_information(int fd)
{
  unsigned int	offset;
  uint8_t	data[256];
  data[0] = (RequestDeviceInformation & 0xff);
  offset = 2;
  offset += improv_encode_string("K6BP Rigcontrol", &data[offset]);
  offset += improv_encode_string(gm_build_version, &data[offset]);
  offset += improv_encode_string("ESP32 Audio Kit", &data[offset]);
  data[1] = offset - 2;
  improv_send(fd, data, RPC_Result, offset);
}

static void
improv_send_scanned_wifi_networks(int fd)
{
  uint8_t		data[256];
  wifi_ap_record_t *	ap_records;
  uint16_t		number_of_access_points;
  char			rssi[5];

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
      // gm_printf("%sBad command %d.\n", name, type);
      improv_send_error(fd, Invalid_RPC_Packet);
      return Invalid_RPC_Packet;
    }
    break;

  default:
    // gm_printf("%sBad type %d.\n", name, type);
    improv_send_error(fd, Unknown_RPC_Command);
    return Unknown_RPC_Command;
  }

  return NoError;
}

static void
timed_out(int sig)
{
  longjmp(jump, 1);
}

void
gm_improv_wifi(int fd)
{
  // Start the WiFi scan as soon as possible, so that the data is ready when the user
  // wishes to configure WiFi.
  uint8_t		data[256];
  wifi_scan_config_t	config = {};
  improv_error_state_t	error;
  improv_type_t		type;
  uint8_t		length;

  config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  // WiFi scan times are in milliseconds per channel.
  config.scan_time.active.min = 120;
  config.scan_time.active.max = 120;
  config.scan_time.passive = 120;

  ESP_ERROR_CHECK(esp_wifi_scan_start(&config, false));

  gm_printf("Waiting for the Web Updater.\n");
  if ( !setjmp(jump) ) {
    signal(SIGALRM, timed_out);
    alarm(5);
    error = improv_read(fd, data, &type, &length);
    alarm(0);
    signal(SIGALRM, SIG_DFL);

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
  }
  else {
    gm_printf("Not connected to the Web updater.\nStarting the command processor.\n");
    // Timed out without receiving an Improv packet.
    // Return so that the REPL can take over the console.
  }
}
