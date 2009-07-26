/* KallistiOS ##version##

   dnslookup.c
   Copyright (C)2004 Dan Potter

   Test DNS lookup util.

*/

// Use these for KOS usage.
#include <kos.h>
#include <lwip/lwip.h>

#include <stdio.h>


int dns(const char * name, struct ip_addr * res)
{
    int i;
    int c;
    unsigned char *ip = (unsigned char *)&res->addr;

    i = 0;
    c = 0;
    
    res->addr = 0;
    while(name[c] != 0) {
	if (name[c] != '.') {
	    ip[i] *= 10;
	    ip[i] += name[c] - '0';
	}
	else
	    i++;
	c++;
    }

    //res->addr = ntohl(res->addr);

    return 0;
}

int main(int argc, char **argv) {
	uint8 ip[4];
	struct sockaddr_in dnssrv;

	// KOS code
	net_init();
	lwip_kos_init();

	// Do the query
	dnssrv.sin_family = AF_INET;
	dnssrv.sin_port = htons(53);
	dns("212.198.2.51", &dnssrv.sin_addr.s_addr);
	//dnssrv.sin_addr.s_addr = htonl(dnssrv.sin_addr.s_addr);
	printf("Requesting ip from %x\n", dnssrv.sin_addr.s_addr);
	if (lwip_gethostbyname(&dnssrv, "www.allusion.net", ip) < 0)
		perror("Can't look up name");
	else {
		printf("www.allusion.net is %d.%d.%d.%d\n",
			ip[0], ip[1], ip[2], ip[3]);
	}
	return 0;
}

