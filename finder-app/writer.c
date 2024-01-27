#include <stdio.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
  /** 
   * This is intended to identify the source of the message, and people 
   * conventionally set it to the name of the program that will submit the 
   * messages.
   */
  openlog(argv[0], LOG_PID, LOG_USER);
  
  if(argc != 3)
  {
    syslog(LOG_ERR, "Expected number of args is 2, not [%d]", argc - 1);
    syslog(LOG_ERR, "The first argument is a full path to a file (including filename) on the filesystem.");
    syslog(LOG_ERR, "The second argument is a text string which will be written within this file.");
    return 1;
  }

  const char *writeFile = argv[1];
  const char *writeStr = argv[2];
  
  /**
   * Creates an empty file for writing. If a file with the same name already 
   * exists, its content is erased and the file is considered as a new empty 
   * file.
   */
  FILE *file = fopen(writeFile, "w");
  
  if(!file)
  {
    syslog(LOG_ERR, "Unable to open file [%s]", writeFile);
    return 1;
  }
  
  fputs(writeStr, file);
  syslog(LOG_DEBUG, "Writing [%s] to [%s]", writeStr, writeFile);

  fclose(file);

  return 0;
}
