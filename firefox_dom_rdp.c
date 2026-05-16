// firefox_rdp.c
// Mozilla Remote Debugging Protocol (RDP) — raw TCP JSON
// Launch Firefox: firefox.exe -start-debugger-server 9224
//
// Compile (MSVC): cl firefox_rdp.c /link ws2_32.lib
// Compile (GCC):  gcc firefox_rdp.c -o firefox_rdp.exe -lws2_32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma comment(lib, "ws2_32.lib")
/* =========================================================
   RDP packet format:
     <length>:<json>\0
   e.g.  "53:{"from":"root","applicationType":"gecko"}\0"
   The length is the byte count of the JSON only (no length
   prefix itself, no null terminator).
   ========================================================= */
static SOCKET tcp_connect(const char *host, int port) {
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port   = htons((u_short)port);
    inet_pton(AF_INET, host, &a.sin_addr);
    if (connect(s, (struct sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) {
        fprintf(stderr, "connect() failed: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }
    return s;
}
/* recv exactly n bytes */
static int recv_exact(SOCKET s, char *buf, int n) {
    int got = 0;
    while (got < n) {
        int r = recv(s, buf + got, n - got, 0);
        if (r <= 0) return got;
        got += r;
    }
    return got;
}
/* Read one RDP packet → malloc'd JSON string. Caller frees. */
static char *rdp_recv(SOCKET s) {
    /* Read digits until ':' */
    char lenbuf[16]; int li = 0; char c;
    while (li < (int)sizeof(lenbuf) - 1) {
        if (recv_exact(s, &c, 1) < 1) return NULL;
        if (c == ':') break;
        if (c < '0' || c > '9') { fprintf(stderr, "Bad length char: %d\n", c); return NULL; }
        lenbuf[li++] = c;
    }
    lenbuf[li] = '\0';
    int len = atoi(lenbuf);
    if (len <= 0 || len > 10*1024*1024) { fprintf(stderr, "Bad packet length: %d\n", len); return NULL; }
    char *buf = malloc(len + 1);
    if (recv_exact(s, buf, len) < len) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}
/* Send one RDP packet */
static void rdp_send(SOCKET s, const char *json) {
    int jlen = (int)strlen(json);
    char header[32];
    int hlen = snprintf(header, sizeof(header), "%d:", jlen);
    send(s, header, hlen, 0);
    send(s, json,   jlen, 0);
}
/* Extract string value for a key from flat JSON (no full parser) */
static int json_str(const char *json, const char *key, char *out, int outsz) {
    char search[128]; snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(json, search);
    if (!p) {
        snprintf(search, sizeof(search), "\"%s\": \"", key);
        p = strstr(json, search);
    }
    if (!p) return 0;
    p += strlen(search);
    const char *e = p;
    while (*e && !(*e == '"' && *(e-1) != '\\')) e++;
    int len = (int)(e - p); if (len >= outsz) len = outsz - 1;
    strncpy(out, p, len); out[len] = '\0';
    return 1;
}
/* Extract a JSON array element's field — finds Nth occurrence of key */
static int json_str_nth(const char *json, const char *key, int n, char *out, int outsz) {
    char search[128]; snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = json; int found = 0;
    while ((p = strstr(p, search)) != NULL) {
        if (found++ == n) {
            p += strlen(search);
            const char *e = p;
            while (*e && !(*e == '"' && *(e-1) != '\\')) e++;
            int len = (int)(e - p); if (len >= outsz) len = outsz - 1;
            strncpy(out, p, len); out[len] = '\0';
            return 1;
        }
        p += strlen(search);
    }
    return 0;
}
int variant1() {
    const char *host = "127.0.0.1";
    int port = 9225;
    SOCKET s = tcp_connect(host, port);
    if (s == INVALID_SOCKET) {
        fprintf(stderr, "Could not connect to Firefox RDP on %s:%d\n"
                        "Launch with:  firefox.exe -start-debugger-server %d\n",
                        host, port, port);
        return 1;
    }
    printf("[+] Connected to Firefox RDP\n");
    /* ---- 1. Greeting packet ---- */
    char *hello = rdp_recv(s);
    if (!hello) { fprintf(stderr, "No greeting\n"); return 1; }
    printf("[hello] %s\n\n", hello);
    char root_actor[64] = "root";
    json_str(hello, "from", root_actor, sizeof(root_actor));
    free(hello);
    /* ---- 2. listTabs ---- */
    char req[256];
    snprintf(req, sizeof(req), "{\"to\":\"%s\",\"type\":\"listTabs\"}", root_actor);
    rdp_send(s, req);
    printf("[>] listTabs\n");
    char *tabs_resp = rdp_recv(s);
    if (!tabs_resp) { fprintf(stderr, "No listTabs response\n"); return 1; }
    printf("[listTabs] %.*s\n\n", 800, tabs_resp); /* print first 800 chars */
    /* Pick first tab actor */
    char tab_actor[128] = {0};
    /* tabs are in "tabs":[{"actor":"...","title":"..."},...] */
    /* find first "actor" inside the tabs array */
    const char *tabs_arr = strstr(tabs_resp, "\"tabs\"");
    if (!tabs_arr) tabs_arr = tabs_resp;
    json_str_nth(tabs_arr, "actor", 0, tab_actor, sizeof(tab_actor));
    printf("[+] Tab actor: %s\n", tab_actor);
    if (!tab_actor[0]) {
        fprintf(stderr, "No tab actor found. Is a page open in Firefox?\n");
        free(tabs_resp);
        return 1;
    }
    free(tabs_resp);
    /* ---- 3. Attach to tab ---- */
    snprintf(req, sizeof(req), "{\"to\":\"%s\",\"type\":\"attach\"}", tab_actor);
    rdp_send(s, req);
    printf("[>] attach tab\n");
    /* May receive several packets (tabAttached, frameUpdate, etc.) */
    char console_actor[128] = {0};
    char inspector_actor[128] = {0};
    for (int i = 0; i < 6; i++) {
        char *pkt = rdp_recv(s);
        if (!pkt) break;
        printf("[attach pkt %d] %.*s\n", i, 400, pkt);
        if (!console_actor[0])   json_str(pkt, "consoleActor",   console_actor,   sizeof(console_actor));
        if (!inspector_actor[0]) json_str(pkt, "inspectorActor", inspector_actor, sizeof(inspector_actor));
        free(pkt);
        if (inspector_actor[0]) break;
    }
    printf("[+] Inspector actor: %s\n", inspector_actor);
    /* ---- 4. Get inspector → walker → document node ---- */
    if (!inspector_actor[0]) {
        fprintf(stderr, "No inspector actor. Trying getWalker on tab directly.\n");
        /* Older Firefox: send getWalker directly to tab actor */
        snprintf(inspector_actor, sizeof(inspector_actor), "%s", tab_actor);
    }
    snprintf(req, sizeof(req), "{\"to\":\"%s\",\"type\":\"getWalker\"}", inspector_actor);
    rdp_send(s, req);
    printf("[>] getWalker\n");
    char walker_actor[128] = {0};
    for (int i = 0; i < 4; i++) {
        char *pkt = rdp_recv(s);
        if (!pkt) break;
        printf("[walker pkt %d] %.*s\n", i, 400, pkt);
        if (!walker_actor[0]) json_str(pkt, "actor", walker_actor, sizeof(walker_actor));
        /* walker response has "walker":{"actor":"..."} */
        const char *wp = strstr(pkt, "\"walker\"");
        if (wp) json_str(wp, "actor", walker_actor, sizeof(walker_actor));
        free(pkt);
        if (walker_actor[0]) break;
    }
    printf("[+] Walker actor: %s\n", walker_actor);
    /* ---- 5. documentElement → outerHTML via innerHTML ---- */
    /* Ask walker for the document node */
    snprintf(req, sizeof(req), "{\"to\":\"%s\",\"type\":\"document\"}", walker_actor);
    rdp_send(s, req);
    printf("[>] document\n");
    char doc_actor[128] = {0};
    for (int i = 0; i < 3; i++) {
        char *pkt = rdp_recv(s);
        if (!pkt) break;
        printf("[doc pkt %d] %.*s\n", i, 400, pkt);
        if (!doc_actor[0]) {
            const char *np = strstr(pkt, "\"node\"");
            if (np) json_str(np, "actor", doc_actor, sizeof(doc_actor));
        }
        free(pkt);
        if (doc_actor[0]) break;
    }
    printf("[+] Document node actor: %s\n", doc_actor);
    /* ---- 6. outerHTML ---- */
    snprintf(req, sizeof(req), "{\"to\":\"%s\",\"type\":\"outerHTML\"}", doc_actor);
    rdp_send(s, req);
    printf("[>] outerHTML\n");
    char *html_pkt = rdp_recv(s);
    if (html_pkt) {
        /* value is in "value":"<!DOCTYPE html>..." */
        char *vp = strstr(html_pkt, "\"value\":\"");
        if (vp) {
            vp += strlen("\"value\":\"");
            /* Print first 2KB */
            printf("\n===== outerHTML (first 2KB) =====\n");
            for (int i = 0; i < 2048 && vp[i]; i++) {
                if (vp[i] == '\\' && vp[i+1] == 'n') { putchar('\n'); i++; }
                else if (vp[i] == '\\' && vp[i+1] == '"') { putchar('"'); i++; }
                else putchar((unsigned char)vp[i]);
            }
            printf("\n...\n");
        }
        FILE *f = fopen("dom_output.json", "wb");
        if (f) { fwrite(html_pkt, 1, strlen(html_pkt), f); fclose(f); }
        printf("[+] Full packet saved to dom_output.json\n");
        free(html_pkt);
    }
    closesocket(s);
    WSACleanup();
    return 0;
}

/* =========================================================
   Buffered socket reader — fixes rdp_recv hanging
   ========================================================= */
#define SOCKBUF_SIZE (1 << 20)  /* 1 MB */
typedef struct {
    SOCKET s;
    char  *buf;
    int    start;   /* first valid byte */
    int    end;     /* one past last valid byte */
} SockBuf;
static SockBuf *sockbuf_new(SOCKET s) {
    SockBuf *sb = malloc(sizeof(SockBuf));
    sb->s     = s;
    sb->buf   = malloc(SOCKBUF_SIZE);
    sb->start = 0;
    sb->end   = 0;
    return sb;
}
/* Pull more data from socket into buffer */
static int sockbuf_fill(SockBuf *sb) {
    if (sb->start > 0) {
        /* Compact: shift unread data to front */
        int remaining = sb->end - sb->start;
        memmove(sb->buf, sb->buf + sb->start, remaining);
        sb->end   = remaining;
        sb->start = 0;
    }
    int space = SOCKBUF_SIZE - sb->end;
    if (space <= 0) return -1; /* buffer full */
    int r = recv(sb->s, sb->buf + sb->end, space, 0);
    if (r > 0) sb->end += r;
    return r;
}
/* Read exactly 1 byte, blocking until available */
static int sockbuf_getc(SockBuf *sb, char *out) {
    while (sb->start >= sb->end) {
        int r = sockbuf_fill(sb);
        if (r <= 0) return 0; /* disconnected */
    }
    *out = sb->buf[sb->start++];
    return 1;
}
/* Read exactly n bytes, blocking until available */
static int sockbuf_read(SockBuf *sb, char *out, int n) {
    int got = 0;
    while (got < n) {
        while (sb->start >= sb->end) {
            int r = sockbuf_fill(sb);
            if (r <= 0) return got;
        }
        int avail = sb->end - sb->start;
        int take  = avail < (n - got) ? avail : (n - got);
        memcpy(out + got, sb->buf + sb->start, take);
        sb->start += take;
        got       += take;
    }
    return got;
}
/* =========================================================
   RDP packet receive — uses SockBuf, never hangs
   Format:  <digits> ':' <json_bytes> '\0'
            e.g.  "53:{"from":"root",...}\0"
   ========================================================= */
static char *rdp_recv_v2(SockBuf *sb) {
    /* --- read length digits until ':' --- */
    char lenbuf[16]; int li = 0; char c;
    for (;;) {
        if (!sockbuf_getc(sb, &c)) {
            fprintf(stderr, "[rdp_recv] socket closed reading length\n");
            return NULL;
        }
        if (c == ':') break;
        if (c == '\0') continue;           /* skip stray nulls between packets */
        if (c < '0' || c > '9') {
            fprintf(stderr, "[rdp_recv] unexpected char 0x%02X reading length\n",
                    (unsigned char)c);
            return NULL;
        }
        if (li < (int)sizeof(lenbuf) - 1) lenbuf[li++] = c;
    }
    lenbuf[li] = '\0';
    if (li == 0) {
        fprintf(stderr, "[rdp_recv] empty length field\n");
        return NULL;
    }
    int len = atoi(lenbuf);
    if (len <= 0 || len > 8 * 1024 * 1024) {
        fprintf(stderr, "[rdp_recv] bad length: %d\n", len);
        return NULL;
    }
    /* --- read exactly len bytes of JSON --- */
    char *pkt = malloc(len + 1);
    int got = sockbuf_read(sb, pkt, len);
    pkt[got] = '\0';
    if (got < len) {
        fprintf(stderr, "[rdp_recv] short read: got %d of %d\n", got, len);
        free(pkt);
        return NULL;
    }
    /* --- consume trailing '\0' if present --- */
    /* peek at next byte without consuming if it's not '\0' */
    if (sb->start < sb->end && sb->buf[sb->start] == '\0')
        sb->start++;
    return pkt;
}
/* Send one RDP packet — unchanged, just shown for completeness */
static void rdp_send_v2(SOCKET s, const char *json) {
    int jlen = (int)strlen(json);
    char header[32];
    int hlen = snprintf(header, sizeof(header), "%d:", jlen);
    send(s, header, hlen, 0);
    send(s, json,   jlen, 0);
}
static int variant2() {
    const char *host = "127.0.0.1";
    int port = 9224;
    SOCKET s = tcp_connect(host, port);
    if (s == INVALID_SOCKET) return 1;
    printf("[+] Connected\n");
    /* Create buffered reader — pass sb everywhere instead of s */
    SockBuf *sb = sockbuf_new(s);
    char *hello = rdp_recv_v2(sb);
    if (!hello) { fprintf(stderr, "No greeting\n"); return 1; }
    printf("[hello] %s\n\n", hello);
    char root_actor[64] = "root";
    json_str(hello, "from", root_actor, sizeof(root_actor));
    free(hello);
    /* listTabs */
    char req[256];
    snprintf(req, sizeof(req), "{\"to\":\"%s\",\"type\":\"listTabs\"}", root_actor);
    rdp_send_v2(s, req);
    char *tabs_resp = rdp_recv_v2(sb);   /* <-- uses sb now, won't hang */
    if (!tabs_resp) { fprintf(stderr, "No listTabs response\n"); return 1; }
    printf("[listTabs] %.800s\n\n", tabs_resp);
    /* ... rest of your code, replace every rdp_recv(s) with rdp_recv(sb) ... */
}

// attach to tab  →  Firefox sends "tabNavigated", "mutations" events automatically
// reflow walker  →  request "watchRootNode" to get MutationRecord pushes
// firefox_dom_watch.c
// Compile (MSVC): cl firefox_dom_watch.c /link ws2_32.lib
// Compile (GCC):  gcc firefox_dom_watch.c -o firefox_dom_watch.exe -lws2_32
//
// Launch Firefox: firefox.exe -start-debugger-server 9224
// #define WIN32_LEAN_AND_MEAN
// #include <windows.h>
// #include <winsock2.h>
// #include <ws2tcpip.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <stdint.h>
// #pragma comment(lib, "ws2_32.lib")
/* =========================================================
   Config — edit these
   ========================================================= */
// #define SOCKBUF_SIZE_v3 (1 << 20)  /* 1 MB */
#define RDP_HOST          "127.0.0.1"
#define RDP_PORT          9224
#define POLL_INTERVAL_MS  1000       /* for polling mode */
/* Tags you want to extract content from */
static const char *WATCH_TAGS[] = { "title", "textarea", "input", "img","h1", "h2", "p", "span", "div", NULL };
/* =========================================================
   Buffered socket reader
   ========================================================= */
#define SOCKBUF_SIZE_v3 (2 << 20)
// typedef struct { SOCKET s; char *buf; int start, end; } SockBuf;
static SockBuf *sockbuf_new_v3(SOCKET s) {
    SockBuf *sb = malloc(sizeof(SockBuf));
    sb->s = s; sb->buf = malloc(SOCKBUF_SIZE_v3); sb->start = sb->end = 0;
    return sb;
}
static int sockbuf_fill_v3(SockBuf *sb) {
    if (sb->start > 0) {
        int rem = sb->end - sb->start;
        memmove(sb->buf, sb->buf + sb->start, rem);
        sb->end = rem; sb->start = 0;
    }
    int r = recv(sb->s, sb->buf + sb->end, SOCKBUF_SIZE_v3 - sb->end, 0);
    if (r > 0) sb->end += r;
    return r;
}
static int sockbuf_getc_v3(SockBuf *sb, char *out) {
    while (sb->start >= sb->end) if (sockbuf_fill_v3(sb) <= 0) return 0;
    *out = sb->buf[sb->start++]; return 1;
}
static int sockbuf_read_v3(SockBuf *sb, char *out, int n) {
    int got = 0;
    while (got < n) {
        while (sb->start >= sb->end) if (sockbuf_fill_v3(sb) <= 0) return got;
        int take = sb->end - sb->start; if (take > n - got) take = n - got;
        memcpy(out + got, sb->buf + sb->start, take);
        sb->start += take; got += take;
    }
    return got;
}
/* =========================================================
   RDP framing
   ========================================================= */
static char *rdp_recv_v3(SockBuf *sb) {
    char lenbuf[16]; int li = 0; char c;
    for (;;) {
        if (!sockbuf_getc_v3(sb, &c)) return NULL;
        if (c == ':') break;
        if (c == '\0') continue;
        if (c < '0' || c > '9') return NULL;
        if (li < 15) lenbuf[li++] = c;
    }
    lenbuf[li] = '\0';
    int len = atoi(lenbuf);
    if (len <= 0 || len > 8*1024*1024) return NULL;
    char *pkt = malloc(len + 1);
    sockbuf_read_v3(sb, pkt, len);
    pkt[len] = '\0';
    if (sb->start < sb->end && sb->buf[sb->start] == '\0') sb->start++;
    return pkt;
}
static void rdp_send_v3(SOCKET s, const char *json) {
    int jlen = (int)strlen(json);
    char hdr[32]; int hlen = snprintf(hdr, sizeof(hdr), "%d:", jlen);
    send(s, hdr, hlen, 0);
    send(s, json, jlen, 0);
}
/* Formatted send — like printf but sends as RDP packet */
static void rdp_sendf_v3(SOCKET s, const char *fmt, ...) {
    char buf[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    rdp_send_v3(s, buf);
}
/* =========================================================
   JSON helpers
   ========================================================= */
static int json_str_v3(const char *json, const char *key, char *out, int outsz) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(json, search);
    if (!p) { snprintf(search, sizeof(search), "\"%s\": \"", key); p = strstr(json, search); }
    if (!p) return 0;
    p += strlen(search);
    const char *e = p;
    while (*e && !(*e == '"' && *(e-1) != '\\')) e++;
    int len = (int)(e - p); if (len >= outsz) len = outsz - 1;
    strncpy(out, p, len); out[len] = '\0';
    return 1;
}
/* Find Nth occurrence of key's string value */
static int json_str_nth_v3(const char *json, const char *key, int n, char *out, int outsz) {
    char search[128]; snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = json; int found = 0;
    while ((p = strstr(p, search)) != NULL) {
        if (found++ == n) {
            p += strlen(search);
            const char *e = p;
            while (*e && !(*e == '"' && *(e-1) != '\\')) e++;
            int len = (int)(e - p); if (len >= outsz) len = outsz - 1;
            strncpy(out, p, len); out[len] = '\0';
            return 1;
        }
        p += strlen(search);
    }
    return 0;
}
/* =========================================================
   Tab listing and selection
   ========================================================= */
typedef struct { char actor[128]; char title[256]; char url[512]; } Tab;
static int list_tabs(SOCKET s, SockBuf *sb, const char *root_actor,
                     Tab *tabs, int max_tabs) {
    rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"listTabs\"}", root_actor);
    char *resp = rdp_recv_v3(sb);
    if (!resp) return 0;
    int count = 0;
    const char *p = resp;
    /* Each tab entry: {"actor":"...","title":"...","url":"..."} */
    while (count < max_tabs) {
        char actor[128], title[256], url[512];
        if (!json_str_nth_v3(p, "actor", count, actor, sizeof(actor))) break;
        json_str_nth_v3(p, "title", count, title, sizeof(title));
        json_str_nth_v3(p, "url",   count, url,   sizeof(url));
        strncpy(tabs[count].actor, actor, sizeof(tabs[count].actor)-1);
        strncpy(tabs[count].title, title, sizeof(tabs[count].title)-1);
        strncpy(tabs[count].url,   url,   sizeof(tabs[count].url)-1);
        count++;
    }
    free(resp);
    return count;
}
static void print_tabs(Tab *tabs, int count) {
    printf("\n=== Open Tabs (%d) ===\n", count);
    for (int i = 0; i < count; i++)
        printf("  [%d] %s\n      %s\n", i, tabs[i].title, tabs[i].url);
    printf("\n");
}
/* Set socket receive timeout (ms). 0 = blocking. */
static void set_recv_timeout(SOCKET s, int ms) {
    DWORD tv = (DWORD)ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
}
/* =========================================================
   Connect to a tab — returns walker actor
   ========================================================= */
// static int attach_tab(SOCKET s, SockBuf *sb, const char *tab_actor,
//                       char *inspector_out, char *walker_out) {
//     /* attach */
//     rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"attach\"}", tab_actor);
//     char console_actor[128]   = {0};
//     char inspector_actor[128] = {0};
//     for (int i = 0; i < 8; i++) {
//         char *pkt = rdp_recv_v3(sb);
//         printf(".");
//         if (!pkt) break;
//         if (!inspector_actor[0]) json_str_v3(pkt, "inspectorActor", inspector_actor, sizeof(inspector_actor));
//         if (!console_actor[0])   json_str_v3(pkt, "consoleActor",   console_actor,   sizeof(console_actor));
//         free(pkt);
//         if (inspector_actor[0]) break;
//         printf(".");
//     }
//     if (!inspector_actor[0]) { fprintf(stderr, "No inspector actor\n"); return 0; }
//     strncpy(inspector_out, inspector_actor, 128);
//     /* getWalker */
//     rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"getWalker\"}", inspector_actor);
//     char walker_actor[128] = {0};
//     for (int i = 0; i < 4; i++) {
//         char *pkt = rdp_recv_v3(sb); if (!pkt) break;
//         const char *wp = strstr(pkt, "\"walker\"");
//         if (wp) json_str_v3(wp, "actor", walker_actor, sizeof(walker_actor));
//         if (!walker_actor[0]) json_str_v3(pkt, "actor", walker_actor, sizeof(walker_actor));
//         free(pkt);
//         if (walker_actor[0]) break;
//     }
//     if (!walker_actor[0]) { fprintf(stderr, "No walker actor\n"); return 0; }
//     strncpy(walker_out, walker_actor, 128);
//     return 1;
// }
// static int attach_tab(SOCKET s, SockBuf *sb, const char *tab_actor,
//                       char *inspector_out, char *walker_out) {
//     /* --- Step 1: attach --- */
//     rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"attach\"}", tab_actor);
//     char inspector_actor[128] = {0};
//     /* Drain until we find inspectorActor OR hit 16 packets with no more data */
//     set_recv_timeout(sb->s, 500);   /* 500 ms between packets */
//     for (;;) {
//         char *pkt = rdp_recv_v3(sb);
//         if (!pkt) break;            /* timeout = no more packets coming */
//         printf("[attach] %.300s\n", pkt);
//         if (!inspector_actor[0])
//             json_str_v3(pkt, "inspectorActor", inspector_actor, sizeof(inspector_actor));
//         free(pkt);
//     }
//     set_recv_timeout(sb->s, 0);     /* restore blocking */
//     if (!inspector_actor[0]) {
//         fprintf(stderr, "[-] No inspectorActor found after attach\n");
//         return 0;
//     }
//     strncpy(inspector_out, inspector_actor, 128);
//     printf("[+] Inspector: %s\n", inspector_actor);
//     /* --- Step 2: getWalker --- */
//     rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"getWalker\"}", inspector_actor);
//     char walker_actor[128] = {0};
//     set_recv_timeout(sb->s, 500);
//     for (;;) {
//         char *pkt = rdp_recv_v3(sb);
//         if (!pkt) break;            /* timeout = done */
//         printf("[walker] %.300s\n", pkt);
//         /* walker response: {"walker":{"actor":"..."},...} */
//         const char *wp = strstr(pkt, "\"walker\"");
//         if (wp) json_str_v3(wp, "actor", walker_actor, sizeof(walker_actor));
//         /* fallback: direct actor field */
//         if (!walker_actor[0])
//             json_str_v3(pkt, "actor", walker_actor, sizeof(walker_actor));
//         free(pkt);
//         if (walker_actor[0]) break; /* got it, stop waiting */
//     }
//     set_recv_timeout(sb->s, 0);
//     if (!walker_actor[0]) {
//         fprintf(stderr, "[-] No walker actor found\n");
//         return 0;
//     }
//     strncpy(walker_out, walker_actor, 128);
//     printf("[+] Walker: %s\n", walker_actor);
//     return 1;
// }
static char *fetch_outerhtml(SOCKET s, SockBuf *sb, const char *walker_actor) {
    /* --- Step 1: get document node --- */
    rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"document\"}", walker_actor);
    char doc_actor[128] = {0};
    set_recv_timeout(sb->s, 2000);
    for (;;) {
        char *pkt = rdp_recv_v3(sb);
        if (!pkt) break;
        printf("[document] %s\n\n", pkt);   /* <-- full packet, no truncation */
        /* try every likely location for the node actor */
        const char *np;
        if ((np = strstr(pkt, "\"node\"")))
            json_str_v3(np, "actor", doc_actor, sizeof(doc_actor));
        if (!doc_actor[0] && (np = strstr(pkt, "\"document\"")))
            json_str_v3(np, "actor", doc_actor, sizeof(doc_actor));
        if (!doc_actor[0])
            json_str_v3(pkt, "actor", doc_actor, sizeof(doc_actor));
        free(pkt);
        if (doc_actor[0]) break;
    }
    set_recv_timeout(sb->s, 0);
    if (!doc_actor[0]) {
        fprintf(stderr, "[-] No document actor — probing walker for valid types\n");
        /* probe what the walker actually accepts */
        const char *probes[] = { "document", "documentElement", "getRootNode",
                                  "getDocument", "root", NULL };
        for (int i = 0; probes[i]; i++) {
            // rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"%s\"}", walker_actor, probes[i]);
            rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"%s\",\"node\":{\"actor\":\"%s\"}}", walker_actor, probes[i], doc_actor);
            set_recv_timeout(sb->s, 1000);
            char *pkt = rdp_recv_v3(sb);
            if (pkt) { printf("[probe:%s] %s\n\n", probes[i], pkt); free(pkt); }
            set_recv_timeout(sb->s, 0);
        }
        return NULL;
    }
    printf("[+] Document actor: %s\n", doc_actor);
    /* --- Step 2: outerHTML --- */
    /* probe multiple type names in case Firefox version differs */
    const char *html_types[] = { "outerHTML", "getOuterHTML", "innerHTML", NULL };
    for (int i = 0; html_types[i]; i++) {
        // rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"%s\"}", doc_actor, html_types[i]);
        rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"%s\",\"node\":{\"actor\":\"%s\"}}", walker_actor, html_types[i], doc_actor);
        set_recv_timeout(sb->s, 2000);
        for (;;) {
            char *pkt = rdp_recv_v3(sb);
            if (!pkt) break;
            printf("[%s] %.500s\n\n", html_types[i], pkt);
            if (strstr(pkt, "\"value\"") || strstr(pkt, "\"html\"")) {
                set_recv_timeout(sb->s, 0);
                return pkt; /* caller frees */
            }
            /* if unrecognized, try next type */
            if (strstr(pkt, "unrecognizedPacketType")) {
                free(pkt); break;
            }
            free(pkt);
        }
        set_recv_timeout(sb->s, 0);
    }
    /* --- Fallback: use console actor to eval outerHTML --- */
    fprintf(stderr, "[-] outerHTML via walker failed — trying console eval fallback\n");
    return NULL;
}
static int attach_tab(SOCKET s, SockBuf *sb, const char *tab_actor,
                      char *inspector_out, char *walker_out) {
    /* --- Step 1: getTarget (replaces old 'attach') --- */
    rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"getTarget\"}", tab_actor);
    char target_actor[128]    = {0};
    char inspector_actor[128] = {0};
    set_recv_timeout(sb->s, 2000);
    for (;;) {
        char *pkt = rdp_recv_v3(sb);
        if (!pkt) break;
        printf("[getTarget] %.400s\n", pkt);
        /* Response: {"frame":{"actor":"...","inspectorActor":"..."},...} */
        const char *fp = strstr(pkt, "\"frame\"");
        if (fp) {
            json_str_v3(fp, "actor",          target_actor,    sizeof(target_actor));
            json_str_v3(fp, "inspectorActor", inspector_actor, sizeof(inspector_actor));
        }
        /* Also try flat (some FF versions put it at top level) */
        if (!target_actor[0])
            json_str_v3(pkt, "actor", target_actor, sizeof(target_actor));
        if (!inspector_actor[0])
            json_str_v3(pkt, "inspectorActor", inspector_actor, sizeof(inspector_actor));
        free(pkt);
        if (inspector_actor[0] || target_actor[0]) break;
    }
    set_recv_timeout(sb->s, 0);
    printf("[+] Target actor:    %s\n", target_actor);
    printf("[+] Inspector actor: %s\n", inspector_actor);
    /* --- Step 2: if inspectorActor not in getTarget response, request it --- */
    if (!inspector_actor[0]) {
        if (!target_actor[0]) {
            fprintf(stderr, "[-] No target actor from getTarget\n");
            return 0;
        }
        rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"getInspector\"}", target_actor);
        set_recv_timeout(sb->s, 2000);
        for (;;) {
            char *pkt = rdp_recv_v3(sb);
            if (!pkt) break;
            printf("[getInspector] %.400s\n", pkt);
            json_str_v3(pkt, "actor", inspector_actor, sizeof(inspector_actor));
            free(pkt);
            if (inspector_actor[0]) break;
        }
        set_recv_timeout(sb->s, 0);
    }
    if (!inspector_actor[0]) {
        fprintf(stderr, "[-] No inspector actor\n");
        return 0;
    }
    strncpy(inspector_out, inspector_actor, 128);
    /* --- Step 3: getWalker --- */
    rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"getWalker\"}", inspector_actor);
    char walker_actor[128] = {0};
    set_recv_timeout(sb->s, 2000);
    for (;;) {
        char *pkt = rdp_recv_v3(sb);
        if (!pkt) break;
        printf("[getWalker] %.400s\n", pkt);
        const char *wp = strstr(pkt, "\"walker\"");
        if (wp) json_str_v3(wp, "actor", walker_actor, sizeof(walker_actor));
        if (!walker_actor[0])
            json_str_v3(pkt, "actor", walker_actor, sizeof(walker_actor));
        free(pkt);
        if (walker_actor[0]) break;
    }
    set_recv_timeout(sb->s, 0);
    if (!walker_actor[0]) {
        fprintf(stderr, "[-] No walker actor\n");
        return 0;
    }
    strncpy(walker_out, walker_actor, 128);
    printf("[+] Walker: %s\n", walker_actor);
    return 1;
}
/* Dump all fields the tab descriptor responds to */
// const char *probe_types[] = { "getTarget", "getFavicon", "getWatcher", NULL };
// for (int i = 0; probe_types[i]; i++) {
//     rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"%s\"}", tabs[tab_idx].actor, probe_types[i]);
//     set_recv_timeout(sb->s, 1000);
//     char *pkt = rdp_recv_v3(sb);
//     if (pkt) { printf("[probe:%s] %s\n\n", probe_types[i], pkt); free(pkt); }
//     set_recv_timeout(sb->s, 0);
// }

