/*

  Copyright (c) 2018 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

/* This is a simple Web application. To start it, open your browser and
   enter "http://localhost:8080" to the address bar. This is just an example
   and to keep it readable all error handling was omitted. */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../libdill.h"

/* The static HTML that will be sent to the browser when it connects. */
const char *html =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "<script>\n"
    "function start() {\n"
    "    var s = new WebSocket('ws://localhost:8081')\n"
    "    s.onmessage = function (event) {\n"
    "      // This will display the incoming WebSocket messages.\n"
    "      document.getElementById('text').value = event.data;\n"
    "    }\n"
    "}\n"
    "</script>\n"
    "</head>\n"
    "<body onload='start();'>\n"
    "<input type='text' id='text' style='border:none;font-size:100px;'/>\n"
    "</body>";

/* Handle individual HTML connection. */
coroutine void html_worker(int s) {
    /* Start HTTP protocol. Client will as for a particular resource, but let's
       ignore that for now. */
    s = http_attach(s);
    assert(s >= 0);
    char command[256];
    char resource[256];
    int rc = http_recvrequest(s, command, sizeof(command),
        resource, sizeof(resource), -1);
    assert(strcmp(command, "GET") == 0);
    assert(rc == 0);

    /* Browser will send some HTTP fields, but we'll ignore them for now. */
    while(1) {
        char name[256];
        char value[256];
        rc = http_recvfield(s, name, sizeof(name), value, sizeof(value), -1);
        if(rc < 0 && errno == EPIPE) break;
        assert(rc == 0);
    }

    /* Send an HTTP reply. */
    rc = http_sendstatus(s, 200, "OK", -1);
    assert(rc == 0);

    /* Perform HTTP terminal handshake. */
    s = http_detach(s, -1);
    assert(s >= 0);

    /* Send the HTML to the browser. */
    rc = bsend(s, html, strlen(html), -1);
    assert(rc == 0);

    /* Close the underlying TCP connection. */
    rc = tcp_close(s, -1);
    assert(rc == 0);
}

/* Handle individual WebSocket connection. */
coroutine void ws_worker(int s) {
    /* Client will as for a particular virtual host and resource, but let's
       ignore that for now. */
    char resource[256];
    char host[256];
    s = ws_attach_server(s, WS_TEXT, resource, sizeof(resource),
        host, sizeof(host), -1);
    assert(s >= 0);

    /* Send some messages to the browser. */
    int c;
    for(c = '9'; c >= '0'; --c) {
        int rc = msend(s, &c, 1, -1);
        assert(rc == 0);
        rc = msleep(now() + 1000);
        assert(rc == 0);
    }
    int rc = msend(s, "Boom!", 5, -1);
    assert(rc == 0);

    /* Perform the final WebSocket handshake. */
    s = ws_detach(s, 1000, "OK", 2, -1);
    assert(s >= 0);

    /* Close the TCP connection. */
    rc = tcp_close(s, -1);
    assert(rc == 0);
}

/* Listen for incoming WebSocket connections on port 8081. */
coroutine void ws_listener(void) {
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 8081, 0);
    assert(rc == 0);
    int ls = tcp_listen(&addr, 10);
    assert(ls >= 0);
    int workers = bundle();
    assert(workers >= 0);
    while(1) {
        int s = tcp_accept(ls, NULL, -1);
        assert(s >= 0);
        rc = bundle_go(workers, ws_worker(s));
        assert(rc == 0);
    }
}

int main(void) {
    /* This will take care of deallocating finished coroutines cleanly. */
    int workers = bundle();
    assert(workers >= 0);

    /* Start listening for WebSocket connections. */
    int rc = bundle_go(workers, ws_listener());
    assert(rc == 0);

    /* Start listening for plain old HTML connections. Port 8080 is used
       instead of 80 to make it possible to run this program without superuser
       privileges. */
    struct ipaddr addr;
    rc = ipaddr_local(&addr, NULL, 8080, 0);
    assert(rc == 0);
    int ls = tcp_listen(&addr, 10);
    assert(ls >= 0);
    while(1) {
        int s = tcp_accept(ls, NULL, -1);
        assert(s >= 0);
        rc = bundle_go(workers, html_worker(s));
        assert(rc == 0);
    }
}

