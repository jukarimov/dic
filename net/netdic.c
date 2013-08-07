/*
    This file is a part of a dic
    Copyright (C) 2012 Jalil Karimov <jukarimov@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "9034"   // port we're listening on

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

const char *banner = "GNU dic, dictionary lookup utility\n\
Copyright (C) 2012 Jalil Karimov.\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law. More at the link or in gpl.txt\n";

struct index {
	char	*k;
	int	vlen;
	int	offt;
};

typedef struct index	IDX;

struct node {
	struct index	*idx;
	struct node	*ln, *rn;
};

typedef struct node NODE;

#define init(var, type)	(var = (type *)malloc(sizeof(type)))

#define BUCKETS 	60000

//#define	DCHARHASH(h, c)	((h) = 0x63c63cd9*(h) + 0x9c39c33d + (c))
#define	DCHARHASH(h, c)	(h = h + c)

struct bucket {
	NODE *n;
	int len;
};

struct bucket HT[BUCKETS];

unsigned int hash(char *k)
{
	unsigned int h;
	for (h=0; *k; ++k)
		DCHARHASH(h, *k);

	return h % BUCKETS;
}

NODE *treeadd(NODE *n, const char *k, int vlen, int offt)
{
	int cmp;
	if (n == NULL) {
		init(n, NODE);
		if (n == NULL)
			fprintf(stderr, "ERR:failed to init node\n");

		init(n->idx, IDX);
		if (n->idx == NULL)
			fprintf(stderr, "ERR:failed to init node\n");

		n->idx->k = strdup(k);
		if (n->idx->k == NULL)
			fprintf(stderr, "ERR:failed to copy key\n");

		n->idx->vlen = vlen;
		n->idx->offt = offt;

		n->ln = n->rn = NULL;
	}
	else if ((cmp = strcmp(k, n->idx->k)) > 0)
		n->rn = treeadd(n->rn, k, vlen, offt);
	else if (cmp < 0)
		n->ln = treeadd(n->ln, k, vlen, offt);
	else
		fprintf(stderr, "WARN: DUPLICATE key: %s\n", k);

	return n;
}

NODE *lookup(char *k, NODE *n)
{
	if (!n)
		return NULL;

	int cmp;
	if ((cmp = strcmp(k, n->idx->k)) == 0)
		return n;
	else if (cmp > 0)
		return lookup(k, n->rn);
	else
		return lookup(k, n->ln);
	
}

void freetree(NODE *n)
{
	if (n != NULL) {

		NODE *ln = n->ln;
		NODE *rn = n->rn;

		freetree(ln);
		freetree(rn);

		free(n->idx->k);
		free(n->idx);
		free(n);
	}
}

char *printree(NODE *n)
{

	if (n != NULL) {
		printree(n->ln);
		//printf("%s\n", n->idx->k);
		return n->idx->k;
		printree(n->rn);
	}
	else
		return NULL;
}

struct cache {
	char *key;
};
struct cache CC[100000];
int cc_len = 0;

void htput(char *k, int vlen, int offt)
{
	unsigned int h = hash(k);
	HT[h].n = treeadd(HT[h].n, k, vlen, offt);
	++HT[h].len;
	CC[cc_len].key = strdup(k);
	++cc_len;
}

FILE *CFP;

int fgetln(char *line)
{
	int c;
	size_t i = 0;
	while ((c=fgetc(CFP)) != EOF) {
		if (c == '\n') {
			line[i] = 0;
			return i;
		}
		line[i++] = c;
	}
	fclose(CFP);
	return -1;
}

void parse(const char *line, char *key, int *len, int *oft)
{
	while (*line != '\n') {
		*key++ = *line++;
		if (*line == '\x88') {
			*key = 0;
			line++;
			*len = atoi(line);
			while (*line++ != '\xaf')
				;
			*oft = atoi(line);
			return;
		}
	}
}

int main(int argc, char *argv[])
{
	printf("%s", banner);

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[256];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    int sender;

	char line[8000];
	char k[8000];
	int h, vlen, offt, len;

	if (argc < 3) {
		puts("Usage: run [foo.idx] [foo.sql]");
		exit(-1);
	}

	CFP = fopen(argv[1], "r");

	for (i=0; i < BUCKETS; i++) {
		HT[i].n = NULL;
		HT[i].len = 0;
	}

	int c = 0;
	while ((len=fgetln(line)) != -1) {

		parse(line, k, &vlen, &offt);
		//printf("%s:%d@%d\n", k, vlen, offt);
		htput(k, vlen, offt);
		c++;
	}
	printf("Loaded \t%d words\n", c);

	int fd = open(argv[2], O_RDONLY);

	struct stat sb;
	fstat(fd, &sb);

	int sz = sb.st_size; // & ~(sysconf(_SC_PAGE_SIZE) - 1);

	void *ad;
	ad = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);

	/********************************/

	char data[100 * 1000];
	int dlen = 0;


    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof(remoteaddr);
                    newfd = accept(listener,
                        	   (struct sockaddr *)&remoteaddr,
                        	   &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {
                    // handle data from a client
		    sender = i;
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
			
                        if (FD_ISSET(sender, &master)) {
				buf[strlen(buf)-1] = 0; // cut \n

				/*strcpy(data, "You sent me:'");
				strcat(data, buf);
				strcat(data, "'\n");*/

				if (!strcmp(buf, "[hi]")) {
					strcpy(data, banner);
					goto sendnow;
				}


				char str[1000];

				strcpy(str, buf);

				bzero(buf, sizeof(buf));


				printf("lookup: '%s'\n", str);
				// Do lookup!!!
				int not_found = 0;

		if (str[0] != '\0') {

			len = strlen(str);

			if (len < 1)
				continue;

			h = hash(str);

			if (HT[h].len) {
				NODE *n = lookup(str, HT[h].n);
				if (n) {

					puts("got here something...");
					
					int offt, vlen;

					offt = n->idx->offt;
					vlen = n->idx->vlen;

					char *p = ad + offt;

					printf("vlen: %d\n", vlen);

					bzero(data, sizeof(data));
					while (--vlen > 0 && p++) {
						data[dlen++] = *p;
						printf("%c", data[dlen]);
					}
					data[dlen++] = 0;
					printf("data: %s\n", data);
					dlen = 0;
					goto sendnow;

				} else {
					not_found = 1;//puts("Not found in tree");
					/*
					int j;
					for (j=0,i=0; i < cc_len; i++)
					{
						if (complet(str, CC[i].key) == 0) {
							printf("%d) %s\n", ++j,
								CC[i].key);
						}
					} //TODO also do levenshtein search if no found
					*/

					strcpy(data, "Not found");
				}
			} else
				not_found = 1;//puts("Not found in table");
				strcpy(data, "Not found");

			}
			/*
                        // we got some data from a client
                        for(j = 0; j <= fdmax; j++) {
                            // send to everyone!
                            if (FD_ISSET(j, &master)) {
                                // except the listener and ourselves
                                if (j != listener && j != i) {
                                    if (send(j, buf, nbytes, 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }
			*/
sendnow:
				strcat(data, "\n");

				int bytes_sent = 0, c = 0;
				int data_len = strlen(data);
				void *data_ptr = data;

				while (bytes_sent < data_len)
			       	{
					c = send(sender, data_ptr + bytes_sent, nbytes, 0);
					if (c == -1) {
						perror("send");
						exit(1);
					}
					bytes_sent += c;
				}
				puts("sent");
                    }
			
		}
	}
	}
	}
	}//for loop

	close(fd);

	for (i=0; i < BUCKETS; i++) {
		if (HT[i].len) {
			//printf("bucket %d: %d keys\n", i, HT[i].len);
			freetree(HT[i].n);
		}
	}

	for (i=0; i < cc_len; i++) {
		free(CC[i].key);
	}

	return 0;
}
