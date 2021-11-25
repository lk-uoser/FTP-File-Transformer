/* File Name: 
*** client.c ***
*/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
 
#define MAXLINE 4096
#define MAXSIZE 1024

int client_sock, data_sock, client_data_sock, n, z;
char buff[MAXSIZE], sendline[4096], line_in[4096];;
char databuff[MAXSIZE];
struct sockaddr_in server_addr, client_addr, data_addr;
char MODE = 'A';
char* IP = "127.0.0.1";
int PORT = 8000;

int command_get(char *filename);
int command_put(char *filename);
void get_command(char command[], char msg[], int len);
void command_pasv();
int command_port(char *ip, char *params);
void command_ls();
void command_dir();
void command_cd();
void command_re();
void command_stat();
void command_pwd();
void command_help();
void command_display();
void command_mode();
void read_server_msg();
void send_server_msg();
int login();

int main(int argv, char** argc)
{
    //从命令行获取IP和PORT
    if(argv == 3)
    {
	IP = argc[1];
	PORT = atoi(argc[2]);
    }

    //创建socket
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(client_sock < 0) {
        printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
        exit(0);
    }

    //初始化
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(PORT);
    client_addr.sin_addr.s_addr = inet_addr(IP);

    //连接服务器
    if(connect(client_sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }

    read_server_msg();

    //登陆函数
    while(login() != 0) {}

    //循环执行命令
    while(1) {
        memset(buff, 0, sizeof(buff));
        memset(sendline, 0, sizeof(sendline));
        printf("FTP> ");
        fgets(sendline, 4096, stdin);

        //获取命令
        sendline[strlen(sendline)-1] = '\0';
        char command[5];
        get_command(command, sendline, strlen(sendline));

        //执行pwd命令
        if(strcmp(command, "pwd") == 0) {
            command_pwd();
        }

        //执行cd命令
        else if(strcmp(command, "cd") == 0) {
            command_cd();
        }

        //被动模式
        else if(strcmp(command, "pasv") == 0) {
            send_server_msg();
            MODE = 'P';
            read_server_msg();
        }

        //执行ls命令
        else if(strcmp(command, "ls") == 0) {
            command_ls();
        }

        //执行dir命令
        else if(strcmp(command, "dir") == 0) {
            command_dir();
        }

        //执行stat命令
        else if(strcmp(command, "stat") == 0) {
            command_stat();
        }

        //执行rename命令
        else if(strcmp(command, "rename") == 0) {
            command_re();
        }

        //主动模式
        else if(strcmp(command, "port") == 0) {
            command_port("127.0.0.1", &sendline[5]);
            if((client_data_sock = accept(data_sock, (struct sockaddr*)NULL, NULL)) == -1) {
                printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            }
            read_server_msg();
        }

        //创建文件夹
        else if(strcmp(command, "mkdir") == 0) {
            send_server_msg();
            read_server_msg();
        }

        //删除文件
        else if(strcmp(command, "delete") == 0) {
            send_server_msg();
            read_server_msg();
        }

        //获取操作系统名
        else if(strcmp(command, "sys") == 0) {
            send_server_msg();
            read_server_msg();
        }

        //下载文件
        else if(strcmp(command, "get") == 0) {
            int r = command_get(&sendline[4]); 
            if(r == -2) {
                printf("Data connection not open!\n");
            }
            else if(r != 0) {
                printf("Get file from server failure!\n");
            }
        }

        //上传文件
        else if(strcmp(command, "put") == 0) {
            int r = command_put(&sendline[4]);
            if(r == -2) {
                printf("Data connection not open!\n");
            }
            else if(r != 0) {
                printf("Put file to server failure!\n");
            }
        }

        //help命令
        else if(strcmp(command, "help") == 0) {
            command_help();
        }

        //？命令
        else if(strcmp(command, "?") == 0) {
            command_display();
        }

        //显示服务器当前模式
        else if(strcmp(command, "mode") == 0) {
            command_mode();
        }

        //退出登陆
        else if(strcmp(command, "quit") == 0) {
            send_server_msg();
            read_server_msg();
            close(client_sock);
            close(data_sock);
            close(client_data_sock);
            break;
        }
    }
}

//登陆
int login() {
    char s[100] = "User<";
    strcat(s, IP);
    strcat(s, ":<none>>: ");
    printf("%s", s);
    fflush(stdin);
    fgets(line_in, 1024, stdin);
    line_in[strlen(line_in)-1] = '\0';
    char user[2048] = "USER ";
    strcat(user, line_in);
    if(write(client_sock, user, strlen(user)) < 0) {
        printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
        exit(0);
    }

    z = read(client_sock, buff, sizeof(buff));
    buff[z] = '\0';
    printf("%s", buff);

    //用户存在，等待输入密码
    if(strncmp("331", buff, 3) == 0) {
        printf("Pass: ");
        fgets(line_in, 1024, stdin);
        line_in[strlen(line_in)-1] = '\0';
        stpcpy(user, "PASS ");
        strcat(user, line_in);
        if(write(client_sock, user, strlen(user)) < 0) {
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
            exit(0);
        }

        //登陆成功
        z = read(client_sock, buff, sizeof(buff));
        buff[z] = '\0';
        printf("%s", buff);
        if(strncmp("220", buff, 3) == 0) {
            memset(buff, 0 , sizeof(buff));
            memset(line_in, 0 , sizeof(line_in));
            return 0;
        }
        else {
            return -1;
        }
    }
    memset(buff, 0 , sizeof(buff));
    memset(line_in, 0 , sizeof(line_in));
    return -1;
}

//主动数据连接
 int command_port(char *ip, char *params)
{
    MODE = 'A';
    unsigned long a0, a1, a2, a3, p0, p1, addr;
    //获取IP与PORT
    sscanf(params, "%lu.%lu.%lu.%lu:%lu %lu", &a0, &a1, &a2, &a3, &p0, &p1);
    data_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(data_sock == -1) {
        close(data_sock);
        printf("data socket() failed");
        return -1;
    }
    //初始化
    memset(&data_addr, 0 ,sizeof(data_addr));
    socklen_t data_addr_len = sizeof data_addr;
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(p0 * 256 + p1);
    data_addr.sin_addr.s_addr = inet_addr(ip);
    if(data_addr.sin_addr.s_addr == INADDR_NONE) {
        printf("INADDR_NONE");
        return -1;
    }
    //绑定
    z = bind(data_sock, (struct sockaddr *)&data_addr, data_addr_len);
    if(z == -1) {
        close(data_sock);
        perror("data_sock bind() failed\n");
        return -1;
    }
    //监听
    z = listen(data_sock, 1);
    if(z < 0) {
        close(data_sock);
        printf("data_sock listen() failed!\n");
        return -1;
    }
    send_server_msg();
    return 0;
}

//获取文件内容
int command_get(char *filename) {
    int f = -1;
    //处于被动模式
    if(MODE == 'P') {
        stpcpy(line_in, "_pasv");
        if(write(client_sock, line_in, strlen(line_in)) < 0) {
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
            exit(0);
        }
        command_pasv();
        read_server_msg();
        send_server_msg();

        n = read(client_sock, buff, sizeof(buff));
        buff[n] = '\0';
        if(strncmp(buff, "450", 3) == 0) {
            printf("%s",buff);
            memset(buff, 0, sizeof(buff));
        }
        else {
            FILE *putfile;
            char *file = "/root/Desktop/network/client";
            char *f_file = (char *) malloc(strlen(file) + strlen(filename));
            strcpy(f_file, file);
            strcat(f_file, filename);
            unsigned char databuf[MAXSIZE] = "";
            int bytes;
            
            if ((putfile = fopen(f_file, "w")) == 0) {
                printf("fopen failed to open file");
                close(data_sock);
                return -1;
            }
            while ((bytes = read(data_sock, databuf, MAXSIZE)) > 0) {
                write(fileno(putfile), databuf, bytes);
                f = 0;
                memset(&databuf, 0, MAXSIZE);
            }
            fclose(putfile);
            close(data_sock);
            read_server_msg();
        }
    }

    //处于主动模式且数据连接成功
    else if(MODE == 'A' && client_data_sock > 0) {
        send_server_msg();
        n = read(client_sock, buff, sizeof(buff));
        buff[n] = '\0';
        if(strncmp(buff, "450", 3) == 0) {
            printf("%s",buff);
            memset(buff, 0, sizeof(buff));
        }
        else {
            FILE *putfile;
            char *file = "/root/Desktop/network/client";
            char *f_file = (char *) malloc(strlen(file) + strlen(filename));
            strcpy(f_file, file);
            strcat(f_file, filename);
            unsigned char databuf[MAXSIZE] = "";
            int bytes;
            
            if ((putfile = fopen(f_file, "w")) == 0) {
                printf("fopen failed to open file");
                close(client_data_sock);
                return -1;
            }
            while ((bytes = read(client_data_sock, databuf, MAXSIZE)) > 0) {
                write(fileno(putfile), databuf, bytes);
                f = 0;
                memset(&databuf, 0, MAXSIZE);
            }
            fclose(putfile);
            close(client_data_sock);
            client_data_sock = -1;
            read_server_msg();
        }
    }

    //数据连接未建立
    else {
        return -2;
    }
    return f;
}

//上传文件内容
int command_put(char *filename) {
    int f = -1;
    //处于被动连接
    if(MODE == 'P') {
        stpcpy(line_in, "_pasv");
        if(write(client_sock, line_in, strlen(line_in)) < 0) {
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
            exit(0);
        }
        command_pasv();
        read_server_msg();
        FILE *getfile;
        unsigned char databuf[MAXSIZE] = "";
        int bytes;
        char buf[255];
        getcwd(buf,sizeof(buf)-1);
        strcat(buf, filename);
        if ((getfile = fopen(buf, "r")) == 0) {
            return -1;
        }
        send_server_msg();
        while((bytes = read(fileno(getfile), databuf, MAXSIZE)) > 0)
        {
            write(data_sock, (const char *)databuf, bytes);
            f = 0;
            memset(&databuf, 0, MAXSIZE);
        }
        read_server_msg();
        memset(&databuf, 0, MAXSIZE);
        fclose(getfile);
        close(data_sock);
        read_server_msg();
    }

    //处于主动连接且数据连接成功
    else if(MODE == 'A' && client_data_sock > 0) {
        send_server_msg();
        FILE *getfile;
        unsigned char databuf[MAXSIZE] = "";
        int bytes;
        char buf[255];
        getcwd(buf,sizeof(buf)-1);
        strcat(buf, filename);
        if ((getfile = fopen(buf, "r")) == 0) {
            printf("fopen failed to open file");
            close(client_data_sock);
            return -1;
        }
        while((bytes = read(fileno(getfile), databuf, MAXSIZE)) > 0)
        {
            write(client_data_sock, (const char *)databuf, bytes);
            f = 0;
            memset(&databuf, 0, MAXSIZE);
        }
        read_server_msg();
        memset(&databuf, 0, MAXSIZE);
        fclose(getfile);
        close(client_data_sock);
        client_data_sock = -1;
        read_server_msg();
    }

    //数据连接未建立
    else {
        return -2;
    }
    return f;
}

//被动方式
void command_pasv() {
    MODE = 'P';
    n = read(client_sock, buff, MAXSIZE);
    buff[n] = '\0';
    unsigned long p;
    p = strtoul(buff, NULL, 0);
    data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(data_sock < 0) {
        printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
        exit(0);
    }

    //初始化
    memset(&data_addr, 0, sizeof(server_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(p);
    data_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    //建立连接
    if(connect(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
}

//pwd命令
void command_pwd() {
    send_server_msg();
    read_server_msg();
}

//cd命令
void command_cd() {
    send_server_msg();
    read_server_msg();
}

//mode命令
void command_mode() {
    send_server_msg();
    read_server_msg();
}

//rename命令
void command_re() {
    send_server_msg();
    read_server_msg();
}

//stat命令
void command_stat() {
    send_server_msg();
    read_server_msg();
}

//ls
void command_ls() {
    //处于被动模式
    if(MODE == 'P') {
        stpcpy(line_in, "_pasv");
        if(write(client_sock, line_in, strlen(line_in)) < 0) {
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
            exit(0);
        }
        command_pasv();
        read_server_msg();
        send_server_msg();
        n = read(data_sock, databuff, sizeof(databuff));
        databuff[n] = '\n';
        printf("%s", databuff);
        memset(databuff, 0, sizeof(databuff));
        close(data_sock);
        read_server_msg();
    }

    //处于主动连接且数据连接成功
    else if(MODE == 'A' && client_data_sock > 0) {
        send_server_msg();
        n = read(client_data_sock, databuff, sizeof(databuff));
        databuff[n] = '\n';
        printf("%s", databuff);
        memset(databuff, 0, sizeof(databuff));
        close(client_data_sock);
        client_data_sock = -1;
        read_server_msg();
    }

    //数据连接未建立
    else {
        printf("Data connection not open!\n");
    }
    
}

//dir
void command_dir() {
    //处于被动模式
    if(MODE == 'P') {
        stpcpy(line_in, "_pasv");
        if(write(client_sock, line_in, strlen(line_in)) < 0) {
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
            exit(0);
        }
        command_pasv();
        read_server_msg();
        send_server_msg();
        n = read(data_sock, databuff, sizeof(databuff));
        databuff[n] = '\n';
        printf("%s", databuff);
        memset(databuff, 0, sizeof(databuff));
        close(data_sock);
        read_server_msg();
    }

    //处于主动连接且数据连接成功
    else if(MODE == 'A' && client_data_sock > 0) {
        send_server_msg();
        n = read(client_data_sock, databuff, sizeof(databuff));
        databuff[n] = '\n';
        printf("%s", databuff);
        memset(databuff, 0, sizeof(databuff));
        close(client_data_sock);
        client_data_sock = -1;
        read_server_msg();
    }

    //数据连接未建立
    else {
        printf("Data connection not open!\n");
    }
    
}

//help命令
void command_help() {
    printf("[command] \t\t [expression]\n");
    printf("get <file> \t\t get the file to local\n");
    printf("put <file> \t\t put the file to server\n");
    printf("pwd \t\t\t display server directory\n");
    printf("dir \t\t\t Lists a list of subdirectories and files under the directory\n");
    printf("ls \t\t\t display file in server directory\n");
    printf("mkdir <directory> \t create directory\n");
    printf("delete <file> \t\t delete file\n");
    printf("sys \t\t\t display server system name\n");
    printf("help \t\t\t user help\n");
    printf("? \t\t\t display all command\n");
    printf("cd <directory> \t\t change server directory\n");
    printf("port <ip:port> \t\t active connection mode\n");
    printf("pasv \t\t\t passive connection mode\n");
    printf("quit \t\t\t disconnect with server\n");
    printf("mode \t\t\t show server mode\n");
    printf("rename \t\t\t rename file or folder\n");
    printf("stat <file> \t\t View file size\n");
    printf("user <username> \t user name\n");
    printf("pass <password> \t user password\n");
}

//?命令
void command_display() {
    printf("get \t\t put \t\t pwd \t\t ls \t\t mkdir \n");
    printf("delete \t\t sys \t\t help \t\t ? \t\t cd \n");
    printf("port \t\t pasv \t\t quit \t\t mode \t\t user\n");
    printf("pass \t\t dir \t\t rename \t stat\n");
}

//获取命令
void get_command(char command[], char msg[], int len) {
    int i;
    for(i = 0; i < len; i++) {
        if(msg[i] == ' ') {
            strncpy(command, msg, i);
            command[i] = '\0';
            break;
        }
    }
    if(i >= len) {
        strcpy(command, msg);
        command[len] = '\0';
    }
}

//获取服务端控制信息并显示
void read_server_msg() {
    n = read(client_sock, buff, sizeof(buff));
    buff[n] = '\0';
    printf("%s",buff);
    memset(buff, 0, sizeof(buff));
}

//发送服务端控制信息
void send_server_msg() {
    if(write(client_sock, sendline, strlen(sendline)) < 0) {
        printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
        exit(0);
    }
}

