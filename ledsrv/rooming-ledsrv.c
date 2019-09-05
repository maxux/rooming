#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <hiredis/hiredis.h>
#include "ws2811.h"

#define LEDS_LENGTH 1080

ws2811_t ledstring = {
    .freq = WS2811_TARGET_FREQ,
    .dmanum = 10,
    .channel = {
        [0] = {
            .gpionum = 12,
            .count = LEDS_LENGTH,
            .invert = 0,
            .brightness = 255,
            .strip_type = WS2811_STRIP_RGB,
        },
        [1] = {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
            .brightness = 0,
        },
    },
};

void diep(char *str) {
    perror(str);
    exit(EXIT_FAILURE);
}

void render() {
    int ret;

    if((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
        fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
        exit(EXIT_FAILURE);
    }
}

int main() {
    int ret;
    redisContext *redis;
    redisReply *reply;

    printf("[+] initializing rooming lights\n");

    if((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
        fprintf(stderr, "[-] ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        exit(EXIT_FAILURE);
    }

    if(!(redis = redisConnect("localhost", 6379)))
        diep("redis");

    if(redis->err) {
        fprintf(stderr, "[-] redis: connection error: %s\n", redis->errstr);
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 1080; i++)
        ledstring.channel[0].leds[i] = 0x000000FF; // 0xWWRRGGBB

    render();

    reply = redisCommand(redis, "SUBSCRIBE light");
    freeReplyObject(reply);

    while(redisGetReply(redis, (void **) &reply) == REDIS_OK) {
        // waiting for a valid matrix message
        if(reply->type != REDIS_REPLY_ARRAY || reply->elements != 3)
            goto nextmsg;

        if(strcmp(reply->element[0]->str, "message"))
            goto nextmsg;

        if(reply->element[2]->len != (LEDS_LENGTH * 3))
            goto nextmsg;

        char *buffer = reply->element[2]->str;

        printf("[+] applying new matrix layout\n");

        for(int i = 0; i < LEDS_LENGTH; i++) {
            ledstring.channel[0].leds[i] = buffer[1] << 16 | buffer[0] << 8 | buffer[2];
            buffer += 3;
        }

        render();

        nextmsg:
        freeReplyObject(reply);
    }

    ws2811_fini(&ledstring);

    return 0;
}