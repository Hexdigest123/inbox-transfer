#include "mail/mail.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int export(char **argv) {
  Connection conn1;
  conn1.host = (char *)malloc(sizeof(char) * 256);
  conn1.ip = (char *)malloc(sizeof(char) * 32);
  conn1.ssl = NULL;
  conn1.use_tls = 0;
  if (!conn1.host || !conn1.ip) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  // TODO: possible buffer overflow, length check is pure regex based
  sscanf(argv[2], "%255[^:]:%d", conn1.host, &conn1.port);

  // START OF CONN SOURCE

  createConnection(&conn1);
  char *banner = NULL;
  int bannerLen;
  readStream(&conn1, &banner, &bannerLen);
  printf("%s", banner);

  if (strncmp(banner, "* OK", 4) != 0) {
    fprintf(stderr, "Invalid IMAP greeting: %s\n", banner);
    free(banner);
    return 1;
  }
  free(banner);
  banner = NULL;

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
      free(response);
      response = NULL;
      free(conn1.host);
      free(conn1.ip);
      return 1;
    }
    free(response);
    response = NULL;
  }

  char username[256];
  char password[256];
  char *authString = (char *)malloc(sizeof(char) * 512);
  memset(username, 0, sizeof(username));
  if (!authString) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  printf("Enter username: ");
  fgets(username, 255, stdin);
  printf("Enter password: ");
  fgets(password, 255, stdin);

  username[strcspn(username, "\n")] = '\0';
  password[strcspn(password, "\n")] = '\0';

  tag = imapNextTag(&conn1);
  snprintf(authString, 512, "%s LOGIN %s %s\r\n", tag, username, password);
  writeStream(&conn1, authString, strlen(authString));
  free(authString);
  authString = NULL;

  while (1) {
    char *response = NULL;
    int responseLen;
    readStream(&conn1, &response, &responseLen);
    if (strncmp(response, tag, strlen(tag)) == 0) {
      printf("%s", response);
      free(response);
      break;
    }
    free(response);
  }
  free(tag);
  tag = NULL;

  tag = imapNextTag(&conn1);
  snprintf(cmd, sizeof(cmd), "%s SELECT INBOX\r\n", tag);
  writeStream(&conn1, cmd, strlen(cmd));

  while (1) {
    char *response = NULL;
    int responseLen;
    readStream(&conn1, &response, &responseLen);
    if (strncmp(response, tag, strlen(tag)) == 0) {
      printf("%s", response);
      free(response);
      break;
    }
    free(response);
  }
  free(tag);
  tag = NULL;

  tag = imapNextTag(&conn1);
  snprintf(cmd, sizeof(cmd), "%s SEARCH ALL\r\n", tag);
  writeStream(&conn1, cmd, strlen(cmd));

  int messageIds[10000];
  int messageCount = 0;

  while (1) {
    char *response = NULL;
    int responseLen;
    readStream(&conn1, &response, &responseLen);
    printf("%s", response);

    if (strncmp(response, tag, strlen(tag)) == 0) {
      free(response);
      break;
    }

    if (strncmp(response, "* SEARCH ", 9) == 0) {
      char *ptr = response + 9;
      while (*ptr != '\0' && *ptr != '\r' && *ptr != '\n') {
        while (*ptr == ' ')
          ptr++;
        if (*ptr >= '0' && *ptr <= '9') {
          messageIds[messageCount++] = atoi(ptr);
          while (*ptr >= '0' && *ptr <= '9')
            ptr++;
        } else {
          break;
        }
      }
    }

    free(response);
  }
  free(tag);
  tag = NULL;

  printf("Found %d messages\n", messageCount);

  if (mkdir("export", 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "Failed to create export directory: %s\n", strerror(errno));
    free(tag);
    tag = NULL;
    closeConnection(&conn1);
    free(conn1.host);
    conn1.host = NULL;
    free(conn1.ip);
    conn1.ip = NULL;
    return 1;
  }

  for (int i = 0; i < messageCount; i++) {
    int msgNum = messageIds[i];
    printf("Fetching message %d...\n", msgNum);

    tag = imapNextTag(&conn1);
    snprintf(cmd, sizeof(cmd), "%s FETCH %d (BODY[])\r\n", tag, msgNum);
    writeStream(&conn1, cmd, strlen(cmd));

    int literalSize = 0;
    char *messageContent = NULL;

    while (1) {
      char *response = NULL;
      int responseLen;
      readStream(&conn1, &response, &responseLen);

      if (strstr(response, "BODY[] {") != NULL) {
        char *sizeStart = strstr(response, "{");
        if (sizeStart) {
          literalSize = atoi(sizeStart + 1);
          messageContent = (char *)malloc(literalSize + 1);
          if (!messageContent) {
            fprintf(stderr, "Memory allocation failed\n");
            free(response);
            free(tag);
            return 1;
          }
          free(response);
          break;
        }
      }

      if (strncmp(response, tag, strlen(tag)) == 0) {
        free(response);
        break;
      }

      free(response);
    }

    if (literalSize > 0 && messageContent) {
      int bytesRead = readLiteral(&conn1, messageContent, literalSize);
      if (bytesRead < 0) {
        fprintf(stderr, "Failed to read message content\n");
        free(messageContent);
        messageContent = NULL;
        free(tag);
        tag = NULL;
        continue;
      }

      char *closing = NULL;
      int closingLen;
      readStream(&conn1, &closing, &closingLen);
      free(closing);
      closing = NULL;

      char *okResponse = NULL;
      int okLen;
      readStream(&conn1, &okResponse, &okLen);
      if (strncmp(okResponse, tag, strlen(tag)) != 0) {
        fprintf(stderr, "Unexpected response: %s\n", okResponse);
      }
      free(okResponse);
      okResponse = NULL;

      char filepath[512];
      snprintf(filepath, sizeof(filepath), "export/message_%d.eml", msgNum);
      FILE *f = fopen(filepath, "w");
      if (f) {
        fwrite(messageContent, 1, literalSize, f);
        fclose(f);
        printf("Exported: %s\n", filepath);
      } else {
        fprintf(stderr, "Failed to write %s: %s\n", filepath, strerror(errno));
      }

      free(messageContent);
      messageContent = NULL;
    }

    free(tag);
    tag = NULL;
  }

  printf("Export complete. %d messages exported to ./export/\n", messageCount);

  closeConnection(&conn1);

  free(conn1.host);
  conn1.host = NULL;
  free(conn1.ip);
  conn1.ip = NULL;
  return 0;
}

