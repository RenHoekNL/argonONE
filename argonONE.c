
// musl-gcc -static -Wall -o argonONE argonONE.c ; strip -s argonONE


// Requires:
// /dev/vcio                 - Reading temperature
// /dev/i2c-1                - Controlling fan and poweroff
// /sbin/shutdown            - Controlled shutdown
// /sys/class/gpio/export    - Keeping track of the power button clicks

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "i2c.h"
#include "ioctl.h"
#include "types.h"

#define die(x, ...)        do{fprintf(stderr, "%s:%s(%u): " x "\n"    , __FILE__, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__); fflush(stderr); exit(1);}while(0)
#define die_err(x, ...)    do{fprintf(stderr, "%s:%s(%u): " x ": %s\n", __FILE__, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__, strerror(errno)); fflush(stderr); exit(1);}while(0)

// https://www.raspberrypi.org/forums/viewtopic.php?t=252196
uint16_t readTemp()
{
static uint16_t init = 1;
static int      fd;

if(init)
  {
  if((fd = open("/dev/vcio", O_RDWR)) == -1)
    die_err("open");

  init = 0;
  }

uint32_t property[] = {
  0x00000040, // buffer size in bytes = 8 * 4 bytes
  0x00000000, // buffer request/response code = 0x00000000: process request
  0x00030006, //   tag identifier = 0x00030006 (Get temperature)
  0x00000008, //   value buffer size in bytes = 8
  0x00000000, //   request codes = 0
  0x00000000, //   value buffer - request/response = temperature_id = 0
  0x00000000, //   value buffer - response = value
  0x00000000  // end tag
  };
  

if(ioctl(fd, _IOWR(100, 0, char *), property) == -1)
  die_err("ioctl");

return property[6] / 1000;
}


// https://elinux.org/Interfacing_with_I2C_Devices
// Argon ONE Pi4 is on 0x1A
// Pi4 I2C is on i2c-1
void sendI2C(uint8_t command)
{
static uint16_t init = 1;
static int      fd;
char            b[1];

b[0] = command;

if(init)
  {
  if((fd = open("/dev/i2c-1", O_RDWR)) == -1)
    die_err("open");

  if(ioctl(fd, I2C_SLAVE, 0x1a) == -1)
    die_err("iotctl");

  init = 0;
  }

write(fd, b, 1);
}




void shutdown()
{
sync();

// I prefer not to invoke the shutdown from here. Better to let let init call us via /lib/systemd/system-shutdown/
// However, lakka (for which I made this) does not call powerdown programs. So I have little choice for now.

//sendI2C(255);    // Power off, takes a few seconds

char *args[]={"/sbin/shutdown", "-h", "now"};
if(execve(args[0], &args[0], NULL) == -1)
  die_err("execve");
}




// Simple function to write a value to a file
void writeFile(char *Filename, char *Value)
{
int fd;

if((fd = open(Filename, O_WRONLY)) == -1)
  die_err("open");

if(write(fd, Value, strlen(Value)) != strlen(Value))
  die_err("write");

close(fd);
}


int readFileValue(int fd)
{
lseek(fd, 0, SEEK_SET);
char buffer[32] = {0};
read(fd, buffer, 32);
return atoi(buffer);
}


// The sysfs ABI is deprecated, but still maintained
// https://elinux.org/images/c/c8/Userspace-drivers-csimmonds-elce-2018_Chris-Simmonds.pdf
// https://www.kernel.org/doc/Documentation/gpio/sysfs.txt
// https://raspberrypi.stackexchange.com/questions/44416/polling-gpio-pin-from-c-always-getting-immediate-response

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <poll.h>
int detectEdge(void)
{
static uint16_t init = 1;
int fd;
static struct pollfd pollfd;
int ret;

struct stat statbuf;

if(init)
  {
  if(stat("/sys/class/gpio/gpio4", &statbuf) == 0)
    writeFile("/sys/class/gpio/unexport", "4");
  writeFile("/sys/class/gpio/export", "4");
  writeFile("/sys/class/gpio/gpio4/active_low", "1");
  writeFile("/sys/class/gpio/gpio4/edge", "rising");
  writeFile("/sys/class/gpio/gpio4/direction", "in");

  if((fd = open("/sys/class/gpio/gpio4/value", O_RDONLY)) == -1)
    die_err("open");

  pollfd.fd = fd;
  pollfd.events =  POLLPRI | POLLERR;
  }

ret = poll(&pollfd, 1, 1000);   // Wait 1 second for events on GPIO4

if(ret == -1)
  die_err("poll");

if(ret > 0)
  {
  readFileValue(pollfd.fd);
  if(init)
    init = 0;                // Eat the first one, it always happens during settig this up
    else
    return 1;
  }

return 0;
}


// This function assumes it gets called with 1 second intervals
//
// I'll keep the speeds at what the manual suggests:
//
// When the CPU temperature is at 55 C, the fan will run at  10% of its maximum speed
// When the CPU temperature is at 60 C, the fan will run at  55% of its maximum speed
// When the CPU temperature is at 65 C, the fan will run at 100% of its maximum speed
//
// When running this function, fan should immediately ramp up if temperature is high, but wait
// 10 seconds before scaling down the fan speed.
//
void setFan(int Temp)
{
static int cooldown = 0;
static int current = 0;

if(cooldown)
  cooldown--;

if(Temp >= 65 && current < 65)
  {
  current = Temp;
  sendI2C(100);
  cooldown = 10;
  }

if(cooldown)
  return;

if(Temp >= 60 && current < 60)
  {
  current = Temp;
  sendI2C(55);
  cooldown = 10;
  return;
  }

if(cooldown)
  return;

if(Temp >= 55 && current < 55)
  {
  current = Temp;
  sendI2C(10);
  cooldown = 10;
  return;
  }

if(cooldown)
  return;

if(Temp < 55 && current >= 55)
  {
  current = Temp;
  sendI2C(0);
  }
}





int main(int argc, char *argv[])
{
// Soft-link yourself into /lib/systemd/system-shutdown/ and when called with that program name, directly call a poweroff

// Go to daemon

int len = strlen(argv[0]);

if(len >= 8 && strcmp(&argv[0][len - 8], "poweroff") == 0)
  {
  sendI2C(255);    // Power off, takes a few seconds
  exit(0);
  }

// Otherwise, start monitoring

while(1)
  {
  setFan(readTemp());
  if(detectEdge())
    shutdown();
  }

return 0;
}

// EOF
