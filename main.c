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

  createConnection(&conn1);
  char *data = NULL;
  int dataLen;
  readStream(&conn1, &data, &dataLen);
  printf("%s", data);
  closeConnection(&conn1);

  return 0;
}
