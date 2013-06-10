#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
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
	int port;
	int flag;
	int size;
	int sock_server;
	int sock_client;
	time_t current;
	time_t last;
	int num;
#define RECV_BUF_LEN 256
	char buffer[ RECV_BUF_LEN ];

	struct sockaddr_in addr_server;
	struct sockaddr_in addr_client;

	if( argc <= 1 )
	{
		printf( "please set your port\n" );
		return 0;
	}

	printf( "your port:%s\n", argv[1] );

#ifdef WIN32
		WSADATA wsadata;
		flag = WSAStartup( 0x101, &wsadata );
		if( flag )
		{
			printf( "your windows socket setup wrong\n" );
			return 0;
		}

#endif

	port = atoi(argv[1]);
	
	addr_server.sin_family = AF_INET;
	addr_server.sin_port = htons( port );
	addr_server.sin_addr.s_addr = htonl( INADDR_ANY );



	sock_server = socket( AF_INET, SOCK_STREAM, 0 );
	flag = bind( sock_server, ( struct sockaddr* )&addr_server, sizeof( struct sockaddr ) );
	if( flag < 0 )
	{
		printf( "your bind is not ok\n" );
		close( sock_server );
		return 0;
	}

	flag = listen( sock_server, 50 );
	if( flag < 0 )
	{
		printf( "your listen is not ok\n");
		close( sock_server );
		return 0;
	}

	size = sizeof( addr_client );
	last = 0;
	num = 0;
	while(1)
	{
		sock_client = accept( sock_server, ( struct sockaddr* )&addr_client, &size );
		if( sock_client <=0 )
		{
			printf( "your accept is no ok\n");
			close( sock_server );
			return 0;
		}

		++num;
		current = time( 0 );
		if( current > last )
		{
			printf( "last sec qps:%d\n", num );
			num = 0;
			last = current;
		}


		flag = recv( sock_client, buffer, RECV_BUF_LEN, 0 );
		if( flag <= 0 )
		{
			printf( "your recv is no ok\n");
			close( sock_client );
			continue;
		}

		if( flag != 64 )
		{
			printf( "your recv does follow the protocal\n");
			close( sock_client );
			continue;
		}

		if( buffer[31] || buffer[63] )
		{
			printf( "your recv does follow the protocal\n");
			close( sock_client );
			continue;
		}

		current = time(0);
		send( sock_client, ( const char* )&current, sizeof( time_t), 0 );

		printf( "your connection is ok\n");
		printf( "now close your connection\n");
		close( sock_client );

	}

	printf( "close server connection\n");
	close( sock_server );

	return 0;
	
}
