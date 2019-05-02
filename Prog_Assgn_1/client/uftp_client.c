/* 
 * @file : uftp_client.c
 * @authors : Shivam Khandelwal & Samuel Solondz
 * @brief : UDP Client code for reliable transfer
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <stdbool.h>
#include <time.h>
#include <dirent.h>

#define NSEC_PER_MSEC							(1000000)
#define BUFSIZE 							(3100)
#define CMD_BUFSIZE							(64)
#define FILENAME_BUFSIZE						(64)
#define DATA_PACKET_RECV_BUFSIZE				        ((2*1024) + 15)
#define DATA_FIELD_LENGTH						(2*1024)
#define MAX_DATA_PACKET_COUNT_110MB			                (55*1024)
#define MAX_FILE_SIZE							(110*1024*1024)
					

/*------------------ Socket Variables ------------------------*/

int sockfd, portno, n;
int serverlen;
struct sockaddr_in serveraddr;
struct hostent *server;
char *hostname;

/*------------------------------------------------------------*/

/*------------------ data variables -------------------------*/

bool recv_data_ack_arr[MAX_DATA_PACKET_COUNT_110MB];
int recv_data_ack_arr_index;
bool send_data_ack_arr[MAX_DATA_PACKET_COUNT_110MB];
int send_data_ack_arr_index;
int max_packet_count;
int filename_len;
char cmd_buff[CMD_BUFSIZE];
char filename_buf[FILENAME_BUFSIZE];

char client_send_buf[BUFSIZE];
char client_recv_buf[BUFSIZE];
char client_data_buf[BUFSIZE];

char cmd_detect[3];
char ack_buf[5];

/*-----------------------------------------------------------*/

char *file_data_init_ptr = NULL;
char *file_data_end_ptr = NULL;
char *file_data_current_ptr = NULL;

char file_data_buf[MAX_FILE_SIZE];

/*------------------- Data Packet Variables ------------------*/

int data_pkt_max_count;
int data_byte_max_count;
int data_byte_count;
int current_data_pkt_count;
int recv_data_pkt_data_len;
int send_data_packet_size;

char *send_data_pkt_init_ptr = NULL;
char *send_data_pkt_current_ptr = NULL;
char *send_data_pkt_end_ptr = NULL;

char *recv_data_pkt_init_ptr = NULL;
char *recv_data_pkt_current_ptr = NULL;
char *recv_data_pkt_end_ptr = NULL;

char data_pkt_recv_buf[DATA_PACKET_RECV_BUFSIZE];
char data_pkt_data_buf[DATA_FIELD_LENGTH];

/*------------------------------------------------------------*/

/*----------------- Bool Variables --------------------------*/

bool get_cmd_ack;		// for get command ack from server
bool get_cmd_enable;		// for first iteration of get command
bool put_cmd_enable;		// for first iteration of put command
bool def_print_enable;		// to print default print command list
bool exit_check;

/*-----------------------------------------------------------*/

/*----------------- File Variables --------------------------*/

FILE *client_get_file;
FILE *client_put_file;

int put_file_found;
long unsigned int put_max_byte_count;

/*-----------------------------------------------------------*/

/*------------ Acknowledgement Variables --------------------*/

int get_cmd_seq_no;
int put_cmd_seq_no;

/*-----------------------------------------------------------*/

/*----------------- Time Variables --------------------------*/

struct timespec get_cmd_send_time;
struct timeval recv_timeout = {2,0};		// timeout - 2 seconds

/*-----------------------------------------------------------*/

/*------------------------------- time_diff()---------------------------------*/

/*
*	@brief : returns time in milliseconds(msec)
*/

int open_packet_client(char *pkt_ptr, char *data_ptr, int pkt_len);

long unsigned int time_diff(struct timespec *spec_1){
	struct timespec *spec_2;
	clock_gettime(CLOCK_REALTIME, spec_2);
	if(spec_2->tv_nsec > spec_1->tv_nsec){
		return ((long unsigned int)((spec_2->tv_nsec  - spec_1->tv_nsec)/NSEC_PER_MSEC));}
	else{
		return ((long unsigned int)((spec_1->tv_nsec  - spec_2->tv_nsec)/NSEC_PER_MSEC));}
}

