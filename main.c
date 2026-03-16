#include "mail/mail.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  initOpenSSL();

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
  conn1.ssl = NULL;
  conn2.ssl = NULL;
  conn1.use_tls = 0;
  conn2.use_tls = 0;
  if (!conn1.host || !conn2.host || !conn1.ip || !conn2.ip) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  // TODO: possible buffer overflow, length check is pure regex based
  sscanf(argv[1], "%255[^:]:%d", conn1.host, &conn1.port);
  sscanf(argv[2], "%255[^:]:%d", conn2.host, &conn2.port);

  createConnection(&conn1);
  char *banner = NULL;
  int bannerLen;
  readStream(&conn1, &banner, &bannerLen);
  printf("%s", banner);

  if (strncmp(banner, "* OK", 4) != 0) {
    fprintf(stderr, "Invalid IMAP greeting: %s\n", banner);
    return 1;
  }

  char *tag = imapNextTag(&conn1);
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "%s CAPABILITY\r\n", tag);
  writeStream(&conn1, cmd, strlen(cmd));

  int has_starttls = 0;

  while (1) {
    char *line = NULL;
    int lineLen;
    readStream(&conn1, &line, &lineLen);
    printf("%s", line);

    if (strncmp(line, tag, strlen(tag)) == 0) {
      free(line);
      break;
    }

    if (strstr(line, "STARTTLS") != NULL) {
      has_starttls = 1;
    }

    free(line);
  }
  free(tag);

  if (has_starttls) {
    tag = imapNextTag(&conn1);
    snprintf(cmd, sizeof(cmd), "%s STARTTLS\r\n", tag);
    writeStream(&conn1, cmd, strlen(cmd));
    free(tag);

    char *response = NULL;
    int responseLen;
    readStream(&conn1, &response, &responseLen);
    printf("%s", response);

    if (strstr(response, "OK") != NULL) {
      handleTLS(&conn1);
      printf("Connection secured with TLS\n");
    } else {
      fprintf(stderr, "Failed to start TLS: %s\n", response);
      return 1;
    }
  }

  closeConnection(&conn1);

  return 0;
}
