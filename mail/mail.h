#ifndef MAIL_H

typedef struct Connection {
  char *host;
  char *ip;
  int port;
  int socketfd;
} Connection;

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
void closeConnection(Connection *pConn);

#endif
