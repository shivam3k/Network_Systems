/* 
 * @file : uftp_server.c
 * @authors : Shivam Khandelwal & Samuel Solondz
 * @brief : UDP Server code for reliable transfer
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>

#define BUFSIZE 								(3100)
#define FILENAME_BUFF_SIZE 						(32)


#define MAX_DATA_PACKETS						(53000)
#define MAX_FILE_SIZE							(110*1024*1024)
#define DATA_PACKET_DATA_SIZE					(2*1024)


char server_send_buf[BUFSIZE]; 						/* message send buf */
char server_recv_buf[BUFSIZE];						/* message recv buf */
char server_data_buf[BUFSIZE];						/* server data buf */
char ack_buf[5];
char filename_buf[FILENAME_BUFF_SIZE];				/* filename size buf */

/*-------------------- Socket Variables ----------------------------*/

  int sockfd; 										/* socket */
  int portno; 										/* port to listen on */
  int clientlen; 									/* byte size of client's address */
  struct sockaddr_in serveraddr; 					/* server's addr */
  struct sockaddr_in clientaddr; 					/* client addr */
  struct hostent *hostp; 							/* client host info */
  char *hostaddrp; 									/* dotted decimal host addr string */
  int optval;										/* flag value for setsockopt */
  int n; 											/* message byte size */

/*------------------------------------------------------------------*/

/*-------------------- File Variables ----------------------------*/

int filefound;
int file_size_var;
int cmp_pkt_file_size;

FILE *get_file;
FILE *put_file;

char *file_data_init_ptr = NULL;
char *file_data_end_ptr = NULL;
char *file_data_current_ptr = NULL;

char file_data_buff[MAX_FILE_SIZE];
char file_name_buffer[128];

/*------------------------------------------------------------------*/

/*-------------------- Data Packet Variables -----------------------*/

char *data_packet_current_ptr = NULL;
char *data_packet_init_ptr = NULL;
char *data_packet_end_ptr = NULL;
int send_max_pkt_count;
char data_packet_data_buff[DATA_PACKET_DATA_SIZE];

/*------------------------------------------------------------------*/

/*----------- Data Acknowledgement Variables -----------------------*/

bool send_ack_seq_arr[MAX_DATA_PACKETS];
int send_ack_seq_arr_index;
int recv_ack_seq_arr_index;

bool exit_check;
bool get_file_done;
bool send_next_packet;

/*------------------------------------------------------------------*/

/*
 * error - wrapper for perror
 */
 
