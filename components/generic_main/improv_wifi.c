// This implements the Improv WiFi protocol as documented at
// https://www.improv-wifi.com/serial/
// This version is an independent development, from the document above.
// I didn't look at their SDK while developing this.
//
// Copyright 2022 Algorithmic LLC, Licensed under AGPL 3. - Bruce Perens
//
// The Improv protocol isn't directly compatible with running the REPL (console command
// processor) on the same serial connection. It really could be, if the magic number
// was "IMPROV " with a space, the data was something like JSON, and the packet ended
// with a carriage-return character. I suggese that if there's ever a later version.
// I kludge around the incompatibility with the REPL by waiting
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

static void
improv_send(int fd, const uint8_t * data, improv_type_t type, unsigned int length)
{
  uint8_t buffer[256 + 11];
  unsigned int	checksum;

  memcpy(buffer, magic, sizeof(magic));
  buffer[6] = ImprovVersion;
  buffer[7] = (type & 0xff);
  buffer[8] = (length & 0xff);
  memcpy(&buffer[9], data, length);

  for ( unsigned int i = 0; i < 9 + length; i++ )
    checksum += buffer[i];

  buffer[9 + length] = (checksum & 0xff);
}

static void
improv_send_current_state(int fd)
{
  const uint8_t	state = (improv_state & 0xff);
  improv_send(fd, &state, CurrentState, 1);
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
  uint8_t		type;
  uint8_t		length;
  uint8_t		checksum;
  unsigned int		internal_checksum;

  *r_type = 0;
  *r_length = 0;

  for ( ; ; ) {
    if ( (status = read(fd, &c, 1)) != 1 ) {
      if ( status < 0 )
        fprintf(stderr, "%sread failure: %s\n", name, strerror(errno));
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
        fprintf(stderr, "%sversion %d not implemented.\n", name, (int)c);
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
    fprintf(stderr, "%schecksum doesn't match.\n", name);
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
improv_encode_string(uint8_t * data, const char * s)
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
  uint8_t	password_length;

  ssid_length = improv_decode_string(&data[11], ssid);
  password_length = improv_decode_string(&data[ssid_length + 12], password);
  improv_state = Provisioning;
  improv_send_current_state(fd);
}

static void
improv_send_device_information(int fd)
{
}

static void
improv_send_scanned_wifi_networks(int fd)
{
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
      fprintf(stderr, "%sBad command %d.\n", name, type);
      improv_send_error(fd, Invalid_RPC_Packet);
      return Invalid_RPC_Packet;
    }
    break;

  default:
    fprintf(stderr, "%sBad type %d.\n", name, type);
    improv_send_error(fd, Unknown_RPC_Command);
    return Unknown_RPC_Command;
  }

  return NoError;
}
