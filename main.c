#include "mail/mail.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc < 3) {
    if (argc == 2) {
      if (strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s <host>:<port> <host2>:<port2> <mode>\n", argv[0]);
        printf("Example: %s localhost:8080 localhost:8081 copy\n", argv[0]);
        printf("Example: %s localhost:8080 localhost:8081 export\n", argv[0]);
        return 0;
      }
    }
    printf("Usage: %s <host>:<port> <host2>:<port2> <mode>\n", argv[0]);
    printf("If you need help %s -h\n", argv[0]);
    return 1;
  }

  Connection conn1, conn2;
  conn1.host = (char *)malloc(sizeof(char) * 256);
  conn2.host = (char *)malloc(sizeof(char) * 256);
  conn1.ip = (char *)malloc(sizeof(char) * 32);
  conn2.ip = (char *)malloc(sizeof(char) * 32);
  if (!conn1.host || !conn2.host || !conn1.ip || !conn2.ip) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  // TODO: possible buffer overflow, length check is pure regex based
  sscanf(argv[1], "%255[^:]:%d", conn1.host, &conn1.port);
  sscanf(argv[2], "%255[^:]:%d", conn2.host, &conn2.port);

  // CONNECTION TO SRC SERVER
  createConnection(&conn1);
  char *banner = NULL;
  int bannerLen;
  readStream(&conn1, &banner, &bannerLen);
  printf("%s", banner);

  writeStream(&conn1, "EHLO example.com\r\n", strlen("EHLO example.com\r\n"));
  char *capString = NULL;
  int capStringLength;
  Capability *capArray = (Capability *)malloc(sizeof(Capability) * 20);
  int capArrayLength = 0;
  readStream(&conn1, &capString, &capStringLength);

  for (int i = 0; i < capStringLength; i++) {
    if (capString[i] == '\r' && capString[i + 1] == '\n') {
      capString[i] = '\0';

      if (capArrayLength % 20 == 0 && capArrayLength != 0) {
        Capability *newCapArray = (Capability *)realloc(
            capArray, sizeof(Capability) * (capArrayLength + 20));
        if (!newCapArray) {
          fprintf(stderr, "Memory reallocation failed\n");
          return 1;
        }
        capArray = newCapArray;
      }
      capArray[capArrayLength].code = (int *)malloc(sizeof(int));
      capArray[capArrayLength].name = (char *)malloc(sizeof(char) * 256);
      capArray[capArrayLength].value = (char *)malloc(sizeof(char) * 256);
      if (!capArray[capArrayLength].code || !capArray[capArrayLength].name ||
          !capArray[capArrayLength].value) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
      }

      sscanf(capString, "%d-%s %s", capArray[capArrayLength].code,
             capArray[capArrayLength].name, capArray[capArrayLength].value);
      capArrayLength = capArrayLength + 1;

      capString += i + 2;
      i = -1;
    }
  }

  for (int i = 0; i < capArrayLength; i++) {
    printf("%s - %s\n", capArray[i].name, capArray[i].value);
    if (strcmp(capArray[i].name, "STARTTLS") == 0) {
    }
  }

  closeConnection(&conn1);
  // END OF CONNECTION TO SRC SERVER

  return 0;
}
