#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifndef __NetBSD__
#include <linux/if.h>
#include <linux/if_tun.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>

#include <sys/select.h>

#include <mcrypt.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>

#include "sha3.h"
#include "keyqueue.h"
#include <curl/curl.h>
#include "post_curl.h"

Queue *q1;
Queue *q2;
KEY *curKey1;
KEY *curKey2;

union sockaddr_4or6 {
    struct sockaddr_in a4;
    struct sockaddr_in6 a6;
    struct sockaddr a;
};

bool getKeyBySha(uint8_t* sha);

void getLastKey() {
    char* nextkey = curl_get_key("last", true);
    uint8_t buff[32];
    if (nextkey != NULL) {
        int i;
        for (i = 0; i < 32; i++) {
            sscanf(nextkey + i * 2, "%02X", &(buff[i]));
        }
        for (i = 0; i < 32; i++) {
            printf("%02X", buff[i]);
        }
        printf("\n");
        printf("NEWKEYOK\n");
        KEY *k1 = ConstructKey(buff);
        KEY *k2 = CopyKey(k1);
        Enqueue(q1, k1);
        Enqueue(q2, k2);
    }
}

bool getKeyBySha(uint8_t* sha) {
    char qbuffer[67];
    qbuffer[0] = 'k';
    qbuffer[1] = 'e';
    qbuffer[2] = 'y';
    int i;
    for (i = 0; i < 32; i++) {
        sprintf(qbuffer + 3 + i * 2, "%02X", sha[i]);
    }
    char* nextkey = curl_get_key(qbuffer, true);
    uint8_t buff[32];
    if (nextkey != NULL) {
        int i;
        for (i = 0; i < 32; i++) {
            sscanf(nextkey + i * 2, "%02X", &(buff[i]));
        }
        for (i = 0; i < 32; i++) {
            printf("%02X", buff[i]);
        }
        printf("\n");
        printf("NEWKEYBYSHAOK\n");
        KEY *k1 = ConstructKey(buff);
        KEY *k2 = CopyKey(k1);
        Enqueue(q1, k1);
        Enqueue(q2, k2);
        return true;
    } else {
        return false;
    }
}
int debug = 0;

void do_debug(char *msg, ...) {
    va_list argp;
    if (debug) {
        va_start(argp, msg);
        vfprintf(stderr, msg, argp);
        va_end(argp);
    }
}
char *progname;

void usage(void) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "%s -i <ifacename> [-s|-c <serverIP>] [-p <port>] [-k <port>] [-u|-a] [-d]\n", progname);
    fprintf(stderr, "%s ./ctapudp -s 0.0.0.0 -p 3443 -q 127.0.0.1 -r 55554 -i udp0 -e 256 -a -d\n", progname);
    fprintf(stderr, "%s ./ctapudp -c 0.0.0.0 -p 0 -q 192.168.1.199 -r 55554 -t 192.168.1.199 -k 3443 -i udp0 -e 256 -a -d\n", progname);


    fprintf(stderr, "%s -h\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "-i <ifacename>: Name of interface to use (mandatory)\n");
    fprintf(stderr, "-s <serverIP>|-c <serverIP>: address for udp socket bind(mandatory)\n");
    fprintf(stderr, "-p <port>: port to listen on\n");
    fprintf(stderr, "-k <ip>: ip to connect\n");
    fprintf(stderr, "-k <port>: port to connect\n");
    fprintf(stderr, "-a: use TAP or TUN (without parameter)\n");
    fprintf(stderr, "-d: outputs debug information while running\n");
    fprintf(stderr, "-e <keysize>: use aes-cbc with key size\n");
    fprintf(stderr, "-q <ip>: ip to connect for keys\n");
    fprintf(stderr, "-r <port>: port to connect for keys\n");
    fprintf(stderr, "-h: prints this help text\n");
    exit(1);
}

