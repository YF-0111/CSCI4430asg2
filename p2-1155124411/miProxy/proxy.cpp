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
#include <math.h>

//TODO: logger: get client ip from struct
//TODO: change bitrate function from int to double
//TODO: a function that return bitrate and segment
//TODO: xml parse bitrate
//TODO: change bitrate function add non segment request

int BITRATE[]={10,500,1000};
int BITRATE_LEN=3;


int update_bitrate(int new_bitrate,int cur_rate,double alpha){
    return (int)(new_bitrate*alpha+(1-alpha)*cur_rate);
}

int get_bitrate(int bandwidth){
    if(BITRATE[0]>bandwidth){
        return BITRATE[0];
    }
    for(int i=1;i<BITRATE_LEN;i++){
        if (BITRATE[i]>bandwidth){
            return BITRATE[i-1];
        }
    }
    return BITRATE[BITRATE_LEN-1];
}

//print log in the format: <browser-ip> <chunkname> <server-ip> <duration> <tput> <avg-tput> <bitrate>
//broswer-ip IP address of the browser issuing the request to the proxy.
//chunkname The name of the file your proxy requested from the web server (that is, the modified file name in the modified HTTP GET message).
//server-ip The IP address of the server to which the proxy forwarded this request.
//duration A floating point number representing the number of seconds it took to download this chunk from the web server to the proxy.
//tput The throughput you measured for the current chunk in Kbps.
//avg-tput Your current EWMA throughput estimate in Kbps.
//bitrate The bitrate your proxy requested for this chunk in Kbps.
void log(FILE* fp,char* browser_ip,char* chunk_name,char* server_ip,int duration,int tput,double avg_tput,int bit_rate){
    fprintf(fp,"%s %s %s %d %d %.1lf %d\n",browser_ip,chunk_name,server_ip,duration,tput,avg_tput,bit_rate);
}

int int_to_string(int num,char* res){
    int num_of_digit= (int)log10(num)+1;
    for(int i=0;i<num_of_digit;i++){
        res[num_of_digit-i-1]=num%10+'0';
        num=num/10;
    }
    return num_of_digit;
}

//this new function changes both bitrate and host
//should create a function to filter out non-segment requests
char *change_http_header(int new_bitrate,char *http_header, int header_len, char *host, int host_len, int *new_len)
{
    char *new_header = (char *)malloc(sizeof(char) * (header_len + host_len));
    memset(new_header, '\0', sizeof(char));
    //two pointers; i for looping old request; j for new request
    int i=0, j=0;
    while(http_header[i]<'0'||http_header[i]>'9'){
        new_header[j++] = http_header[i++];
    }
    while(http_header[i]<='9'&&http_header[i]>='0'){
        i++;
    }
    char bitrate[5]={0}; int digit=0;
    digit= int_to_string(new_bitrate,bitrate);
    for(int d=0;d<digit;d++){
        new_header[j++]=bitrate[d];
    }
    for (i, j; i < header_len;)
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

int main(int argc, char** argv)
{
    if(argc!=6){
        printf("Usage: ./miProxy --nodns <listen-port> <www-ip> <alpha> <log>\n");
        return 0;
    }
    int proxy_port= atoi(argv[2]);
    char* server_ip=argv[3];
    double alpha= atof(argv[4]);
    char* log_path=argv[5];
    //creating the proxy_fd where you can read input from browsers
    int proxy_fd, browser_fd;
    proxy_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int result;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));           // every byte = 0
    addr.sin_family = AF_INET;                // use IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // IPv4 address
    addr.sin_port = htons(proxy_port);                // port
    bind(proxy_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(proxy_fd, 8);
    struct sockaddr_in addr_brower;
    socklen_t addr_len = sizeof(addr_brower);
    fd_set readfds, testfds;
    FD_ZERO(&readfds);
    FD_SET(proxy_fd, &readfds);
    char buffer[8000];
    long byte_recved;
    //creating server fd
    int server_fd = creat_server_socket(server_ip, 80);
    int cur_bitrate=0;
    //open log file
    FILE* fp= fopen(log_path,"w");
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
        //For each fd, see if any IO activity occurs
        //ONLY ONE HTTP REQUEST AND RESPONSE PER SELECT RETURN
        for (fd = 0; fd < FD_SETSIZE; fd++)
        {
            if (FD_ISSET(fd, &testfds))
            {
                //Input on proxy_fd, indicating that a client is trying to connect
                if (fd == proxy_fd)
                {
                    //Try to connect
                    addr_len = sizeof(addr_brower);
                    browser_fd = accept(proxy_fd, (struct sockaddr *)&addr_brower, &addr_len);
                    FD_SET(browser_fd, &readfds);
                    //accept http request from user
                    int len = read(browser_fd, &buffer, 1000);
                    //change http header
                    char *change_http = change_http_header(cur_bitrate,buffer, len, server_ip, strlen(server_ip), &len);
                    //change_bitrate(change_http,)
                    //send http request to server
                    send(server_fd, change_http, len, MSG_NOSIGNAL);
                    //timers
                    time_t start_rcv,end_rcv;
                    //receive http response from server
                    //What if Content Length: is not available in the first recv?
                    time(&start_rcv);
                    len = recv(server_fd, buffer, 4000, MSG_NOSIGNAL);
                    //send the response (header and some content) to client
                    send(browser_fd, buffer, len, MSG_NOSIGNAL);
                    //get Content-Length:
                    int sum = get_pock_length(buffer, len);
                    int i = len;
                    while (i < sum)
                    {
                        //receiving the majority of the body
                        len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);
                        send(fd, buffer, len, MSG_NOSIGNAL);
                        i += len;
                    }
                    time(&end_rcv);
                    int t_new=sum/(end_rcv-start_rcv)*8/1000;
                    cur_bitrate= update_bitrate(t_new,cur_bitrate,alpha);

                    log(fp,NULL,NULL,server_ip,end_rcv-start_rcv,t_new,cur_bitrate,0);
                }
                else
                {
                    //another request from established client
                    // ioctl(fd, FIONREAD, &nrea d);
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
                        char *change_http = change_http_header(cur_bitrate,buffer, len, server_ip, strlen(server_ip), &len);
                        //change bitrate
                        send(server_fd, change_http, len, MSG_NOSIGNAL);
                        time_t start_rcv;
                        time_t end_rcv;
                        time(&start_rcv);
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
                        time(&end_rcv);
                        int t_new=sum/(end_rcv-start_rcv)*8/1000;
                        cur_bitrate= update_bitrate(t_new,cur_bitrate,alpha);
                    }
                }
            }
        }
    }
}
