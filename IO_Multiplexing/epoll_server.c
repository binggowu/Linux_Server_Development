#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#define PORT 8001
#define OPEN_MAX 1024

int main()
{
    int lfd, cfd;
    int epfd;
    int nready;
    struct sockaddr_in serv_addr, clie_addr;
    socklen_t clie_addr_len;
    char clie_ip[INET_ADDRSTRLEN];
    char buf[4096];
    ssize_t n;
    int exitcode = 0;

    if (-1 == (lfd = socket(AF_INET, SOCK_STREAM, 0)))
    {
        perror("socket failed");
        return 1;
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

    if (-1 == (epfd = epoll_create(OPEN_MAX)))
    {
        perror("epoll_create failed");
        goto lblexit;
    }
    printf("epfd: %d\n", epfd);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;
    if (-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev))
    {
        perror("epoll_ctl failed");
        goto lblexit;
    }

    struct epoll_event evs[OPEN_MAX];

    while (1)
    {
        printf("----------------- while(1) ---------------------\n");
        nready = epoll_wait(epfd, evs, OPEN_MAX, -1);
        if (-1 == nready)
        {
            perror("epoll_wait failed");
            goto lblexit;
        }
        printf("nready: %d\n", nready);

        for (int i = 0; i < nready; ++i)
        {
            if (!(evs[i].events & EPOLLIN)) // 非读事件
            {
                printf("非读事件\n");
                continue;
            }

            // 有连接到来
            printf("有连接到来\n");
            if (evs[i].data.fd == lfd)
            {
                // 获取cfd
                clie_addr_len = sizeof(clie_addr);
                cfd = accept(lfd, (struct sockaddr *)&clie_addr, &clie_addr_len);
                if (-1 == cfd)
                {
                    perror("accept failed");
                    goto lblexit;
                }
                printf("cfd: %d\n", cfd);

                memset(clie_ip, 0, sizeof(clie_ip));
                inet_ntop(AF_INET, &clie_addr.sin_addr, clie_ip, sizeof(clie_ip));
                printf("client: %s - %d\n", clie_ip, ntohs(clie_addr.sin_port));

                // cfd上树
                printf("cfd上树\n");
                memset(&ev, 0, sizeof(ev));
                ev.events = EPOLLIN;
                ev.data.fd = cfd;
                if (-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev))
                {
                    perror("epoll_ctl failed");
                    goto lblexit;
                }
            }
            else
            {
                // 有客户端数据到来
                printf("有客户端数据到来\n");
                cfd = evs[i].data.fd;
                printf("cfd: %d\n", cfd);
                n = recv(cfd, buf, sizeof(buf), 0);
                printf("recv: %ld\n", n);
                if (-1 == n)
                {
                    perror("recv failed");
                    goto lblexit;
                }
                else if (0 == n)
                {
                    // cfd下数
                    printf("cfd下树\n");
                    if (-1 == epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL))
                    {
                        perror("epoll_ctl failed");
                        goto lblexit;
                    }

                    close(cfd);
                }

                for (int j = 0; j < n; ++j)
                {
                    buf[j] = toupper(buf[j]);
                }
                // 发送数据
                printf("发送数据\n");
                send(cfd, buf, n, 0);
            }
        }
    }

lblexit:
    close(lfd);
    close(epfd);

    return exitcode;
}