int main(int argc, char **argv) {
    progname = argv[0];
    unsigned char iv[] = {0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba, 0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6, 0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d, 0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d};
    unsigned char iv_cur[32];
    memcpy(iv_cur, iv, AES_BLOCK_SIZE);
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    char key_ip[30] = "127.0.0.1";
    unsigned int port_key = 55554;


    int dev, cnt, sock, slen;
    unsigned char buf[2000];
    union sockaddr_4or6 addr, from;
#ifndef __NetBSD__
    struct ifreq ifr;
#endif

    //printf("IN: %s\n", text);
    AES_KEY enc_key, dec_key;

    MCRYPT td;
    int i;
    char *key;
    char *block_buffer;
    int blocksize = 0;
    int aes = 0;
    int keysize = 32; /* 256 bits == 32 bytes */
    char enc_state[1024];
    int enc_state_size;
    char* tun_device = "/dev/net/tun";
    char* dev_name = "tun%d";
    int tuntap_flag = IFF_TAP;

    if (getenv("TUN_DEVICE")) {
        tun_device = getenv("TUN_DEVICE");
    }
    if (getenv("DEV_NAME")) {
        dev_name = getenv("DEV_NAME");
    }

    if (getenv("MCRYPT_KEYFILE")) {
        if (getenv("MCRYPT_KEYSIZE")) {
            keysize = atoi(getenv("MCRYPT_KEYSIZE")) / 8;
        }
        key = calloc(1, keysize);
        FILE* keyf = fopen(getenv("MCRYPT_KEYFILE"), "r");
        if (!keyf) {
            perror("fopen keyfile");
            return 10;
        }
        memset(key, 0, keysize);
        fread(key, 1, keysize, keyf);
        fclose(keyf);

        char* algo = "twofish";
        char* mode = "cbc";

        if (getenv("MCRYPT_ALGO")) {
            algo = getenv("MCRYPT_ALGO");
        }
        if (getenv("MCRYPT_MODE")) {
            mode = getenv("MCRYPT_MODE");
        }


        td = mcrypt_module_open(algo, NULL, mode, NULL);
        if (td == MCRYPT_FAILED) {
            fprintf(stderr, "mcrypt_module_open failed algo=%s mode=%s keysize=%d\n", algo, mode, keysize);
            return 11;
        }
        blocksize = mcrypt_enc_get_block_size(td);
        //block_buffer = malloc(blocksize);

        mcrypt_generic_init(td, key, keysize, NULL);

        enc_state_size = sizeof enc_state;
        mcrypt_enc_get_state(td, enc_state, &enc_state_size);
    } else if (getenv("AES_KEYSIZE")) {
        aes = atoi(getenv("AES_KEYSIZE"));
        if (getenv("KEYWORKER_IP")) {
            memcpy(key_ip, getenv("KEYWORKER_IP"), 30);
        }
        if (getenv("KEYWORKER_PORT")) {
            port_key = atoi(getenv("KEYWORKER_PORT"));
        }
    }
    tuntap_flag = IFF_TUN;
    int option;
    int ip_family = AF_INET;
    slen = sizeof (struct sockaddr_in);

    int autoaddress = 1;
    char* laddr = NULL;
    char* lport = NULL;
    char* rhost = NULL;
    char* rport = NULL;
    while ((option = getopt(argc, argv, "i:q:r:k:s:c:p:a:h:d:t:v:e:")) > 0) {
        switch (option) {
            case 'd':
                debug = 1;
                break;
            case 'h':
                usage();
                break;
            case 'i':
                dev_name=optarg;
                break;
            case 's':
                autoaddress = 1;
                laddr=optarg;
                break;
            case 'c':
                autoaddress = 0;
                laddr=optarg;
                break;
            case 'q':
                strncpy(key_ip, optarg, 15);
                break;
            case 'r':
                port_key = atoi(optarg);
                break;
            case 'p':
                lport=optarg;
                break;
            case 't':
                rhost=optarg;
                break;
            case 'k':
                rport=optarg;
                break;
            case 'a':
                tuntap_flag = IFF_TAP;
                break;
            case 'e':
                aes = atoi(optarg);
                break;
            case 'v':
                ip_family = AF_INET6;
                slen = sizeof (struct sockaddr_in6);
                break;
            default:
                do_debug("Unknown option %c\n", option);
                usage();
        }
    }
    do_debug("loaded\n");
    /*
    if (argc < 3) {
        fprintf(stderr,
                "Usage: udptap_tunnel [-6] <localip> <localport> [<remotehost> <remoteport>]\n"
                "    Environment variables:\n"
                "    TUN_DEVICE  /dev/net/tun\n"
                "    DEV_NAME    name of the device, default tun%%d\n"
                "    IFF_TUN     if set, uses point-to-point instead ot TAP.\n"
                "    \n"
                "    MCRYPT_KEYFILE  -- turn on encryption, read key from this file\n"
                "    MCRYPT_KEYSIZE  -- key size in bits, default 256\n"
                "    MCRYPT_ALGO     -- algorithm, default is twofish. aes256 is called rijndael-256\n"
                "    MCRYPT_MODE     -- mode, default is CBC\n"
                "    IPV6_V6ONLY     -- bind socket only to IPv6\n"
                );
        exit(1);
    }

    if (!strcmp(argv[1], "-6")) {
        ++argv;
        ip_family = AF_INET6;
        slen = sizeof (struct sockaddr_in6);
    } else {
        ip_family = AF_INET;
        slen = sizeof (struct sockaddr_in);
    }
    int autoaddress = 1;
    char* laddr = argv[1];
    char* lport = argv[2];
    char* rhost = NULL;
    char* rport = NULL;
    if (argc == 5) {
        autoaddress = 0;
        rhost = argv[3];
        rport = argv[4];
    }
     */
    if ((dev = open(tun_device, O_RDWR)) < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", tun_device, strerror(errno));
        exit(2);
    }

#ifndef __NetBSD__
    memset(&ifr, 0, sizeof (ifr));
    ifr.ifr_flags = tuntap_flag | IFF_NO_PI;
    strncpy(ifr.ifr_name, dev_name, IFNAMSIZ);
    if (ioctl(dev, TUNSETIFF, (void*) &ifr) < 0) {
        perror("ioctl(TUNSETIFF) failed");
        exit(3);
    }
#endif

    memset(&ifr, 0, sizeof (ifr));
    strncpy(ifr.ifr_name, dev_name, IFNAMSIZ);
    int socktun = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(socktun, SIOCSIFFLAGS, (void *) &ifr) < 0) {
        perror("ioctl(FLAGS)");
        exit(3);
    }

    if ((sock = socket(ip_family, SOCK_DGRAM, 0)) == -1) {
        perror("socket() failed");
        exit(4);
    }

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_family = ip_family;

    if (getaddrinfo(laddr, lport, &hints, &result)) {
        perror("getaddrinfo for local address");
        exit(5);
    }
    if (!result) {
        fprintf(stderr, "getaddrinfo for remote returned no addresses\n");
        exit(6);
    }
    if (result->ai_next) {
        fprintf(stderr, "getaddrinfo for local returned multiple addresses\n");
    }
    memcpy(&addr.a, result->ai_addr, result->ai_addrlen);

