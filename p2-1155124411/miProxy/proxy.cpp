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
int main(){
    int socket_fd ,browser_fd;
    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int result;
    struct sockaddr_in addr ;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8001);
    bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(socket_fd,8);
    struct sockaddr_in addr_conn;
    socklen_t addr_len = sizeof(addr_conn);
    fd_set readfds, testfds;
    FD_ZERO(&readfds);
    FD_SET(socket_fd, &readfds);
    
    char buffer[1000];
    long byte_recved;
    
    while(1){
        char ch;
        int fd;
        int nread;
        testfds = readfds;
        printf("waiting\n");
        result = select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0,NULL);
        if(result < 1){
            printf("error");
            exit(1);
        }
        for(fd = 0; fd<FD_SETSIZE; fd++){
            if(FD_ISSET(fd,&testfds)){
                addr_len = sizeof(addr_conn);
                browser_fd = accept(socket_fd, (struct sockaddr * )&addr_conn, &addr_len);
                FD_SET(browser_fd, &readfds);
                read(browser_fd, &buffer, 1000);
                printf("%s", buffer);
                printf("add browser on fd %d\n", browser_fd);
                // http parsing
                // another socket
                // send to server
            }else{
                ioctl(fd, FIONREAD, &nread);
                if(nread==0){
                    close(fd);
                    FD_CLR(fd, &readfds);
                    printf("removing %d", fd);
                }else{
                    read(fd, &buffer, 1000);
                    write(fd,"",0);
                }
            }
        }
    }
    return 0;
}
