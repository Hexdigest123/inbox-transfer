#ifndef MAIL_H

#include <openssl/err.h>
#include <openssl/ssl.h>

typedef struct Connection {
  char *host;
  char *ip;
  int port;
  int socketfd;
  SSL *ssl;
  int use_tls;
  int tag_counter;
} Connection;

typedef struct Capability {
  char *name;
} Capability;

/**
 * @brief initializes OpenSSL library (call once at program start)
 */
void initOpenSSL(void);

/**
 * @brief generates next IMAP command tag (A001, A002, etc.)
 */
char *imapNextTag(Connection *pConn);

/**
 * @brief establishes a connection to the mail server
 *
 * @param pConn pointer to connection object
 */
void createConnection(Connection *pConn);

/**
 * @brief reads all sorts of data from the mail server
 *
 * @param pConn pointer to connection object
 * @param pData pointer to data buffer
 * @param nDataLen pointer to length of the data buffer
 */
void readStream(Connection *pConn, char **pData, int *nDataLen);

/**
 * @brief write data to the server
 *
 * @param pConn pointer to connection object
 * @param data data to be sent to the server
 * @param nDataLen length of the data to be sent
 */
void writeStream(Connection *pConn, char *data, int nDataLen);

/**
 * @brief handles TLS/SSL upgrade via STARTTLS command
 *
 * @param pConn pointer to connection object
 */
void handleTLS(Connection *pConn);

/**
 * @brief closes the connection to the mail server
 *
 * @param pConn pointer to connection object
 */
void closeConnection(Connection *pConn);

#endif
