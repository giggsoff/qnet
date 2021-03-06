#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
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
#include <stdlib.h>
#include <errno.h>

#include <sys/select.h>

#include <mcrypt.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>

#include "sha.h"
#include "keyqueue.h"
#include <curl/curl.h>
#include "post_curl.h"

#include "zlib.h"

#include <pthread.h>
#include "thpool.h"
#include <math.h>
#include "leopard.h"
#include "LeopardCommon.h"

pthread_mutex_t lock;
pthread_mutex_t locksend;

Queue *q1;
Queue *q2;
KEY *curKey1;
KEY *curKey2;
uint8_t *curKeyToSend;
char buffer_addr[30];
int haslimit = 0;
int waslimit = 0;
int deviceid = 0;
int isserver = 1;
volatile uint8_t curpakagenum = 0;
int rs = 0;
int curcount = 0;
int curnum_recv = 0;
int curnum_recv_drop = 0;
int curnum_send = 0;

unsigned original_count = 9;
unsigned recovery_count = 4;
size_t buffer_bytes = 192;
unsigned encode_work_count = 0;
unsigned decode_work_count = 0;
uint8_t **original_data_send = NULL;
uint8_t **encode_work_data_send = NULL;
uint8_t **original_data_recv = NULL;
uint8_t **encode_work_data_recv = NULL;
uint8_t **decode_work_data = NULL;

union sockaddr_4or6 {
    struct sockaddr_in a4;
    struct sockaddr_in6 a6;
    struct sockaddr a;
};

typedef struct PHEADER {
    uint8_t flag;
    uint8_t pnum;
    uint16_t length;
} PHEADER;

int debug = 0;
bool getKeyBySha(uint8_t* sha);

int putKey(uint8_t* sha) {
    char qbuffer[64];
    int i;
    for (i = 0; i < 32; i++) {
        sprintf(qbuffer + i * 2, "%02X", sha[i]);
    }
    if (debug) {
        printf("putKey: \n");
        for (i = 0; i < 32; i++) {
            printf("%02X", sha[i]);
        }
        printf("\n");
    }
    char* nextkey = curl_get_key(buffer_addr, qbuffer, true);
    if (nextkey != NULL) {
        free(nextkey);
        return 0;
    }
    return 1;
}

uint8_t *requestKey() {
    char response_buffer[64];
    sprintf(response_buffer, "request%d\0", deviceid);
    char* nextkey = curl_get_key(buffer_addr, response_buffer, true);
    if (nextkey != NULL) {
        uint8_t *buff = (uint8_t*) malloc(32);
        int i;
        for (i = 0; i < 32; i++) {
            sscanf(nextkey + i * 2, "%02X", &(buff[i]));
        }
        if (debug) {
            printf("requestKey: \n");
            for (i = 0; i < 32; i++) {
                printf("%02X", buff[i]);
            }
            printf("\n");
        }
        free(nextkey);
        return buff;
    }
    return NULL;
}

void getLastKey() {
    char response_buffer[64];
    sprintf(response_buffer, "last%d\0", deviceid);
    char* nextkey = curl_get_key(buffer_addr, response_buffer, true);
    uint8_t buff[32];
    if (nextkey != NULL) {
        int i;
        for (i = 0; i < 32; i++) {
            sscanf(nextkey + i * 2, "%02X", &(buff[i]));
        }
        if (debug) {
            for (i = 0; i < 32; i++) {
                printf("%02X", buff[i]);
            }
            printf("\n");
            printf("NEWKEYOK\n");
        }
        KEY *k1 = ConstructKey(buff);
        KEY *k2 = CopyKey(k1);
        Enqueue(q1, k1);
        Enqueue(q2, k2);
        free(nextkey);
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
    char* nextkey = curl_get_key(buffer_addr, qbuffer, true);
    uint8_t buff[32];
    if (nextkey != NULL) {
        int i;
        for (i = 0; i < 32; i++) {
            sscanf(nextkey + i * 2, "%02X", &(buff[i]));
        }
        if (debug) {
            for (i = 0; i < 32; i++) {
                printf("%02X", buff[i]);
            }
            printf("\n");
            printf("NEWKEYBYSHAOK\n");
        }
        KEY *k1 = ConstructKeySha(buff, sha);
        KEY *k2 = CopyKey(k1);
        Enqueue(q1, k1);
        Enqueue(q2, k2);
        free(nextkey);
        return true;
    } else {
        return false;
    }
}

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
    fprintf(stderr, "%s ./ctapudp -s 0.0.0.0 -p 3443 -q 127.0.0.1 -r 55554 -i udp0 -e 100 -a 1-d 1\n", progname);
    fprintf(stderr, "%s ./ctapudp -c 0.0.0.0 -p 0 -q 192.168.1.199 -r 55554 -t 192.168.1.199 -k 3443 -i udp0 -e 100 -a 1 -d 1\n", progname);


    fprintf(stderr, "%s -h\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "-i <ifacename>: Name of interface to use (mandatory)\n");
    fprintf(stderr, "-s <serverIP>|-c <serverIP>: address for udp socket bind(mandatory)\n");
    fprintf(stderr, "-p <port>: port to listen on\n");
    fprintf(stderr, "-t <ip>: ip to connect\n");
    fprintf(stderr, "-k <port>: port to connect\n");
    fprintf(stderr, "-a 1: use TAP or TUN (without parameter)\n");
    fprintf(stderr, "-d 1: outputs debug information while running\n");
    fprintf(stderr, "-e <ppk>: use aes-cbc with key per packet\n");
    fprintf(stderr, "-z <compress level (0-9)>: use gzip\n");
    //fprintf(stderr, "-f <filepath>: use mcrypt with file\n");
    fprintf(stderr, "-q <ip>: ip to connect for keys\n");
    fprintf(stderr, "-r <port>: port to connect for keys\n");
    fprintf(stderr, "-w <cores>: cores count (1-6)\n");
    fprintf(stderr, "-l 1: restrict key reuse\n");
    fprintf(stderr, "-y <deviceid>: id of device\n");
    fprintf(stderr, "-h: prints this help text\n");
    exit(1);
}

int blocksize = 0;
int aes = 0;
int gzip = 0;

MCRYPT td;

void intHandler(int dummy) {
    printf("EXIT\n");
    /*if (rs) {
        for (unsigned i = 0, count = original_count; i < count; ++i)
            leopard::SIMDSafeFree(original_data_send[i]);
        for (unsigned i = 0, count = original_count; i < count; ++i)
            leopard::SIMDSafeFree(original_data_recv[i]);
        for (unsigned i = 0, count = encode_work_count; i < count; ++i)
            leopard::SIMDSafeFree(encode_work_data_send[i]);
        for (unsigned i = 0, count = encode_work_count; i < count; ++i)
            leopard::SIMDSafeFree(encode_work_data_recv[i]);
        for (unsigned i = 0, count = decode_work_count; i < count; ++i)
            leopard::SIMDSafeFree(decode_work_data[i]);
    }*/
    if (aes) {
        //curl_clear();
        EVP_cleanup();
    }
    if (blocksize) {
        mcrypt_generic_deinit(td);
        mcrypt_module_close(td);
    }
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&locksend);
    exit(0);
}