/* =========================================================
   Fetch outerHTML for a walker
   ========================================================= */
// static char *fetch_outerhtml(SOCKET s, SockBuf *sb, const char *walker_actor) {
//     /* document node */
//     rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"document\"}", walker_actor);
//     char doc_actor[128] = {0};
//     for (int i = 0; i < 4; i++) {
//         char *pkt = rdp_recv_v3(sb); if (!pkt) break;
//         const char *np = strstr(pkt, "\"node\"");
//         if (np) json_str_v3(np, "actor", doc_actor, sizeof(doc_actor));
//         free(pkt);
//         if (doc_actor[0]) break;
//     }
//     if (!doc_actor[0]) return NULL;
//     /* outerHTML */
//     rdp_sendf_v3(s, "{\"to\":\"%s\",\"type\":\"outerHTML\"}", doc_actor);
//     for (int i = 0; i < 4; i++) {
//         char *pkt = rdp_recv_v3(sb); if (!pkt) break;
//         if (strstr(pkt, "\"value\"")) return pkt; /* caller frees */
//         free(pkt);
//     }
//     return NULL;
// }
/* =========================================================
   HTML tag content extractor
   Finds all <tag>content</tag> in html, calls cb for each
   ========================================================= */
typedef void (*tag_cb)(const char *tag, const char *content, void *userdata);
static void extract_tags(const char *html, const char *tag, tag_cb cb, void *ud) {
    char open[64], close[64];
    snprintf(open,  sizeof(open),  "<%s",  tag);
    snprintf(close, sizeof(close), "</%s>", tag);
    const char *p = html;
    while ((p = strstr(p, open)) != NULL) {
        /* skip to end of opening tag (handles attributes) */
        const char *gt = strchr(p, '>');
        if (!gt) break;
        gt++; /* content starts here */
        const char *end = strstr(gt, close);
        if (!end) { p = gt; continue; }
        /* copy content, strip inner tags */
        int len = (int)(end - gt);
        char *raw = malloc(len + 1);
        strncpy(raw, gt, len); raw[len] = '\0';
        /* strip HTML tags from content */
        char *clean = malloc(len + 1); int ci = 0;
        int in_tag = 0;
        for (int i = 0; i < len; i++) {
            if      (raw[i] == '<') in_tag = 1;
            else if (raw[i] == '>') in_tag = 0;
            else if (!in_tag)       clean[ci++] = raw[i];
        }
        clean[ci] = '\0';
        /* decode common JSON escape sequences (HTML came via JSON) */
        char *decoded = malloc(ci + 1); int di = 0;
        for (int i = 0; i < ci; i++) {
            if (clean[i] == '\\' && i+1 < ci) {
                i++;
                if      (clean[i] == 'n')  decoded[di++] = '\n';
                else if (clean[i] == 't')  decoded[di++] = '\t';
                else if (clean[i] == '"')  decoded[di++] = '"';
                else if (clean[i] == '\\') decoded[di++] = '\\';
                else { decoded[di++] = '\\'; decoded[di++] = clean[i]; }
            } else {
                decoded[di++] = clean[i];
            }
        }
        decoded[di] = '\0';
        /* trim whitespace */
        char *trimmed = decoded;
        while (*trimmed == ' ' || *trimmed == '\n' || *trimmed == '\r' || *trimmed == '\t')
            trimmed++;
        int tlen = (int)strlen(trimmed);
        while (tlen > 0 && (trimmed[tlen-1]==' '||trimmed[tlen-1]=='\n'||
                             trimmed[tlen-1]=='\r'||trimmed[tlen-1]=='\t'))
            trimmed[--tlen] = '\0';
        if (tlen > 0) cb(tag, trimmed, ud);
        free(raw); free(clean); free(decoded);
        p = end + strlen(close);
    }
}
/* =========================================================
   Mutation event listener
   Firefox sends "mutations" packets when DOM changes.
   We set a non-blocking timeout and drain all pending events.
   ========================================================= */
