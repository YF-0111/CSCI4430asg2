#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>

const int SERVER_PORT = 80;

double msElapsed(struct timeval *start, struct timeval *end)
{
    int sec = end->tv_sec - start->tv_sec;
    int ms = end->tv_usec - start->tv_usec;
    double ans=1000.0 * sec + ms/1000.0;
    if(ans<0){
    	printf("sec: %d, ms: %d\n",sec,ms);
    }
    return ans;
}

int is_number(char c)
{
    if (c <= '9' && c >= '0')
    {
        return 1;
    }
    return 0;
}

char *change_http_type(char *http_header, char *type, char *value, int *new_len)
{
    char *new_header = (char *)malloc(sizeof(char) * (strlen(http_header) + strlen(value)));
    memset(new_header, '\0', sizeof(char));
    char *p = strstr(http_header, type);
    int cat_len = p - http_header;
    strncat(new_header, http_header, cat_len);
    strcat(new_header, type);
    if (!strcmp(type, "GET"))
    {
        strcat(new_header, " ");
    }
    else
    {
        strcat(new_header, ": ");
    }

    strcat(new_header, value);
    p = strstr(p, "\r\n");
    strcat(new_header, p);
    *new_len = strlen(new_header);
    return new_header;
}

// sample url : "/www/vod/1000Seg2-Frag3"
int is_frag_request(char *http_request)
{
    char url[100] = {0};
    int url_index = 4;
    while (http_request[url_index] != ' ')
    {
        url[url_index - 4] = http_request[url_index];
        url_index = url_index + 1;
    }
    char *seg_start = strstr(http_request, "Seg");
    char *frag_start = strstr(http_request, "Frag");
    if (seg_start != NULL && frag_start != NULL)
    {
        return 1;
    }
    return 0;
}

int is_manifest(char *http_request)
{
    if (strstr(http_request, "f4m") != NULL)
    {
        return 1;
    }
    return 0;
}

// TODO: implement this function:
// IMPORTANT: When the video player requests big_buck_bunny.f4m,
//  you should instead return big_buck_bunny_nolist.f4m.
//  This file does not list the available bitrates,
//  preventing the video player from attempting its own bitrate adaptation.
//  You proxy should, however, fetch big_buck_bunny.f4m for itself (i.e., don’t return it to the client) so you can parse the list of available encodings as described above.
//  Your proxy should keep this list of available
//  bitrates in a global container (not on a connection by connection basis).
void parse_xml(char *http_res, int *rates, int *len)
{
    // bitrates should be arranged in ascending order
    char *p = http_res;
    int n = 0;
    while (p = strstr(p, "bitrate=\""))
    {
        p += 9;
        printf("%d", atoi(p));
        rates[n++] = atoi(p);
    }

    *len = n;
}

double update_bitrate(double new_bitrate, double cur_rate, double alpha)
{
    return new_bitrate * alpha + (1 - alpha) * cur_rate;
}

int get_bitrate(double bandwidth, int bit_rates[], int len)
{
    bandwidth = bandwidth / 1.5;
    if (bit_rates[0] > bandwidth)
    {
        return bit_rates[0];
    }
    for (int i = 1; i < len; i++)
    {
        if (bit_rates[i] > bandwidth)
        {
            return bit_rates[i - 1];
        }
    }
    return bit_rates[len - 1];
}

// TODO: implement log function for each video chunk

//broswer-ip IP address of the browser issuing the request to the proxy.
//chunkname The name of the file your proxy requested from the web server (that is, the modified file name in the modified HTTP GET message).
//server-ip The IP address of the server to which the proxy forwarded this request.
//duration A floating point number representing the number of seconds it took to download this chunk from the web server to the proxy.
//tput The throughput you measured for the current chunk in Kbps.
//avg-tput Your current EWMA throughput estimate in Kbps.
//bitrate The bitrate your proxy requested for this chunk in Kbps.
// print log in the format: <browser-ip> <chunkname> <server-ip> <duration> <tput> <avg-tput> <bitrate>
void log_info(FILE *fp, char *browser_ip, char *chunk_name, char *server_ip, double duration, double tput, double avg_tput, int bit_rate)
{

    fprintf(fp, "%s %s %s %.2f %.2f %.2lf %d\n", browser_ip, chunk_name, server_ip, duration, tput, avg_tput, bit_rate);
    fflush(fp);
    printf("%s %s %s %.2f %.2f %.2lf %d\n", browser_ip, chunk_name, server_ip, duration, tput, avg_tput, bit_rate);
}

int int_to_string(int num, char *res)
{
    int num_of_digit = (int)log10(num) + 1;
    for (int i = 0; i < num_of_digit; i++)
    {
        res[num_of_digit - i - 1] = num % 10 + '0';
        num = num / 10;
    }
    return num_of_digit;
}