void handleErrors(void) {
    ERR_print_errors_fp(stderr);
    //abort();
}

int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
        unsigned char *iv, unsigned char *ciphertext) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;
    if (!(ctx = EVP_CIPHER_CTX_new())) handleErrors();
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handleErrors();
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
        handleErrors();
    ciphertext_len = len;
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) handleErrors();
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
        unsigned char *iv, unsigned char *plaintext) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        handleErrors();
        return 0;
    }
    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) {
        handleErrors();
        return 0;
    }
    if (1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len)) {
        handleErrors();
        return 0;
    }
    plaintext_len = len;
    if (1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len)) {
        handleErrors();
        return 0;
    }
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

uLong zip(unsigned char *input_data, int len, unsigned char *output_data, int level) {
    if (level > 9) {
        level = 9;
    } else if (level < 0) {
        level = 0;
    }
    unsigned long slen = len;
    unsigned long buflen = 3000;
    compress2((Bytef *) output_data, &buflen, (const Bytef *) input_data, slen, level);
    return buflen;
}

uLong unzip(unsigned char *input_data, int len, unsigned char *output_data) {
    unsigned long slen = len;
    unsigned long buflen = 3000;
    uncompress((Bytef *) output_data, &buflen, (const Bytef *) input_data, slen);
    return buflen;
}

int dev, sock, slen;
union sockaddr_4or6 addr, addr2, from;

struct zipandsend_args {
    int gziplevel;
    unsigned char *buf;
    int cnt;
    struct sockaddr_in addrDest;
    int dir;
    KEY *curKey;
    unsigned char *iv;
};

void zipandsend(void *argp) {
    struct zipandsend_args *args = argp;
    socklen_t addrlen = sizeof (struct sockaddr_in);
    unsigned char *buf = NULL;
    buf = (unsigned char*) malloc(2000 * sizeof (unsigned char));
    int cnt = args->cnt;
    memcpy(buf, args->buf, cnt);
    if (args->dir == 0) {
        do_debug("INIT LENGTH: %lu\n", cnt);
        if (gzip) {
            unsigned char *bufzip = NULL;
            bufzip = (unsigned char*) malloc(2000 * sizeof (unsigned char));
            uLong ziplength = zip(buf, args->cnt, bufzip, args->gziplevel);
            cnt = ziplength;
            do_debug("ZIPPED LENGTH: %lu\n", cnt);
            memcpy(buf, bufzip, ziplength);
            free(bufzip);
        }
        if (aes) {
            unsigned char *bufaes = NULL;
            bufaes = (unsigned char*) malloc(2000 * sizeof (unsigned char));
            memcpy(bufaes + 34, buf, cnt);
            memcpy(bufaes, args->curKey->sha, 32);
            memcpy(bufaes + 32, &cnt, 2);
            cnt += 2;
            int newcnt = encrypt(bufaes + 32, cnt, args->curKey->key, args->iv, bufaes + 32);
            cnt = newcnt + 32;
            memcpy(buf, bufaes, cnt);
            free(bufaes);
            free(args->curKey);
            free(args->iv);
        }
        pthread_mutex_lock(&locksend);
        if (0 > (cnt = sendto(sock, buf, cnt, 0, (struct sockaddr*) &(args->addrDest), addrlen))) {
            perror("Error while sending a packet.\n");
            switch (errno) {
                case EFAULT:
                    printf("Invalid user space address.\n");
                    break;
                case EAGAIN:
                    printf("Something with blocking.\n");
                    break;
                case EBADF:
                    printf("Invalid descriptor.\n");
                    break;
                case EINVAL:
                    printf("Invalid argument.\n");
                    break;
                case EDESTADDRREQ:
                    printf("No Peer address.\n");
                    break;
                case EISCONN:
                    printf("Connection mode socket.\n");
                    break;
                case ENOTSOCK:
                    printf("The given socket is not a socket.\n");
                    break;
                case ENOTCONN:
                    printf("No Target.\n");
                    break;
                case ENOBUFS:
                    printf("Network output is full.\n");
                    break;
                default:
                    printf("No spezific error.\n");
            }
            //exit(1);
        } else {
            do_debug("SENDED LENGTH: %lu\n", cnt);
        }
        pthread_mutex_unlock(&locksend);
    } else {
        if (aes) {
            unsigned char *bufaes = NULL;
            bufaes = (unsigned char*) malloc(2000 * sizeof (unsigned char));
            cnt -= 32;
            decrypt(buf + 32, cnt, args->curKey->key, args->iv, bufaes + 32);
            memcpy(&cnt, bufaes + 32, 2);
            memcpy(buf, bufaes + 34, cnt);
            free(bufaes);
            do_debug("NET2TAP: sended %d bytes\n", cnt);
            free(args->curKey);
            free(args->iv);
        }
        if (gzip) {
            unsigned char *bufzip = (unsigned char *) malloc(2000 * sizeof (unsigned char));
            do_debug("ZIPPED LENGTH: %lu\n", args->cnt);
            uLong ziplength = unzip(buf, args->cnt, bufzip);
            do_debug("UNZIPPED LENGTH: %lu\n", ziplength);
            cnt = ziplength;
            memcpy(buf, bufzip, cnt);
            free(bufzip);
        }
        pthread_mutex_lock(&lock);
        cnt = write(dev, (void*) buf, cnt);
        pthread_mutex_unlock(&lock);
        do_debug("TO_TAP_WRITED: %d\n", cnt);
    }
    free(args->buf);
    free(args);
    free(buf);
}

