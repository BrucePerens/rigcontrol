#include <string.h>
#include <stdlib.h>
#include <esp_http_server.h>
#include <miniz.h>
#include "generic_main.h"
#include "compressed_fs.h"

static const uint32_t	maximum_chunk_size = 4096;

// The embedded filesystem.
static const char fs[1] asm("_binary_fs_start");

static esp_err_t
http_root_handler(httpd_req_t *req)
{
  // Redirect to /index.html
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_status(req, "301 Moved Permanently");
  httpd_resp_set_hdr(req, "Location", "/index.html");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

static void
send_chunks(httpd_req_t * req, const char * data, uint32_t size)
{
  while ( size > 0 ) {
    uint32_t chunk_size = size;

    if ( chunk_size > maximum_chunk_size )
      chunk_size = maximum_chunk_size;

    httpd_resp_send_chunk(req, data, chunk_size);
    size -= chunk_size;
    data += chunk_size;
  }
  httpd_resp_send_chunk(req, "", 0);
}

static int
process_decompressed_data(const void * data, int length, void * context)
{
  send_chunks((httpd_req_t *)context, data, length);
  return 1; // Success code.
}

static void
send_uncompressed_file(httpd_req_t * req, const char * fs, const struct compressed_fs_entry * e)
{
  size_t	size = e->compressed_size;

  tinfl_decompress_mem_to_callback(fs + e->data_offset, &size, process_decompressed_data, req, TINFL_FLAG_PARSE_ZLIB_HEADER);
  httpd_resp_send_chunk(req, "", 0);
}

static void
read_file(httpd_req_t * req, const char * fs, const struct compressed_fs_entry * e)
{
  char	buffer[128];
  char * found;

  switch ( e->method ) {
  case ZERO_LENGTH:
    httpd_resp_send_chunk(req, "", 0);
    break;
  case NONE:
    send_chunks(req, fs + e->data_offset, e->size);
    httpd_resp_send_chunk(req, "", 0);
    break;
  case ZLIB:

    if ( httpd_req_get_hdr_value_str(req, "Accept-Encoding", buffer, sizeof(buffer)) == ESP_OK ) {
      if ( (found = strstr(buffer, "deflate")) ) {
        char c = found[7];
        switch ( c ) {
        case ',':
        case ';':
        case '\0':
        case '\r':
        case '\n':
          break;
        default:
          send_uncompressed_file(req, fs, e);
          httpd_resp_send_chunk(req, "", 0);
          return;
        }
      }
    }
    // Send the data compressed, and allow the browser to decompress it.
    httpd_resp_set_hdr(req, "Content-Encoding", "deflate");
    send_chunks(req, fs + e->data_offset, e->compressed_size);
    httpd_resp_send_chunk(req, "", 0);
    break;
  }
}

static esp_err_t
http_file_handler(httpd_req_t *req)
{
  // The header is at the start of the output file, and says where everything else is.
  // The filesystem entries are at the end of the filesystem, after all of the file
  // names and file data, because their table can
  // only be written after all files have been processed. File names and file data
  // are interleaved after the header. Position of things does matter somewhat because
  // it's a block-based FLASH device.
  struct compressed_fs_header * header = (struct compressed_fs_header *)fs;
  struct compressed_fs_entry * entries = (struct compressed_fs_entry *)(fs + header->table_offset);

  const char * path = req->uri;
  if ( *path == '/' )
    path++;

  // Really simple sequential search for now.
  // There would have to be lots of files for this to be a problem.
  for ( int i = 0; i < header->number_of_files; i++ ) {
    const char * const name = (fs + entries[i].name_offset);
    if ( strcmp(name, path) == 0 ) {
      // The file was found. Serve it.
      read_file(req, fs, &entries[i]);
      return ESP_OK;
    }
  }
  // If we get here, the file was not found.
  if ( gm_web_handler_run(req, GET) == 0 ) {
    // We found a handler for this URL. Return OK.
    return ESP_OK;
  }
  else {
    httpd_resp_send_404(req);
    return ESP_OK;
  }
}

void
gm_compressed_fs_web_handlers(httpd_handle_t server)
{
  static const httpd_uri_t root = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = http_root_handler,
      .user_ctx  = NULL
  };
  httpd_register_uri_handler(server, &root);

  static const httpd_uri_t file = {
      .uri       = "/*",
      .method    = HTTP_GET,
      .handler   = http_file_handler,
      .user_ctx  = NULL
  };
  httpd_register_uri_handler(server, &file);
}
