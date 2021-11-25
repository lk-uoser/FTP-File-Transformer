/* File Name: 
**** server.c ****
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/utsname.h>

#define PORT 8000
#define MAXLINE 4096
#define MAXSIZE 1024

int server_soct, client_sock, data_sock, server_data_sock;
struct sockaddr_in server_addr, data_addr;
char client_msg[4096], reply[100], username[20];
int n, sar, flag = 1;
unsigned long p = PORT;
char MODE = 'A';

void command_port(char *params, char *reply);
void command_get(char *filename, char *reply);
void command_put(char *filename, char *reply);
void command_pwd(char *reply);
void command_stat(char *filename, char *reply);
void command_pasv();
void command_sys();
void command_mode();
int command_list(char *reply);
int command_cwd(char *dir);
int command_mkdir(char *filename, char *reply);
int command_re(char *filename, char *filename1, char *reply);
int command_delet(char *filename, char *reply);
void get_command(char command[], char msg[], int len);
void send_client_msg();
void read_client_msg();
void client_start(int client_sock);


int main(int argc, char** argv) {
    //创建server socket
    server_soct = socket(AF_INET, SOCK_STREAM, 0);
    if(server_soct == -1) {
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    //定义进程标识变量
    pid_t pid;

    //初始化server IP & PORT
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);//设置的端口为PORT = 8000

     // 设置套接字选项避免地址使用错误  
    int on=1;  
    if((setsockopt(server_soct,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0)  
    {  
        perror("setsockopt failed");  
        exit(EXIT_FAILURE);  
    }  

    //将本地地址绑定到所创建的套接字上
    sar = bind(server_soct, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(sar == -1) {
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    
    //开始监听是否有客户端连接
    if(listen(server_soct, 5) == -1)    {
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);

    printf("======waiting for client's request======\n");
    while(flag) {
        //阻塞直到有客户端连接
        client_sock = accept(server_soct, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(client_sock == -1) {
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            continue;
        }

        //父进程继续监听
        if((pid = fork()) > 0) {
            close(client_sock);	//在父进程中关闭客户端描述符
			continue;	//父进程返回while循环
        }
        
        //子进程处理事务
        else if(pid == 0) {
			close(server_soct);	//在子进程中关闭主套接字描述符
			client_start(client_sock);	//在子进程中调用client_start函数基于新建的从套接字处理与客户之间的通信
			exit(0);	//处理完毕与客户的通信之后退出子进程
		}
        //调用fork()创建从进程出错退出系统
		else {
			printf("fork error\n");
			exit(0);
		}
    }
}
//客户端已建立连接，开始处理命令
void client_start(int client_sock) {
        //FTP服务准备就绪,reply为信息回送
        stpcpy(reply, "220------------ Welcome to FTP ------------\r\n"); 
        printf("%s", reply);
	send_client_msg(); //通知客户端FTP服务开始

        //循环处理命令
        while(1) {
            //获取命令
            read_client_msg();
            char command[5];
            get_command(command, client_msg, n);

            //执行pwd命令
            if(strcmp(command, "pwd") == 0) {
		command_pwd(reply);
            }

            //登陆验证
            else if(strcmp(command, "USER") == 0) {
                //用户名匹配
                if(strcmp(&client_msg[5], "lk") == 0 || strcmp(&client_msg[5], "lk2") == 0) {
                    stpcpy(reply, "331 User ");
                    strcat(reply, &client_msg[5]);
                    stpcpy(username, &client_msg[5]);
                    strcat(reply, " OK. Password required\r\n");
                    printf("User: %s, send_msg is %s", username, reply);
                    write(client_sock, reply, strlen(reply)); //返回信息
                }
                //未找到用户
                else {
                    stpcpy(reply, "332 Username not find\r\n");
                    printf("User: none, send_msg is %s", reply);
                    write(client_sock, reply, strlen(reply));
                }
            }
            //验证密码
            else if(strcmp(command, "PASS") == 0) { 
                if(strncmp(&client_msg[5], "123", 3) == 0) {
                    stpcpy(reply, "220 ");
                    strcat(reply, username);
                    strcat(reply, " Login successfully\r\n");
                    printf("User: %s, send_msg is %s", username, reply);
                    write(client_sock, reply, strlen(reply));
                }
                //密码错误
                else {
                    stpcpy(reply, "Password error\r\n");
                    printf("User: %s, send_msg is %s", username, reply);
                    write(client_sock, reply, strlen(reply));
                    
                }
            }

            //执行cd命令
            else if(strcmp(command, "cd") == 0) {
                command_cwd(&client_msg[3]) == 0;   
            }

            //执行被动数据连接
            else if(strcmp(command, "_pasv") == 0) {
                command_pasv();
            }

            //切换成被动模式
            else if(strcmp(command, "pasv") == 0) {
                stpcpy(reply, "227 Entering Passive Mode\r\n");
                printf("User: %s, send_msg is %s", username, reply);
                MODE = 'P';
                send_client_msg();
            }

            //执行port命令，即主动模式
            else if(strcmp(command, "port") == 0) {
                command_port(&client_msg[5], reply);
            }

            //执行stat命令
            else if(strcmp(command, "stat") == 0) {
                char f[4086]; char l[4086];
                sscanf(client_msg,"%s %s",f,l);
                command_stat(l, reply);
            }

            //执行ls命令
            else if(strcmp(command, "ls") == 0) {
                if(command_list(reply) ==0) {
                    stpcpy(reply, "226 Transfer complete\r\n");
                    printf("User: %s, send_msg is %s\n", username, reply);
                    send_client_msg();
                }
                else {
                    stpcpy(reply, "ls faile\r\n");
                    printf("User: %s, send_msg is %s\n", username, reply);
                    send_client_msg();
                }
            }

            //执行dir命令
            else if(strcmp(command, "dir") == 0) {
                if(command_dir(reply) ==0) {
                    stpcpy(reply, "226 Transfer complete\r\n");
                    printf("User: %s, send_msg is %s\n", username, reply);
                    send_client_msg();
                }
                else {
                    stpcpy(reply, "ls faile\r\n");
                    printf("User: %s, send_msg is %s\n", username, reply);
                    send_client_msg();
                }
            }

            //创建文件或目录
            else if(strcmp(command, "mkdir") == 0) {
                if(command_mkdir(&client_msg[6], reply) == 0) {
                    stpcpy(reply, "250 Directory ");
                    strcat(reply, &client_msg[6]);
                    strcat(reply, " create successful\r\n");
                    printf("User: %s, send_msg is %s", username, reply);
                    send_client_msg();
                }
                //创建失败
                else {
                    stpcpy(reply, "450 Directory creat failure\r\n");
                    printf("User: %s, send_msg is %s", username, reply);
                    send_client_msg();
                }
            }

            //change目录或文件
            else if(strcmp(command, "rename") == 0) {
                char f[4086]; char l[4086]; char a[4086];
                sscanf(client_msg,"%s %s %s",a,f,l);
                if(command_re(f, l, reply) == 0) {
                    stpcpy(reply, "250 Directory ");
                    strcat(reply, &client_msg[7]);
                    strcat(reply, " rename successful\r\n");
                    printf("User: %s, send_msg is %s", username, reply);
                    send_client_msg();
                }
                //change失败
                else {
                    stpcpy(reply, "450 Directory rename failure\r\n");
                    printf("User: %s, send_msg is %s", username, reply);
                    send_client_msg();
                }
            }

            //删除空目录或文件
            else if(strcmp(command, "delete") == 0) {
                if(command_delet(&client_msg[7], reply) == 0) {
                    stpcpy(reply, "250 Directory ");
                    strcat(reply, &client_msg[7]);
                    strcat(reply, " delete successful\r\n");
                    printf("User: %s, send_msg is %s", username, reply);
                    send_client_msg();
                }
                //删除失败
                else {
                    stpcpy(reply, "450 Directory delete failure\r\n");
                    printf("User: %s, send_msg is %s", username, reply);
                    send_client_msg();
                }
            }

            //显示操作系统名
            else if(strcmp(command, "sys") == 0) {
                command_sys();

            //显示服务端模式
            }else if(strcmp(command, "mode") == 0) {
                command_mode();
            }

            //执行get命令
            else if(strcmp(command, "get") == 0) {
                
                command_get(&client_msg[4], reply);
            }

            //执行put命令
            else if(strcmp(command, "put") == 0) {
                command_put(&client_msg[4], reply);
            }

            //客户端退出登录
            else if(strcmp(command, "quit") == 0) {
                stpcpy(reply, "221 Goodbye\r\n");
                printf("User: %s, send_msg is %s\n", username, reply);
		        send_client_msg();
		        break;
            }
        }
        close(client_sock);
}

//主动创建数据连接
void command_port(char *params, char *reply) {
    MODE = 'A';
    unsigned long a0, a1, a2, a3, p0, p1, addr;
    //获取IP & PORT
    sscanf(params, "%lu.%lu.%lu.%lu:%lu %lu", &a0, &a1, &a2, &a3, &p0, &p1);
        if(data_sock > 0) {
        close(data_sock);
    }
    data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(data_sock < 0) {
        printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
        exit(0);
    }

    //保证主动连接和被动连接端口不冲突
    if(p == p0 * 256 + p1) {
        p1 = p1 + 10;
    }

    //初始化
    memset(&data_addr, 0, sizeof(server_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(p0 * 256 + p1);
    data_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //连接客户端
    if(connect(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    stpcpy(reply, "227 Entering Active Mode 127.0.0.1:");
    char s[20];
    sprintf(s, "%lu", p0 * 256 + p1);
    strcat(reply, s);
    strcat(reply, "\r\n");
    printf("User: %s, send_msg is %s", username, reply);
    send_client_msg();
}

//创建被动数据连接
void command_pasv() {
    MODE = 'P';
    p = p + 10;
    if(data_sock > 0) {
        close(data_sock);
    }
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
    //将本地地址绑定到所创建的套接字上
    int dar = bind(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr));

    //绑定失败说明其他客户端占用了端口，应该更换
    if(dar == -1) {
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        p = p + 5;
        memset(&data_addr, 0, sizeof(server_addr));
        data_addr.sin_family = AF_INET;
        data_addr.sin_port = htons(p);
        data_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        int dar1 = bind(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr));
        if(dar1 == -1) {
            printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
            exit(0);
        }
    }
    
    //开始监听是否有客户端连接
    if(listen(data_sock, 5) == -1)    {
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
     
    char po[20];
    sprintf(po, "%lu" , p);//转字符串
    write(client_sock, po, sizeof(po));
    //接受连接
    if((server_data_sock = accept(data_sock, (struct sockaddr*)NULL, NULL)) == -1) {
        printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
    }
    stpcpy(reply, "200 Port command successful");
    strcat(reply, "\r\n");
    printf("User: %s, send_msg is %s", username, reply);
	send_client_msg();
}

//创建文件或目录
int command_mkdir(char *filename, char *reply) {
    unsigned char databuf[MAXSIZE] = "";
    int bytes;
    char buf[255];
    //获取当前所处目录
    getcwd(buf,sizeof(buf)-1);
    strcat(buf, filename);
    //创建成功
    if(mkdir(buf,S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH)<0) {
       return -1;
    }
    else {
       return 0;
    }
}

//change文件或目录
int command_re(char *filename, char *filename1, char *reply) {
    unsigned char databuf[MAXSIZE] = "";
    int bytes;
    char buf[255], buf1[255];
    //获取当前所处目录
    getcwd(buf,sizeof(buf)-1);
    strcat(buf, filename);
    getcwd(buf1,sizeof(buf)-1);
    strcat(buf1, filename1);
    //rename成功
    if(rename(buf, buf1) < 0) {
       return -1;
    }
    else {
       return 0;
    }
}

//删除文件，及空文件夹
int command_delet(char *filename, char *reply) {
    unsigned char databuf[MAXSIZE] = "";
    int bytes;
    char buf[255];
    //获取当前所处目录
    getcwd(buf,sizeof(buf)-1);
    strcat(buf, filename);
    //删除成功
    if(remove(buf) == -1) {
        return -1;
    }
    else {
        return 0;
    }
}

//获取操作系统名
void command_sys() {
    struct utsname buf;
    if(uname(&buf)) {
        perror("uname");
        exit(1);
    }
    stpcpy(reply, "System Type is ");
    //获得操作系统名
    strcat(reply, buf.sysname);
    strcat(reply, "\r\n");
    printf("User: %s, send_msg is %s", username, reply);
    write(client_sock, reply, strlen(reply));
}

//服务器模式
void command_mode() {
    if(MODE == 'A') {
        stpcpy(reply, "Server Mode is Active\r\n");
        printf("User: %s, send_msg is %s", username, reply);
        write(client_sock, reply, strlen(reply));
    }
    if(MODE == 'P') {
        stpcpy(reply, "Server Mode is Passive\r\n");
        printf("User: %s, send_msg is %s", username, reply);
        write(client_sock, reply, strlen(reply));
    }
}

//下载文件
void command_get(char *filename, char *reply) {
    //已经处于主动模式，将数据连接切换到主动模式的数据连接
    if(MODE == 'A') {
        server_data_sock = data_sock;
    }
    struct timeval t1, t2;
    FILE *getfile;
    unsigned char databuf[MAXSIZE] = "";
    int bytes;
    char buf[255];
    getcwd(buf,sizeof(buf)-1);
    strcat(buf, filename);
    getfile = fopen(buf, "rb");

    struct stat buf1;   //注意结构体，获取磁盘文件的信息
    int result;
    result = stat(buf, &buf1);

    if(getfile == 0) {
	memset(reply, 0, sizeof(reply));
        stpcpy(reply, "450 file open filed\r\n");
        printf("User: %s, send_msg is %s", username, reply);
        send_client_msg();
	    return;
    }
    gettimeofday(&t1, NULL);
    flock(fileno(getfile), LOCK_EX);
    //打开文件成功，开始将数据写入到数据连接
    while((bytes = read(fileno(getfile), databuf, MAXSIZE)) > 0)
    {
	write(server_data_sock, (const char *)databuf, bytes);
        memset(&databuf, 0, MAXSIZE);
    }
    flock(fileno(getfile), LOCK_UN);
    //获取传输时间
    gettimeofday(&t2, NULL);
    double time = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
    memset(reply, 0, sizeof(reply));
    stpcpy(reply, "226 Successfully transferred ");
    strcat(reply, filename);
    strcat(reply, "\r\n");
    printf("User: %s, send_msg is %s", username, reply);
    send_client_msg();
    memset(reply, 0, sizeof(reply));
    //发送传输信息
    char time_s[200] = "ftp: 发送 ";
    char zj[20];
    char ti[20];
    char su[20];
    sprintf(zj, "%ld" ,buf1.st_size);
    strcat(time_s, zj);
    strcat(time_s, " 字节，用时 ");
    sprintf(ti, "%.4lf" ,time);
    strcat(time_s, ti);
    strcat(time_s, "ms ");
    sprintf(su, "%.4lf" ,buf1.st_size/time);
    strcat(time_s, su);
    strcat(time_s, "bytes/ms\r\n");
    stpcpy(reply, time_s);
    printf("User: %s, send_msg is %s", username, reply);
    send_client_msg();
    memset(&databuf, 0, MAXSIZE);   
    fclose(getfile);
    //关闭数据连接
    close(server_data_sock);
    return;
}

//上传文件
void command_put(char *filename, char *reply) {
    //已经处于主动模式，将数据连接切换到主动模式的数据连接
    if(MODE == 'A') {
        server_data_sock = data_sock;
    }
    struct timeval t1, t2;
    FILE *putfile;
    unsigned char databuf[MAXSIZE] = "";
    int bytes;
    char buf[255];
    getcwd(buf,sizeof(buf)-1);
    char *file = (char *) malloc(sizeof(buf));
    strcpy(file, buf);
    char *f_file = (char *) malloc(strlen(file) + strlen(filename));
    strcpy(f_file, file);
    strcat(f_file, filename);
    putfile = fopen(f_file, "w");

    memset(reply, 0, sizeof(reply));
    stpcpy(reply, "226 Successfully transferred ");
    strcat(reply, filename);
    strcat(reply, "\r\n");
    printf("User: %s, send_msg is %s", username, reply);
    send_client_msg();
    memset(reply, 0, sizeof(reply));

    if(putfile == 0) {
        perror("fopen() failed");
        close(server_data_sock);
        return;
    }
    gettimeofday(&t1, NULL);
    
    flock(fileno(putfile), LOCK_EX);//filelock
    while((bytes = read(server_data_sock, databuf, MAXSIZE)) > 0) {
        write(fileno(putfile), databuf, bytes);
        memset(&databuf, 0, MAXSIZE);
    }
    flock(fileno(putfile), LOCK_UN);//unfilelock
    gettimeofday(&t2, NULL);
    double time = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;

    memset(&databuf, 0, MAXSIZE);
    fclose(putfile);
    struct stat buf1;   //创建一个结构体获取磁盘文件信息
    int result;
    result = stat(f_file, &buf1);
    //返回传输信息
    char time_s[200] = "ftp: 收到 ";
    char zj[20];
    char ti[20];
    char su[20];
    sprintf(zj, "%ld" ,buf1.st_size);
    strcat(time_s, zj);
    strcat(time_s, " 字节，用时 ");
    sprintf(ti, "%.4lf" ,time);
    strcat(time_s, ti);
    strcat(time_s, "ms ");
    sprintf(su, "%.4lf" ,buf1.st_size/time);
    strcat(time_s, su);
    strcat(time_s, "bytes/ms\r\n");
    stpcpy(reply, time_s);
    printf("User: %s, send_msg is %s", username, reply);
    send_client_msg();
    close(server_data_sock);
    return;
}

//获取当前目录
void command_pwd(char *reply)
{
    char buf[255];
    memset(reply, 0, sizeof(reply));
    getcwd(buf,sizeof(buf)-1);
    strcat(reply,buf);
    strcat(reply,"\r\n");
    printf("User: %s, send_msg is %s", username, reply);
    send_client_msg();
}

//file state
void command_stat(char *filename, char *reply)
{
    struct stat buf1;
    char buf[255];
    memset(reply, 0, sizeof(reply));
    getcwd(buf,sizeof(buf)-1);
    strcat(buf, filename);
    stat(buf, &buf1);
    sprintf(reply,"file size is:%d",buf1.st_size);
    strcat(reply,"\r\n");
    printf("User: %s, send_msg is %s", username, reply);
    send_client_msg();
}

//列出当前目录所有文件
int command_list(char *reply)
{
    FILE *f;
    char databuf[MAXSIZE];
    int n;
    if(MODE == 'A') {
        server_data_sock = data_sock;
    }
    if(server_data_sock <= 0) {
        return -1;
    }
    //调用系统执行ls命令
    f = popen("ls -l","r");
    if(f == 0) {
        stpcpy(reply, "500 Transfer error\r\n");
        printf("User: %s, send_msg is %s", username, reply);
        close(server_data_sock);
        return -1;
    }
    //数据连接传输
    memset(databuf, 0, MAXSIZE - 1);
    while((n = read(fileno(f), databuf, MAXSIZE)) > 0) {
        write(server_data_sock, databuf, sizeof(databuf));
        memset(databuf, 0, MAXSIZE - 1);
    }
    memset(databuf, 0, MAXSIZE - 1);
    close(server_data_sock);
    return 0;
}

//列出当前目录\文件
int command_dir(char *reply)
{
    FILE *f;
    char databuf[MAXSIZE];
    int n;
    if(MODE == 'A') {
        server_data_sock = data_sock;
    }
    if(server_data_sock <= 0) {
        return -1;
    }
    //调用系统执行ls命令
    f = popen("dir","r");
    if(f == 0) {
        stpcpy(reply, "500 Transfer error\r\n");
        printf("User: %s, send_msg is %s", username, reply);
        close(server_data_sock);
        return -1;
    }
    //数据连接传输
    memset(databuf, 0, MAXSIZE - 1);
    while((n = read(fileno(f), databuf, MAXSIZE)) > 0) {
        write(server_data_sock, databuf, sizeof(databuf));
        memset(databuf, 0, MAXSIZE - 1);
    }
    memset(databuf, 0, MAXSIZE - 1);
    close(server_data_sock);
    return 0;
}

//改变当前目录
int command_cwd(char *add) {
    //调用系统chdir
    if(chdir(add) != 0) {
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        stpcpy(reply, "450 No such file or directory\r\n");
        printf("User: %s, send_msg is %s", username, reply);
        send_client_msg();
        return -1;
    }
    else {
        stpcpy(reply, "250 Directory changed to /");
        strcat(reply, &client_msg[3]);
        strcat(reply, "\r\n");
        printf("User: %s, send_msg is %s", username, reply);
        send_client_msg();
        return 0;
    }
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

//读客户端控制信息
void read_client_msg() {
    memset(client_msg, 0, sizeof(client_msg));
    n = read(client_sock, client_msg, sizeof(client_msg));
    client_msg[n] = '\0';
    if(strlen(username) <= 0) {
        printf("User: none, get_msg is %s\n", client_msg);
    }
    else {
        printf("User: %s, get_msg is %s\n", username, client_msg);
    }
}

//发送客户端响应代码
void send_client_msg() {
    if(write(client_sock, reply, strlen(reply)) < 0) {
        printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
        exit(0);
    }
    memset(reply, 0, sizeof(reply));
}
