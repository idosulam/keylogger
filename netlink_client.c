#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <errno.h>

#define NETLINK_USER 30  // same customized protocol as in my kernel module
#define MAX_PAYLOAD 1024 // maximum payload size
#define buffersize 2000

char buffer[buffersize];
struct sockaddr saddr;
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct nlmsghdr *nlh2 = NULL;
struct msghdr msg, resp; // famous struct msghdr, it includes "struct iovec *   msg_iov;"
struct iovec iov, iov2;
struct ethernet_header
{
    unsigned char h_dest[6];   // mac address where data is sent to
    unsigned char h_source[6]; // mac address where data is sent from
    unsigned short h_type;     // what type of protocol we are using
};
int sock_fd;

static int init_socket();
static int bindsockets();
static void receivepackets();
int main(int args, char *argv[])
{

    sock_fd = init_socket();
    if (bindsockets() != 1)
        exit(1);

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (nlh == NULL)
    {
        exit(1);
    }

    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid(); // self pid
    nlh->nlmsg_flags = 0;

    // nlh2: contains received msg
    nlh2 = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (nlh2 == NULL)
    {
        exit(1);
    }

    memset(nlh2, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh2->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh2->nlmsg_pid = getpid(); // self pid
    nlh2->nlmsg_flags = 0;

    strcpy(NLMSG_DATA(nlh), "Hello this is a msg from userspace"); // put "Hello" msg into nlh

    iov.iov_base = (void *)nlh; // iov -> nlh
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr; // msg_name is Socket name: dest
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov; // msg -> iov
    msg.msg_iovlen = 1;

    iov2.iov_base = (void *)nlh2; // iov -> nlh2
    iov2.iov_len = nlh2->nlmsg_len;
    resp.msg_name = (void *)&dest_addr; // msg_name is Socket name: dest
    resp.msg_namelen = sizeof(dest_addr);
    resp.msg_iov = &iov2; // resp -> iov
    resp.msg_iovlen = 1;

    printf("Sending message to kernel\n");

    int ret = sendmsg(sock_fd, &msg, 0);
    printf("send ret: %d\n", ret);

    printf("Waiting for message from kernel\n");

    /* Read message from kernel */
    recvmsg(sock_fd, &resp, 0); // msg is also receiver for read

    printf("Received message payload: %s\n", (char *)NLMSG_DATA(nlh2));

    char usermsg[MAX_PAYLOAD];
    while (1)
    {
        printf("Input your msg for sending to kernel: ");
        scanf("%s", usermsg);
        if (strncmp(usermsg, "exit", 4) == 0)
        {
            printf("leaving kernel\n");
            break;
        }

        strcpy(NLMSG_DATA(nlh), usermsg); // put "Hello" msg into nlh

        printf("Sending message \" %s \" to kernel\n", usermsg);

        ret = sendmsg(sock_fd, &msg, 0);
        printf("send ret: %d\n", ret);

        printf("Waiting for message from kernel\n");

        recvmsg(sock_fd, &resp, 0);

        printf("Received message payload: %s\n", (char *)NLMSG_DATA(nlh2));
    }
    free(nlh);
    free(nlh2);
    close(sock_fd);

    return 0;
}

static int init_socket()
{
    int s = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (s < 0)
        exit(1);
    return s;
}

static int bindsockets()
{

    memset(&src_addr, 0, sizeof(src_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));

    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid(); /* self pid */
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;    /* For Linux Kernel */
    dest_addr.nl_groups = 0; /* unicast */

    // int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)))
    {
        exit(1);
    }
    return 1;
}

// receives packets from socket
static void receivepackets()
{

    int receive;
    int saddrsize = sizeof(saddr);
    while (1)
    {
        memset(buffer, 0, buffersize);
        receive = recvfrom(sock_fd, buffer, buffersize, 0, &saddr, &saddrsize);
        if (receive > 0)
        {
            struct ethernet_header *eth = (struct ethernet_header *)buffer;

            if (ntohs(eth->h_type) == 2048)
            {
                printf("\n------------------------------------------------------ETHERNET HEADER------------------------------------------------------\n");
                printf("\t|-Source Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n", eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5]); // at least 2 numbers in hex base printing the src mac address
                printf("\t|-Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
                printf("\t|-Protocol : %d\n", eth->h_type);
            }
        }
    }
}