/* Try to read one packet with a timeout. Returns NULL on timeout or error. */
static char *rdp_recv_timeout(SockBuf *sb, int timeout_ms) {
    set_recv_timeout(sb->s, timeout_ms);
    char *pkt = rdp_recv_v3(sb);
    set_recv_timeout(sb->s, 0); /* restore blocking */
    return pkt;
}
/* Drain all pending events. Returns 1 if any "mutations" event was seen. */
// static int drain_events_v3(SockBuf *sb, int timeout_ms) {
//     int had_mutation = 0;
//     char *pkt;
//     while ((pkt = rdp_recv_timeout(sb, timeout_ms)) != NULL) {
//         if (strstr(pkt, "\"mutations\"") || strstr(pkt, "mutation")) {
//             printf("[event] DOM mutation received\n");
//             had_mutation = 1;
//         } else if (strstr(pkt, "\"tabNavigated\"")) {
//             printf("[event] Tab navigated\n");
//             had_mutation = 1; /* re-fetch after navigation */
//         }
//         free(pkt);
//         timeout_ms = 50; /* after first packet, short-poll for more */
//     }
//     return had_mutation;
// }
/* =========================================================
   Drain events — returns reason for wakeup
   ========================================================= */
#define WAKE_TIMEOUT    0
#define WAKE_MUTATION   1
#define WAKE_NAVIGATED  2
#define WAKE_DISCONNECT 3
static int drain_events_v3(SockBuf *sb, int timeout_ms) {
    int result = WAKE_TIMEOUT;
    char *pkt;
    set_recv_timeout(sb->s, timeout_ms);
    while ((pkt = rdp_recv_v3(sb)) != NULL) {
        printf("[event] %.200s\n", pkt);
        if (strstr(pkt, "tabNavigated") || strstr(pkt, "frameUpdate") ||
            strstr(pkt, "tabDetached")  || strstr(pkt, "newURI")) {
            /* Only upgrade to NAVIGATED if we haven't seen disconnect */
            if (result != WAKE_DISCONNECT) result = WAKE_NAVIGATED;
        } else if (strstr(pkt, "mutations") || strstr(pkt, "mutation")) {
            if (result == WAKE_TIMEOUT) result = WAKE_MUTATION;
        } else if (strstr(pkt, "\"error\"")) {
            result = WAKE_DISCONNECT;
        }
        free(pkt);
        set_recv_timeout(sb->s, 50); /* short-poll for more queued events */
    }
    set_recv_timeout(sb->s, 0);
    return result;
}
/* =========================================================
   Tag change detector — compares old vs new extracted values
   ========================================================= */
