#include "mail.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void createConnection(Connection *pConn) {
  pConn->ssl = NULL;
  pConn->use_tls = 0;

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

void readStream(Connection *pConn, char **pData, int *nDataLen) {
  *pData = (char *)malloc(sizeof(char) * 1024);
  ssize_t totalBytesRead = 0;
  size_t capacity = 1024;

  while (1) {
    ssize_t n;
    if (pConn->use_tls && pConn->ssl) {
      n = SSL_read(pConn->ssl, *pData + totalBytesRead, 1);
    } else {
      n = recv(pConn->socketfd, *pData + totalBytesRead, 1, 0);
    }
    if (n <= 0) {
      break;
    }

    totalBytesRead += n;

    if ((size_t)totalBytesRead >= capacity) {
      capacity *= 2;
      *pData = realloc(*pData, capacity);
    }

    if (totalBytesRead >= 2 && (*pData)[totalBytesRead - 2] == '\r' &&
        (*pData)[totalBytesRead - 1] == '\n') {

      int lineStart = 0;
      for (int i = totalBytesRead - 3; i >= 0; i--) {
        if (i >= 1 && (*pData)[i - 1] == '\r' && (*pData)[i] == '\n') {
          lineStart = i + 1;
          break;
        }
      }

      if (totalBytesRead - lineStart >= 4) {
        char separator = (*pData)[lineStart + 3];
        if (separator == ' ') {
          break;
        }
      }
    }
  }

  *nDataLen = totalBytesRead;
}

void writeStream(Connection *pConn, char *data, int nDataLen) {
  ssize_t totalBytesSent = 0;
  while (totalBytesSent < nDataLen) {
    ssize_t n;
    if (pConn->use_tls && pConn->ssl) {
      n = SSL_write(pConn->ssl, data + totalBytesSent, nDataLen - totalBytesSent);
    } else {
      n = send(pConn->socketfd, data + totalBytesSent, nDataLen - totalBytesSent, 0);
    }
    if (n < 0) {
      fprintf(stderr, "Failed to send data to %s:%d\n", pConn->host,
              pConn->port);
      exit(EXIT_FAILURE);
    }
    totalBytesSent += n;
    printf("Sent %zd bytes to %s:%d\n", n, pConn->host, pConn->port);
  }
}

void initOpenSSL(void) {
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();
}
void handleTLS(Connection *pConn) {
  // Create context
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx) {
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }

  // Use system CA bundle for verification
  SSL_CTX_set_default_verify_paths(ctx);

  // Create SSL object
  pConn->ssl = SSL_new(ctx);
  SSL_set_fd(pConn->ssl, pConn->socketfd);

  // SNI - critical for virtual hosting
  SSL_set_tlsext_host_name(pConn->ssl, pConn->host);

  // Handshake
  if (SSL_connect(pConn->ssl) != 1) {
    ERR_print_errors_fp(stderr);
    SSL_free(pConn->ssl);
    SSL_CTX_free(ctx);
    exit(EXIT_FAILURE);
  }

  pConn->use_tls = 1;
  SSL_CTX_free(ctx); // SSL holds reference
}

void closeConnection(Connection *pConn) {
  if (pConn->ssl) {
    SSL_shutdown(pConn->ssl);
    SSL_free(pConn->ssl);
    pConn->ssl = NULL;
  }
  close(pConn->socketfd);
  free(pConn->host);
  free(pConn->ip);
  pConn->socketfd = -1;
  pConn->host = NULL;
  pConn->ip = NULL;
  pConn->use_tls = 0;
}
