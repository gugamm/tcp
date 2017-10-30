#ifndef __GMM_TCP_SOCKET
#define __GMM_TCP_SOCKET

namespace TCP {
    // Represents a connection in which we can send or receive data
    class TCPConnection {
    private:
        int socketID;
    public:
        size_t chunkBufSize = 512;
        
        // Creates a TCPConnection wrapper
        TCPConnection(int socketID);
        // Closes the TCPConnection 
        ~TCPConnection();
        // read bytes from connection stream and store into an array
        char *read(size_t len = 1024, ssize_t *oBytesRead = NULL);
        // read bytes from connection stream, store into buffer and return bytes read
        ssize_t read(char *buf, size_t len);
        // write bytes to connection stram
        ssize_t write(void *buf, size_t len);
        // read a chunk of data and store into a new messageBuf
        // free the old buf and update currBufSize and currOffset to new bufSize and new currOffset
        // returns NULL if fails (and the messageBuf is not modified)
        // returns a new messageBuf if success and free the old buf
        // if messageBuf = NULL, initialize a messageBuf with BUF_SIZE(512)
        char *readChunk(char *messageBuf, size_t *currBufSize, size_t *currOffset, ssize_t *bytesRead = NULL);
        // sends a file in kernel mode to destination (uses sendfile under the hood)
        ssize_t sendfile(int fileFD, off_t *offset, size_t count);
    };
    
    TCPConnection *connect(const char *host, const char *port);

    // Represents a server in which we can accept new connections
    class TCPServer {
    private:
        int socketID;
        bool listening;
    public:
        // Creates a wrapper for a TCPServer
        TCPServer();
        // Free resources of a server
        ~TCPServer();
        // listen to a port and returns a boolean indicating if no error occured
        bool listen(const char *port, int backlog = 10);
        // Accepts a connection
        TCPConnection *accept();
    };
}

#endif
