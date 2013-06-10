#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#else
#include <WinSock2.h>
#define close( f ) closesocket( f )
#endif


int main( int argc, char** argv )
{
	int flag;
	int size;
	int sock_client;
	const char* ip;
	int port;
	time_t current;

	char buffer[64];

	struct sockaddr_in  addr_server;

	if( argc < 2 )
	{
		printf( "please set server ip and port\n ");
		return 0;
	}

#ifdef WIN32
	WSADATA wsadata;
	flag = WSAStartup( 0x101, &wsadata );
	if( flag )
	{
		printf( "your windows socket setup wrong\n" );
		return 0;
	}

#endif


	ip = argv[ 1 ];
	port = atoi( argv[ 2 ] );

	printf( "ip:%s,port:%d\n", ip, port );


	
	addr_server.sin_family = AF_INET;
	addr_server.sin_port = htons( port );
	addr_server.sin_addr.s_addr = inet_addr( ip );

	while(1)
	{
		sock_client = socket( AF_INET, SOCK_STREAM, 0 );
		flag = connect( sock_client, ( struct sockaddr* ) &addr_server, sizeof( addr_server ) );
		if( flag < 0 )
		{
			printf( "your connect is not ok\n");
			close( sock_client );
			return 0;
		}

		sprintf(buffer, "%s", "username");
		sprintf(buffer + 32, "%s", "password" );

		buffer[31]=buffer[63] = 0;

		flag = send( sock_client, buffer, 64, 0 );

		printf( "send ok\n");

		flag = recv( sock_client, buffer, 64, 0 );
		if( flag != sizeof( time_t ) )
		{
			printf( "recv does not follow protocal\n");
			close( sock_client );
			continue;
		}

		memcpy( &current, buffer, sizeof( time_t ) );
		struct tm *ptm = localtime( &current );
		printf( "system time:%04d-%02d-%02d-%02d:%02d:%02d\n", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
			ptm->tm_hour, ptm->tm_min, ptm->tm_sec );
		printf( "ok,now we close connection\n" );
		close( sock_client );
	}
	return 0;
}
