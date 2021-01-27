// TODO: Check out https://rextester.com/BUAFK86204

#include "os/c.h"
#include "util/int.h"
#include "util/io.h"
#include "util/string-view.h"
#include "util/string.h"

extern "C" {
// bits/alltypes.h
typedef uint16_t in_port_t;
typedef uint16_t sa_family_t;
typedef uint32_t socklen_t;

// netinet/in.h
typedef uint32_t in_addr_t;
struct in_addr {
    in_addr_t s_addr;
};
struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    uint8_t sin_zero[8];
};
uint16_t
htons(uint16_t) noexcept;

// sys/socket.h
#define AF_INET 2
#define INADDR_ANY 0
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_REUSEPORT 15
struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};
int
accept(int, struct sockaddr*, socklen_t*) noexcept;
int
bind(int, const struct sockaddr*, socklen_t) noexcept;
int
listen(int, int) noexcept;
int
setsockopt(int, int, int, const void*, socklen_t) noexcept;
int
socket(int, int, int) noexcept;
}

static String data;

static void
handleGet(int socket) noexcept {
    StringView contentType;
    if (data.size && (data[0] == '[' || data[0] == '{')) {
        contentType = "application/json";
    }
    else {
        contentType = "text/plain; charset=utf-8";
    }

    String response("HTTP/1.1 200 OK\nConnection: close\nContent-Type: ");
    response << contentType << "\nContent-Length: ";
    response << data.size << "\n\n" << data;

    write(socket, response.data, response.size);
    close(socket);
}

static void
handlePost(int socket, StringView req) noexcept {
    StringPosition body = req.find("\r\n\n");
    if (body != SV_NOT_FOUND) {
        data = req.substr(body + 2);
    }
    else {
        body = req.find("\r\n\r\n");
        if (body != SV_NOT_FOUND) {
            data = req.substr(body + 4);
        }
        else {
            String response = "HTTP/1.1 400 Bad Request\n";

            write(socket, response.data, response.size);
            close(socket);
            return;
        }
    }

    String resBody;
    resBody << data.size << '\n';

    String response = "HTTP/1.1 200 OK\nConnection: close\nContent-Length: ";
    response << resBody.size << "\n\n" << resBody;

    write(socket, response.data, response.size);
    close(socket);
}

static void
handleRequest(int socket, StringView req) noexcept {
    if (!req.size) {
        return;
    }
    switch (req[0]) {
        case 'G':
            handleGet(socket);
            break;
        case 'P':
            handlePost(socket, req);
            break;
        default:
            sout << "Unrecognized HTTP request\n";
            break;
    }
}

// TODO: Read larger HTTP requests.
static void
serve() noexcept {
    int sockfd, newsockfd, portno, n;
    unsigned int clilen;
    char buffer[4096];
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        serr << "socket()\n";
        return;
    }

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd,
             reinterpret_cast<struct sockaddr*>(&serv_addr),
             sizeof(serv_addr)) < 0) {
        serr << "bind()\n";
        return;
    }

    if (listen(sockfd, 5) < 0) {
        serr << "listen()\n";
        return;
    }

    sout << "Listening on port 8080\n";

    while (true) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd,
                           reinterpret_cast<struct sockaddr*>(&cli_addr),
                           &clilen);
        if (newsockfd < 0) {
            serr << "accept()\n";
            return;
        }

        memset(buffer, 0, sizeof(buffer));
        n = read(newsockfd, buffer, sizeof(buffer));
        if (n < 0) {
            serr << "read()\n";
            return;
        }

        StringView request(buffer, n);

        sout << buffer << '\n';

        handleRequest(newsockfd, request);
    }
}

int
main() noexcept {
    serve();
    return 0;
}
