#include <types.h>
#include <user.h>
#include <date.h>


static char* months[] = {
"January",
"Feburary",
"March",
"April",
"May",
"June",
"July",
"August",
"September",
"October",
"November",
"December",
};

int
main (int argc, char *argv[])
{
  struct rtcdate r;
  if (date(&r)) {
    printf(2, "date() syscall failed\n");
    exit();
  }
  printf(0,"Current UTC time: %d %s %d  %d:%d:%ds", r.day, 
                  months[r.month - 1], r.year, 
                  r.hour, r.minute, r.second);
  exit();
}