/*---------------------------------------------------------------------------*/

/*------------------------------- get_time()---------------------------------*/

void get_time(struct timespec *strct){
	clock_gettime(CLOCK_REALTIME, strct);
}

/*---------------------------------------------------------------------------*/	

/*------------------ bool_vars_init() -----------------------*/

/*
*	@brief : initialize declared bool variables used as flags
*/

void bool_vars_init(void){
	get_cmd_ack = false;
	get_cmd_enable = false;
	def_print_enable = true;
	exit_check = true;
}

/*-----------------------------------------------------------*/

/* 
 * error - wrapper for perror
 */
 
void error(char *msg) {
    perror(msg);
    exit(0);
}



int copy_filename(char* src, char* dst) {
        int l;
		l = 3;
		while(src[l] != '\n'){
			dst[l-3] = src[l];
			l++;
		}
		dst[l-3] = '\0';
		l++;
return (l-3);
}

/*----------------- calculate_filesize() -------------------

	@brief : Calculate file size of the requested file
	
	@param : filename - ptr to file name buffer
	
	@return : file size of the requested file

-----------------------------------------------------------*/

long unsigned int calculate_filesize(char *filename){
	long unsigned int cnt;
	cnt = 0;
	char temp;
	FILE *fd;
	fd = fopen(filename,"rb");
	if(fd == NULL){
		perror("\nNo file found\n"); 
	}
	else{
		file_data_init_ptr = file_data_buf;
		file_data_current_ptr = file_data_buf;
		while(!(feof(fd))){
			*file_data_current_ptr++ = fgetc(fd);	
		}
		cnt = (long unsigned int)(file_data_current_ptr - file_data_init_ptr) - 1;
		file_data_end_ptr = file_data_current_ptr;
		file_data_current_ptr = file_data_buf;
		printf("\ncount - %ld bytes",cnt);
		max_packet_count = (int)((cnt/2048) + 1);
		printf("\nTotal packets to be sent : %ld",((cnt/2048) + 1));
		printf("\nlast packet byte count : %ld\n",(cnt%2048));
	}
	fclose(fd);
	return cnt;
} 

/*----------------- calculate_power() -------------------

	@brief : calculate base number multiplied by power of 10 
	
	@param : base - char to be multiplied by power of 10
			 power - power of 10
	
	@return : base number

-----------------------------------------------------------*/

int calculate_power(char base, int power){
    int var1, var2;
    var1 = 10;
    var2 = 1;
    while(power>0){
        var2 *= var1;
        power--;
    }
    return ((var2)*((int)(base - '0')));
}

/*----------------- int_to_str() ----------------------------

	@brief : Convert integer (int) to a char string (6 bytes)
	
	@param : ptr - ptr to string buffer
			 num - number to be converted to string
	
	@return : length of char string

-----------------------------------------------------------*/

int int_to_str(int num, char *ptr){
    
    int len,p,d;
    char swap;
    len = 0;
    p = 0;
    if(num == 0){
        *(ptr + len) = (char)((num%10) + '0');
        len++;
    }
    while(num>0){
        *(ptr + len) = (char)((num%10) + '0');
        num /= 10;
        len++;
    }
    for(p=0;p<(len/2);p++){
        swap = *(ptr + p);
        *(ptr + p) = *(ptr + len - 1 - p);
        *(ptr + len - 1 - p) = swap;
    }
    d = 6 - len;
    if(len < 6){
        for(p=len-1;p>=0;p--){
            *(ptr + p + d) = *(ptr + p);
        }
        for(p=0;p<d;p++){
            *(ptr + p) = '*';
        }
        //ptr += 6;
        return 6;
    }
    else{return len;}
}

/*------------------ create_packet()------------------------

    @brief : Creates packet of specified type - 
			 D - Data Packet type
             C - Command packet type
             A - Acknowledgement packet type
             F - File Size packet type 
             K - File Size Acknowledgement packet type 
			 
    @param  : 1. pkt_type - type of packet (D,C,A,F,K)
			  2. cmd_type - type of command
			  3. pkt_ptr  - ptr to packet buffer
			  4. seq_no   - packet sequence number
			  5. data_ptr - ptr to packet data buffer
			  6. data_len - length of packet data
			  
    @return : packet length
----------------------------------------------------------*/

