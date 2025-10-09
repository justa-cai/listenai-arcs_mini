
#include "HTTPClientWrapper.h"
#include	"lwip/netdb.h"
// #include "os_time.h"
#include "rtos_al.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : Stdc: HTTPWrapperIsAscii
// Last updated : 15/05/2005
// Notes	    : Same as stdc: isascii
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperIsAscii(int c)
{
        return (!(c & ~0177));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : Stdc: HTTPWrapperToUpper
// Last updated : 15/05/2005
// Notes	    : Convert character to uppercase.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperToUpper(int c)
{
        // -32
        if(HTTPWrapperIsAscii(c) > 0)
        {
                if(c >= 97 && c <= 122)
                {
                        return (c - 32);
                }
        }

        return c;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : Stdc: HTTPWrapperToLower
// Last updated : 13/06/2006
// Notes	       : Convert character to lowercase.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperToLower(int c)
{
        // +32
        if(HTTPWrapperIsAscii(c) > 0)
        {
                if(c >= 65 && c <= 90)
                {
                        return (c + 32);
                }
        }

        return c;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : Stdc: isalpha
// Last updated : 15/05/2005
// Notes	    : returns nonzero if c is a particular representation of an alphabetic character
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperIsAlpha(int c)
{

        if(HTTPWrapperIsAscii(c) > 0)
        {
                if( (c >= 97 && c <= 122) || (c >= 65 && c <= 90))
                {
                        return c;
                }
        }
        return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : Stdc: isalnum
// Last updated : 15/05/2005
// Notes	    : returns nonzero if c is a particular representation of an alphanumeric character
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperIsAlNum(int c)
{
        if(HTTPWrapperIsAscii(c) > 0)
        {

                if(HTTPWrapperIsAlpha(c) > 0)
                {
                        return c;
                }

                if( c >= 48 && c <= 57)
                {
                        return c;
                }

        }
        return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : HTTPWrapper_itoa
// Last updated : 15/05/2005
// Notes	    : same as stdc itoa() // hmm.. allmost the same
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* HTTPWrapperItoa(char *s,int a)
{
        unsigned int b;

        if(a > 2147483647)
        {
                return 0; // overflow
        }

        if (a < 0) b = -a, *s++ = '-';
        else b = a;
        for(;a;a=a/10) s++;
        for(*s='\0';b;b=b/10) *--s=b%10+'0';
        return s;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : HTTPWrapper_ShutDown
// Last updated : 15/05/2005
// Notes	    : Handles parameter changes in the socket shutdown()
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperShutDown (int s,int how)
{
        return shutdown(s,how);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : HTTPWrapper_GetSocketError
// Last updated : 15/05/2005
// Notes	    : WSAGetLastError Wrapper (Win32 Specific)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperGetSocketError (int s)
{
#ifdef _WIN32
        return WSAGetLastError();
#else
        return HTTPC_ERRNO;
#endif
}

static int resolve_dns(const char *host, struct sockaddr *ip, size_t *addrlen)
{
	struct addrinfo hints;
	struct addrinfo *res;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int st = getaddrinfo(host, NULL, &hints, &res);
	if (st) {
		return -1;
	}
	memcpy((struct sockaddr *)ip, (struct sockaddr *)res->ai_addr, sizeof(struct sockaddr));
	*addrlen = res->ai_addrlen;
	freeaddrinfo(res);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : HTTPWrapper_GetHostByName
// Last updated : 15/05/2005
// Notes	    : gethostbyname for Win32
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned long HTTPWrapperGetHostByName(char *name,unsigned long *address)
{
        HTTP_HOSTNET     *HostEntry;
        int     iPos = 0, iLen = 0,iNumPos = 0,iDots =0;
        long    iIPElement;
        char    c = 0;
        char    Num[4];
        int     iHostType = 0; // 0 : numeric IP

        // Check if the name is an IP or host
        iLen = strlen(name);
        for(iPos = 0; iPos <= iLen;iPos++)
        {
                c = name[iPos];
                if((c >= 48 && c <= 57)  || (c == '.') )
                {
                        // c is numeric or dot
                        if(c != '.')
                        {
                                // c is numeric
                                if(iNumPos > 3)
                                {
                                        iHostType++;
                                        break;
                                }
                                Num[iNumPos] = c;
                                Num[iNumPos + 1] = 0;
                                iNumPos ++;
                        }
                        else
                        {
                                iNumPos = 0;
                                iDots++;
                                iIPElement = atol(Num);
                                if(iIPElement > 256 || iDots > 3)
                                {
                                        return 0; // error invalid IP
                                }
                        }
                }
                else
                {

                        break; // this is an alpha numeric address type
                }
        }

        if(c == 0 && iHostType == 0 && iDots == 3)
        {
                iIPElement = atol(Num);
                if(iIPElement > 256)
                {
                        return 0; // error invalid IP
                }
        }
        else
        {
                iHostType++;
        }

        if(iHostType > 0)
        {
			struct sockaddr remote_ip = {0};
			size_t addrlen = -1;
			memset(&remote_ip, 0, sizeof(struct sockaddr));
			if (resolve_dns(name, &remote_ip, &addrlen) < 0) {
				return 1;
			}
			*address = ((struct sockaddr_in *)(&remote_ip))->sin_addr.s_addr;
			return 0;
        }

        else // numeric address - no need for DNS resolve
        {
                *(address) = inet_addr(name);
                return 1;

        }
        return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : HTTPWrapper_GetRandomeNumber
// Last updated : 15/05/2005
// Notes	    : GetRandom number for Win32
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HTTPWrapperInitRandomeNumber()
{
        // srand((unsigned int)time(NULL));
}

int HTTPWrapperGetRandomeNumber()
{
        // int num;
        // num = (int)(((double) rand()/ ((double)RAND_MAX+1)) * 16);
        // return num;
		return (int) random ();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : HTTPWrapper_GetRTC
// Last updated : 15/05/2005
// Author Name	: Eitan Michaelson
// Notes	    : Get uptime under Win32 & AMT
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

long HTTPWrapperGetUpTime()
{
#ifdef _WIN32

        long lTime = 0;

        lTime = (GetTickCount() / CLOCKS_PER_SEC);
        return lTime;

#elif _LINUX

        struct timespec tp;

        clock_gettime(CLOCK_MONOTONIC , &tp);
        return tp.tv_sec;
#else
        return (long)(rtos_now(false) / 1000U);
#endif
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Section      : TSL Wrapper
// Last updated : 15/05/2005
// Notes	    : HTTPWrapper_Sec_Connect
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef HTTPC_SSL
int HTTPWrapperSSLConnect(HTTP_SSL *handle, int s,const struct sockaddr *name,int namelen,char *hostname)
{
        return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperSSLNegotiate(HTTP_SSL *handle, int s,const struct sockaddr *name,int namelen,char *hostname)
{
        return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperSSLSend(HTTP_SSL *handle, int s,char *buf, int len,int flags)
{
        return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int HTTPWrapperSSLRecv(HTTP_SSL *handle, int s,char *buf, int len,int flags)
{
        return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HTTPWrapperSSLSetRecvTimeout(HTTP_SSL *handle, int s)
{
        // return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int HTTPWrapperSSLRecvPending(HTTP_SSL *handle, int s)
{
        return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int HTTPWrapperSSLClose(HTTP_SSL *handle, int s)
{
        return -1;

}
#else
#include "HTTPMbedTLSWrapper.h"
#endif
