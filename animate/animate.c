#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <turbojpeg.h>
#include <hiredis/hiredis.h>

#define LEDS_LENGTH 1080

void diep(char *str) {
    perror(str);
    exit(EXIT_FAILURE);
}

int animate(redisContext *redis, char *imgfile) {
    tjhandle jpeg;
    unsigned char *imgraw, *source;
    int subsamp, width, height, colorsp;
    int fd;

    if((fd = open(imgfile, O_RDONLY)) < 0)
        diep("open");

    off_t length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if(!(source = malloc(length)))
        diep("malloc");

    if(read(fd, source, length) != length)
        diep("read");

    close(fd);



    if(!(jpeg = tjInitDecompress()))
        diep("jpeg init");

    if(tjDecompressHeader3(jpeg, source, length, &width, &height, &subsamp, &colorsp) < 0)
        fprintf(stderr, "jpeg header: %s\n", tjGetErrorStr2(jpeg));

    printf("[+] jpeg: %d x %d x %d\n", width, height, colorsp);

    if(width != LEDS_LENGTH) {
        printf("image width mismatch leds map\n");
        exit(1);
    }

    if(!(imgraw = malloc(width * height * 3)))
        diep("malloc");

    if(tjDecompress2(jpeg, source, length, imgraw, width, 0, height, TJPF_RGB, TJFLAG_FASTDCT) < 0)
        fprintf(stderr, "jpeg decompress: %s\n", tjGetErrorStr2(jpeg));

    // int inith = (frame > height) ? frame % height : frame;
    int inith = 0;
    printf("init height: %d\n", inith);

    unsigned char *rawptr = imgraw + (inith * width * 3);
    int direction = 1;
    int h = 0;

    while(1) {
        printf("[+] sending line: %d\n", h);

        redisReply *reply;
        reply = redisCommand(redis, "PUBLISH light %b", rawptr + (h * width * 3), LEDS_LENGTH * 3);
        freeReplyObject(reply);

        usleep(50000);

        h += direction;

        if(h == height) {
            direction = -1;
            h -= 1;
        }

        if(h == 0)
            direction = 1;
    }


    free(source);
    free(imgraw);
    tjDestroy(jpeg);

    return 0;
}

int main(int argc, char *argv[]) {
    redisContext *redis;
    char *redishost = "light0";
    char *imgfile = "/tmp/ledjpeg/followline1.jpg";

    printf("[+] connecting remote lighting redis\n");

    if(!(redis = redisConnect(redishost, 6379)))
        diep("redis");

    if(redis->err) {
        fprintf(stderr, "[-] redis: connection error: %s\n", redis->errstr);
        exit(EXIT_FAILURE);
    }

    if(argc > 1)
        imgfile = argv[1];

    animate(redis, imgfile);

    return 0;
}

