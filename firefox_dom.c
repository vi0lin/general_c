#include <windows.h>
#include "firefox_dom_cdp.h"
#include "firefox_dom_rdp.h"
#include "timeout.h"
#include <shellapi.h>
/* =========================================================
   Main
   ========================================================= */
int do_firefox_dom(void) {
    // "C:\\Program Files\\Firefox Developer Edition\\firefox.exe",
    ShellExecuteA(NULL, "open",
        "C:\\Program Files\\Mozilla Firefox\\firefox.exe",
        "-start-debugger-server 9224",
        NULL, SW_SHOW);
        // "--remote-debugging-port=9225 --no-remote",
    //    "C:\\Program Files\\Mozilla Firefox\\firefox.exe",
    //    "--remote-debugging-port=9225 --no-remote",
    timeout(10);
    do_firefox_dom_rdp();
    timeout(30000);
    // do_firefox_dom_cdp() {
}

// #include <winsock2.h>
// #include <ws2tcpip.h>
// #include <stdio.h>
// #pragma comment(lib, "ws2_32.lib")
// #define HOST "127.0.0.1"
// #define PORT 9225
// SOCKET connect_to_firefox() {
//     WSADATA wsa;
//     WSAStartup(MAKEWORD(2,2), &wsa);
//     SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
//     struct sockaddr_in addr = {0};
//     addr.sin_family = AF_INET;
//     addr.sin_port   = htons(PORT);
//     inet_pton(AF_INET, HOST, &addr.sin_addr);
//     connect(sock, (struct sockaddr*)&addr, sizeof(addr));
//     return sock;
// }
// 
// // 1. GET /json to list tabs → parse tab's webSocketDebuggerUrl
// // 2. Upgrade to WebSocket
// // 3. Send: {"id":1,"method":"DOM.getDocument","params":{}}
// // 4. Then:  {"id":2,"method":"DOM.getOuterHTML","params":{"nodeId":1}}
// // Launch Firefox with debugging enabled:
// ShellExecuteA(NULL, "open",
//     "C:\\Program Files\\Mozilla Firefox\\firefox.exe",
//     "--remote-debugging-port=9225 --no-remote",
//     NULL, SW_SHOW);
