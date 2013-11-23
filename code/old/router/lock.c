#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int
main() {
  return (0 > open("/tmp/lock_file", O_CREAT | O_EXCL)) ? 1 : 0;
}