// this new function changes both bitrate and host
// should create a function to filter out non-segment requests
char *change_http_header(int new_bitrate, char *http_header, int header_len, char *host, int host_len, int *new_len)
{
    printf("Header Change Starts\n");
    char *new_header = (char *)malloc(sizeof(char) * (header_len + host_len));
    memset(new_header, '\0', sizeof(char));
    // two pointers; i for looping old request; j for new request
    int i = 0, j = 0;
    while (http_header[i] < '0' || http_header[i] > '9')
    {
        new_header[j++] = http_header[i++];
    }
    while (http_header[i] <= '9' && http_header[i] >= '0')
    {
        i++;
    }
    char bitrate[5] = {0};
    int digit = 0;
    digit = int_to_string(new_bitrate, bitrate);
    for (int d = 0; d < digit; d++)
    {
        new_header[j++] = bitrate[d];
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
    // printf("%s\n", new_header);
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
    // enable port reuse
    int yesval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yesval, sizeof(yesval)) == -1)
    {
        perror("Error setting socket options");
        return -1;
    }
    if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("don't connect server");
        exit(1);
    }
    return server_fd;
}

int get_pock_length(char *http_header, int *header_len)
{
    char str_len_key[] = "Content-Length: ";
    char *tmp = strstr(http_header, str_len_key);
    tmp = strchr(tmp, ':');
    int sum = atoi(tmp + 1);
    tmp = strstr(http_header, "\r\n\r\n");
    *header_len = (int)(tmp - http_header + 4);
    return sum;
}

char *get_chunk_name(char *http)
{
    char *start = strstr(http, "vod/");
    start += 4;
    char *end = strstr(start, " HTTP");
    int len = (int)(end - start);
    char *name = (char *)malloc(sizeof(char) * (len + 1));
    strncat(name, start, len);
    name[len] = '\0';
    return name;
}

//GET /1000
int get_chunk_bitrate(char* http){
    char bitrate[5]={0};
    int index_start=5;
    while(!is_number(http[index_start])){
        index_start++;
    }
    int num_start=index_start;
    while(is_number(http[index_start])){
        bitrate[index_start-num_start]=http[index_start];
        index_start++;
    }
    return atoi(bitrate);
}