/*--------------------------------------------------------*/
int create_packet(char pkt_type, char cmd_type, char *pkt_ptr, int seq_no, char *data_ptr,int data_len){
    
    char *pkt_temp_ptr;
    pkt_temp_ptr = pkt_ptr;
    int pkt_len;
    int temp_var1;
    
    if(data_ptr != NULL){
        switch(pkt_type){
            
            /*-------------------- Data packet type -------------------*/
            case 'D':
               *pkt_ptr++ = 'D';
               temp_var1 = int_to_str(seq_no,pkt_ptr);
               pkt_ptr += temp_var1;
               temp_var1 = int_to_str(data_len,pkt_ptr);
               pkt_ptr += temp_var1;
               for(temp_var1=0;temp_var1<data_len;temp_var1++){
                   *pkt_ptr++ = *(data_ptr + temp_var1);
               }
               pkt_len = (int)(pkt_ptr - pkt_temp_ptr);
            break;
            /*-------------------- Command packet type -------------------*/
            case 'C':
                *pkt_ptr++ = 'C';
               temp_var1 = int_to_str(seq_no,pkt_ptr);
               pkt_ptr += temp_var1;
               temp_var1 = int_to_str(data_len,pkt_ptr);
               pkt_ptr += temp_var1;
			   *pkt_ptr++ = cmd_type;
               for(temp_var1=0;temp_var1<data_len;temp_var1++){
                   *pkt_ptr++ = *(data_ptr + temp_var1);
               }
               pkt_len = (int)(pkt_ptr - pkt_temp_ptr);
                
            
            break;
            /*-------------------- Acknowledgement packet type -------------------*/
            case 'A':
				
			   *pkt_ptr++ = 'A';
               temp_var1 = int_to_str(seq_no,pkt_ptr);
               pkt_ptr += temp_var1;
               temp_var1 = int_to_str(data_len,pkt_ptr);
               pkt_ptr += temp_var1;
			   *pkt_ptr++ = cmd_type;
               for(temp_var1=0;temp_var1<data_len;temp_var1++){
                   *pkt_ptr++ = *(data_ptr + temp_var1);
               }
               pkt_len = (int)(pkt_ptr - pkt_temp_ptr);
				
            break;
            /*-------------------- File Size packet type -------------------*/
            case 'F':
			
				*pkt_ptr++ = 'F';
               temp_var1 = int_to_str(seq_no,pkt_ptr);
               pkt_ptr += temp_var1;
               temp_var1 = int_to_str(data_len,pkt_ptr);
               pkt_ptr += temp_var1;
               for(temp_var1=0;temp_var1<data_len;temp_var1++){
                   *pkt_ptr++ = *(data_ptr + temp_var1);
               }
               pkt_len = (int)(pkt_ptr - pkt_temp_ptr);
			
            break;
            /*--------- File Size Acknowledgement packet type -------------*/
            case 'K':
	       *pkt_ptr++ = 'K';
               temp_var1 = int_to_str(seq_no,pkt_ptr);
               pkt_ptr += temp_var1;
               temp_var1 = int_to_str(data_len,pkt_ptr);
               pkt_ptr += temp_var1;
               for(temp_var1=0;temp_var1<data_len;temp_var1++){
                   *pkt_ptr++ = *(data_ptr + temp_var1);
               }
               pkt_len = (int)(pkt_ptr - pkt_temp_ptr);
			
            break;
            
            default:
            break;
        }
    }
    else{
        printf("\nInvalid data ptr\n");
    }
    return pkt_len;
}

/*----------- estimate_data_packet_count() -------------------

	@brief : Estimate the number of data packets 
	
	@param : pkt_ptr - ptr to packet buffer
			 data_len - data length of char string
	
	@return : total packet count

-----------------------------------------------------------*/

