#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>

#define ICC_FAN     "/dev/icc_fan"
#define ICC_THERMAL "/dev/icc_thermal"

#define IOCTL_FAN_GET   0x40044901
#define IOCTL_FAN_SET   0x80044902
#define IOCTL_FAN_AUTO  0x20004903
#define IOCTL_TEMP_GET  0x40044801

typedef struct { uint32_t val; } IccArg;

int fan_fd, therm_fd;

int fan_open(void) {
    fan_fd   = open(ICC_FAN,     O_RDWR);
    therm_fd = open(ICC_THERMAL, O_RDONLY);
    return (fan_fd < 0 || therm_fd < 0) ? -1 : 0;
}

float get_temp(void) {
    IccArg a = {0};
    if (ioctl(therm_fd, IOCTL_TEMP_GET, &a) < 0) return -1;
    return a.val / 1000.0f;
}

int get_fan(void) {
    IccArg a = {0};
    if (ioctl(fan_fd, IOCTL_FAN_GET, &a) < 0) return -1;
    return (int)((a.val / 255.0f) * 100);
}

int set_fan(int pct) {
    IccArg a = { .val = (uint32_t)((pct / 100.0f) * 255) };
    return ioctl(fan_fd, IOCTL_FAN_SET, &a);
}

int set_auto(void) {
    IccArg a = {0};
    return ioctl(fan_fd, IOCTL_FAN_AUTO, &a);
}

uint32_t auto_curve(float t) {
    if (t < 50) return 31;
    if (t < 60) return 47;
    if (t < 70) return 66;
    if (t < 80) return 82;
    return 100;
}

int main(void) {
    SceUserServiceInitializeParams u = { .priority = 256 };
    sceUserServiceInitialize(&u);
    scePadInit();
    int uid; sceUserServiceGetInitialUser(&uid);
    int pad = scePadOpen(uid, 0, 0, NULL);

    if (fan_open() < 0) return 1;

    int mode = 0, manual = 50;
    OrbisPadData pd;

    while (1) {
        float temp = get_temp();
        int   fan  = get_fan();

        sceSystemServiceRequestFlipWithCanvas(0, 0, 0);

        char buf[256];
        snprintf(buf, sizeof(buf),
            "PS4 Fan Control\n"
            "Temp : %.1f C\n"
            "Fan  : %d%%\n"
            "Mode : %s\n"
            "Cross=toggle  Up/Dn=speed  Circle=exit",
            temp, fan, mode ? "MANUAL" : "AUTO");

        if (mode == 0) set_fan(auto_curve(temp));

        scePadReadState(pad, &pd);

        if (pd.buttons & 0x4000) { mode ^= 1; if (!mode) set_auto(); sceKernelUsleep(300000); }
        if (mode && pd.buttons & 0x0010) { manual = manual+5 > 100 ? 100 : manual+5; set_fan(manual); sceKernelUsleep(150000); }
        if (mode && pd.buttons & 0x0040) { manual = manual-5 < 0   ? 0   : manual-5; set_fan(manual); sceKernelUsleep(150000); }
        if (pd.buttons & 0x2000) break;

        sceKernelUsleep(500000);
    }

    set_auto();
    close(fan_fd);
    close(therm_fd);
    return 0;
}