int main(int argc, char **argv)
{
    if (argc != 6)
    {
        printf("Usage: ./miProxy --nodns <listen-port> <www-ip> <alpha> <log>\n");
        return 0;
    }
    int proxy_port = atoi(argv[2]);
    char *server_ip = argv[3];
    double alpha = atof(argv[4]);
    char *log_path = argv[5];
    // creating the proxy_fd where you can read input from browsers
    int proxy_fd, browser_fd;
    proxy_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int enable = 1;
    if (setsockopt(proxy_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        printf("setsockopt(SO_REUSEADDR) failed");
    int result;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));           // every byte = 0
    addr.sin_family = AF_INET;                // use IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // IPv4 address
    addr.sin_port = htons(proxy_port);        // port
    bind(proxy_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(proxy_fd, 8);
    struct sockaddr_in addr_brower;
    struct sockaddr_in addr_server;
    socklen_t addr_len = sizeof(addr_brower);
    fd_set readfds, testfds;
    FD_ZERO(&readfds);
    FD_SET(proxy_fd, &readfds);
    char buffer[1001];
    memset(buffer, '\0', 1001);
    long byte_recved;
    // creating server fd
    int server_fd = creat_server_socket(server_ip, SERVER_PORT);
    double cur_bitrate = 0.0;
    // open log file
    FILE *fp = fopen(log_path, "w");
    // bitrates
    int bitrates[10] = {0};
    int bitrate_len = 0;
    while (1)
    {
        int fd;
        int nread;
        testfds = readfds;
        printf("waiting\n");
        memset(buffer, '\0', 1001);
        result = select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, NULL);
        printf("select returns!\n");
        if (result < 1)
        {
            perror("run error");
            exit(1);
        }
        // TODO: problem with select. If a new clients connects while the old client
        // is playing the video, the video in the old session just stuck there
        // For each fd, see if any IO activity occurs
        // ONLY ONE HTTP REQUEST AND RESPONSE PER SELECT RETURN
        for (fd = 0; fd < FD_SETSIZE; fd++)
        {
            if (FD_ISSET(fd, &testfds))
            {
                // Input on proxy_fd, indicating that a client is trying to connect
                if (fd == proxy_fd)
                {
                    // Try to connect
                    addr_len = sizeof(addr_brower);
                    browser_fd = accept(proxy_fd, (struct sockaddr *)&addr_brower, &addr_len);

                    printf("New connection accepeted with fd: %d\n", browser_fd);
                    FD_SET(browser_fd, &readfds);
                    // accept http request from user
                    int len = read(browser_fd, &buffer, 1000);
                    // change http header accoring to type
                    int is_video_frag = is_frag_request(buffer);
                    char *change_http = NULL;
                    if (is_video_frag)
                    {
                        // get bitrate of video with current bitrate
                        int encoded_rate = get_bitrate(cur_bitrate, bitrates, bitrate_len);
                        change_http = change_http_header(encoded_rate, buffer, len, server_ip, strlen(server_ip), &len);
                        // printf("Video-Fragment request received");
                    }
                    else
                    {

                        change_http = change_http_type(buffer, (char *)"Host", server_ip, &len);
                        // printf("Video-Fragment request received");
                    }
                    // send http request to server
                    send(server_fd, change_http, len, MSG_NOSIGNAL);
                    // timers
                    struct timeval start_rcv, end_rcv;
                    // receive http response from server
                    // What if Content Length: is not available in the first recv
                    gettimeofday(&start_rcv, NULL);
                    len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);

                    // server response received
                    // send the response (header and some content) to client
                    send(browser_fd, buffer, len, MSG_NOSIGNAL);
                    // get Content-Length:
                    int headr_len = 0;

                    int sum = get_pock_length(buffer, &headr_len);
                    int i = len - headr_len;
                    while (i < sum)
                    {

                        // receiving the majority of the body
                        len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);
                        // server response received
                        send(browser_fd, buffer, len, MSG_NOSIGNAL);
                        i += len;
                    }
                    gettimeofday(&end_rcv, NULL);
                    if (is_video_frag)
                    {
                        // assert(end_rcv!=start_rcv);
                        // unit of sum: bytes; unit of time: ms; unit of t_new Kbps
                        double t_new = sum / msElapsed(&start_rcv, &end_rcv) * 8;
                        cur_bitrate = update_bitrate(t_new, cur_bitrate, alpha);
                        char* name = get_chunk_name(change_http);
                        log_info(fp, inet_ntoa(addr_brower.sin_addr), name, server_ip, msElapsed(&start_rcv, &end_rcv)/1000.0, t_new, cur_bitrate,
                                 get_chunk_bitrate(change_http));
                    }

                    printf("brower:%s ", inet_ntoa(addr_brower.sin_addr));
                }
                else
                {
                    // another request from established client
                    //  ioctl(fd, FIONREAD, &nrea d);
                    int len = recv(fd, buffer, 1000, MSG_NOSIGNAL);
                    addr_len = sizeof(addr_brower);
                    getpeername(fd, (struct sockaddr *)&addr_brower, &addr_len);
                    char *name = NULL;
                    if (len == 0)
                    {
                        close(fd);
                        FD_CLR(fd, &readfds);
                        printf("removeing %d", fd);
                    }
                    else
                    {
                        int is_video_frag = is_frag_request(buffer);
                        int is_f4m = is_manifest(buffer);
                        char *change_http = NULL;
                        if (is_video_frag)
                        {
                            int encoded_rate = get_bitrate(cur_bitrate, bitrates, bitrate_len);
                            change_http = change_http_header(encoded_rate, buffer, len, server_ip, strlen(server_ip), &len);
                            printf("Video\n");
                            name = get_chunk_name(change_http);
                        }
                        else if (is_f4m)
                        {
                            change_http = change_http_type(buffer, (char *)"Host", server_ip, &len);
                            send(server_fd, change_http, len, MSG_NOSIGNAL);
                            int f4m_len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);
                            int header_len = 0;
                            int sum = get_pock_length(buffer, &header_len);
                            char *xml = (char *)malloc(sizeof(char) * (sum + header_len + 1));
                            memset(xml, '\0', (sum + 1));
                            int i = f4m_len - header_len;
                            strncat(xml, buffer, f4m_len);
                            while (i < sum)
                            {
                                len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);
                                strncat(xml, buffer, len);
                                i += len;
                            }
                            parse_xml(xml, bitrates, &bitrate_len);
		            cur_bitrate = bitrates[0];
                            free(xml);
                            change_http = change_http_type(change_http, (char *)"GET", (char *)"/vod/big_buck_bunny_nolist.f4m HTTP/1.1", &len);
                        }
                        else
                        {
                            change_http = change_http_type(buffer, (char *)"Host", server_ip, &len);
                            printf("Non video\n");
                        }
                        // change bitrate
                        send(server_fd, change_http, len, MSG_NOSIGNAL);

                        struct timeval start_rcv;
                        struct timeval end_rcv;
                        gettimeofday(&start_rcv, NULL);
                        len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);
                        int header_len = 0;
                        int sum = get_pock_length(buffer, &header_len);

                        int i = len - header_len;

                        send(fd, buffer, len, MSG_NOSIGNAL);
                        while (i < sum)
                        {
                            len = recv(server_fd, buffer, 1000, MSG_NOSIGNAL);
                            send(fd, buffer, len, MSG_NOSIGNAL);
                            i += len;
                        }
                        gettimeofday(&end_rcv, NULL);
                        if (is_manifest(change_http))
                        {
                        }
                        if (is_video_frag)
                        {
                            // assert(end_rcv!=start_rcv);
                            // unit of sum: bytes; unit of time: ms; unit of t_new Kbps
                            double t_new = sum / msElapsed(&start_rcv, &end_rcv) * 8;
                            cur_bitrate = update_bitrate(t_new, cur_bitrate, alpha);
                            log_info(fp, inet_ntoa(addr_brower.sin_addr), name, server_ip, msElapsed(&start_rcv, &end_rcv)/1000.0, t_new, cur_bitrate,
                                     get_chunk_bitrate(change_http));
                            free(name);
                        }
                    }
                }
            }
        }
    }
}
