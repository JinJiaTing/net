#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#else
#include <WinSock2.h>
#define close( f ) closesocket( f )
#endif

struct my_event_s
{
	int fd;
	char recv[64];
	char send[64];

	int rc_pos;
	int sd_pos;
};

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
	int e_num;
	int my_empty_index;
	int i,j;
	int event_flag;
#define EPOLL_MAX 51200
	struct epoll_event wait_events[ EPOLL_MAX ];
	struct my_event_s my_event[ EPOLL_MAX ];
	struct my_event_s* tobe_myevent;
	struct epoll_event tobe_event;
	int epfd;

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
	flag = fcntl( sock_server, F_GETFL, 0 );
	fcntl( sock_server, F_SETFL, O_NONBLOCK );

	flag = bind( sock_server, ( struct sockaddr* )&addr_server, sizeof( struct sockaddr ) );
	if( flag < 0 )
	{
		printf( "your bind is not ok\n" );
		close( sock_server );
		return 0;
	}

	flag = listen( sock_server, 1024 );
	if( flag < 0 )
	{
		printf( "your listen is not ok\n");
		close( sock_server );
		return 0;
	}

	
	epfd = epoll_create( EPOLL_MAX );
	if( epfd <= 0 )
	{
		printf( "event module could not be setup\n");
		close( sock_server );
		return 0;
	}

	tobe_event.events = EPOLLIN;
	tobe_event.data.fd = sock_server;

	epoll_ctl( epfd,  EPOLL_CTL_ADD, sock_server,  &tobe_event );


	size = sizeof( addr_client );
	num = 0;
	last = 0;
	my_empty_index = 0;

	while(1)
	{
#define WAIT_TIME_OUT 600
		e_num = epoll_wait( epfd, wait_events, EPOLL_MAX, WAIT_TIME_OUT );
		if( e_num <= 0 )
		{
			continue;
		}

		for( i = 0; i < e_num; ++i )
		{
			if( sock_server == wait_events[ i ].data.fd )
			{
				while(1)
				{
					sock_client = accept( sock_server, ( struct sockaddr* )&addr_client, ( socklen_t*)&size );
					if( sock_client < 0 )
					{
						if( errno == EAGAIN )
						{
							break;
						}
						if( errno == EINTR )
						{
							continue;
						}
						break;
					}

					tobe_myevent = my_event + my_empty_index;
					memset( tobe_myevent, 0, sizeof( struct my_event_s ) );
					tobe_myevent->fd = sock_client;

					flag = fcntl( sock_client, F_GETFL, 0 );
					fcntl( sock_client, F_SETFL, flag );

					tobe_event.events = EPOLLIN | EPOLLET;
					tobe_event.data.u32 = my_empty_index;

					epoll_ctl( epfd, EPOLL_CTL_ADD, sock_client, &tobe_event );

					
					for( j = my_empty_index + 1; j < EPOLL_MAX; ++j )
					{
						if( !my_event[ j ].fd )
						{
							my_empty_index = j;
							break;
						}
					}
					
					if( my_event[j].fd )
					{
						for( j = 0; j < EPOLL_MAX; ++j )
						{
							if( !my_event[ j ].fd )
							{
								my_empty_index = j;
								break;
							}
						}

						if( my_event[ j ].fd )
						{
							printf( "your events has been none else\n");
							close( sock_client );
							close( sock_server );
							return 0;
						}
					}

					++num;
					current = time( 0 );
					if( current > last )
					{
						printf( "last sec qps:%d\n", num );
						num = 0;
						last = current;
					}

					memcpy( tobe_myevent->send, &current, sizeof(time_t) );
					
					flag = recv( sock_client, tobe_myevent->recv, 64, 0 );
					if( flag < 64 )
					{
						if( flag > 0 )
							tobe_myevent->rc_pos += flag;
						continue;
					}

					if( tobe_myevent->recv[31] || tobe_myevent->recv[63] )
					{
						printf( "your recv does follow the protocal\n");
						tobe_myevent->fd = 0;
						close( sock_client );
						continue;
					}
					

					flag = send( sock_client, tobe_myevent->send, sizeof( time_t ), 0 );
					if( flag < sizeof( time_t ) )
					{
						tobe_event.events =  EPOLLET | EPOLLOUT;
						epoll_ctl( epfd, EPOLL_CTL_MOD, sock_client, &tobe_event );
						if( flag > 0 )
							tobe_myevent->sd_pos += flag;
						continue;
					}
					tobe_myevent->fd = 0;
					close( sock_client );

				}
				
			}
			else
			{
				tobe_myevent = my_event + wait_events[ i ].data.u32;
				sock_client = tobe_myevent->fd;
				event_flag = wait_events[ i ].events;

				if( event_flag | EPOLLHUP )
				{
					tobe_myevent->fd = 0;
					close( sock_client );
					continue;
				}
				else if( event_flag | EPOLLERR )
				{
					tobe_myevent->fd = 0;
					close( sock_client );
					continue;
				}
				else if( event_flag | EPOLLOUT )
				{
					if( tobe_myevent->rc_pos != 64 )
					{
						continue;
					}

					if( tobe_myevent->sd_pos >= sizeof( time_t ) )
					{
						tobe_myevent->fd = 0;
						close( sock_client );
						continue;
					}

					flag = send( sock_client, tobe_myevent->send + tobe_myevent->sd_pos, sizeof( time_t ) - tobe_myevent->sd_pos, 0 );
					if( flag < 0 )
					{
						if( errno == EAGAIN )
						{
							continue;
						}
						else if( errno == EINTR )
						{
							continue;
						}
						tobe_myevent->fd = 0;
						close( sock_client );
						continue;
					}

					if( flag >0 )
					{
						tobe_myevent->sd_pos += flag;
						if( tobe_myevent->sd_pos >= sizeof( time_t ) )
						{
							tobe_myevent->fd = 0;
							close( sock_client );
							continue;
						}
					}
				}
				if( event_flag | EPOLLIN )
				{
					if( tobe_myevent->rc_pos < 64 )
					{
						flag = recv( sock_client, tobe_myevent->recv + tobe_myevent->rc_pos, 64 - tobe_myevent->rc_pos, 0 );
						if( flag <= 0 )
						{
							continue;
						}

						tobe_myevent->rc_pos += flag;

						if( tobe_myevent->rc_pos < 64 )
						{
							continue;
						}

						if( tobe_myevent->recv[31] || tobe_myevent->recv[63] )
						{
							printf( "your recv does follow the protocal\n");
							tobe_myevent->fd = 0;
							close( sock_client );
							continue;
						}

						flag = send( sock_client, tobe_myevent->send, sizeof( time_t ), 0 );
						if( flag < sizeof( time_t ) )
						{
							if( flag > 0 )
								tobe_myevent->sd_pos += flag;
							tobe_event.events = EPOLLET | EPOLLOUT;
							tobe_event.data.u32 = wait_events[i].data.u32;
							epoll_ctl( epfd, EPOLL_CTL_MOD, sock_client, &tobe_event );
							continue;
						}
						tobe_myevent->fd = 0;
						close( sock_client );
					}

				}
			}


		}
		
	}
	printf( "close server connection\n");
	close( sock_server );

	return 0;
	
}
