#include "mail.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @brief establishes a connection to the mail server
 *
 * @param pConn pointer to connection object
 */
void createConnection(Connection *pConn) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "%s:%d\n", pConn->host, pConn->port);
    exit(EXIT_FAILURE);
  }
  struct hostent *resHost = gethostbyname(pConn->host);
  if (resHost == NULL || resHost->h_addr == NULL) {
    fprintf(stderr, "Failed to resolve hostname %s\n", pConn->host);
    exit(EXIT_FAILURE);
  }
  char *ipStr = inet_ntoa(*(struct in_addr *)resHost->h_addr);
  pConn->ip = strdup(ipStr);

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(pConn->port);
  serv_addr.sin_addr.s_addr = inet_addr(pConn->ip);
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Connection to %s:%d failed\n", pConn->host, pConn->port);
    exit(EXIT_FAILURE);
  };
  pConn->socketfd = sockfd;
  printf("Connected to %s:%d\n", pConn->host, pConn->port);
}
/**
 * @brief reads all sorts of data from the mail server
 *
 * @param pConn pointer to connection object
 * @param pData pointer to data buffer
 * @param nDataLen pointer to length of the data buffer
 */
void readStream(Connection *pConn, char **pData, int *nDataLen) {
  *pData = (char *)malloc(sizeof(char) * 1024);
  ssize_t totalBytesRead = 0;
  ssize_t n;

  while ((n = recv(pConn->socketfd, *pData + totalBytesRead, 1, 0)) > 0) {
    totalBytesRead += n;

    if (totalBytesRead >= 2 && (*pData)[totalBytesRead - 2] == '\r' &&
        (*pData)[totalBytesRead - 1] == '\n') {
      break;
    }

    if (totalBytesRead % 1024 == 0) {
      *pData = realloc(*pData, sizeof(char) * (totalBytesRead + 1024));
    }
  }

  *nDataLen = totalBytesRead;
}

/**
 * @brief handles TLS/SSL requests from the server
 *
 * @param pConn pointer to connection object
 */
void handleTLS(Connection *pConn);

/**
 * @brief closes the connection to the mail server
 *
 * @param pConn pointer to connection object
 */
void closeConnection(Connection *pConn) {
  close(pConn->socketfd);
  free(pConn->host);
  free(pConn->ip);
}
