#include "ObjMsg.h"

#include "WebsocketHost.h"

/** HTTP root handler - serve the embedded file index.html
 *
 * Embedded CMakefiles.txt directive: EMBED_FILES "index.html"
 */
esp_err_t http_root_handler(httpd_req_t *req)
{
    extern const unsigned char index_html_start[] asm("_binary_index_html_start");
    extern const unsigned char index_html_end[] asm("_binary_index_html_end");
    const size_t index_html_size = (index_html_end - index_html_start);
    printf("index.html (%d bytes) from %p to %p\n", index_html_size, index_html_start, index_html_end);
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);
    return ESP_OK;
}

/** HTTP favicon handler - serve the embedded file favicon.ico
 *
 * Embedded CMakefiles.txt directive: EMBED_FILES "favicon.ico"
 */
esp_err_t http_favicon_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    printf("favicon.ico (%d bytes) from %p to %p\n", favicon_ico_size, favicon_ico_end);
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

/** HTTP joy.js handler - serve the embedded file joy.js
 *
 * Embedded CMakefiles.txt directive: EMBED_FILES "joy.js"
 */
esp_err_t http_joy_js_handler(httpd_req_t *req)
{
    extern const unsigned char joy_js_start[] asm("_binary_joy_js_start");
    extern const unsigned char joy_js_end[] asm("_binary_joy_js_end");
    const size_t joy_js_size = (joy_js_end - joy_js_start);
    printf("joy.js (%d bytes) from %p to %p\n", joy_js_size, joy_js_end);
    httpd_resp_send(req, (const char *)joy_js_start, joy_js_size);
    return ESP_OK;
}

void HttpPaths(WebsocketHost& ws)
{
    ws.Add("/", http_root_handler, false);
    ws.Add("/favicon.ico", http_favicon_handler, false);
    ws.Add("/joy.js", http_joy_js_handler, false);
}