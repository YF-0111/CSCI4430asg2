#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

char *change_http_header(char *http_header, int header_len, char *host, int host_len, int *new_len)
{
    char *new_header = (char *)malloc(sizeof(char) * (header_len + host_len));
    memset(new_header, '\0', sizeof(char));
    int i, j;
    for (i = 0, j = 0; i < header_len;)
    {
        if (http_header[i] == '\n')
        {
            new_header[j++] = http_header[i++];
            if (http_header[i - 2] == '\r' && http_header[i] == 'H' && http_header[i + 1] == 'o' && http_header[i + 2] == 's' && http_header[i + 3] == 't')
            {
                char type[] = "Host: ";
                for (int z = 0; z < strlen(type); z++)
                {
                    new_header[j++] = type[z];
                }
                for (int z = 0; z < host_len; z++)
                {
                    new_header[j++] = host[z];
                }
                new_header[j++] = '\r';
                new_header[j++] = '\n';
                while (http_header[i++] != '\n')
                    ;
            }
        }
        else
        {
            new_header[j++] = http_header[i++];
        }
    }
    *new_len = j;
    return new_header;
}

int creat_server_socket(char *host, int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in server_addr;
    struct hostent *server = (struct hostent *)gethostbyname(host);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;                                      // use IPv4
    server_addr.sin_addr.s_addr = *(unsigned int *)server->h_addr_list[0]; // IPv4 address
    server_addr.sin_port = htons(port);                                    // port
    if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("don't connect server");
        exit(1);
    }
    return server_fd;
}

int get_pock_length(char *http_header, int header_len)
{
    char str_len_key[] = "Content-Length: ";
    char *tmp = strstr(http_header, str_len_key);
    tmp = strchr(tmp, ':');
    return atoi(tmp + 1);
}

int main()
{
    int proxy_fd, browser_fd;
    proxy_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int result;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));           // every byte = 0
    addr.sin_family = AF_INET;                // use IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // IPv4 address
    addr.sin_port = htons(81);                // port
    bind(proxy_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(proxy_fd, 8);
    struct sockaddr_in addr_brower;
    socklen_t addr_len = sizeof(addr_brower);
    fd_set readfds, testfds;
    FD_ZERO(&readfds);
    FD_SET(proxy_fd, &readfds);
    char buffer[8000];
    long byte_recved;
    char *host = "10.0.0.1:80";
    int server_fd = creat_server_socket("10.0.0.1", 80);
    while (1)
    {
        int fd;
        int nread;
        testfds = readfds;
        printf("waiting\n");
        result = select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, NULL);
        if (result < 1)
        {
            perror("run error");
            exit(1);
        }
        for (fd = 0; fd < FD_SETSIZE; fd++)
        {
            if (FD_ISSET(fd, &testfds))
            {
                if (fd == proxy_fd)
                {
                    // printf("create");
                    addr_len = sizeof(addr_brower);
                    browser_fd = accept(proxy_fd, (struct sockaddr *)&addr_brower, &addr_len);
                    FD_SET(browser_fd, &readfds);
                    int len = read(browser_fd, &buffer, 1000);
                    char *change_http = change_http_header(buffer, len, host, strlen(host), &len);
                    send(server_fd, change_http, len, MSG_NOSIGNAL);
                    len = recv(server_fd, buffer, 4000, MSG_NOSIGNAL);
                    send(browser_fd, buffer, len, MSG_NOSIGNAL);
                    int sum = get_pock_length(buffer, len);
                    int i = len;
                    while (i < sum)
                    {
                        len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);
                        send(fd, buffer, len, MSG_NOSIGNAL);
                        i += len;
                    }

                    // printf("removeing");
                }
                else
                {
                    // ioctl(fd, FIONREAD, &nread);
                    int len = recv(fd, buffer, 4000, MSG_NOSIGNAL);
                    if (len == 0)
                    {
                        close(fd);
                        FD_CLR(fd, &readfds);
                        printf("removeing %d", fd);
                    }
                    else
                    {
                        // int len = recv(fd, buffer, 4000, MSG_NOSIGNAL);
                        char *change_http = change_http_header(buffer, len, host, strlen(host), &len);
                        send(server_fd, change_http, len, MSG_NOSIGNAL);

                        len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);
                        send(fd, buffer, len, MSG_NOSIGNAL);
                        int sum = get_pock_length(buffer, len);
                        char* end = strstr(buffer,"\r\n\r\n");
                        len-=end-buffer+4;
                        printf("%d %d\n", sum, len);
                        int i = len;
                        while (i < sum)
                        {
                            len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);
                            send(fd, buffer, len, MSG_NOSIGNAL);
                            i += len;
                        }
                    }
                }
            }
        }
    }
}