struct mainlistenerudp_args {
    int gziplevel;
    int sock;
    unsigned char *iv;
    threadpool pool;
    int autoaddress;
    int ip_family;
    union sockaddr_4or6 addr;
};

void mainlistenerudp(void *argp) {
    struct mainlistenerudp_args *argsinp = argp;
    int cnt = 0;
    unsigned char *buf = NULL;
    buf = (unsigned char*) malloc(2000 * sizeof (unsigned char));
    if (buf == NULL) {
        return;
    }
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(argsinp->sock, &rfds);
        int ret = select(argsinp->sock + 1, &rfds, NULL, NULL, NULL);
        if (ret < 0) continue;
        if (FD_ISSET(argsinp->sock, &rfds)) {
            cnt = recvfrom(argsinp->sock, buf, 2000, 0, &from.a, &slen);
            //if (cores) {
            thpool_wait_max(argsinp->pool);
            //}
            do_debug("NET2TAP: received %d bytes\n", cnt);
            if (aes) {
                int keychanged = 0;
                if (curKey2 == NULL) {
                    if (isEmpty(q2)) {
                        continue;
                    } else {
                        curKey2 = Dequeue(q2);
                        keychanged = 1;
                    }
                    if (keychanged) {
                        //AES_set_decrypt_key(curKey2->key, aes, &dec_key);
                    }
                }
            }
            int address_ok = 0;

            if (!argsinp->autoaddress) {
                if (argsinp->ip_family == AF_INET) {
                    if ((from.a4.sin_addr.s_addr == argsinp->addr.a4.sin_addr.s_addr) && (from.a4.sin_port == argsinp->addr.a4.sin_port)) {
                        address_ok = 1;
                    }
                } else {
                    if ((!memcmp(
                            from.a6.sin6_addr.s6_addr,
                            argsinp->addr.a6.sin6_addr.s6_addr,
                            sizeof (argsinp->addr.a6.sin6_addr.s6_addr))
                            ) && (from.a6.sin6_port == argsinp->addr.a6.sin6_port)) {
                        address_ok = 1;
                    }
                }
            } else {
                memcpy(&(argsinp->addr).a, &from.a, slen);
                address_ok = 1;
            }

            if (address_ok) {
                struct zipandsend_args *args = malloc(sizeof *args);
                if (args != NULL) {
                    unsigned char *bufforthread = (unsigned char *) malloc(2000 * sizeof (unsigned char));
                    if (bufforthread != NULL) {
                        memcpy(bufforthread, buf, cnt);
                        args->buf = bufforthread;
                        args->cnt = cnt;
                        args->gziplevel = argsinp->gziplevel;
                        args->dir = 1;
                        if (aes) {
                            if (cnt < 32 + 16) {
                                continue;
                            }
                            bool toexit = false;
                            int keychanged = 0;
                            while (memcmp(buf, curKey2->sha, 32) != 0) {
                                if (isEmpty(q2)) {
                                    int i;
                                    if (debug) {
                                        do_debug("\nSHAbuf:\n");
                                        for (i = 0; i < 32; i++) {
                                            do_debug("%02X", buf[i]);
                                        }
                                        do_debug("\n");
                                    }
                                    if (getKeyBySha(buf) == false) {
                                        do_debug("NET2TAP: ERROR no sha\n");
                                        toexit = true;
                                        break;
                                        //sleep(1);
                                    }
                                    if (debug) {
                                        do_debug("\nSHAcur:\n");
                                        for (i = 0; i < 32; i++) {
                                            do_debug("%02X", curKey2->sha[i]);
                                        }
                                        do_debug("\nGETNEW\n");
                                    }
                                    if (isEmpty(q2)) {
                                        do_debug("NET2TAP: No keys\n");
                                        usleep(100);
                                    } else {
                                        curKey2 = Dequeue(q2);
                                        keychanged = 1;
                                    }
                                } else {
                                    curKey2 = Dequeue(q2);
                                    do_debug("NET2TAP: New key\n");
                                    keychanged = 1;
                                }
                            }
                            if (toexit) {
                                continue;
                            }
                            unsigned char *iv_to_send = (unsigned char *) malloc(AES_BLOCK_SIZE);
                            memcpy(iv_to_send, argsinp->iv, AES_BLOCK_SIZE);
                            args->iv = iv_to_send;
                            args->curKey = CopyKey(curKey2);
                        }
                        thpool_add_work(argsinp->pool, (void*) zipandsend, args);
                    }
                }
            }
        }
    }
    free(buf);
}