int estimate_data_packet_count(char *str_ptr, int data_len){
	int loop_var1,filesize,filename_len;
	char temp_buf[50];
	for(loop_var1 = 0; loop_var1 < data_len; loop_var1++){
		temp_buf[loop_var1] = *(str_ptr + loop_var1);
	}
	filesize = atoi(temp_buf);
	filename_len = (int)(strlen(filename_buf));
	printf("\nfilename : %s",filename_buf);
	filename_buf[filename_len] = '\0'; 
	client_get_file = fopen(filename_buf,"wb");
	if(client_get_file == NULL){
		printf("\nfile could not be created\n");
	}
	else{
		data_byte_max_count = filesize;
		recv_data_ack_arr_index = 0;
	}
	printf("\nfilesize : %d bytes",filesize);
	return ((filesize/(2*1024)) + 1);
}

/*----------------- str_to_int() ------------------------------

	@brief : Converts given string of 6 bytes to int datatype
	
	@param : str - ptr to string buffer
	
	@return : number (int)

-----------------------------------------------------------*/

int str_to_int(char *str){
	int var1,var2,var3;
	char temp_buf[6];
	var2 = 0;
	for(var1 = 5; var1 >= 0;var1--){
		if(*(str + var1) != '*'){
			var3 = calculate_power(*(str + var1),5-var1);
			var2 += var3;
		}
	}
	return var2;
}

/*----------------- wait_for_data_pkt() ----------------------

	@brief : Waits for data packet to be received from server
	
	@param : none
	
	@return : none

-----------------------------------------------------------*/

