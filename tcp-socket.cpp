#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tcp-socket.hpp"

// Reallocate buffer to a new size, copying contents from buff to new buffer
char *realoc(char *from, size_t currSize, size_t nextSize) {
    char *to = (char *) malloc(sizeof(char) * nextSize);

    if (to == NULL) {
        free(from);
        return NULL;
    }

    for (size_t i = 0; i < currSize; i++) {
        to[i] = from[i];
    }

    free(from);

    return to;
}

namespace TCP
{
/**************************
 * TCP CONNECTION METHODS *
 **************************/

// Constructor method - Store the socket of the connection
TCPConnection::TCPConnection(int socketID)
{
    this->socketID = socketID;
}
// Destructor method - Free connection resources and closes the connection
TCPConnection::~TCPConnection()
{
    // close connection to client
    close(this->socketID);
}

char *TCPConnection::read(size_t len, ssize_t *oBytesRead) {
    char *buf = (char *) malloc(sizeof(char) * len);
    ssize_t bytesRead;
    if (buf == NULL) {
        return NULL;
    }

    bytesRead = ::recv(this->socketID, buf, len, 0);
    if (oBytesRead != NULL) {
        *oBytesRead = bytesRead;
    }

    if (bytesRead < 1) {
        free(buf);
        return NULL;
    }

    return buf;
}

ssize_t TCPConnection::read(char *buf, size_t len) {
    return ::recv(this->socketID, buf, len, 0);
}

ssize_t TCPConnection::write(void *buf, size_t len) {
    return send(this->socketID, buf, len, 0);
}

char *TCPConnection::readChunk(char *messageBuf, size_t *bufSize, size_t *bufOffset, ssize_t *oBytesRead) {
    // Initialize messageBuf if it's NULL
    if (messageBuf == NULL) {
        messageBuf = (char *) malloc(sizeof(char) * this->chunkBufSize);
        *bufSize = this->chunkBufSize;
        *bufOffset = 0;
        if (oBytesRead != NULL) {
            *oBytesRead = 0;
        }
        if (!messageBuf) {
            return NULL;
        }
    }

    ssize_t bytesRead;

    // read a chunk of data
    char *chunkBuf = this->read(this->chunkBufSize, &bytesRead);

    if (oBytesRead != NULL) {
        *oBytesRead = bytesRead;
    }

    // error reading chunk of data
    if (chunkBuf == NULL) {
        return NULL;
    }

    // check if enough space to store memory (if equals double to avoid future realocs)
    if (bytesRead + *bufOffset >= *bufSize) {
        // doubles the size
        *bufSize = *bufSize * 2;
        messageBuf = ::realoc(messageBuf, *bufOffset, *bufSize);
        if (messageBuf == NULL) {
            return NULL;
        }
    }

    // copy data from chunk to message buf
    ssize_t i;
    size_t idx;
    for (idx = *bufOffset, i = 0; i < bytesRead; i++, idx++) {
        messageBuf[idx] = chunkBuf[i];
    }

    // store new offset and free chunk
    free(chunkBuf);
    *bufOffset = *bufOffset + bytesRead;

    return messageBuf;
}

ssize_t TCPConnection::sendfile(int fileFD, off_t *offset, size_t count) {
    return ::sendfile(this->socketID, fileFD, offset, count);
}

/**********************
 * TCP CONNECT METHOD *
 **********************/

TCPConnection *connect(const char *host, const char *port) {
    int socketFD;

    struct addrinfo hints;    // hints to getaddrinfo
    struct addrinfo *result;  // result of getaddrinfo
    struct addrinfo *rp;      // result pointer of getaddrinfo list
    int addrInfoResult;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_protocol = 0;           /* TCP Protocol */

    addrInfoResult = getaddrinfo(host, port, &hints, &result);

    if (addrInfoResult != 0) {
        return NULL;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        socketFD = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        // error
        if (socketFD == -1)
        {
            continue;
        }

        // connect success
        if (connect(socketFD, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            break;
        }

        close(socketFD);
    }

    // clear space
    freeaddrinfo(result);

    // could not connect
    if (rp == NULL) {
        return NULL;
    }

    // connected
    return new TCPConnection(socketFD);
}

/**********************
 * TCP SERVER METHODS *
 **********************/

TCPServer::TCPServer()
{
    this->listening = false;
    this->socketID = 0;
}
TCPServer::~TCPServer()
{
    // if listening
    if (this->listening)
    {
        // close server
        close(this->socketID);
    }
}

// Listen method - takes a port and set the socket to listening mode (must be called before accept)
bool TCPServer::listen(const char *port, int backlog)
{
    // prevents from listening twice
    if (this->listening)
    {
        return true;
    }

    struct addrinfo hints;
    struct addrinfo *result; // addrinfo result (linked list)
    struct addrinfo *rp;     // result pointer to iterate over the result list
    int socketFD;
    int addrInfoResult;
    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* For wildcard IP address */
    hints.ai_protocol = 0;           /* TCP Protocol */

    addrInfoResult = getaddrinfo(NULL, port, &hints, &result);
    if (addrInfoResult != 0)
    { 
        return false;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        socketFD = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        // error
        if (socketFD == -1)
        {
            continue;
        }

        // bind success
        if (bind(socketFD, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            break;
        }

        close(socketFD);
    }

    // clear space
    freeaddrinfo(result);

    // It wasn't possible to bind to any address
    if (rp == NULL)
    {
        return false;
    }

    // put socket into listening mode
    if (::listen(socketFD, backlog) == -1)
    {
        close(socketFD);
        return false;
    };

    // socket is listening
    this->socketID = socketFD;
    this->listening = true;

    // success!
    return true;
}
// Accept method - accepts a TCPConnection from the connections queue
TCPConnection *TCPServer::accept()
{
    // accept a connection
    int clientSocket;

    // accept a connection
    clientSocket = ::accept(this->socketID, NULL, NULL);

    // error
    if (clientSocket == -1) {
        return NULL;
    }

    return new TCPConnection(clientSocket);
}
}