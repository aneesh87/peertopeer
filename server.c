#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define true 1
#define false 0
#define MAX_NAME_LEN 255
#define MAX_TITLE 1024
struct rfclist {
	int rfcnum;
	char title[MAX_TITLE];
	struct rfclist * next;
};
struct peer {
	unsigned int ipaddr;
	int client_fd;
	unsigned int port;
	char peer_name[MAX_NAME_LEN];
	int active;
	struct rfclist * rfchead;
	struct peer * next;
};

struct dummy_obj {
	struct sockaddr_in cliaddr;
	int client_fd;
};


struct peer * head;

void insertrfc (struct rfclist ** rfcdb, struct rfclist * newrfc) {
	if (*rfcdb == NULL) {
		*rfcdb = newrfc;
		return;
	}
	struct rfclist * tmp = *rfcdb;
	while (tmp) {
		if (tmp->rfcnum == newrfc->rfcnum) {
			return;
		}
		tmp = tmp->next;
	}
	newrfc->next = *rfcdb;
	*rfcdb = newrfc;
}
void * process_thread(void *obj) {
        int client_fd = ((struct dummy_obj *)obj)->client_fd;
        unsigned int ip_addr = ((struct dummy_obj *)obj)->cliaddr.sin_addr.s_addr;

	while (1) {
	    char buffer[2048];
            char * saveptr = NULL;
	    memset(buffer, 0, 2048);
	    int len = read(client_fd, buffer, 2048);
	    fprintf(stdout, "Client Mesg: %s\n", buffer);
	    const char *token = strtok_r(buffer, " \n", &saveptr);

	    if (strcmp(token, "EXIT") == 0 ) {
	    	struct peer * temp = head;
	    	while (temp != NULL) {
	    		if (client_fd == temp->client_fd && temp->active == true) {
	    			temp->active = false;
	    		}
	    		temp = temp->next;
	    	}
	    	len = write(client_fd, "Got your message, closing connection", 16);
	    	close(client_fd);
	        return NULL;
	    }
	    if (strcmp(token, "ADD") == 0) {


	    	struct peer * temp = (struct peer *)calloc(1,sizeof(struct peer));
	        temp->active = true;
	    	temp->client_fd = client_fd;
	    	temp->ipaddr = ip_addr;

	    	token = strtok_r(NULL, " \n", &saveptr);
	    	token = strtok_r(NULL, " \n", &saveptr);
	    	int rfcnum = atoi(token);
	    	token = strtok_r(NULL, " \n", &saveptr);
	    	token = strtok_r(NULL, " \n", &saveptr);
	    	token = strtok_r(NULL, " \n", &saveptr);
	    	strncpy(temp->peer_name,token, MAX_NAME_LEN);
	    	token = strtok_r(NULL, " \n", &saveptr);
	    	token = strtok_r(NULL, " \n", &saveptr);
	    	temp->port = atoi(token);
	    	token = strtok_r(NULL, " \n", &saveptr);
	    	token = strtok_r(NULL, " \n", &saveptr);

	    	struct rfclist * newrfc = (struct rfclist *) calloc(1, sizeof(struct rfclist));
	    	newrfc->rfcnum = rfcnum;
	    	strncpy(newrfc->title,token,MAX_TITLE);

	    	if (head == NULL) {
	    		head = temp;
	    		head->rfchead = newrfc;
	    	} else {
	    		struct peer * iter = head;
	    		while (iter != NULL) {
	    			if (iter->port == temp->port && strcmp(iter->peer_name,temp->peer_name)==0) {
	    				iter->active = true;
	    				free(temp);
	    				insertrfc(&iter->rfchead, newrfc);
	    				break;
	    			}
                             iter=iter->next;
	    		}
	    		if (iter == NULL) {
	    		    temp->next = head;
	    		    head = temp;
	    		    insertrfc(&head->rfchead, newrfc);
	    		}
	    	}
	    	len = write(client_fd, "Got your message, RFC added", 16);
	    
            } else if (strcmp(token, "LIST") == 0) {
	    	struct peer * temp = head;
	    	char sendbuffer[65536] = "";
	        char tmpbuf[2048] = "";
	        while (temp!= NULL) {
	        	struct rfclist * iter = temp->rfchead;
	        	while (iter != NULL) {
	        	       snprintf(tmpbuf, MAX_NAME_LEN, "RFC number %d title %s hostname %s port %d\n", 
	        		            iter->rfcnum, iter->title, temp->peer_name, temp->port);
	        	       strncat(sendbuffer, tmpbuf, 65536 - strlen(sendbuffer));
	        	       fprintf(stderr, "%s\n", sendbuffer);
	        	       iter= iter->next;
	        	}
	        	temp = temp->next;
	        }
	        len = write(client_fd, sendbuffer, strlen(sendbuffer) + 1);
	    }
	}
}

int main(int argc, char ** argv) {

	int sock;
	int port;
	int client_fd;
	struct sockaddr_in server_addr, client_addr;
	int client_len = sizeof(client_addr);
        struct dummy_obj x;

	int rc = 0;

	if (argc < 2) {
		fprintf(stderr, "./server <port no>");
		exit(1);
	}
	port = atoi(argv[1]);
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		fprintf(stderr, "socket() failed: %s", strerror(errno));
		exit(1);
	}
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	rc = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

	if (rc != 0) {
		fprintf(stderr, "Not able to bind to socket: %s\n",
			    strerror(errno));
		exit(1);
	}
	listen(sock, 10);
	while (1) {
		client_fd = accept(sock, (struct sockaddr *) &client_addr, (socklen_t *)&client_len);
		if (client_fd < 0)  {
			fprintf(stderr, "accept failed: %s\n", strerror(errno));
			continue;
		}
	x.cliaddr = client_addr;
	x.client_fd = client_fd;

		pthread_t peer_thread;
        if (pthread_create(&peer_thread, NULL, &process_thread, &x) != 0) {
            fprintf(stderr, "Unable to start a thread\n"); 
        }  
		/*
		peer * x = new peer;
		x->client_fd =  client_fd;
		x->active = true;
		x->ipaddr = client_addr.sin_addr.s_addr;
		peer_db[client_fd] = *x;
		pthread_create
		*/
	}
	return 0;
}
