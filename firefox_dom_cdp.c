// firefox_dom_cdp.c
// Compile: cl firefox_dom.c /link ws2_32.lib
// Or GCC:  gcc firefox_dom.c -o firefox_dom.exe -lws2_32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wincrypt.h>

#include "timeout.h"
// #pragma comment(lib, "advapi32.lib")
// #pragma comment(lib, "ws2_32.lib")

/* =========================================================
   SHA-1  (RFC 3174)
   ========================================================= */
typedef struct { uint32_t h[5]; uint8_t buf[64]; uint64_t bits; uint32_t idx; } SHA1;
static void sha1_init(SHA1 *s) {
    s->h[0]=0x67452301; s->h[1]=0xEFCDAB89; s->h[2]=0x98BADCFE;
    s->h[3]=0x10325476; s->h[4]=0xC3D2E1F0;
    s->bits=0; s->idx=0;
}
#define ROL32(v,n) (((v)<<(n))|((v)>>(32-(n))))

static void sha1_compress(SHA1 *s) {
    uint32_t w[80], a,b,c,d,e,f,k,t;
    for(int i=0;i<16;i++)
        w[i]=((uint32_t)s->buf[i*4]<<24)|((uint32_t)s->buf[i*4+1]<<16)
            |((uint32_t)s->buf[i*4+2]<<8)|(s->buf[i*4+3]);
    for(int i=16;i<80;i++) w[i]=ROL32(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    a=s->h[0];b=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];
    for(int i=0;i<80;i++){
        if(i<20){f=(b&c)|(~b&d);k=0x5A827999;}
        else if(i<40){f=b^c^d;k=0x6ED9EBA1;}
        else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
        else{f=b^c^d;k=0xCA62C1D6;}
        t=ROL32(a,5)+f+e+k+w[i];
        e=d;d=c;c=ROL32(b,30);b=a;a=t;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;
}

static void sha1_update(SHA1 *s, const uint8_t *data, size_t len) {
    for(size_t i=0;i<len;i++){
        s->buf[s->idx++]=data[i]; s->bits+=8;
        if(s->idx==64){sha1_compress(s);s->idx=0;}
    }
}

static void sha1_final(SHA1 *s, uint8_t out[20]) {
    s->buf[s->idx++]=0x80;
    if(s->idx>56){while(s->idx<64)s->buf[s->idx++]=0;sha1_compress(s);s->idx=0;}
    while(s->idx<56)s->buf[s->idx++]=0;
    for(int i=7;i>=0;i--){s->buf[56+(7-i)]=(uint8_t)(s->bits>>(i*8));}
    sha1_compress(s);
    for(int i=0;i<5;i++){out[i*4]=(s->h[i]>>24)&0xFF;out[i*4+1]=(s->h[i]>>16)&0xFF;
        out[i*4+2]=(s->h[i]>>8)&0xFF;out[i*4+3]=s->h[i]&0xFF;}
}

/* =========================================================
   Base64 encode
   ========================================================= */
static const char b64t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void base64_encode(const uint8_t *in, size_t len, char *out) {
    size_t i=0,j=0;
    for(;i+2<len;i+=3){
        out[j++]=b64t[in[i]>>2];
        out[j++]=b64t[((in[i]&3)<<4)|(in[i+1]>>4)];
        out[j++]=b64t[((in[i+1]&0xF)<<2)|(in[i+2]>>6)];
        out[j++]=b64t[in[i+2]&0x3F];
    }
    if(i<len){
        out[j++]=b64t[in[i]>>2];
        if(i+1<len){out[j++]=b64t[((in[i]&3)<<4)|(in[i+1]>>4)];out[j++]=b64t[(in[i+1]&0xF)<<2];}
        else{out[j++]=b64t[(in[i]&3)<<4];out[j++]='=';}
        out[j++]='=';
    }
    out[j]='\0';
}

/* =========================================================
   Sec-WebSocket-Accept  (RFC 6455 §4.2.2)
   ========================================================= */
static void ws_accept_key(const char *client_key, char out_b64[64]) {
    const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char concat[256];
    snprintf(concat, sizeof(concat), "%s%s", client_key, magic);
    SHA1 sha; sha1_init(&sha);
    sha1_update(&sha,(uint8_t*)concat,strlen(concat));
    uint8_t digest[20]; sha1_final(&sha,digest);
    base64_encode(digest,20,out_b64);
}

/* =========================================================
   TCP helpers
   ========================================================= */
static SOCKET tcp_connect(const char *host, int port) {
    WSADATA wsa; WSAStartup(MAKEWORD(2,2),&wsa);
    SOCKET s=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in a={0};
    a.sin_family=AF_INET; a.sin_port=htons((u_short)port);
    inet_pton(AF_INET,host,&a.sin_addr);
    if(connect(s,(struct sockaddr*)&a,sizeof(a))==SOCKET_ERROR){
        fprintf(stderr,"connect() failed: %d\n",WSAGetLastError());
        return INVALID_SOCKET;
    }
    return s;
}

/* recv until we see \r\n\r\n — returns total bytes in buf */
static int recv_http_headers(SOCKET s, char *buf, int bufsz) {
    int total=0; char c;
    while(total<bufsz-1){
        int r=recv(s,&c,1,0); if(r<=0)break;
        buf[total++]=c;
        if(total>=4 && memcmp(buf+total-4,"\r\n\r\n",4)==0) break;
    }
    buf[total]='\0'; return total;
}

/* recv exactly n bytes */
static int recv_exact(SOCKET s, uint8_t *buf, int n) {
    int got=0;
    while(got<n){int r=recv(s,(char*)buf+got,n-got,0);if(r<=0)return got;got+=r;}
    return got;
}

/* =========================================================
   HTTP GET — returns malloc'd body (caller frees), NULL on error
   ========================================================= */
static char *http_get(const char *host, int port, const char *path) {
    SOCKET s=tcp_connect(host,port);
    if(s==INVALID_SOCKET) return NULL;
    char req[512];
    snprintf(req,sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n",
        path,host,port);
    send(s,req,(int)strlen(req),0);
    /* read everything */
    char *buf=malloc(65536); int total=0,r;
    while((r=recv(s,(char*)buf+total,65536-total-1,0))>0) total+=r;
    buf[total]='\0';
    closesocket(s);
    /* skip headers */
    char *body=strstr(buf,"\r\n\r\n");
    if(!body){free(buf);return NULL;}
    body+=4;
    char *result=_strdup(body);
    free(buf);
    return result;
}

/* =========================================================
   Parse first "webSocketDebuggerUrl" from /json response
   e.g. "ws://127.0.0.1:9225/devtools/page/XXXX"
   ========================================================= */
static int parse_ws_url(const char *json, char *host_out, int *port_out, char *path_out) {
    const char *key="\"webSocketDebuggerUrl\": \"ws://";
    const char *p=strstr(json,key);
    if(!p){
        /* try without space after colon */
        key="\"webSocketDebuggerUrl\":\"ws://";
        p=strstr(json,key);
    }
    if(!p) return 0;
    p+=strlen(key);
    /* host */
    const char *col=strchr(p,':');
    if(!col) return 0;
    strncpy(host_out,p,(int)(col-p)); host_out[col-p]='\0';
    p=col+1;
    /* port */
    *port_out=atoi(p);
    /* path */
    const char *sl=strchr(p,'/');
    if(!sl) return 0;
    const char *end=strchr(sl,'"');
    if(!end) return 0;
    strncpy(path_out,sl,(int)(end-sl)); path_out[end-sl]='\0';
    return 1;
}

/* =========================================================
   WebSocket handshake  (RFC 6455)
   Returns connected SOCKET or INVALID_SOCKET
   ========================================================= */
static SOCKET ws_connect(const char *host, int port, const char *path) {
    SOCKET s=tcp_connect(host,port);
    if(s==INVALID_SOCKET) return INVALID_SOCKET;
    /* Build a 16-byte random key, Base64-encode it */
    uint8_t raw_key[16];
    HCRYPTPROV hprov;
    CryptAcquireContext(&hprov,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT);
    CryptGenRandom(hprov,16,raw_key);
    CryptReleaseContext(hprov,0);
    char client_key[32]; base64_encode(raw_key,16,client_key);
    /* Compute expected Sec-WebSocket-Accept */
    char expected_accept[64]; ws_accept_key(client_key,expected_accept);
    /* Send HTTP Upgrade request */
    char req[1024];
    snprintf(req,sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path,host,port,client_key);
    send(s,req,(int)strlen(req),0);
    /* Read response headers */
    char hdrs[4096]; recv_http_headers(s,hdrs,sizeof(hdrs));
    if(!strstr(hdrs,"101")){
        fprintf(stderr,"WS upgrade failed:\n%s\n",hdrs);
        closesocket(s); return INVALID_SOCKET;
    }
    if(!strstr(hdrs,expected_accept)){
        fprintf(stderr,"Sec-WebSocket-Accept mismatch!\nExpected: %s\nHeaders:\n%s\n",
            expected_accept,hdrs);
        closesocket(s); return INVALID_SOCKET;
    }
    printf("[+] WebSocket handshake OK\n");
    return s;
}

/* =========================================================
   WebSocket frame send  (client→server, masked, RFC 6455 §5)
   ========================================================= */
static void ws_send_text(SOCKET s, const char *payload) {
    size_t plen=strlen(payload);
    uint8_t frame[32]; int fi=0;
    frame[fi++]=0x81; /* FIN + opcode text */
    /* masking bit set, length */
    if(plen<=125){
        frame[fi++]=(uint8_t)(0x80|plen);
    } else if(plen<=65535){
        frame[fi++]=0xFE;
        frame[fi++]=(uint8_t)(plen>>8);
        frame[fi++]=(uint8_t)(plen&0xFF);
    } else {
        frame[fi++]=0xFF;
        for(int i=7;i>=0;i--) frame[fi++]=(uint8_t)((plen>>(i*8))&0xFF);
    }
    /* 4-byte mask */
    uint8_t mask[4];
    HCRYPTPROV hp;
    CryptAcquireContext(&hp,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT);
    CryptGenRandom(hp,4,mask);
    CryptReleaseContext(hp,0);
    memcpy(frame+fi,mask,4); fi+=4;
    send(s,(char*)frame,fi,0);
    /* send masked payload in chunks */
    uint8_t chunk[4096]; size_t sent=0;
    while(sent<plen){
        size_t n=plen-sent; if(n>sizeof(chunk))n=sizeof(chunk);
        for(size_t i=0;i<n;i++) chunk[i]=((uint8_t)payload[sent+i])^mask[(sent+i)%4];
        send(s,(char*)chunk,(int)n,0);
        sent+=n;
    }
}

/* =========================================================
   WebSocket frame receive  (server→client, never masked)
   Returns malloc'd text payload, NULL on error. Caller frees.
   Handles large frames and skips control frames (ping/pong/close).
   ========================================================= */
static char *ws_recv_text(SOCKET s) {
    for(;;){
        uint8_t hdr[2]; if(recv_exact(s,hdr,2)<2) return NULL;
        int opcode=hdr[0]&0x0F;
        int masked =(hdr[1]&0x80)!=0;
        uint64_t plen=hdr[1]&0x7F;
        if(plen==126){
            uint8_t ext[2]; recv_exact(s,ext,2);
            plen=((uint64_t)ext[0]<<8)|ext[1];
        } else if(plen==127){
            uint8_t ext[8]; recv_exact(s,ext,8);
            plen=0; for(int i=0;i<8;i++) plen=(plen<<8)|ext[i];
        }
        uint8_t mask[4]={0};
        if(masked) recv_exact(s,mask,4);
        char *buf=malloc((size_t)plen+1);
        recv_exact(s,(uint8_t*)buf,(int)plen);
        buf[plen]='\0';
        if(masked) for(uint64_t i=0;i<plen;i++) buf[i]^=mask[i%4];
        /* skip control frames (ping=9, pong=10, close=8) */
        if(opcode==9){free(buf);continue;} /* ping — ideally send pong */
        if(opcode==8||opcode==10){free(buf);return NULL;}
        return buf; /* opcode 1=text, 2=binary */
    }
}

/* =========================================================
   CDP helpers
   ========================================================= */
/* Extract a string value for a JSON key — very small, no full parser */
static int json_str(const char *json, const char *key, char *out, int outsz) {
    char search[256]; snprintf(search,sizeof(search),"\"%s\":",key);
    const char *p=strstr(json,search);
    if(!p) return 0;
    p+=strlen(search);
    while(*p==' ')p++;
    if(*p=='"'){
        p++;
        const char *e=p;
        while(*e && !(*e=='"' && *(e-1)!='\\')) e++;
        int len=(int)(e-p); if(len>=outsz)len=outsz-1;
        strncpy(out,p,len); out[len]='\0';
        return 1;
    }
    return 0;
}

static char *cdp_call(SOCKET s, int id, const char *method, const char *params_json) {
    char msg[1024];
    snprintf(msg,sizeof(msg),
        "{\"id\":%d,\"method\":\"%s\",\"params\":%s}",
        id,method,params_json);
    printf("[>] %s\n",msg);
    ws_send_text(s,msg);
    /* Read frames until we get one with matching id */
    for(;;){
        char *resp=ws_recv_text(s);
        if(!resp) return NULL;
        char idstr[32]; snprintf(idstr,sizeof(idstr),"\"id\":%d",id);
        if(strstr(resp,idstr)) return resp; /* caller frees */
        free(resp); /* discard events */
    }
}

int do_firefox_dom_cdp() {
    const char *dbg_host="127.0.0.1";
    int         dbg_port=9225;
    /* ---- Step 1: get tab list ---- */
    printf("[*] Fetching tab list from http://%s:%d/json\n",dbg_host,dbg_port);
    char *tab_list=http_get(dbg_host,dbg_port,"/json/list");
    if(!tab_list){
      fprintf(stderr,
        "Could not reach Firefox debug port.\n"
        "Launch Firefox with:  firefox.exe --remote-debugging-port=9225\n");
      timeout(20);
      return 1;
    }
    printf("[+] Tab list received (%zu bytes)\n",strlen(tab_list));
    /* ---- Step 2: parse webSocketDebuggerUrl ---- */
    char ws_host[128]; int ws_port; char ws_path[256];
    if(!parse_ws_url(tab_list,ws_host,&ws_port,ws_path)){
        fprintf(stderr,"No webSocketDebuggerUrl found in tab list:\n%s\n",tab_list);
        free(tab_list);
        timeout(3);
        return 1;
    }
    free(tab_list);
    printf("[+] WS target  ws://%s:%d%s\n",ws_host,ws_port,ws_path);
    /* ---- Step 3: WebSocket handshake ---- */
    SOCKET ws=ws_connect(ws_host,ws_port,ws_path);
    if(ws==INVALID_SOCKET) {
      fprintf(stderr,"Invalid Socket\n");
      timeout(20);
      return 1;
    }
    /* ---- Step 4: DOM.getDocument ---- */
    char *r1=cdp_call(ws,1,"DOM.getDocument","{\"depth\":0}");
    if(!r1){
      fprintf(stderr,"No response for DOM.getDocument\n");
      timeout(20);
      return 1;
    }
    /* Extract root nodeId */
    char node_id_str[32]="1";
    json_str(r1,"nodeId",node_id_str,sizeof(node_id_str));
    printf("[+] Root nodeId: %s\n",node_id_str);
    free(r1);
    /* ---- Step 5: DOM.getOuterHTML ---- */
    char params[64]; snprintf(params,sizeof(params),"{\"nodeId\":%s}",node_id_str);
    char *r2=cdp_call(ws,2,"DOM.getOuterHTML",params);
    if(!r2){
      fprintf(stderr,"No response for DOM.getOuterHTML\n");
      timeout(20);
      return 1;
    }
    /* Extract outerHTML */
    char *html_start=strstr(r2,"\"outerHTML\":\"");
    if(html_start){
        html_start+=strlen("\"outerHTML\":\"");
        /* The HTML can be huge; just print the first 2 KB */
        char preview[2048]; int i=0;
        while(i<(int)sizeof(preview)-1 && *html_start && !(*html_start=='"' && *(html_start-1)!='\\')){
            preview[i++]=*html_start++;
        }
        preview[i]='\0';
        printf("\n===== DOM (first 2 KB) =====\n%s\n...\n",preview);
    }
    /* ---- Save full response to file ---- */
    FILE *f=fopen("dom_output.json","wb");
    if(f){fwrite(r2,1,strlen(r2),f);fclose(f);printf("[+] Full response saved to dom_output.json\n");}
    free(r2);
    closesocket(ws);
    WSACleanup();
    timeout(20);
    return 0;
}