void error(char *msg) {
  perror(msg);
  exit(1);
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

/*----------------- extract_num() -------------------

	@brief : Extract number from a char string
	
	@param : ptr - ptr to string buffer
	
	@return : number extracted from string

-----------------------------------------------------------*/

int extract_num(char *ptr){
	int num,p;
	num = 0;
	p=0;
	do{
		num += calculate_power(*(ptr-p),p);
		p++;
	}
	while((p < 6) && (*(ptr - p) != '*'));
	return num;
}

/*----------------- int_to_str() -------------------

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
        return 6;
    }
    else{return len;}
}

/*----------------- str_to_int() -------------------

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
    int temp_var1,var2;
    
    if(data_ptr != NULL){
        switch(pkt_type){
            
            /*-------------------- Data packet type -------------------*/
            case 'D':
               *pkt_ptr = 'D';
		pkt_ptr++;
               temp_var1 = int_to_str(seq_no,pkt_ptr);
		for(var2=0;var2<temp_var1;var2++){
			printf("%c",*(pkt_ptr + var2));
		}
               pkt_ptr += temp_var1;
               temp_var1 = int_to_str(data_len,pkt_ptr);
		for(var2=0;var2<temp_var1;var2++){
			printf("%c",*(pkt_ptr + var2));
		}
               pkt_ptr += temp_var1;
               for(temp_var1=0;temp_var1<data_len;temp_var1++){
		   if(temp_var1 < 4){
			printf("%c",*(data_ptr + temp_var1));
		   }
                   *pkt_ptr++ = *(data_ptr + temp_var1);
               }
		printf("\n");
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
			   *pkt_ptr++ = cmd_type;
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

/*----------------- calculate_filesize() -------------------

	@brief : Calculate file size of the requested file
	
	@param : filename - ptr to file name buffer
	
	@return : file size of the requested file

-----------------------------------------------------------*/

long unsigned int calculate_filesize(char *filename){
	int cnt;
	cnt = 0;
	char temp;
	FILE *fd;
	fd = fopen(filename,"rb");
	if(fd == NULL){
		perror("\nNo file found\n"); 
	}
	while(!(feof(fd))){
		temp = fgetc(fd);
		cnt++;	
	}
	printf("\ncount - %d bytes",cnt);
	printf("\nTotal packets to be sent : %d",((cnt/2048) + 1));
	printf("\nlast packet byte count : %d\n",(cnt%2048));
	fclose(fd);
	return cnt;
}

/*----------------- check_file() -------------------

	@brief : Check whether file present in the server directory
	
	@param : filename - ptr to file name buffer
			 filename_len - length of filename
	
	@return : file found status ( 1 if found )

-----------------------------------------------------------*/

int check_file(char *filename, int filename_len){
	int pkt_len1, pkt_len2,filesize, file_found;
	file_found = 0;
	struct dirent *pDirent;
	*(filename + filename_len - 1) = '\0';
    	DIR *pDir;
    	char *dirpath = "./";
    	pDir = opendir (dirpath);
    	if (pDir == NULL) {
        	printf ("Cannot open directory - %s\n", dirpath);
    	}
	while ((pDirent = readdir(pDir)) != NULL) {
            //printf ("[%s]\n", pDirent->d_name);	
	    if(strcmp(pDirent->d_name,filename) == 0){
		file_found = 1;
		filesize = calculate_filesize(filename);
		sprintf(filename_buf,"%d",filesize);
		printf("\nstrlen filesize : %ld\n", strlen(filename_buf));	
	    }
        }
    	closedir (pDir);
	bzero(server_send_buf,BUFSIZE);
	if(file_found == 1){
		pkt_len1 = create_packet('K','0',server_send_buf,1,filename_buf,strlen(filename_buf));
		printf("\n%s file found",filename);
	}
	else{
		pkt_len1 = create_packet('K','0',server_send_buf,2,filename_buf,strlen(filename_buf));
		printf("\nFile not found!");
	}
	pkt_len2 = sendto(sockfd, server_send_buf, pkt_len1, 0, (struct sockaddr *)&clientaddr,clientlen);
			
	if (pkt_len2 < 0){error("ERROR in sendto");}
	else{
		printf("\n\nFile ACK packet sent to client\n");
		//get_cmd_ack = false;
	
	}
	return file_found;
}

/*----------------- delete_file() -------------------

	@brief : Delete file if present in the server directory
	
	@param : filename - ptr to file name buffer
			 filename_len - length of filename
	
	@return : none

-----------------------------------------------------------*/

void delete_file(char *filename, int filename_len){
	int pkt_len1, pkt_len2, file_found;
	file_found = 0;
	struct dirent *pDirent;
	*(filename + filename_len - 1) = '\0';
    	DIR *pDir;
    	char *dirpath = "./";
    	pDir = opendir (dirpath);
    	if (pDir == NULL) {
        	printf ("Cannot open directory - %s\n", dirpath);
    	}
	while ((pDirent = readdir(pDir)) != NULL) {	
	    if(strcmp(pDirent->d_name,filename) == 0){
			file_found = 1;
			
	    }
    }
	closedir (pDir);
	if(file_found){
		printf("\n%s - File found\nDeleting file ....",filename);
		if (remove(filename) == 0) {printf("\nFile deleted successfully"); }
		else{printf("\nUnable to delete the file");}
		
		bzero(server_send_buf,BUFSIZE);
		pkt_len1 = create_packet('A','X',server_send_buf,1,filename_buf,strlen(filename_buf));
		pkt_len2 = sendto(sockfd, server_send_buf, pkt_len1, 0, (struct sockaddr *)&clientaddr,clientlen);		
		if (pkt_len2 < 0){error("ERROR in sendto");}
		else{printf("\n\nFile delete ACK packet sent to client\n");}
		bzero(server_send_buf,BUFSIZE);
	}
	else{
		printf("\nFile not found!");
		bzero(server_send_buf,BUFSIZE);
		pkt_len1 = create_packet('A','X',server_send_buf,2,filename_buf,strlen(filename_buf));
		pkt_len2 = sendto(sockfd, server_send_buf, pkt_len1, 0, (struct sockaddr *)&clientaddr,clientlen);		
		if (pkt_len2 < 0){error("ERROR in sendto");}
		else{printf("\n\nFile delete ACK packet sent to client\n");}
		bzero(server_send_buf,BUFSIZE);
	}
    
}

/*----------------- create_file_list() -------------------

	@brief : Create file of the server directory
	
	@param : ptr - ptr to file list buffer
	
	@return : length of file list buffer

-----------------------------------------------------------*/

int create_file_list(char *ptr){
	int var1,i;
	var1 = 0;
	
	struct dirent *pDirent;
    DIR *pDir;
    char *dirpath = "./";
    pDir = opendir (dirpath);
    if (pDir == NULL) {
       	printf ("Cannot open directory - %s\n", dirpath);
    }
	while ((pDirent = readdir(pDir)) != NULL) {	
		for(i=0;i<strlen(pDirent->d_name);i++){
			*(ptr + var1) = *(pDirent->d_name + i);
			var1++;
		}
		*(ptr + var1) = '\n';
		var1++;
    }
	closedir (pDir);
	return var1;
}

/*----------------- send_recvd_data_ack() -------------------

	@brief : Send ACK for each data packet received
	
	@param : none
	
	@return : none

-----------------------------------------------------------*/

void send_recvd_data_ack(void){
	int var1,var2;
	char temp;
	bzero(server_send_buf,BUFSIZE);
	var1 = create_packet('A','D',server_send_buf,recv_ack_seq_arr_index,&temp,1);
    var2 = sendto(sockfd, server_send_buf, var1, 0, (struct sockaddr *)&clientaddr,clientlen);
			
	if (var2 < 0){error("ERROR in sendto");}
	else{
		printf("\n ACK packet %d sent to client", recv_ack_seq_arr_index + 1);
	}
}

/*----------------- open_packet_server() -------------------

	@brief : Opens packet received by the server.
	
	@param : pkt_ptr - ptr to packet buffer
			 data_ptr - ptr to data buffer
	
	@return : none

-----------------------------------------------------------*/

void open_packet_server(char *pkt_ptr, char *data_ptr){
	char pkt_type;
	pkt_type = *pkt_ptr;
	int data_len;
	int loop_var1,var2;
	char chat_msg_buff[150];
	
	switch(pkt_type){
		case 'D':
			pkt_ptr++;
			recv_ack_seq_arr_index = str_to_int(pkt_ptr);
			pkt_ptr += 6;
			data_len = str_to_int(pkt_ptr);
			printf("\nData packet %d\tsize : %d",recv_ack_seq_arr_index + 1, data_len);
			pkt_ptr += 6;
			for(var2=0;var2<data_len;var2++){
				fputc(*(pkt_ptr + var2),put_file);
			}
			send_recvd_data_ack();
		break;
		case 'C':
			if(*(pkt_ptr + 13) == 'G'){					// Get Command Received
				pkt_ptr += 12;
				data_len = extract_num(pkt_ptr);		// Extract data length from packet
				pkt_ptr += 2;
				for(loop_var1 = 0; loop_var1 < data_len; loop_var1++){
					*(data_ptr + loop_var1) = *pkt_ptr++;
				}
				filefound = check_file(data_ptr,data_len);
				strcpy(file_name_buffer,data_ptr);
			}
			if(*(pkt_ptr + 13) == 'X'){
				pkt_ptr += 12;
				data_len = extract_num(pkt_ptr);
				pkt_ptr += 2;
				memcpy(chat_msg_buff, pkt_ptr, data_len);
				chat_msg_buff[data_len] = '\0';
				printf("\nReceived message: %s", chat_msg_buff);
				bzero(chat_msg_buff, strlen(chat_msg_buff) + 1);
			}
			if(*(pkt_ptr + 13) == 'P'){	
				pkt_ptr += 7;
				var2 = str_to_int(pkt_ptr);
				pkt_ptr += 7;
				char temp_arr[64];
				for(loop_var1=0;loop_var1<var2;loop_var1++){
					temp_arr[loop_var1] = *pkt_ptr++;
				}
				temp_arr[var2] = '\0';
				printf("\nfilename : %s\t%d\t%ld",temp_arr, var2,strlen(temp_arr));
				put_file = fopen(temp_arr,"wb");
				bzero(server_send_buf,BUFSIZE);
				var2 = create_packet('A','P',server_send_buf,0,temp_arr,strlen(temp_arr));
				loop_var1 = sendto(sockfd, server_send_buf, var2, 0, (struct sockaddr *)&clientaddr,clientlen);
				if (loop_var1 < 0){error("ERROR in sendto");}
				else{
					printf("\nPut file ACK packet sent to client\n");
				}
			}
			if(*(pkt_ptr + 13) == 'E'){
				printf("\nFile exit command received from client");
				exit_check = false;
				
			}
			if(*(pkt_ptr + 13) == 'D'){
				pkt_ptr += 7;
				var2 = str_to_int(pkt_ptr);
				pkt_ptr += 7;
				printf("\nFile delete command received from client");
				printf("\nChecking file status ....");
				delete_file(pkt_ptr,var2);
			}
			if(*(pkt_ptr + 13) == 'L'){
				printf("\nFile List request received");
				char temp_buffer[1024];
				var2 = create_file_list(temp_buffer);
				bzero(server_send_buf,BUFSIZE);
				loop_var1 = create_packet('A','L',server_send_buf,0,temp_buffer,var2-1);
				var2 = sendto(sockfd, server_send_buf, loop_var1, 0, (struct sockaddr *)&clientaddr,clientlen);
				if (var2 < 0){error("ERROR in sendto");}
				else{printf("\nFile List ACK sent to client");}
			}
			
		break;
		case 'A':
			if(*(pkt_ptr + 13) == 'F'){
				printf("\n\nFile Size ACK Received from client\n");
				if(filefound == 1){
					filefound = 0;
					send_ack_seq_arr_index = 0;
					send_next_packet = false;
					file_data_init_ptr = file_data_buff;
					file_data_current_ptr = file_data_buff;
					
					get_file = fopen(file_name_buffer,"rb");
					if(get_file == NULL){
						printf("\nCould not open file\n");
					}
					else{
						printf("\nFile Opened\n");
					}
					
					while(!(feof(get_file))){
						*file_data_current_ptr++ = fgetc(get_file);
					}
					fclose(get_file);
					file_data_end_ptr = file_data_current_ptr;
					file_data_current_ptr = file_data_init_ptr;
					send_max_pkt_count = (((int)(file_data_end_ptr - file_data_init_ptr))/DATA_PACKET_DATA_SIZE) + 1;
					if((file_data_end_ptr - file_data_init_ptr) < DATA_PACKET_DATA_SIZE){
						cmp_pkt_file_size = (int)(file_data_end_ptr - file_data_init_ptr);
					} 
					else{cmp_pkt_file_size = DATA_PACKET_DATA_SIZE;}
					printf("\nfile size ptr diff : %d\n",(int)(file_data_end_ptr - file_data_init_ptr));
					data_packet_init_ptr = data_packet_data_buff;
					data_packet_current_ptr = data_packet_init_ptr;
					printf("\ndata loop check - ");
					for(loop_var1 = 0; loop_var1 < cmp_pkt_file_size; loop_var1++){
						 *data_packet_current_ptr++ = *file_data_current_ptr++;
						if(loop_var1 < 4){
							printf("%c",*(data_packet_current_ptr - 1));
						}
					}
					printf("\n");
					data_packet_current_ptr = data_packet_init_ptr;
					send_ack_seq_arr_index = 0;
					bzero(server_send_buf, BUFSIZE);
					loop_var1 = create_packet('D','0',server_send_buf,send_ack_seq_arr_index,data_packet_data_buff,cmp_pkt_file_size);
					printf("\npacket size : %d\n",loop_var1);
					var2 = sendto(sockfd, server_send_buf, loop_var1, 0, (struct sockaddr *)&clientaddr,clientlen);
					if (var2 < 0){error("ERROR in sendto");}
					else{
						printf("\nSent data packet %d of %d bytes", send_ack_seq_arr_index, cmp_pkt_file_size);
					}
				}	
			}
			else if(*(pkt_ptr + 13) == 'D'){
				
				pkt_ptr++;
				var2 = str_to_int(pkt_ptr);
				if(send_ack_seq_arr_index < (send_max_pkt_count - 1)){
					if(var2 == send_ack_seq_arr_index){
						send_ack_seq_arr[send_ack_seq_arr_index] = true;
						printf("\nACK for packet %d received\n",send_ack_seq_arr_index);
						send_ack_seq_arr_index++;
						send_next_packet = true;	
					}
				}
				else if(send_ack_seq_arr_index == (send_max_pkt_count - 1)){
					get_file_done = true;
					printf("\nACK for packet %d received\n",send_ack_seq_arr_index);
					printf("\nAll packets sent!");
					printf("\nTotal packets sent to client : %d",send_max_pkt_count);
					send_next_packet = false;
				}
				else{printf("\nFile sequence error");}
				if(send_next_packet){
					send_next_packet = false;
					if((file_data_end_ptr - file_data_current_ptr)){
						if((file_data_end_ptr - file_data_current_ptr) < DATA_PACKET_DATA_SIZE){
							cmp_pkt_file_size = (int)(file_data_end_ptr - file_data_current_ptr) - 1;
						} 
						else{cmp_pkt_file_size = DATA_PACKET_DATA_SIZE;}
						bzero(data_packet_data_buff,DATA_PACKET_DATA_SIZE);
						data_packet_init_ptr = data_packet_data_buff;
						data_packet_current_ptr = data_packet_init_ptr;
						for(loop_var1 = 0; loop_var1 < cmp_pkt_file_size; loop_var1++){
							*data_packet_current_ptr++ = *file_data_current_ptr++;
						}
						data_packet_current_ptr = data_packet_init_ptr;
						bzero(server_send_buf, BUFSIZE);
						loop_var1 = create_packet('D','0',server_send_buf,send_ack_seq_arr_index,data_packet_data_buff,cmp_pkt_file_size);
						var2 = sendto(sockfd, server_send_buf, loop_var1, 0, (struct sockaddr *)&clientaddr,clientlen);
						if (var2 < 0){error("ERROR in sendto");}
						else{
							printf("\nSent data packet %d of %d bytes", send_ack_seq_arr_index, cmp_pkt_file_size);
						}
					}
				}
			}
		break;
		case 'F':
		break;
		case 'K':
			printf("\nAll packets received!\n");
			fclose(put_file);
		break;
		default:
		break;
	}
}

int main(int argc, char **argv) {

	  bzero(server_send_buf, BUFSIZE);
	  bzero(server_recv_buf, BUFSIZE);

	  /* 
	   * check command line arguments 
	   */
	  if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	  }
	  portno = atoi(argv[1]);

	  /* 
	   * socket: create the parent socket 
	   */
	  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	  if (sockfd < 0) 
		error("ERROR opening socket");

	  /* setsockopt: Handy debugging trick that lets 
	   * us rerun the server immediately after we kill it; 
	   * otherwise we have to wait about 20 secs. 
	   * Eliminates "ERROR on binding: Address already in use" error. 
	   */
	  optval = 1;
	  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
			 (const void *)&optval , sizeof(int));

	  /*
	   * build the server's Internet address
	   */
	  bzero((char *) &serveraddr, sizeof(serveraddr));
	  serveraddr.sin_family = AF_INET;
	  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	  serveraddr.sin_port = htons((unsigned short)portno);

	  /* 
	   * bind: associate the parent socket with a port 
	   */
	  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
		   sizeof(serveraddr)) < 0) 
		error("ERROR on binding");

	  /* 
	   * main loop: wait for a datagram, then echo it
	   */
	  clientlen = sizeof(clientaddr);
		
	  
	  exit_check = true;
	  
	  while (exit_check) {
			/*
			 * recvfrom: receive a UDP datagram from a client
			 */
			
			n = recvfrom(sockfd, server_recv_buf, BUFSIZE, 0,
				 (struct sockaddr *) &clientaddr, &clientlen);
			if (n < 0){error("ERROR in recvfrom");}
			else{
				printf("server received %d bytes\n", n);
				open_packet_server(server_recv_buf,server_data_buf);
				bzero(server_send_buf, BUFSIZE);
				bzero(server_recv_buf, BUFSIZE);
			}

			/* 
			 * gethostbyaddr: determine who sent the datagram
			 */
			hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
					  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
			if (hostp == NULL)
			  error("ERROR on gethostbyaddr");
			hostaddrp = inet_ntoa(clientaddr.sin_addr);
			if (hostaddrp == NULL)
			  error("ERROR on inet_ntoa\n");
			printf("\nserver received datagram from %s (%s)\n", hostp->h_name, hostaddrp);
			
		}
		close(sockfd);
		printf("\nClosing socket ...\nExiting gracefully\nGoodbye!\n\n");
		return 0;
}

