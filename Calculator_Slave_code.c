// TI-84 Plus CE DUCO Miner - Slave
// Uses USB serial to communicate with computer master

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <usbdrvce.h>
#include <serial.h>

// == SHA-1 IMPLEMENTATIO==
// For calculators without native SHA-1 in CryptX

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} SHA1_CTX;

const uint32_t sha1_k[4] = {
    0x5A827999, 0x6ED9EBA1,
    0x8F1BBCDC, 0xCA62C1D6
};

void sha1_transform(SHA1_CTX *ctx) {
    uint32_t w[80], a, b, c, d, e, temp;
    int i;
    
    for (i = 0; i < 16; i++) {
        w[i] = (ctx->buffer[i*4] << 24) | (ctx->buffer[i*4+1] << 16) |
               (ctx->buffer[i*4+2] << 8) | ctx->buffer[i*4+3];
    }
    for (i = 16; i < 80; i++) {
        w[i] = ROTLEFT(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }
    
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    
    for (i = 0; i < 20; i++) {
        temp = ROTLEFT(a,5) + ((b&c)|(~b&d)) + e + w[i] + sha1_k[0];
        e = d; d = c; c = ROTLEFT(b,30); b = a; a = temp;
    }
    for (i = 20; i < 40; i++) {
        temp = ROTLEFT(a,5) + (b^c^d) + e + w[i] + sha1_k[1];
        e = d; d = c; c = ROTLEFT(b,30); b = a; a = temp;
    }
    for (i = 40; i < 60; i++) {
        temp = ROTLEFT(a,5) + ((b&c)|(b&d)|(c&d)) + e + w[i] + sha1_k[2];
        e = d; d = c; c = ROTLEFT(b,30); b = a; a = temp;
    }
    for (i = 60; i < 80; i++) {
        temp = ROTLEFT(a,5) + (b^c^d) + e + w[i] + sha1_k[3];
        e = d; d = c; c = ROTLEFT(b,30); b = a; a = temp;
    }
    
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void sha1_init(SHA1_CTX *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count[0] = ctx->count[1] = 0;
}

void sha1_update(SHA1_CTX *ctx, const uint8_t *data, uint32_t len) {
    uint32_t i, index, partLen;
    index = (ctx->count[0] >> 3) & 63;
    
    if ((ctx->count[0] += (len << 3)) < (len << 3)) ctx->count[1]++;
    ctx->count[1] += (len >> 29);
    
    partLen = 64 - index;
    if (len >= partLen) {
        memcpy(&ctx->buffer[index], data, partLen);
        sha1_transform(ctx);
        for (i = partLen; i + 63 < len; i += 64) {
            memcpy(ctx->buffer, &data[i], 64);
            sha1_transform(ctx);
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[index], &data[i], len - i);
}

void sha1_final(SHA1_CTX *ctx, uint8_t *digest) {
    uint8_t bits[8];
    uint32_t index, padLen;
    int i;
    
    for (i = 0; i < 8; i++) {
        bits[i] = (ctx->count[i >> 2] >> ((3 - (i & 3)) * 8)) & 0xFF;
    }
    
    index = (ctx->count[0] >> 3) & 63;
    padLen = (index < 56) ? (56 - index) : (120 - index);
    
    uint8_t padding[64];
    padding[0] = 0x80;
    memset(padding + 1, 0, padLen - 1);
    
    sha1_update(ctx, padding, padLen);
    sha1_update(ctx, bits, 8);
    
    for (i = 0; i < 20; i++) {
        digest[i] = (ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 0xFF;
    }
}

void sha1_string(const char* input, uint8_t* output) {
    SHA1_CTX ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, (const uint8_t*)input, strlen(input));
    sha1_final(&ctx, output);
}

// ==================== HELPER FUNCTIONS ====================

uint8_t hex_char_to_byte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

void hex_to_bytes(const char* hex, uint8_t* bytes, int len) {
    for (int i = 0; i < len; i++) {
        bytes[i] = (hex_char_to_byte(hex[i*2]) << 4) | hex_char_to_byte(hex[i*2+1]);
    }
}

void bytes_to_hex(const uint8_t* bytes, int len, char* output) {
    const char* hex = "0123456789abcdef";
    for (int i = 0; i < len; i++) {
        output[i*2] = hex[(bytes[i] >> 4) & 0x0F];
        output[i*2+1] = hex[bytes[i] & 0x0F];
    }
    output[len*2] = '\0';
}

void int_to_str(uint32_t num, char* out) {
    if (num == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    char temp[12];
    int i = 0;
    while (num > 0) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    for (int j = 0; j < i; j++) {
        out[j] = temp[i - 1 - j];
    }
    out[i] = '\0';
}

// ==================== GLOBALS ====================
char last_hash[41];
char expected_hash[41];
uint8_t expected_bytes[20];
int difficulty = 10;
int has_job = 0;
uint32_t total_hashes = 0;
uint32_t accepted = 0;
uint32_t rejected = 0;

char serial_buffer[128];
int serial_idx = 0;

// ==================== MINING FUNCTION ====================
void mine() {
    if (!has_job) return;
    
    uint32_t max_nonce = difficulty * 100;
    if (max_nonce < 1000) max_nonce = 1000;
    if (max_nonce > 30000) max_nonce = 30000;
    
    char input_str[52];
    char nonce_str[12];
    uint8_t result[20];
    char result_hex[41];
    
    strcpy(input_str, last_hash);
    int hash_len = strlen(last_hash);
    
    for (uint32_t nonce = 0; nonce < max_nonce && has_job; nonce++) {
        int_to_str(nonce, nonce_str);
        strcpy(input_str + hash_len, nonce_str);
        sha1_string(input_str, result);
        total_hashes++;
        
        if (memcmp(result, expected_bytes, 20) == 0) {
            printf("FOUND,%lu\n", nonce);
            has_job = 0;
            accepted++;
            return;
        }
        
        // Update screen every 512 nonces
        if ((nonce & 0x1FF) == 0) {
            os_PutStrFull(".");
        }
    }
    
    if (has_job) {
        printf("NONCE_NOT_FOUND\n");
        has_job = 0;
    }
}

// ==================== COMMAND PARSING ====================
void process_command(char* cmd) {
    if (strncmp(cmd, "JOB,", 4) == 0) {
        char* ptr = cmd + 4;
        
        char* comma = strchr(ptr, ',');
        if (!comma) return;
        int len = comma - ptr;
        if (len > 40) len = 40;
        memcpy(last_hash, ptr, len);
        last_hash[len] = '\0';
        
        ptr = comma + 1;
        comma = strchr(ptr, ',');
        if (!comma) return;
        len = comma - ptr;
        if (len > 40) len = 40;
        memcpy(expected_hash, ptr, len);
        expected_hash[len] = '\0';
        
        ptr = comma + 1;
        difficulty = atoi(ptr);
        if (difficulty < 1) difficulty = 10;
        
        hex_to_bytes(expected_hash, expected_bytes, 20);
        has_job = 1;
        
        printf("JOB_READY\n");
    }
    else if (strcmp(cmd, "PING") == 0) {
        printf("PONG\n");
    }
    else if (strcmp(cmd, "STATS") == 0) {
        printf("STATS,%lu,%lu,%lu\n", total_hashes, accepted, rejected);
    }
    else if (strcmp(cmd, "RESET") == 0) {
        total_hashes = 0;
        accepted = 0;
        rejected = 0;
        printf("RESET_OK\n");
    }
}

// ==================== SERIAL READ ====================
void read_serial() {
    int c;
    while ((c = getchar()) != EOF) {
        if (c == '\n') {
            serial_buffer[serial_idx] = '\0';
            if (serial_idx > 0) {
                process_command(serial_buffer);
            }
            serial_idx = 0;
        } else if (serial_idx < 127) {
            serial_buffer[serial_idx++] = c;
        } else {
            serial_idx = 0;
        }
    }
}

// ==================== MAIN ====================
int main(void) {
    // Initialize display
    gfx_Begin();
    gfx_SetDefaultPalette();
    gfx_SetDrawBuffer();
    
    // Clear screen
    gfx_FillScreen(255);
    gfx_PrintStringXY("DUCO Miner v1.0", 10, 10);
    gfx_PrintStringXY("Calculator Slave", 10, 30);
    gfx_PrintStringXY("Waiting for jobs...", 10, 50);
    gfx_SwapDraw();
    
    // Initialize USB serial for communication
    serial_Init(USB_CDC, SERIAL_DEFAULT);
    
    // Send ready signal
    printf("READY\n");
    
    uint32_t last_update = 0;
    uint32_t start_time = rtc_Time();
    
    while (1) {
        // Process incoming commands
        read_serial();
        
        // Mine if we have a job
        if (has_job) {
            mine();
        }
        
        // Update display every 2 seconds
        uint32_t now = rtc_Time();
        if (now - last_update >= 2) {
            last_update = now;
            gfx_FillRectangle(0, 80, 320, 60, 255);
            gfx_PrintStringXY("Hashes: ", 10, 90);
            char buf[20];
            sprintf(buf, "%lu", total_hashes);
            gfx_PrintStringXY(buf, 100, 90);
            gfx_PrintStringXY("Accepted: ", 10, 110);
            sprintf(buf, "%lu", accepted);
            gfx_PrintStringXY(buf, 100, 110);
            gfx_PrintStringXY("Rejected: ", 10, 130);
            sprintf(buf, "%lu", rejected);
            gfx_PrintStringXY(buf, 100, 130);
            gfx_SwapDraw();
        }
        
        delay(10);
    }
    
    gfx_End();
    return 0;
}
