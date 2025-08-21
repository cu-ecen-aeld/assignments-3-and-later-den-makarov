#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  if(argc != 3) {
     syslog(LOG_ERR, "The script accepts the following arguments:\n");
     syslog(LOG_ERR, "    - the first argument is a full path to a file (including filename) on the filesystem\n");
     syslog(LOG_ERR, "    - the second argument is a text string which will be written within this file\n");
     return EXIT_FAILURE;
  }

  const char* filename = argv[1];
  const char* message = argv[2];
  syslog(LOG_DEBUG, "Writing %s to %s\n", message, filename);

  FILE* fd = fopen(argv[1], "w");
  if(fd == NULL) {
    syslog(LOG_ERR, "Error openning file %s: %m\n", filename);
    return EXIT_FAILURE;
  }

  size_t wr = fwrite(message, 1, strlen(message), fd);
  if(wr != strlen(message)) {
    syslog(LOG_ERR, "Error writting to file %s message %s of length %ld while only %ld bytes written: %m\n", filename, message, strlen(message), wr);
    fclose(fd);
    return EXIT_FAILURE;
  }

  fclose(fd);
  exit(EXIT_SUCCESS);
}