#define MAX_RESULTS 512
typedef struct { char tag[32]; char content[512]; } TagResult;
typedef struct {
    TagResult results[MAX_RESULTS];
    int       count;
} TagSnapshot;
static void snapshot_cb(const char *tag, const char *content, void *ud) {
    TagSnapshot *snap = (TagSnapshot*)ud;
    if (snap->count >= MAX_RESULTS) return;
    strncpy(snap->results[snap->count].tag,     tag,     31);
    strncpy(snap->results[snap->count].content, content, 511);
    snap->count++;
}
static void take_snapshot(const char *html, TagSnapshot *snap) {
    snap->count = 0;
    for (int i = 0; WATCH_TAGS[i]; i++)
        extract_tags(html, WATCH_TAGS[i], snapshot_cb, snap);
}
static void diff_snapshots(const TagSnapshot *old_snap, const TagSnapshot *new_snap) {
    /* Find added/changed entries */
    for (int i = 0; i < new_snap->count; i++) {
        const TagResult *nr = &new_snap->results[i];
        int found = 0;
        for (int j = 0; j < old_snap->count; j++) {
            const TagResult *or_ = &old_snap->results[j];
            if (strcmp(nr->tag, or_->tag) == 0 && strcmp(nr->content, or_->content) == 0) {
                found = 1; break;
            }
        }
        if (!found)
            printf("  [+] <%s>: %s\n", nr->tag, nr->content);
    }
    /* Find removed entries */
    for (int i = 0; i < old_snap->count; i++) {
        const TagResult *or_ = &old_snap->results[i];
        int found = 0;
        for (int j = 0; j < new_snap->count; j++) {
            const TagResult *nr = &new_snap->results[j];
            if (strcmp(or_->tag, nr->tag) == 0 && strcmp(or_->content, nr->content) == 0) {
                found = 1; break;
            }
        }
        if (!found)
            printf("  [-] <%s>: %s\n", or_->tag, or_->content);
    }
}
/* =========================================================
   Main
   ========================================================= */