int import(char **argv) {
  Connection conn1;
  conn1.host = (char *)malloc(sizeof(char) * 256);
  conn1.ip = (char *)malloc(sizeof(char) * 32);
  conn1.ssl = NULL;
  conn1.use_tls = 0;
  if (!conn1.host || !conn1.ip) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  // TODO: possible buffer overflow, length check is pure regex based
  sscanf(argv[2], "%255[^:]:%d", conn1.host, &conn1.port);

  createConnection(&conn1);
  char *banner = NULL;
  int bannerLen;
  readStream(&conn1, &banner, &bannerLen);
  printf("%s", banner);

  if (strncmp(banner, "* OK", 4) != 0) {
    fprintf(stderr, "Invalid IMAP greeting: %s\n", banner);
    free(banner);
    free(conn1.host);
    free(conn1.ip);
    return 1;
  }
  free(banner);
  banner = NULL;

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
      free(response);
      free(conn1.host);
      free(conn1.ip);
      return 1;
    }
    free(response);
    response = NULL;
  }

  char username[256];
  char password[256];
  char *authString = (char *)malloc(sizeof(char) * 512);
  memset(username, 0, sizeof(username));
  if (!authString) {
    fprintf(stderr, "Memory allocation failed\n");
    free(conn1.host);
    free(conn1.ip);
    return 1;
  }

  printf("Enter username: ");
  fgets(username, 255, stdin);
  printf("Enter password: ");
  fgets(password, 255, stdin);

  username[strcspn(username, "\n")] = '\0';
  password[strcspn(password, "\n")] = '\0';

  tag = imapNextTag(&conn1);
  snprintf(authString, 512, "%s LOGIN %s %s\r\n", tag, username, password);
  writeStream(&conn1, authString, strlen(authString));
  free(authString);
  authString = NULL;

  while (1) {
    char *response = NULL;
    int responseLen;
    readStream(&conn1, &response, &responseLen);
    if (strncmp(response, tag, strlen(tag)) == 0) {
      printf("%s", response);
      free(response);
      break;
    }
    free(response);
  }
  free(tag);
  tag = NULL;

  DIR *dir = opendir("export");
  if (dir == NULL) {
    fprintf(stderr, "Failed to open export directory: %s\n", strerror(errno));
    closeConnection(&conn1);
    free(conn1.host);
    free(conn1.ip);
    return 1;
  }

  int importCount = 0;
  struct dirent *entry;
  char filepath[512];
  char *appendCmd = NULL;
  int appendCmdLen = 0;

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN) {
      continue;
    }

    char *ext = strrchr(entry->d_name, '.');
    if (ext == NULL || strcmp(ext, ".eml") != 0) {
      continue;
    }

    snprintf(filepath, sizeof(filepath), "export/%s", entry->d_name);

    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
      fprintf(stderr, "Failed to open %s: %s\n", filepath, strerror(errno));
      continue;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize <= 0) {
      fprintf(stderr, "Skipping empty file: %s\n", filepath);
      fclose(f);
      continue;
    }

    char *fileContent = (char *)malloc(fileSize + 1);
    if (fileContent == NULL) {
      fprintf(stderr, "Memory allocation failed for %s\n", filepath);
      fclose(f);
      continue;
    }

    size_t bytesRead = fread(fileContent, 1, fileSize, f);
    fclose(f);

    if (bytesRead != (size_t)fileSize) {
      fprintf(stderr, "Failed to read complete file: %s\n", filepath);
      free(fileContent);
      continue;
    }

    printf("Importing: %s (%ld bytes)\n", entry->d_name, fileSize);

    tag = imapNextTag(&conn1);
    appendCmdLen = snprintf(NULL, 0, "%s APPEND INBOX {%ld}\r\n", tag, fileSize) + 1;
    appendCmd = (char *)malloc(appendCmdLen);
    if (appendCmd == NULL) {
      fprintf(stderr, "Memory allocation failed\n");
      free(fileContent);
      free(tag);
      continue;
    }

    snprintf(appendCmd, appendCmdLen, "%s APPEND INBOX {%ld}\r\n", tag, fileSize);
    writeStream(&conn1, appendCmd, strlen(appendCmd));
    free(appendCmd);
    appendCmd = NULL;

    char *continuation = NULL;
    int contLen;
    readStream(&conn1, &continuation, &contLen);

    if (continuation == NULL || strncmp(continuation, "+", 1) != 0) {
      fprintf(stderr, "Server not ready for APPEND: %s\n", continuation ? continuation : "(null)");
      free(continuation);
      free(fileContent);
      free(tag);
      continue;
    }
    free(continuation);

    writeStream(&conn1, fileContent, fileSize);
    writeStream(&conn1, "\r\n", 2);

    free(fileContent);
    fileContent = NULL;

    while (1) {
      char *response = NULL;
      int responseLen;
      readStream(&conn1, &response, &responseLen);
      if (strncmp(response, tag, strlen(tag)) == 0) {
        printf("%s", response);
        if (strstr(response, "OK") != NULL) {
          importCount++;
        } else {
          fprintf(stderr, "Failed to import %s: %s\n", entry->d_name, response);
        }
        free(response);
        break;
      }
      free(response);
    }
    free(tag);
    tag = NULL;
  }

  closedir(dir);

  printf("Import complete. %d messages imported.\n", importCount);

  closeConnection(&conn1);

  free(conn1.host);
  conn1.host = NULL;
  free(conn1.ip);
  conn1.ip = NULL;
  return 0;
}

int main(int argc, char **argv) {
  initOpenSSL();

  if (argc < 3) {
    if (argc == 2) {
      if (strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s [--export/--import] <host>:<port>\n", argv[0]);
        printf("Example: %s --export localhost:8080 will export all contents "
               "of the inbox to ./export\n",
               argv[0]);
        printf("Example: %s --import localhost:8080 will import whatever .eml "
               "file is present in ./export\n",
               argv[0]);
        return 0;
      }
    }
    printf("Usage: %s [--export/--import] <host>:<port>\n", argv[0]);
    printf("If you need help %s -h\n", argv[0]);
    return 1;
  }

  if (strcmp(argv[1], "--export") == 0) {
    return export(argv);
  } else if (strcmp(argv[1], "--import") == 0) {
    return import(argv);
  } else {
    fprintf(stderr, "Unknown option: %s\n", argv[1]);
    return 1;
  }

  return 0;
}