struct mainlistenertap_args {
    int gziplevel;
    struct sockaddr_in addrDest;
    int ppk;
    unsigned char *iv;
    threadpool pool;
};

void mainlistenertap(void *argp) {
    struct mainlistenertap_args *argsinp = argp;
    int cnt = 0;
    unsigned char *buf = NULL;
    buf = (unsigned char*) malloc(2000 * sizeof (unsigned char));
    if (buf == NULL) {
        return;
    }
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(dev, &rfds);
        int ret = select(dev + 1, &rfds, NULL, NULL, NULL);
        if (ret < 0) continue;
        if (FD_ISSET(dev, &rfds)) {
            cnt = read(dev, (void*) buf, 1518);
            thpool_wait_max(argsinp->pool);
            struct zipandsend_args *args = malloc(sizeof *args);
            if (args != NULL) {
                unsigned char *bufforthread = NULL;
                bufforthread = (unsigned char *) malloc(2000 * sizeof (unsigned char));
                if (bufforthread != NULL) {
                    int i = 0;
                    memcpy(bufforthread, buf, cnt * sizeof (unsigned char));
                    if (aes) {
                        if (curKey1 == NULL) {
                            int keychanged = 0;
                            if (isEmpty(q1)) {
                                getLastKey();
                                if (isEmpty(q1)) {
                                    do_debug("TAP2NET: No keys\n");
                                    continue;
                                } else {
                                    curKey1 = Dequeue(q1);
                                    do_debug("TAP2NET: New key 1\n");
                                    keychanged = 1;
                                }
                            } else {
                                curKey1 = Dequeue(q1);
                                if (debug) {
                                    PrintKey(curKey1);
                                }
                                do_debug("TAP2NET: New key 2\n");
                                keychanged = 1;
                            }
                            if (keychanged) {
                            }
                        }
                        unsigned char *iv_to_send = (unsigned char *) malloc(AES_BLOCK_SIZE);
                        memcpy(iv_to_send, argsinp->iv, AES_BLOCK_SIZE);
                        args->iv = iv_to_send;
                        args->curKey = CopyKey(curKey1);
                        curKey1->usage++;
                    }
                    args->buf = bufforthread;
                    args->cnt = cnt;
                    args->gziplevel = argsinp->gziplevel;
                    args->addrDest = argsinp->addrDest;
                    args->dir = 0;
                    thpool_add_work(argsinp->pool, (void*) zipandsend, args);
                    if (aes) {
                        if (curKey1->usage > argsinp->ppk) {
                            int keychanged = 0;
                            do_debug("TAP2NET: %d usages of key\n", curKey1->usage);
                            if (!isEmpty(q1)) {
                                curKey1 = Dequeue(q1);
                                keychanged = 1;
                            } else {
                                getLastKey();
                                if (isEmpty(q1)) {
                                    do_debug("TAP2NET: No keys, last usage\n");
                                } else {
                                    curKey1 = Dequeue(q1);
                                    do_debug("TAP2NET: New key 3\n");
                                    keychanged = 1;
                                }
                            }
                            if (keychanged) {
                                //AES_set_encrypt_key(curKey1->key, aes, &enc_key);
                            }
                        }
                    }
                }
            }
        }
    }
    free(buf);
}