void wait_for_data_pkt(void){
	int recv_pkt_size;
	bzero(data_pkt_recv_buf,DATA_PACKET_RECV_BUFSIZE);
	bzero(data_pkt_data_buf,DATA_FIELD_LENGTH);
	recv_pkt_size = recvfrom(sockfd, data_pkt_recv_buf, DATA_PACKET_RECV_BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
		    if (recv_pkt_size < 0) {printf("ERROR in recvfrom");}
			else{
				open_packet_client(data_pkt_recv_buf,data_pkt_data_buf,recv_pkt_size);
			}
}

/*----------------- client_send_data_ack() ----------------------

	@brief : Send data ACK when data received from server
	
	@param : ack_seq_no - sequence number for the ACK to be sent
	
	@return : none

-----------------------------------------------------------*/

void client_send_data_ack(int ack_seq_no){
	
	int var1, var2;
	bzero(client_send_buf,BUFSIZE);
	char temp;
	char *temp_ptr;
	temp_ptr = &temp;
	var1 = create_packet('A','D',client_send_buf,ack_seq_no,temp_ptr,0);
	var2 = sendto(sockfd, client_send_buf, var1, 0, (struct sockaddr *)&serveraddr, serverlen);
	if (var2 < 0){error("ERROR in sendto");}
	else{printf("\nACK for packet %d sent\n",ack_seq_no + 1);}
	
	if(ack_seq_no < (data_pkt_max_count - 1)){
		printf("\n ack no before wait data pkt : %d",ack_seq_no + 1);
		wait_for_data_pkt();
	}
	else{
		fclose(client_get_file);
		printf("\nFile transfer complete\n");
		def_print_enable = true;
	}
}

/*----------------- wait_for_data_ack() ----------------------

	@brief : Waits for data packet ACK to be received from server
	
	@param : none
	
	@return : none

-----------------------------------------------------------*/

void wait_for_data_ack(void){
	int var1, var2;
	bzero(client_recv_buf,BUFSIZE);
	var1 = recvfrom(sockfd, data_pkt_recv_buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
		    if (var1 < 0) {printf("ERROR in recvfrom");}
			else{
				open_packet_client(data_pkt_recv_buf,data_pkt_data_buf,var1);
			}
}

/*----------------- send_data_packet() ----------------------

	@brief : Send data packet when ACK for previous packet received from server
	
	@param : none
	
	@return : none

-----------------------------------------------------------*/

void send_data_packet(void){
	int loop_var1,pkt_len1,pkt_len2,var2;
	bzero(data_pkt_data_buf,DATA_FIELD_LENGTH);
					
					if((file_data_end_ptr - file_data_current_ptr) < DATA_FIELD_LENGTH){
						send_data_packet_size = (int)(file_data_end_ptr - file_data_current_ptr) - 1;
					}
					else{send_data_packet_size = DATA_FIELD_LENGTH;}
					send_data_pkt_init_ptr = data_pkt_data_buf;
					send_data_pkt_current_ptr = send_data_pkt_init_ptr;
					for(loop_var1 = 0; loop_var1 < send_data_packet_size; loop_var1++){
						 *send_data_pkt_current_ptr++ = *file_data_current_ptr++;
					}					
					send_data_pkt_current_ptr = send_data_pkt_init_ptr;
					bzero(client_send_buf,BUFSIZE);
					pkt_len1 = create_packet('D','0',client_send_buf,send_data_ack_arr_index,data_pkt_data_buf,send_data_packet_size);
					pkt_len2 = sendto(sockfd, client_send_buf, pkt_len1, 0, (struct sockaddr *)&serveraddr, serverlen);
					if (pkt_len2 < 0){error("ERROR in sendto");}
					else{
						printf("\nSent to server - data packet %d of %d bytes",send_data_ack_arr_index + 1,send_data_packet_size);
						wait_for_data_ack();
					}
}


/*----------------- open_packet_server() -------------------

	@brief : Opens packet received by the client.
	
	@param : pkt_ptr - ptr to packet buffer
			 data_ptr - ptr to data buffer
			 pkt_len - length of received packet

-----------------------------------------------------------*/

int open_packet_client(char *pkt_ptr, char *data_ptr, int pkt_len){
	int pkt_len1, pkt_len2,loop_var1,data_len,seq_number;
	char pkt_type;
	pkt_type = *pkt_ptr;
	switch(pkt_type){
		case 'D':				
				pkt_ptr++;
				recv_data_ack_arr_index = str_to_int(pkt_ptr);
				pkt_ptr += 6;
				recv_data_pkt_data_len = str_to_int(pkt_ptr);
				pkt_ptr += 6;
				printf("\nReceived data packet %d of %d bytes\n",recv_data_ack_arr_index + 1,recv_data_pkt_data_len);
				
				for(loop_var1 = 0;loop_var1 < recv_data_pkt_data_len; loop_var1++){
					fputc(*(pkt_ptr + loop_var1),client_get_file);
				}
				recv_data_ack_arr[recv_data_ack_arr_index] = true;
				client_send_data_ack(recv_data_ack_arr_index);
				
		break;
		case 'C':
		break;
		case 'A':
				if(*(pkt_ptr + 13) == 'P'){
					printf("\n\nACK from server received\nStarting File Transfer ....\n");
					send_data_ack_arr_index = 0;
					bzero(data_pkt_data_buf,DATA_FIELD_LENGTH);
					if(put_max_byte_count < DATA_FIELD_LENGTH){
						send_data_packet_size = put_max_byte_count;
					}
					else{send_data_packet_size = DATA_FIELD_LENGTH;}
					send_data_pkt_init_ptr = data_pkt_data_buf;
					send_data_pkt_current_ptr = send_data_pkt_init_ptr;
					for(loop_var1 = 0; loop_var1 < send_data_packet_size; loop_var1++){
						 *send_data_pkt_current_ptr++ = *file_data_current_ptr++;
					}					
					send_data_pkt_current_ptr = send_data_pkt_init_ptr;
					bzero(client_send_buf,BUFSIZE);
					pkt_len1 = create_packet('D','0',client_send_buf,send_data_ack_arr_index,data_pkt_data_buf,send_data_packet_size);
					pkt_len2 = sendto(sockfd, client_send_buf, pkt_len1, 0, (struct sockaddr *)&serveraddr, serverlen);
					if (pkt_len2 < 0){error("ERROR in sendto");}
					else{
						printf("\nSent to server - data packet %d of %d bytes",send_data_ack_arr_index + 1,send_data_packet_size);
						wait_for_data_ack();
					}
				}
				if(*(pkt_ptr + 13) == 'D'){
					pkt_ptr++;
					if((str_to_int(pkt_ptr)) == send_data_ack_arr_index){
						send_data_ack_arr[send_data_ack_arr_index] = true;
						printf("\nACK for packet %d received from server",send_data_ack_arr_index + 1);
						
						if(send_data_ack_arr_index < (max_packet_count - 1)){
							send_data_ack_arr_index++;
							send_data_packet();
						}
						else{
							printf("\nAll packets sent!");
							int temp_var1,temp_var2;
							def_print_enable = true;
							bzero(client_send_buf,BUFSIZE);
							char temp1;
							temp_var1 = create_packet('K','0',client_send_buf,0,&temp1,1);
							temp_var2 = sendto(sockfd, client_send_buf, temp_var1, 0, (struct sockaddr *)&serveraddr, serverlen);
							if (temp_var2 < 0){error("ERROR in sendto");}
							else{
								printf("\nSent file transfer complete message to server");
							}
							break;
						}
					}
					
				}
				if(*(pkt_ptr + 13) == 'X'){
					pkt_ptr++;
					if(str_to_int(pkt_ptr) == 1){
						printf("\nFile deleted at server\n");
					}
					if(str_to_int(pkt_ptr) == 2){
						printf("\nFile not found at server!\n");
					}
				}
				if(*(pkt_ptr + 13) == 'L'){
					printf("\n\nFile List - \n\n");
					pkt_ptr += 7;
					data_len = str_to_int(pkt_ptr);
					pkt_ptr += 7;
					for(loop_var1=0;loop_var1<data_len;loop_var1++){
						printf("%c",*(pkt_ptr + loop_var1));
					}
					printf("\n\nFile List printed\n\n");
				}
				
		break;
		case 'F':
				
		break;
		case 'K':
			printf("\n%s\n",pkt_ptr);
			pkt_ptr++;
			seq_number = str_to_int(pkt_ptr);
			if(seq_number == 1){
				printf("\nFile found");	
				pkt_ptr += 6;	
				data_len = str_to_int(pkt_ptr);
				pkt_ptr += 7;
				data_pkt_max_count = estimate_data_packet_count(pkt_ptr,data_len);
				printf("\ndata packet count : %d\n",data_pkt_max_count);
			
				bzero(client_send_buf,BUFSIZE);
				pkt_len1 = create_packet('A','F',client_send_buf,1,ack_buf,0);
				pkt_len2 = sendto(sockfd, client_send_buf, pkt_len1, 0, (struct sockaddr *)&serveraddr, serverlen);
				bzero(client_send_buf,BUFSIZE);
				get_cmd_ack = false;
				wait_for_data_pkt();
			}
			else{
				printf("\n\nFILE NOT FOUND AT SERVER\n");
				def_print_enable = true;
			}		
		break;
		default:
		break;
	}
}


/*----------------- check_cmd() -------------------

	@brief : Check command entered by user to copy filename
	
	@param : cmd_check_buf - buffer defined to detect command
	
	@return : true if file name entered

-----------------------------------------------------------*/

bool check_cmd(char *cmd_check_buf){
	if(strcmp(cmd_check_buf,"gt") == 0){
		return true;
	}
	else if(strcmp(cmd_check_buf,"pt") == 0){
		return true;
	}
	else if(strcmp(cmd_check_buf,"dl") == 0){
		return true;
	}
	else{
		return false;
	}
}

int main(int argc, char **argv) {
	
	/*--------------------------------------------------------------*/
	
    int exit_cmd;
    char exit_char;
    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET ;
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
	serverlen = sizeof(serveraddr);
   
	setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&recv_timeout,sizeof(struct timeval));

	/*--------------------------------------------------------------*/
	
	/*------ initialize bool variables --------*/
	
	bool_vars_init();	

	while(exit_check){
		
		/* get a message from the user */
		if(def_print_enable){
			printf("\n\nEnter one of the following commands\n");
			printf("gt [file_name] : Get file from server\n");
			printf("pt [file_name] : Put/Send file to server\n");
			printf("dl [file_name] : Delete file at server\n");
			printf("ls : List the files in the server\n");
			printf("ch : Chat with server");
			printf("ex : Exit server gracefully\n\n\n - ");
			bzero(cmd_buff, CMD_BUFSIZE);
			fgets(cmd_buff, CMD_BUFSIZE, stdin);
			int cmd_check;
			for(cmd_check = 0;cmd_check < 2;cmd_check++){
				cmd_detect[cmd_check] = cmd_buff[cmd_check];
			}
			cmd_detect[2] = '\0';
			if(check_cmd(cmd_detect)){
				filename_len = copy_filename(cmd_buff,filename_buf);
			}
			bzero(cmd_buff,CMD_BUFSIZE);
		}
		

		/****************** Get File Request **********************/

		if(strcmp(cmd_detect,"gt") == 0){
			bzero(cmd_detect,2);
			def_print_enable = false;
			get_cmd_enable = true;
			get_cmd_ack = true;
		}

		/****************** Put File Request **********************/

		else if(strcmp(cmd_detect,"pt") == 0){
			bzero(cmd_detect,3);
			def_print_enable = false;
			put_cmd_enable = true;
		}
		
		/****************** Delete File Request **********************/
		
		else if(strcmp(cmd_detect,"dl") == 0){
			int var1;
			def_print_enable = false;
			bzero(client_send_buf, BUFSIZE); 
			bzero(client_data_buf,BUFSIZE);
			printf("\nFile to be deleted - %s\tFilename_len : %ld bytes",filename_buf,strlen(filename_buf));
			var1 = create_packet('C','D',client_send_buf,0,filename_buf,filename_len);	
			n = sendto(sockfd, client_send_buf, var1, 0, (struct sockaddr *)&serveraddr, serverlen);
			printf("\nPakcet length - %d bytes", n);
		    if (n < 0){error("ERROR in sendto");}
			else{printf("\nFile delete command packet sent\n");}
			n = recvfrom(sockfd, client_recv_buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
			if (n < 0) {error("ERROR in recvfrom");}
			else{
				printf("\nFile delete ACK received from server");
				open_packet_client(client_recv_buf,client_data_buf,n);
			}
			bzero(client_send_buf,BUFSIZE);
			bzero(client_data_buf,BUFSIZE);
			bzero(client_recv_buf,BUFSIZE);
			def_print_enable = true;
		}
		
		/****************** List File Request **********************/
		
		else if(strcmp(cmd_detect,"ls") == 0){
			int var1;
			def_print_enable = false;
			bzero(client_send_buf, BUFSIZE); 
			bzero(client_data_buf,BUFSIZE);
			printf("\nFile list requested from server");
			var1 = create_packet('C','L',client_send_buf,0,client_data_buf,1);	
			n = sendto(sockfd, client_send_buf, var1, 0, (struct sockaddr *)&serveraddr, serverlen);
			printf("\nPakcet length - %d bytes", n);
		    if (n < 0){error("ERROR in sendto");}
			else{printf("\nFile list command packet sent");}
			n = recvfrom(sockfd, client_recv_buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
			if (n < 0) {error("ERROR in recvfrom");}
			else{
				printf("\nFile list received from server");
				open_packet_client(client_recv_buf,client_data_buf,n);
			}
			bzero(client_send_buf,BUFSIZE);
			bzero(client_data_buf,BUFSIZE);
			bzero(client_recv_buf,BUFSIZE);
			def_print_enable = true;
		}
		
		/****************** Exit server Request **********************/
		
		else if(strcmp(cmd_detect,"ex") == 0){
			bzero(cmd_detect,3);
			def_print_enable = false;
			serverlen = sizeof(serveraddr);
			bzero(client_send_buf,BUFSIZE);
			exit_cmd = create_packet('C','E',client_send_buf,0,&exit_char,1);
		    	n = sendto(sockfd, client_send_buf, exit_cmd, 0, (struct sockaddr *)&serveraddr, serverlen);
		    	if (n < 0) { error("ERROR in sendto");}
				else{printf("\nExit message sent to server");}

			exit_check = false;
			
		}
		else if(strcmp(cmd_detect, "ch") == 0){
			bzero(cmd_detect,3);
			char chat_msg_buff[150];
			def_print_enable = false;
			serverlen = sizeof(serveraddr);
			while(1){

				printf("\nEnter your message: ");
				scanf("%s", chat_msg_buff);
				if(strcmp("EXIT",chat_msg_buff) == 0){
					printf("\nExiting chat mode");
					break;
				}
				bzero(client_send_buf,BUFSIZE);
				exit_cmd = create_packet('C', 'X', client_send_buf, 0, chat_msg_buff, strlen(chat_msg_buff));
				n = sendto(sockfd, client_send_buf, exit_cmd, 0, (struct sockaddr *)&serveraddr, serverlen);
				if (n < 0) { error("ERROR in sendto");}

			}
			def_print_enable = true;
			
		}
		else{
			
		}
		
		if(get_cmd_enable){
			if(get_cmd_ack){
				if(get_cmd_enable){
					get_cmd_enable = false;
					get_cmd_seq_no = 0;
				}
				else{
					get_cmd_seq_no++;
				}
				get_cmd_ack = false;
				int get_cmd_send_pkt_len;
				/*----------- clear send buffer ---------------*/
				bzero(client_send_buf, BUFSIZE); 
				printf("\nRequested file - %sFilename_len : %ld bytes\n",filename_buf,strlen(filename_buf));
				get_cmd_send_pkt_len = create_packet('C','G',client_send_buf,get_cmd_seq_no,filename_buf,filename_len);
				
		    	n = sendto(sockfd, client_send_buf, get_cmd_send_pkt_len, 0, (struct sockaddr *)&serveraddr, serverlen);
		    	if (n < 0){error("ERROR in sendto");}
				else{printf("\nCommand packet sent\n");}
				bzero(client_send_buf,BUFSIZE);
			}
			bzero(client_recv_buf,BUFSIZE);
			n = recvfrom(sockfd, client_recv_buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
		        if (n < 0) {printf("ERROR in recvfrom");}
			else{
				printf("\nFile info. received: %d\n",n);
				open_packet_client(client_recv_buf,client_data_buf,n);
			}
		}
		
		if(put_cmd_enable){
			put_cmd_enable = false;
			put_cmd_seq_no = 0;

			int var1,var2;
			struct dirent *pDirent;
			DIR *pDir;
			char *dirpath = "./";
			pDir = opendir (dirpath);
			if (pDir == NULL) {
				printf ("Cannot open directory - %s\n", dirpath);
			}
			while ((pDirent = readdir(pDir)) != NULL) {	
				if(strcmp(pDirent->d_name,filename_buf) == 0){
					put_file_found = 2;
					printf("\n%s found\n",filename_buf);
					put_max_byte_count = calculate_filesize(filename_buf);	
				}
			}
			closedir (pDir);
			/* Check whether file exists in the directory */
			if(put_file_found == 2){
					
				bzero(client_send_buf,BUFSIZE);	

				/* Create put command packet */
				var1 = create_packet('C','P',client_send_buf,0,filename_buf,strlen(filename_buf));
				/* Send put command packet to server */
				var2 = sendto(sockfd, client_send_buf, var1, 0, (struct sockaddr *)&serveraddr, serverlen);
		    		if (var2 < 0){error("ERROR in sendto");}
				
				printf("\n Put Command packet sent");
				bzero(client_send_buf,BUFSIZE);
				bzero(client_recv_buf,BUFSIZE);
				bzero(client_data_buf,BUFSIZE);
				
				/* Wait for Put Command ACK */
				var2 = recvfrom(sockfd, client_recv_buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
		        	if (var2 < 0) {printf("ERROR in recvfrom");}
				else{
					printf("\nFileame ACK received");
					open_packet_client(client_recv_buf,client_data_buf,var2);
				}
			}
			else{
				printf("\nFile not found in the directory");
				def_print_enable = true;
			}
		}
	}
	
	/* Close socket and exit */
    close(sockfd);
    printf("\nClosing socket ...");
    printf("\nGoodbye!\n\n");
    return 0;
}