static SOCKET tcp_connect_v3(const char *host, int port) {
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_port = htons((u_short)port);
    inet_pton(AF_INET, host, &a.sin_addr);
    if (connect(s, (struct sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) {
        fprintf(stderr, "connect() failed: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }
    return s;
}
static int variant3() {
    SOCKET s = tcp_connect_v3(RDP_HOST, RDP_PORT);
    if (s == INVALID_SOCKET) return 1;
    SockBuf *sb = sockbuf_new_v3(s);
    /* greeting */
    char *hello = rdp_recv_v3(sb);
    if (!hello) { fprintf(stderr, "No greeting\n"); return 1; }
    char root_actor[64] = "root";
    json_str_v3(hello, "from", root_actor, sizeof(root_actor));
    free(hello);
    /* list tabs */
    Tab tabs[32]; int tab_count = list_tabs(s, sb, root_actor, tabs, 32);
    if (tab_count == 0) { fprintf(stderr, "No tabs open\n"); return 1; }
    print_tabs(tabs, tab_count);
    /* select tab */
    int tab_idx = 0;
    if (tab_count > 1) {
        printf("Select tab [0-%d]: ", tab_count - 1);
        fflush(stdout);
        scanf("%d", &tab_idx);
        if (tab_idx < 0 || tab_idx >= tab_count) tab_idx = 0;
    }
    printf("[*] Attaching to: %s\n", tabs[tab_idx].title);
    /* attach */
    char inspector_actor[128], walker_actor[128];
    if (!attach_tab(s, sb, tabs[tab_idx].actor, inspector_actor, walker_actor)) return 1;
    // printf("[+] Walker: %s\n\n", walker_actor);
    /* initial fetch */
    char *html_pkt = fetch_outerhtml(s, sb, walker_actor);
    if (!html_pkt) { fprintf(stderr, "Could not fetch DOM\n"); return 1; }
    TagSnapshot snap_old = {0}, snap_new = {0};
    /* extract outerHTML string from JSON packet */
    char *html_val = strstr(html_pkt, "\"value\":\"");
    if (html_val) html_val += strlen("\"value\":\"");
    else           html_val = html_pkt;
    take_snapshot(html_val, &snap_old);
    printf("=== Initial tag contents ===\n");
    for (int i = 0; i < snap_old.count; i++)
        printf("  <%s>: %s\n", snap_old.results[i].tag, snap_old.results[i].content);
    printf("\n[*] Watching for changes (Ctrl+C to stop)...\n\n");
    free(html_pkt);
    /* ---- Main watch loop ---- */
    for (;;) {
        /* Wait up to POLL_INTERVAL_MS for a mutation event.
           If one arrives early we re-fetch immediately.
           If timeout expires we re-fetch anyway (polling fallback). */
        int mutation = drain_events_v3(sb, POLL_INTERVAL_MS);
        (void)mutation; /* either way we re-fetch */
        html_pkt = fetch_outerhtml(s, sb, walker_actor);
        if (!html_pkt) {
            fprintf(stderr, "[!] Lost connection or tab closed\n");
            break;
        }
        html_val = strstr(html_pkt, "\"value\":\"");
        if (html_val) html_val += strlen("\"value\":\"");
        else           html_val = html_pkt;
        take_snapshot(html_val, &snap_new);
        /* diff */
        int changed = 0;
        for (int i = 0; i < snap_new.count; i++) {
            int found = 0;
            for (int j = 0; j < snap_old.count; j++)
                if (strcmp(snap_new.results[i].tag,     snap_old.results[j].tag)     == 0 &&
                    strcmp(snap_new.results[i].content, snap_old.results[j].content) == 0)
                    { found = 1; break; }
            if (!found) changed = 1;
        }
        if (changed) {
            SYSTEMTIME st; GetLocalTime(&st);
            printf("[%02d:%02d:%02d] DOM changed:\n", st.wHour, st.wMinute, st.wSecond);
            diff_snapshots(&snap_old, &snap_new);
            printf("\n");
            snap_old = snap_new; /* update baseline */
        }
        free(html_pkt);
    }
    closesocket(s);
    WSACleanup();
    return 0;
}

/* =========================================================
   Tab session — groups all actors for one page lifetime
   ========================================================= */
typedef struct {
    char tab_actor[128];
    char inspector_actor[128];
    char walker_actor[128];
} TabSession;
/* Fully re-attaches to a tab after navigation. Returns 1 on success. */
static int reattach_tab(SOCKET s, SockBuf *sb, TabSession *sess) {
    printf("[*] Re-attaching to tab %s\n", sess->tab_actor);
    memset(sess->inspector_actor, 0, sizeof(sess->inspector_actor));
    memset(sess->walker_actor,    0, sizeof(sess->walker_actor));
    /* Small delay — give Firefox time to set up the new page actors */
    Sleep(500);
    return attach_tab(s, sb, sess->tab_actor,
                      sess->inspector_actor,
                      sess->walker_actor);
}
/* =========================================================
   Safe fetch — re-attaches automatically on navigation
   Returns malloc'd packet or NULL on unrecoverable error.
   ========================================================= */
static char *safe_fetch_outerhtml(SOCKET s, SockBuf *sb, TabSession *sess) {
    for (int attempt = 0; attempt < 3; attempt++) {
        char *pkt = fetch_outerhtml(s, sb, sess->walker_actor);
        if (pkt) return pkt;
        printf("[!] Fetch failed (attempt %d) — re-attaching\n", attempt + 1);
        if (!reattach_tab(s, sb, sess)) {
            fprintf(stderr, "[-] Re-attach failed\n");
            Sleep(1000);
            continue;
        }
        printf("[+] Re-attached. Walker: %s\n", sess->walker_actor);
    }
    return NULL;
}
static int variant4() {
    SOCKET s = tcp_connect_v3(RDP_HOST, RDP_PORT);
    if (s == INVALID_SOCKET) return 1;
    SockBuf *sb = sockbuf_new_v3(s);
    char *hello = rdp_recv_v3(sb);
    if (!hello) { fprintf(stderr, "No greeting\n"); return 1; }
    char root_actor[64] = "root";
    json_str_v3(hello, "from", root_actor, sizeof(root_actor));
    free(hello);
    Tab tabs[32];
    int tab_count = list_tabs(s, sb, root_actor, tabs, 32);
    if (tab_count == 0) { fprintf(stderr, "No tabs\n"); return 1; }
    print_tabs(tabs, tab_count);
    int tab_idx = 0;
    if (tab_count > 1) {
        printf("Select tab [0-%d]: ", tab_count - 1);
        fflush(stdout);
        scanf("%d", &tab_idx);
        if (tab_idx < 0 || tab_idx >= tab_count) tab_idx = 0;
    }
    /* --- init session --- */
    TabSession sess = {0};
    strncpy(sess.tab_actor, tabs[tab_idx].actor, sizeof(sess.tab_actor) - 1);
    if (!attach_tab(s, sb, sess.tab_actor, sess.inspector_actor, sess.walker_actor))
        return 1;
    printf("[+] Walker: %s\n\n", sess.walker_actor);
    /* initial snapshot */
    char *html_pkt = safe_fetch_outerhtml(s, sb, &sess);
    if (!html_pkt) { fprintf(stderr, "Initial fetch failed\n"); return 1; }
    TagSnapshot snap_old = {0}, snap_new = {0};
    char *html_val = strstr(html_pkt, "\"value\":\"");
    if (html_val) html_val += strlen("\"value\":\"");
    else           html_val = html_pkt;
    take_snapshot(html_val, &snap_old);
    printf("=== Initial tags ===\n");
    for (int i = 0; i < snap_old.count; i++)
        printf("  <%s>: %s\n", snap_old.results[i].tag, snap_old.results[i].content);
    printf("\n[*] Watching (Ctrl+C to stop)...\n\n");
    free(html_pkt);
    /* ---- main loop ---- */
    for (;;) {
        int wake = drain_events_v3(sb, POLL_INTERVAL_MS);
        if (wake == WAKE_DISCONNECT) {
            fprintf(stderr, "[!] Disconnect event\n");
            break;
        }
        if (wake == WAKE_NAVIGATED) {
            printf("[*] Navigation detected — re-attaching\n");
            if (!reattach_tab(s, sb, &sess)) {
                fprintf(stderr, "[-] Could not re-attach after navigation\n");
                break;
            }
            printf("[+] New walker: %s\n", sess.walker_actor);
            /* Reset snapshot for new page */
            snap_old.count = 0;
        }
        html_pkt = safe_fetch_outerhtml(s, sb, &sess);
        if (!html_pkt) {
            fprintf(stderr, "[!] Unrecoverable fetch failure\n");
            break;
        }
        html_val = strstr(html_pkt, "\"value\":\"");
        if (html_val) html_val += strlen("\"value\":\"");
        else           html_val = html_pkt;
        take_snapshot(html_val, &snap_new);
        int changed = 0;
        for (int i = 0; i < snap_new.count; i++) {
            int found = 0;
            for (int j = 0; j < snap_old.count; j++)
                if (strcmp(snap_new.results[i].tag,     snap_old.results[j].tag)     == 0 &&
                    strcmp(snap_new.results[i].content, snap_old.results[j].content) == 0)
                    { found = 1; break; }
            if (!found) changed = 1;
        }
        if (changed || wake == WAKE_NAVIGATED) {
            SYSTEMTIME st; GetLocalTime(&st);
            printf("[%02d:%02d:%02d] %s\n",
                st.wHour, st.wMinute, st.wSecond,
                wake == WAKE_NAVIGATED ? "New page:" : "DOM changed:");
            diff_snapshots(&snap_old, &snap_new);
            printf("\n");
            snap_old = snap_new;
        }
        free(html_pkt);
    }
    closesocket(s);
    WSACleanup();
    return 0;
}

/* =========================================================
   Main RDP flow:
   1. Connect → receive hello packet (tells us "root" actor)
   2. listTabs  → get tab actors
   3. attach to tab → get tab's actors
   4. attach to page → get DOM via inspectNode / getDocument
 nks  ========================================================= */
int do_firefox_dom_rdp() {
    variant4();
}
