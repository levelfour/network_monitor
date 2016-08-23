#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <errno.h>

#define INTERVAL    1
#define PACKET_MSS  1500
#define PORT_SSH    22
#define NB_NODE     3

const char json_template[] =
  "{"
  "  \"nodes\":["
       "%s"
  "  ],"
  "  \"links\":["
       "%s"
  "  ]"
  "}";

const char json_node_template[] =
  "{"
  "  \"name\": \"%s\","
  "  \"group\": 1"
  "}%c";

const char json_link_template[] =
  "{"
  "  \"source\": %d,"
  "  \"target\": %d,"
  "  \"value\": %d"
  "}%c";

const char cluster_node[NB_NODE][16] = {
  "192.168.1.11",
  "192.168.1.12",
  "192.168.1.13",
};

uint32_t cluster_node_ip[NB_NODE];

char logging_string_buffer[0x1ff];
char logging_node_buffer[0xff];
char logging_link_buffer[0xff];

int logging(const char *fname, int (*M)[NB_NODE]) {
  FILE *fp = fopen(fname, "w");

  if (!fp) {
    return -1;
  }

  memset(logging_string_buffer, 0, 0x1ff);
  memset(logging_node_buffer, 0, 0xff);
  memset(logging_link_buffer, 0, 0xff);

  int i, j;
  char *s = logging_node_buffer;

  for (i = 0; i < NB_NODE; i++) {
    char delimiter = i == NB_NODE - 1 ? ' ' : ',';
    int c = sprintf(s, json_node_template, cluster_node[i], delimiter);
    s += c;
  }
  *s = '\0';

  s = logging_link_buffer;

  for (i = 0; i < NB_NODE; i++) {
    for (j = i+1; j < NB_NODE; j++) {
      int value = M[i][j] + M[j][i];
      char delimiter = (i == NB_NODE - 2 && j == NB_NODE - 1) ? ' ' : ',';
      int c = sprintf(s, json_link_template, i, j, value, delimiter);
      s += c;
      M[i][j] = M[j][i] = 0;
    }
  }

  *s = '\0';

  sprintf(logging_string_buffer, json_template, logging_node_buffer, logging_link_buffer);

  int c = fputs(logging_string_buffer, fp);
  fclose(fp);

  return c;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s [network-interface] [logfile]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char *netif = argv[1];
  char *logfilename = argv[2];

  int sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
  
  if (sock < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, netif, strlen(netif));
  if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
    perror("ioctl");
    close(sock);
    exit(EXIT_FAILURE);
  }
  
  struct packet_mreq mreq;
  memset(&mreq, 0, sizeof(mreq));
  mreq.mr_type = PACKET_MR_PROMISC;
  mreq.mr_ifindex = ifr.ifr_ifindex;
  if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq)) < 0){
    perror("setsockopt");
    close(sock);
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in from;
  socklen_t fromlen;
  struct in_addr saddr, daddr;
  uint16_t sport, dport;
  struct {
    struct iphdr ip;
    struct tcphdr tcp;
    char data[PACKET_MSS];
  } pkt;
  int rc;
  int stat[NB_NODE][NB_NODE];

  time_t last_clk;
  time_t cur_time;

  time(&last_clk);

  int i, j;
  for (i = 0; i < NB_NODE; i++) {
    for (j = 0; j < NB_NODE; j++) {
      stat[i][j] = 0;
    }

    struct in_addr addr;
    inet_aton(cluster_node[i], &addr);
    cluster_node_ip[i] = addr.s_addr;
  }

  while (1) {
    time(&cur_time);
    if (cur_time - last_clk > INTERVAL) {
      logging(logfilename, stat);
      last_clk = cur_time;
    }

    fromlen = sizeof(struct sockaddr_in);
    rc = recvfrom(sock, &pkt, PACKET_MSS, 0, (struct sockaddr *)&from, &fromlen);

    if (rc < 0) {
      perror("recvfrom");
      break;
    } else if (fromlen == 0) {
      continue;
    } else if (pkt.ip.protocol != IPPROTO_TCP) {
      continue;
    }

    saddr.s_addr = pkt.ip.saddr;
    daddr.s_addr = pkt.ip.daddr;
    sport = ntohs(pkt.tcp.source);
    dport = ntohs(pkt.tcp.dest);

    if (sport == PORT_SSH || dport == PORT_SSH) {
      continue;
    }

    for (i = 0; i < NB_NODE; i++) {
      for (j = 0; j < NB_NODE; j++) {
        if (saddr.s_addr == cluster_node_ip[i] && daddr.s_addr == cluster_node_ip[j]) {
          stat[i][j]++;
        }
      }
    }

    memset(&pkt, 0, rc);

    usleep(1000);
  }

  close(sock);

  return 0;
}
