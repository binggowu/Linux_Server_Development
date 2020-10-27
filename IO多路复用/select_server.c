#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>

#define PORT 8001

int main()
{
    int lfd, cfd;
    struct sockaddr_in serv_addr, clie_addr;
    socklen_t clie_addr_len;
    char clie_ip[INET_ADDRSTRLEN];
    char buf[4096];
    ssize_t n;
    int exitcode = 0;

    int nready;
    int maxfd;
    fd_set read_fds, tmpfds; // read_fd用于增加和删除, tmpfds用于监控和判断
    int cfds[FD_SETSIZE];
    memset(cfds, -1, FD_SETSIZE);

    if (-1 == (lfd = socket(AF_INET, SOCK_STREAM, 0)))
    {
        perror("socket failed");
        goto lblexit;
    }
    printf("lfd: %d\n", lfd);

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (-1 == bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)))
    {
        perror("bind failed");
        goto lblexit;
    }

    if (-1 == listen(lfd, SOMAXCONN))
    {
        perror("listen failed");
        goto lblexit;
    }

    FD_ZERO(&read_fds);
    FD_SET(lfd, &read_fds);
    maxfd = lfd;
    while (1)
    {
        printf(" ------------------ while(1) ----------------\n");
        tmpfds = read_fds;
        printf("maxfd: %d\n", maxfd);
        nready = select(maxfd + 1, &tmpfds, NULL, NULL, NULL);
        if (-1 == nready)
        {
            perror("select failed");
            goto lblexit;
        }
        printf("nready: %d\n", nready);

        if (FD_ISSET(lfd, &tmpfds))
        {
            printf("有连接到来\n");
            clie_addr_len = sizeof(clie_addr);
            if (-1 == (cfd = accept(lfd, (struct sockaddr *)&clie_addr, &clie_addr_len)))
            {
                perror("accept failed");
                goto lblexit;
            }
            printf("cfd: %d\n", cfd);

            memset(clie_ip, 0, sizeof(clie_ip));
            inet_ntop(AF_INET, &clie_addr.sin_addr, clie_ip, sizeof(clie_ip));
            printf("client: %s - %d\n", clie_ip, ntohs(clie_addr.sin_port));

            int i = 0;
            for (; i < FD_SETSIZE; ++i)
            {
                if (-1 == cfds[i])
                {
                    printf("把cfd %d 插入到第 %d 个位置\n", cfd, i);
                    cfds[i] = cfd;
                    break;
                }
            }
            if (i == FD_SETSIZE)
            {
                printf("到达接受上限, 不能再接受连接了.");
                break;
            }

            FD_SET(cfd, &read_fds);
            if (maxfd < cfd)
            {
                printf("更新maxfd为 %d\n", cfd);
                maxfd = cfd;
            }

            --nready; // 每处理了一个事件, nready就要-1
        }

        while (nready > 0)
        {
            int i = 0;
            for (; i < FD_SETSIZE; ++i)
            {
                if (-1 != cfds[i])
                {
                    cfd = cfds[i];
                    if (FD_ISSET(cfd, &tmpfds))
                    {
                        printf("有数据到来, cfd: %d\n", cfd);
                        break;
                    }
                }
            }
            if (i == FD_SETSIZE)
            {
                printf("全部的cfd都没有数据到来\n");
                continue;
            }

            n = recv(cfd, buf, sizeof(buf), 0);
            printf("recv: %ld\n", n);
            if (-1 == n)
            {
                perror("recv failed");
                goto lblexit;
            }
            else if (0 == n)
            {
                printf("读取完毕\n");
                FD_CLR(cfd, &read_fds);
                cfds[i] = -1;
            }

            for (int j = 0; j < n; ++j)
            {
                buf[j] = toupper(buf[j]);
            }
            printf("发送数据\n");
            send(cfd, buf, n, 0);

            --nready; // 每处理了一个事件, nready就要-1
        }
    }

lblexit:
    close(lfd);
    return exitcode;
}
