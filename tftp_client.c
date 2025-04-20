#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>



struct sockaddr_in * dest;



void send_packet(int self,short seqno,FILE * fp,long size)
{
	uint8_t packet[size+4];
	packet[0]=0;
	packet[1]=3;
	packet[3]=seqno%256;
	seqno/=256;
	packet[2]=seqno%256;
	fread(packet+4,1,size,fp);
	sendto(self,packet,size+4,0,(struct sockaddr*)dest,sizeof(*dest));
}

void recv_ack(int self,short seqno)
{
	long long size=sizeof(*dest);
	uint8_t packet[516];
	memset(packet,0,516);

	int bytes_recv=recvfrom(self,packet,sizeof(packet),0,(struct sockaddr*)dest,(socklen_t*)&size);

	short opcode=packet[0]*256+packet[1];
	short bno=packet[2]*256+packet[3];
	if(opcode==5)
	{
		printf("Error:");
		for(int i=4;i<bytes_recv;++i)
			printf("%c",(char)packet[i]);
		printf("\n");
		exit(0);
	}
	if(opcode!=4)
		recv_ack(self,seqno);
	if(bno!=seqno)
		recv_ack(self,seqno);
	if(bytes_recv!=4)
		recv_ack(self,seqno);
}
void writeop(int self,char * fname)
{
	uint8_t packet[516];
	memset(packet,0,516);
	packet[0]=0;
	packet[1]=2;
	for(int i=0;i<strlen(fname);++i)
		packet[i+2]=(uint8_t)fname[i];
	packet[3+strlen(fname)]=(uint8_t)'o';
	packet[4+strlen(fname)]=(uint8_t)'c';
	packet[5+strlen(fname)]=(uint8_t)'t';
	packet[6+strlen(fname)]=(uint8_t)'e';
	packet[7+strlen(fname)]=(uint8_t)'t';

	sendto(self,packet,9+strlen(fname),0,(struct sockaddr*)dest,sizeof(*dest));

	recv_ack(self,0);
	

	char * filename;
	char * token=strtok(fname,"/");
	while(token)
	{
		if(token!=NULL)
			filename=token;
		token=strtok(NULL,"/");
	}

	FILE * fp=fopen(filename,"rb");
	fseek(fp,0,SEEK_END);
	long len=ftell(fp);
	fseek(fp,0,SEEK_SET);
	short seqno=1;
	while(len>0)
	{
		long size;
		if(len>=512)
		{
			size=512;
			len-=512;
		}
		else
		{
			size=len;
			len=0;
		}
		send_packet(self,seqno,fp,size);
		recv_ack(self,seqno);
		seqno++;
		if(len==0)
		{
			printf("Success\n");
			fclose(fp);
			free(dest);
			exit(0);
		}
	}
}

int recv_packet(int self,uint8_t * packet,short seqno)
{
	long long size=sizeof(*dest);
	memset(packet,0,516);
	int bytes_recv=recvfrom(self,packet,516,0,(struct sockaddr*)dest,(socklen_t*)&size);
	short opcode=packet[0]*256+packet[1];
	short bno=packet[2]*256+packet[3];
	if(opcode==5)
	{
		printf("Error:");
		for(int i=4;i<bytes_recv;++i)
			printf("%c",(char)packet[i]);
		printf("\n");
		exit(0);
	}
	if(opcode!=3)
		recv_packet(self,packet,seqno);

	if(bno!=seqno)
		recv_packet(self,packet,seqno);
	return bytes_recv;
}
void send_ack(int self,short seqno)
{
	uint8_t packet[4];
	packet[0]=0;
	packet[1]=4;
	packet[3]=seqno%256;
	seqno/=256;
	packet[2]=seqno%256;

	long long size=sizeof(*dest);
	sendto(self,packet,4,0,(struct sockaddr*)dest,sizeof(*dest));
}
void readop(int self,char * fname)
{
	long long size=sizeof(*dest);
	uint8_t packet[516];
	memset(packet,0,516);
	packet[0]=0;
	packet[1]=1;
	for(int i=0;i<strlen(fname);++i)
		packet[i+2]=(uint8_t)fname[i];
	packet[3+strlen(fname)]=(uint8_t)'o';
	packet[3+strlen(fname)+1]=(uint8_t)'c';
	packet[3+strlen(fname)+2]=(uint8_t)'t';
	packet[3+strlen(fname)+3]=(uint8_t)'e';
	packet[3+strlen(fname)+4]=(uint8_t)'t';

	sendto(self,packet,9+strlen(fname),0,(struct sockaddr*)dest,sizeof(*dest));

	char * filename;
	char * token=strtok(fname,"/");
	while(token)
	{
		if(token!=NULL)
		{
			filename=token;
		}
		token=strtok(NULL,"/");
	}
	FILE * fp=fopen(filename,"wb");
	short seqno=1;
	while(1)
	{
		int res=recv_packet(self,packet,seqno);
		fseek(fp,0,SEEK_END);
		fwrite(packet+4,1,res-4,fp);
		send_ack(self,seqno);
		seqno++;
		if(res<516)
		{
			printf("Success\n");
			fclose(fp);
			free(dest);
			exit(0);
		}
	}
}

int main(int argc,char * argv[])
{
	dest=(struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	int port=69;
	char filename[100];
	strcpy(filename,argv[2]);

	dest->sin_family=AF_INET;
	dest->sin_port=htons(port);
	dest->sin_addr.s_addr=inet_addr(argv[1]);

	int self=socket(AF_INET,SOCK_DGRAM,0);
	if(atoi(argv[3])==1)
		readop(self,filename);
	else
		writeop(self,filename);
	free(dest);
}