int main(int argc, char **argv) {
    signal(SIGINT, intHandler);
    int gziplevel = 5;
    uint16_t ppk = 100;
    progname = argv[0];
    unsigned char iv[] = {0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba, 0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6, 0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d, 0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d};
    unsigned char iv_cur[32];
    memcpy(iv_cur, iv, AES_BLOCK_SIZE);
    struct addrinfo hints;
    struct addrinfo *result;
    char key_ip[30] = "127.0.0.1";
    unsigned int port_key = 55554;
    unsigned char buf[4000];
    unsigned char bufzip[3000];
#ifndef __NetBSD__
    struct ifreq ifr;
#endif
    char *key;
    int keysize = 32; /* 256 bits == 32 bytes */
    char enc_state[1024];
    int enc_state_size;
    char* tun_device = "/dev/net/tun";
    char* dev_name = "tun%d";
    int tuntap_flag = IFF_TAP;
    tuntap_flag = IFF_TUN;
    int option, cnt;
    int ip_family = AF_INET;
    slen = sizeof (struct sockaddr_in);

    struct sockaddr_in addrDest;
    struct sockaddr_in addrDest2;
    int autoaddress = 1;

    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&locksend, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }

    char* laddr = NULL;
    char* lport = NULL;
    char* rhost = NULL;
    char* rhost2 = NULL;
    char* rport = NULL;
    char* fcname = NULL;
    int cores = 0;
    int curhost = -1;
    while ((option = getopt(argc, argv, "i:q:r:k:s:c:p:a:h:d:t:v:e:f:z:w:l:y:b:g:")) > 0) {
        switch (option) {
            case 'd':
                debug = 1;
                break;
            case 'h':
                usage();
                break;
            case 'i':
                dev_name = optarg;
                break;
            case 's':
                isserver = 1;
                autoaddress = 0;
                laddr = optarg;
                break;
            case 'c':
                isserver = 0;
                autoaddress = 0;
                laddr = optarg;
                break;
            case 'q':
                strncpy(key_ip, optarg, 15);
                break;
            case 'r':
                port_key = atoi(optarg);
                break;
            case 'z':
                gzip = 1;
                gziplevel = atoi(optarg);
                break;
            case 'p':
                lport = optarg;
                break;
            case 't':
                rhost = optarg;
                break;
            case 'b':
                rhost2 = optarg;
                curhost = 0;
                break;
            case 'k':
                rport = optarg;
                break;
            case 'a':
                tuntap_flag = IFF_TAP;
                break;
            case 'e':
                aes = 256;
                ppk = atoi(optarg);
                break;
            case 'y':
                deviceid = atoi(optarg);
                break;
            case 'l':
                haslimit = 1;
                break;
            case 'w':
                cores = atoi(optarg);
                if (cores < 0) {
                    cores = 1;
                } else if (cores > 6) {
                    cores = 6;
                }
                break;
                //case 'f':
                //    aes = 0;
                //    blocksize = 1;
                //    fcname = optarg;
                //    break;
            case 'v':
                ip_family = AF_INET6;
                slen = sizeof (struct sockaddr_in6);
                break;
            case 'g':
                rs = atoi(optarg);
                break;
            default:
                do_debug("Unknown option %c\n", option);
                usage();
        }
    }
    if (rs) {
        if (0 != leo_init()) {
            printf("ERROR REED_SOLOMON INIT\n");
            return -1;
        }
        encode_work_count = leo_encode_work_count(original_count, recovery_count);
        decode_work_count = leo_decode_work_count(original_count, recovery_count);
        do_debug("LEO COUNT ENCODE %d\n", encode_work_count);
        do_debug("LEO COUNT DECODE %d\n", decode_work_count);
        encode_work_data_send = (uint8_t**) malloc(encode_work_count * sizeof ( uint8_t*));
        encode_work_data_recv = (uint8_t**) malloc(encode_work_count * sizeof ( uint8_t*));
        decode_work_data = (uint8_t**) malloc(decode_work_count * sizeof ( uint8_t*));
        original_data_send = (uint8_t**) malloc(original_count * sizeof ( uint8_t*));
        original_data_recv = (uint8_t**) malloc(original_count * sizeof ( uint8_t*));
    }
    threadpool thpoolenc = NULL;
    threadpool thpooldec = NULL;
    threadpool thpoolserv = NULL;
    if (cores) {
        thpoolenc = thpool_init(cores, (int) pow(5, cores));
        thpooldec = thpool_init(cores, (int) pow(5, cores));
        thpoolserv = thpool_init(2, (int) pow(5, cores));
    }
    do_debug("loaded\n");
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
    memset(&addr2.a, 0, result->ai_addrlen);
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
        if (curhost == 0) {
            if (getaddrinfo(rhost2, rport, &hints, &result)) {
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
            memcpy(&addr2.a, result->ai_addr, result->ai_addrlen);
            freeaddrinfo(result);
        }
    }
    addrDest.sin_family = AF_INET;
    addrDest.sin_port = htons(atoi(rport));
    addrDest.sin_addr.s_addr = inet_addr(rhost);
    if (curhost == 0) {
        addrDest2.sin_family = AF_INET;
        addrDest2.sin_port = htons(atoi(rport));
        addrDest2.sin_addr.s_addr = inet_addr(rhost2);
    }
    if (aes) {
        OpenSSL_add_all_digests();
        q1 = ConstructQueue(10);
        q2 = ConstructQueue(10);
        int len = sprintf(buffer_addr, "http://%s:%d", key_ip, port_key);
        curl_init_addr();
        getLastKey();
    }
    fcntl(sock, F_SETFL, O_NONBLOCK);
    fcntl(dev, F_SETFL, O_NONBLOCK);
    if (cores) {
        struct mainlistenertap_args *argsmtap = malloc(sizeof *argsmtap);
        argsmtap->addrDest = addrDest;
        argsmtap->gziplevel = gziplevel;
        argsmtap->iv = iv;
        argsmtap->pool = thpoolenc;
        argsmtap->ppk = ppk;
        thpool_add_work(thpoolserv, (void*) mainlistenertap, argsmtap);
        do_debug("ADDED inp\n");
        struct mainlistenerudp_args *argsmudp = malloc(sizeof *argsmudp);
        argsmudp->gziplevel = gziplevel;
        argsmudp->iv = iv;
        argsmudp->pool = thpooldec;
        argsmudp->sock = sock;
        argsmudp->autoaddress = autoaddress;
        argsmudp->ip_family = ip_family;
        argsmudp->addr = addr;
        thpool_add_work(thpoolserv, (void*) mainlistenerudp, argsmudp);
    }
    do_debug("ADDED out\n");
    int maxfd = (sock > dev) ? sock : dev;
    int fdinp = 0;
    int fdout = 0;
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        FD_SET(dev, &rfds);
        int ret = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        ///!!!!!!!!!!!!!!!!!!!!!
        if (cores) {
            continue;
        }
        ///!!!!!!!!!!!!!!!!!!!!!
        if (ret < 0) continue;
        if (FD_ISSET(dev, &rfds)) {
            cnt = read(dev, (void*) &(*(buf + sizeof (PHEADER))), 1518);
            int tosend = 1;

            if (gzip) {
                do_debug("PREZIPPED LENGTH: %lu\n", cnt);
                uLong ziplength = zip(buf + sizeof (PHEADER), cnt, bufzip, gziplevel);
                do_debug("ZIPPED LENGTH: %lu\n", ziplength);
                memcpy(buf + sizeof (PHEADER), bufzip, ziplength);
                cnt = ziplength;
            }
            PHEADER *ph = (PHEADER *) malloc(sizeof (PHEADER));
            ph->flag = 0;
            ph->pnum = curpakagenum++;
            int rcnt = cnt;
            if (aes && deviceid > 0 && !isserver && curKey1->usage == ppk / 2) {
                ph->flag = 1;
                do_debug("TAP2NET: REQUEST T1\n");
            } else if (curKeyToSend != NULL) {
                ph->flag = 2;
                do_debug("TAP2NET: REQUEST T2\n");
                memmove((void*) &(*(buf + sizeof (PHEADER) + 32)), (void*) &(buf), cnt);
                memcpy(buf + sizeof (PHEADER), curKeyToSend, 32);
                free(curKeyToSend);
                curKeyToSend = NULL;
                cnt += 32;
            }
            ph->length = rcnt;
            do_debug("TAP2NET: prepare package %d\n", ph->pnum);
            memcpy(buf, ph, sizeof (PHEADER));
            free(ph);
            cnt += sizeof (PHEADER);
            if (aes) {
                memmove((void*) &(*(buf + 32)), (void*) &(buf), cnt);
                if (curKey1 == NULL || waslimit) {
                    int keychanged = 0;
                    if (isEmpty(q1)) {
                        getLastKey();
                        if (isEmpty(q1)) {
                            do_debug("TAP2NET: No keys\n");
                            tosend = 0;
                        } else {
                            if (haslimit) {
                                KEY *lastKeyLoaded = Dequeue(q1);
                                if (memcmp(lastKeyLoaded->sha, curKey1->sha, 32) == 0) {
                                    do_debug("TAP2NET: LIMIT REUSE\n");
                                    waslimit = 1;
                                    tosend = 0;
                                } else {
                                    waslimit = 0;
                                    curKey1 = lastKeyLoaded;
                                }
                            } else {
                                curKey1 = Dequeue(q1);
                            }
                            do_debug("TAP2NET: New key 1\n");
                            keychanged = 1;
                        }
                    } else {
                        curKey1 = Dequeue(q1);
                        if (debug) {
                            PrintKey(curKey1);
                        }
                        do_debug("TAP2NET: New key 2\n");
                        keychanged = 1;
                    }
                    if (keychanged) {
                    }
                }
                if (tosend) {
                    curKey1->usage++;
                    memcpy(buf, curKey1->sha, 32);
                    memcpy(iv_cur, iv, AES_BLOCK_SIZE);
                    int newcnt = encrypt(buf + 32, cnt, curKey1->key, iv_cur, buf + 32);
                    cnt = newcnt + 32;
                }
            }
            if (tosend) {
                if (rs) {
                    for (unsigned i = 0, count = encode_work_count; i < count; ++i)
                        encode_work_data_send[i] = leopard::SIMDSafeAllocate(buffer_bytes);
                    for (unsigned i = 0, count = original_count; i < count; ++i)
                        original_data_send[i] = leopard::SIMDSafeAllocate(buffer_bytes);
                    uint8_t *bufp = buf;
                    memmove(bufp + sizeof (uint16_t), bufp, cnt);
                    uint16_t cnt16 = cnt;
                    memcpy(bufp, &cnt16, sizeof (uint16_t));
                    for (unsigned i = 0; i < original_count; i++) {
                        memcpy(original_data_send[i] + 0, bufp + i*buffer_bytes, buffer_bytes);
                    }
                    LeopardResult encodeResult = leo_encode(
                            buffer_bytes,
                            original_count,
                            recovery_count,
                            encode_work_count,
                            (void**) &original_data_send[0],
                            (void**) &encode_work_data_send[0] // recovery data written here
                            );
                    uint8_t i = 0;
                    uint8_t curn = curnum_send;
                    for (i = 0; i < original_count; i++) {
                        memcpy(bufp, &curn, sizeof (uint8_t));
                        memcpy(bufp + sizeof (uint8_t), original_data_send[i] + 0, buffer_bytes);
                        if (curhost < 0) {
                            sendto(sock, bufp, buffer_bytes + sizeof (uint8_t), 0, &addr.a, slen);
                        } else {
                            if (curhost == 0) {
                                sendto(sock, bufp, buffer_bytes + sizeof (uint8_t), 0, &addr.a, slen);
                                curhost = 1;
                            } else {
                                sendto(sock, bufp, buffer_bytes + sizeof (uint8_t), 0, &addr2.a, slen);
                                curhost = 0;
                            }
                        }
                        curn++;
                    }
                    for (i = original_count; i < recovery_count + original_count; i++) {
                        memcpy(bufp, &curn, sizeof (uint8_t));
                        memcpy(bufp + sizeof (uint8_t), encode_work_data_send[i - original_count] + 0, buffer_bytes);
                        if (curhost < 0) {
                            sendto(sock, bufp, buffer_bytes + sizeof (uint8_t), 0, &addr.a, slen);
                        } else {
                            if (curhost == 0) {
                                sendto(sock, bufp, buffer_bytes + sizeof (uint8_t), 0, &addr.a, slen);
                                curhost = 1;
                            } else {
                                sendto(sock, bufp, buffer_bytes + sizeof (uint8_t), 0, &addr2.a, slen);
                                curhost = 0;
                            }
                        }
                        curn++;
                    }
                    for (unsigned i = 0, count = encode_work_count; i < count; ++i) {
                        leopard::SIMDSafeFree(encode_work_data_send[i]);
                        encode_work_data_send[i] = nullptr;
                    }
                    for (unsigned i = 0, count = original_count; i < count; ++i) {
                        leopard::SIMDSafeFree(original_data_send[i]);
                        original_data_send[i] = nullptr;
                    }
                    if (curnum_send > 10*(recovery_count + original_count)) {
                        curnum_send = 0;
                    } else {
                        curnum_send += recovery_count + original_count;
                    }
                } else {
                    if (curhost < 0) {
                        sendto(sock, &buf, cnt, 0, &addr.a, slen);
                    } else {
                        if (curhost == 0) {
                            sendto(sock, &buf, cnt, 0, &addr.a, slen);
                            curhost = 1;
                        } else {
                            sendto(sock, &buf, cnt, 0, &addr2.a, slen);
                            curhost = 0;
                        }
                    }
                }
                do_debug("TAP2NET: sended %d bytes\n", cnt);
                if (aes) {
                    if (curKey1->usage > ppk) {
                        int keychanged = 0;
                        do_debug("TAP2NET: %d usages of key\n", curKey1->usage);
                        if (!isEmpty(q1)) {
                            curKey1 = Dequeue(q1);
                            keychanged = 1;
                        } else {
                            getLastKey();
                            if (isEmpty(q1)) {
                                if (haslimit) {
                                    usleep(100000);
                                    do_debug("TAP2NET: LIMIT REUSE\n");
                                    continue;
                                }
                                do_debug("TAP2NET: No keys, last usage\n");
                            } else {
                                if (haslimit) {
                                    KEY *lastKeyLoaded = Dequeue(q1);
                                    if (memcmp(lastKeyLoaded->sha, curKey1->sha, 32) == 0) {
                                        do_debug("TAP2NET: LIMIT REUSE\n");
                                        waslimit = 1;
                                        continue;
                                    } else {
                                        waslimit = 0;
                                        curKey1 = lastKeyLoaded;
                                    }
                                } else {
                                    curKey1 = Dequeue(q1);
                                }
                                do_debug("TAP2NET: New key 3\n");
                                keychanged = 1;
                            }
                        }
                        if (keychanged) {
                            //AES_set_encrypt_key(curKey1->key, aes, &enc_key);
                        }
                    }
                }
            }
        }
        if (FD_ISSET(sock, &rfds)) {
            if (rs) {
loop1:
                cnt = recvfrom(sock, &buf, buffer_bytes + sizeof (uint8_t), MSG_WAITALL, &from.a, &slen);
                if (cnt < buffer_bytes) {
                    continue;
                }
            } else {
                cnt = recvfrom(sock, &buf, 2000, 0, &from.a, &slen);
                do_debug("NET2TAP: received %d bytes\n", cnt);
            }
            if (aes) {
                int keychanged = 0;
                if (curKey2 == NULL) {
                    if (isEmpty(q2)) {
                        continue;
                    } else {
                        curKey2 = Dequeue(q2);
                        keychanged = 1;
                    }
                    if (keychanged) {
                        //AES_set_decrypt_key(curKey2->key, aes, &dec_key);
                    }
                }
            }
            int address_ok = 0;

            if (!autoaddress) {
                if (ip_family == AF_INET) {
                    if (((from.a4.sin_addr.s_addr == addr.a4.sin_addr.s_addr) || (curhost >= 0 && (from.a4.sin_addr.s_addr == addr2.a4.sin_addr.s_addr))) && (from.a4.sin_port == addr.a4.sin_port)) {
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
                if (rs) {
                    uint8_t *podr = nullptr;
                    uint8_t *bufp = buf;
                    uint8_t curind = 0;
                    memcpy(&curind, bufp, sizeof (uint8_t));
                    do_debug("RECV #%d %d\n", curind, curnum_recv);
                    if ((curcount == 0 || curnum_recv_drop > recovery_count+2) && (curnum_recv == 0 || curnum_recv != (curind - (curind % (recovery_count + original_count))))) {
                        if (curind < 10*(recovery_count + original_count)) {
			    if(curind > recovery_count){
                            	curnum_recv = curind - (curind % (recovery_count + original_count)) + recovery_count + original_count;
                            }else{
				curnum_recv = 0;
			    }
			    curnum_recv_drop = 0;
                            curcount = 0;
                            for (unsigned i = 0, count = original_count; i < count; ++i) {
                                leopard::SIMDSafeFree(original_data_recv[i]);
                                original_data_recv[i] = nullptr;
                            }
                            for (unsigned i = 0, count = recovery_count; i < count; ++i) {
                                leopard::SIMDSafeFree(encode_work_data_recv[i]);
                                encode_work_data_recv[i] = nullptr;
                            }
			    printf("FREE\n");
                        }
			    printf("STRANGE\n");
                    } else {
                        if (curnum_recv != curind - (curind % (recovery_count + original_count))) {
                            curnum_recv_drop++;
			    printf("DROP\n");
			    continue;
                        }
                    }
                    curind = curind % (recovery_count + original_count);
                    if (curind < original_count) {
                        podr = leopard::SIMDSafeAllocate(buffer_bytes);
                        //original_data_recv[curind] = malloc(buffer_bytes);
                        memcpy(podr, (uint8_t *) (bufp + sizeof (uint8_t)), buffer_bytes);
                        original_data_recv[curind] = podr;
                        curcount++;
                    } else if (curind < original_count + recovery_count) {
                        podr = leopard::SIMDSafeAllocate(buffer_bytes);
                        //encode_work_data_recv[curind - original_count] = malloc(buffer_bytes);
                        memcpy(podr, (uint8_t *) (bufp + sizeof (uint8_t)), buffer_bytes);
                        encode_work_data_recv[curind - original_count] = podr;
                        curcount++;
                    }
                    if (curcount > original_count) {
                        for (uint8_t i = 0, count = decode_work_count; i < count; ++i) {
                            podr = leopard::SIMDSafeAllocate(buffer_bytes);
                            decode_work_data[i] = podr;
                        }
                        uint16_t cnt16 = 0;
                        //leopard::SIMDSafeFree(original_data_recv[1]);
                        //original_data_recv[1] = nullptr;
                        LeopardResult decodeResult = leo_decode(
                                buffer_bytes,
                                original_count,
                                recovery_count,
                                decode_work_count,
                                (void**) original_data_recv,
                                (void**) encode_work_data_recv,
                                (void**) decode_work_data);
                        do_debug("LEO RESULT DEC %d\n", decodeResult);
                        curcount = 0;
                        if (decodeResult == Leopard_Success) {
                            if (original_data_recv[0]) {
                                memcpy(&cnt16, original_data_recv[0] + 0, sizeof (uint16_t));
                                memcpy(bufp, original_data_recv[0] + sizeof (uint16_t), buffer_bytes - sizeof (uint16_t));
                            } else {
                                memcpy(&cnt16, decode_work_data[0] + 0, sizeof (uint16_t));
                                memcpy(bufp, decode_work_data[0] + sizeof (uint16_t), buffer_bytes - sizeof (uint16_t));
                            }
                            for (unsigned i = 1; i < original_count; i++) {
                                if (original_data_recv[i]) {
                                    memcpy(bufp + i * buffer_bytes - sizeof (uint16_t), original_data_recv[i] + 0, buffer_bytes);
                                } else {
                                    memcpy(bufp + i * buffer_bytes - sizeof (uint16_t), decode_work_data[i] + 0, buffer_bytes);
                                }
                            }
                        }
                        for (unsigned i = 0, count = original_count; i < count; ++i) {
                            leopard::SIMDSafeFree(original_data_recv[i]);
                            original_data_recv[i] = nullptr;
                        }
                        for (unsigned i = 0, count = recovery_count; i < count; ++i) {
                            leopard::SIMDSafeFree(encode_work_data_recv[i]);
                            encode_work_data_recv[i] = nullptr;
                        }
                        for (unsigned i = 0, count = decode_work_count; i < count; ++i) {
                            leopard::SIMDSafeFree(decode_work_data[i]);
                            decode_work_data[i] = nullptr;
                        }
                        cnt = cnt16;
                        if (curnum_recv > 10*(recovery_count + original_count)) {
                            curnum_recv = 0;
			    printf("TO 0\n");
                        } else {
                        curnum_recv += recovery_count + original_count;
                        }
                        if (decodeResult != Leopard_Success) {
                            continue;
                        }
                    } else {
//                        goto loop1;
			continue;
                    }
                }
                if (aes) {
                    if (cnt < 32 + 16) {
                        continue;
                    }
                    bool toexit = false;
                    int keychanged = 0;
                    while (memcmp(buf, curKey2->sha, 32) != 0) {
                        if (isEmpty(q2)) {
                            int i;
                            if (debug) {
                                do_debug("\nSHAbuf:\n");
                                for (i = 0; i < 32; i++) {
                                    do_debug("%02X", buf[i]);
                                }
                                do_debug("\n");
                            }
                            if (getKeyBySha(buf) == false) {
                                do_debug("NET2TAP: ERROR no sha\n");
                                toexit = true;
                                break;
                                //sleep(1);
                            }
                            if (debug) {
                                do_debug("\nSHAcur:\n");
                                for (i = 0; i < 32; i++) {
                                    do_debug("%02X", curKey2->sha[i]);
                                }
                                do_debug("\nGETNEW\n");
                            }
                            if (isEmpty(q2)) {
                                do_debug("NET2TAP: No keys\n");
                                usleep(100);
                            } else {
                                curKey2 = Dequeue(q2);
                                keychanged = 1;
                            }
                        } else {
                            curKey2 = Dequeue(q2);
                            do_debug("NET2TAP: New key\n");
                            keychanged = 1;
                        }
                    }
                    if (toexit) {
                        if (rs) {
                        }
                        continue;
                    }
                    if (keychanged) {
                        //AES_set_decrypt_key(curKey2->key, aes, &dec_key);
                    }
                    //memcpy(iv_cur, iv, AES_BLOCK_SIZE);
                    cnt -= 32;
                    /*int diff = cnt % 128;
                    if (diff != 0) {
                        cnt = cnt + (128 - diff);
                    } else {
                        cnt = cnt;
                    }*/
                    do_debug("NET2TAP: PRESIZE %d\n", cnt);
                    memcpy(iv_cur, iv, AES_BLOCK_SIZE);
                    if (decrypt(buf + 32, cnt, curKey2->key, iv_cur, buf + 32) == 0) {
                        printf("ERROR TO DECODE\n");
                        continue;
                    }

                    memmove((void*) &(buf), (void*) &(*(buf + 32)), cnt);
                } else {
                }
                PHEADER *ph = (PHEADER *) malloc(sizeof (PHEADER));
                memcpy(ph, buf, sizeof (PHEADER));
                do_debug("NET2TAP: ready package %d\n", ph->pnum);
                cnt = ph->length;
                if (ph->flag == 1) {
                    do_debug("NET2TAP: REQUEST T1\n");
                    curKeyToSend = requestKey();
                } else if (ph->flag == 2) {
                    uint8_t *newkeyreceived = (uint8_t *) malloc(32);
                    memcpy(newkeyreceived, buf + 32 + sizeof (PHEADER), 32);
                    putKey(newkeyreceived);
                    free(newkeyreceived);
                    memmove((void*) &(buf), (void*) &(*(buf + 32)), cnt);
                    do_debug("NET2TAP: REQUEST T2\n");
                }
                free(ph);

                if (gzip) {
                    do_debug("ZIPPED LENGTH: %lu\n", cnt);
                    uLong ziplength = unzip(buf + sizeof (PHEADER), cnt, bufzip);
                    do_debug("UNZIPPED LENGTH: %lu\n", ziplength);
                    memcpy(buf + sizeof (PHEADER), bufzip, ziplength);
                    cnt = ziplength;
                }
                write(dev, (void*) &(*(buf + sizeof (PHEADER))), cnt);
                if (rs) {
                    //goto loop1;
                }
            }
        }
    }
}