#ifdef IPV6_V6ONLY
    if (ip_family == AF_INET6) {
        int s = 0;
        if (getenv("IPV6_V6ONLY")) s = atoi(getenv("IPV6_V6ONLY"));
        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &s, sizeof (int));
    }
#endif //IPV6_V6ONLY


    if (bind(sock, (struct sockaddr *) &addr.a, slen)) {
        fprintf(stderr, "bind() to port %d failed: %s\n", lport, strerror(errno));
        exit(5);
    }

    memset(&addr.a, 0, result->ai_addrlen);
    freeaddrinfo(result);

    if (!autoaddress) {
        if (getaddrinfo(rhost, rport, &hints, &result)) {
            perror("getaddrinfo for remote address");
            exit(5);
        }
        if (result->ai_next) {
            fprintf(stderr, "getaddrinfo for remote returned multiple addresses\n");
        }
        if (!result) {
            fprintf(stderr, "getaddrinfo for remote returned no addresses\n");
            exit(6);
        }
        memcpy(&addr.a, result->ai_addr, result->ai_addrlen);
        freeaddrinfo(result);
    }

    if (aes) {
        q1 = ConstructQueue(10);
        q2 = ConstructQueue(10);
        char buffer_addr[30];
        int len = sprintf(buffer_addr, "http://%s:%d", key_ip, port_key);
        curl_init_addr(buffer_addr, len);
        getLastKey();
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);
    fcntl(dev, F_SETFL, O_NONBLOCK);
    int maxfd = (sock > dev) ? sock : dev;
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        FD_SET(dev, &rfds);
        int ret = select(maxfd + 1, &rfds, NULL, NULL, NULL);

        if (ret < 0) continue;

        if (FD_ISSET(dev, &rfds)) {
            if (blocksize) {
                cnt = read(dev, (void*) &(buf), 1518);
                cnt = ((cnt - 1) / blocksize + 1) * blocksize; // pad to block size
                mcrypt_generic(td, buf, cnt);
                mcrypt_enc_set_state(td, enc_state, enc_state_size);
            } else
                if (aes) {
                cnt = read(dev, (void*) &(*(buf + 32)), 1518);
                if (curKey1 == NULL) {
                    if (isEmpty(q1)) {
                        getLastKey();
                        if (isEmpty(q1)) {
                            do_debug("TAP2NET: No keys\n");
                            continue;
                        } else {
                            curKey1 = Dequeue(q1);
                            do_debug("TAP2NET: New key 1\n");
                        }
                    } else {
                        curKey1 = Dequeue(q1);
                        PrintKey(curKey1);
                        do_debug("TAP2NET: New key 2\n");
                    }
                } else if (curKey1->usage > 10) {
                    do_debug("TAP2NET: %d usages of key\n", curKey1->usage);
                    if (!isEmpty(q1)) {
                        curKey1 = Dequeue(q1);
                    } else {
                        getLastKey();
                        if (isEmpty(q1)) {
                            do_debug("TAP2NET: No keys, last usage\n");
                        } else {
                            curKey1 = Dequeue(q1);
                            do_debug("TAP2NET: New key 3\n");
                        }
                    }
                }
                curKey1->usage++;
                memcpy(buf, curKey1->sha, 32);
                memcpy(iv_cur, iv, AES_BLOCK_SIZE);
                int diff = cnt % 128;
                if (diff != 0) {
                    cnt = cnt + (128 - diff);
                } else {
                    cnt = cnt;
                }
                AES_set_encrypt_key(curKey1->key, aes, &enc_key);
                AES_cbc_encrypt((buf + 32), (buf + 32), cnt, &enc_key, iv_cur, AES_ENCRYPT);
                cnt += 32;
            }
            sendto(sock, &buf, cnt, 0, &addr.a, slen);
        }

        if (FD_ISSET(sock, &rfds)) {
            cnt = recvfrom(sock, &buf, 2000, 0, &from.a, &slen);

            if (curKey2 == NULL) {
                if (isEmpty(q2)) {
                    continue;
                } else {
                    curKey2 = Dequeue(q2);
                }
            }
            int address_ok = 0;

            if (!autoaddress) {
                if (ip_family == AF_INET) {
                    if ((from.a4.sin_addr.s_addr == addr.a4.sin_addr.s_addr) && (from.a4.sin_port == addr.a4.sin_port)) {
                        address_ok = 1;
                    }
                } else {
                    if ((!memcmp(
                            from.a6.sin6_addr.s6_addr,
                            addr.a6.sin6_addr.s6_addr,
                            sizeof (addr.a6.sin6_addr.s6_addr))
                            ) && (from.a6.sin6_port == addr.a6.sin6_port)) {
                        address_ok = 1;
                    }
                }
            } else {
                memcpy(&addr.a, &from.a, slen);
                address_ok = 1;
            }

            if (address_ok) {
                if (blocksize) {
                    cnt = ((cnt - 1) / blocksize + 1) * blocksize; // pad to block size
                    mdecrypt_generic(td, buf, cnt);
                    mcrypt_enc_set_state(td, enc_state, enc_state_size);
                    write(dev, (void*) &buf, cnt);
                } else if (aes) {
                    bool toexit = false;
                    while (memcmp(buf, curKey2->sha, 32) != 0) {
                        if (isEmpty(q2)) {
                            int i;
                            printf("\nSHAbuf:\n");
                            for (i = 0; i < 32; i++) {
                                printf("%02X", buf[i]);
                            }
                            printf("\n");
                            fflush(stdout);
                            if (getKeyBySha(buf) == false) {
                                printf("NET2TAP: ERROR no sha\n");
                                toexit = true;
                                break;
                                //sleep(1);
                            }
                            printf("\n");
                            printf("\nSHAcur:\n");
                            for (i = 0; i < 32; i++) {
                                printf("%02X", curKey2->sha[i]);
                            }
                            printf("\n");
                            printf("GETNEW\n");
                            if (isEmpty(q2)) {
                                do_debug("NET2TAP: No keys\n");
                                sleep(1);
                            } else {
                                curKey2 = Dequeue(q2);
                            }
                        } else {
                            curKey2 = Dequeue(q2);
                            do_debug("NET2TAP: New key\n");
                        }
                    }
                    if (toexit) {
                        continue;
                    }
                    memcpy(iv_cur, iv, AES_BLOCK_SIZE);
                    cnt -= 32;
                    int diff = cnt % 128;
                    if (diff != 0) {
                        cnt = cnt + (128 - diff);
                    } else {
                        cnt = cnt;
                    }
                    AES_set_decrypt_key(curKey2->key, aes, &dec_key);
                    AES_cbc_encrypt((buf + 32), (buf + 32), cnt, &dec_key, iv_cur, AES_DECRYPT);
                    write(dev, (void*) &(*(buf + 32)), cnt);
                }
            }
        }
    }

    if (blocksize) {
        mcrypt_generic_deinit(td);
        mcrypt_module_close(td);
    }
}