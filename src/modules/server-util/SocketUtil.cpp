/*
 * SocketUtil.cpp
 *
 *  Created on: 03-Dec-2014
 *      Author: sumeetc
 */

#include "SocketUtil.h"

SocketUtil::SocketUtil(const SOCKET& fd) {
	lock.lock();
	inited = false;
	init(fd);
	lock.unlock();
}

void SocketUtil::init(const SOCKET& fd) {
	if(inited) {
		return;
	}
	this->fd = fd;
	logger = LoggerFactory::getLogger("SocketUtil");
	if(SSLHandler::getInstance()->getIsSSL())
	{
		sbio=BIO_new_socket(fd,BIO_NOCLOSE);
		ssl=SSL_new(SSLHandler::getInstance()->getCtx());
		SSL_set_bio(ssl,sbio,sbio);

		io=BIO_new(BIO_f_buffer());
		ssl_bio=BIO_new(BIO_f_ssl());
		BIO_set_ssl(ssl_bio,ssl,BIO_CLOSE);
		BIO_push(io,ssl_bio);

		int r = SSL_accept(ssl);
		if(r<=0)
		{
			logger << "SSL accept error" << endl;
			closeSocket(false);
			return;
		}
		SSLHandler::getInstance()->isValid = true;

		if (SSLHandler::getInstance()->securityProperties.client_auth==2 || SSLHandler::getInstance()->securityProperties.client_auth==1)
		{
			X509* client_cert = NULL;
			/* Get the client's certificate (optional) */
			client_cert = SSL_get_peer_certificate(ssl);
			if (client_cert != NULL)
			{
				printf ("Client certificate:\n");
				char* str = X509_NAME_oneline(X509_get_subject_name(client_cert), 0, 0);
				if(str == NULL)
				{
					logger << "Could not get client certificate subject name" << endl;
					closeSocket(false);
					return;
				}
				printf ("\t subject: %s\n", str);
				free (str);
				str = X509_NAME_oneline(X509_get_issuer_name(client_cert), 0, 0);
				if(str == NULL)
				{
					logger << "Could not get client certificate issuer name" << endl;
					closeSocket(false);
					return;
				}
				printf ("\t issuer: %s\n", str);
				free (str);
				X509_free(client_cert);
			}
			else
			{
				logger << ("The SSL client does not have certificate.\n") << endl;
			}
		}
	}
	else
	{
		sbio=BIO_new_socket(fd,BIO_CLOSE);
		io=BIO_new(BIO_f_buffer());
		BIO_push(io,sbio);
	}
	closed = false;
	inited = true;
}

SocketUtil::~SocketUtil() {
	// TODO Auto-generated destructor stub
}

int SocketUtil::writeData(const string& data, const bool& flush, const int& offset/* = 0*/)
{
	if(data.length()>0)
	{
		if(SSLHandler::getInstance()->getIsSSL())
		{
			lock.lock();
			bool fl = handleRenegotiation();
			lock.unlock();
			if(fl)
			{
				return 0;
			}
		}
		lock.lock();
		int er = 0;
		if(!closed)
		{
			er = BIO_write(io, &data[offset] , data.length()-offset);
			if(SSLHandler::getInstance()->getIsSSL())
			{
				er = handleSSlErrors(er)?0:er;
			}
			else if(er<=0)
			{
				if(!BIO_should_retry(io))
				{
					closeSocket(false);
					er = 0;
				}
				else
				{
					er = -1;
				}
			}
			if(er>0 && flush)
			{
				this->flush(false);
			}
		}
		lock.unlock();
		return er;
	}
	return -1;
}

bool SocketUtil::flush(const bool& lk) {
	bool fl = false;
	if(lk)lock.lock();
	if(!closed && BIO_flush(io)<=0 && !BIO_should_retry(io))
	{
		logger << "Error flushing BIO" << endl;
		closeSocket(lk);
		fl = true;
	}
	if(lk)lock.unlock();
	return fl;
}

int SocketUtil::readLine(string& line)
{
	char buf[MAXBUFLENM];
	memset(buf, 0, sizeof(buf));
	if(SSLHandler::getInstance()->getIsSSL())
	{
		int er = 0;
		lock.lock();
		if(!closed)
		{
			er = BIO_gets(io,buf,BUFSIZZ-1);
			er = handleSSlErrors(er)?0:er;
			if(er<=0)
			{
				lock.unlock();
				return er;
			}
			line.append(buf, er);
			memset(&buf[0], 0, sizeof(buf));
		}
		lock.unlock();
		return er;
	}
	else
	{
		int er = 0;
		lock.lock();
		if(!closed)
		{
			er = BIO_gets(io,buf,BUFSIZZ-1);
			if(er<=0)
			{
				if(!BIO_should_retry(io))
				{
					closeSocket(false);
					er = 0;
				}
				else
				{
					er = -1;
				}
				lock.unlock();
				return er;
			}
			line.append(buf, er);
			memset(&buf[0], 0, sizeof(buf));
		}
		lock.unlock();
		return er;
	}
}

int SocketUtil::readData(int cntlen, string& content)
{
	if(cntlen>0)
	{
		char buf[MAXBUFLENM];
		memset(buf, 0, sizeof(buf));
		if(SSLHandler::getInstance()->getIsSSL())
		{
			int er = 0;
			lock.lock();
			if(!closed)
			{
				int readLen = MAXBUFLENM;
				if(cntlen<MAXBUFLENM)
					readLen = cntlen;
				er = BIO_read(io,buf,readLen);
				er = handleSSlErrors(er)?0:er;
				if(er<=0)
				{
					lock.unlock();
					return er;
				}
				content.append(buf, er);
				memset(&buf[0], 0, sizeof(buf));
			}
			lock.unlock();
			return er;
		}
		else
		{
			int er = 0;
			lock.lock();
			if(!closed)
			{
				int readLen = MAXBUFLENM;
				if(cntlen<MAXBUFLENM)
					readLen = cntlen;
				er = BIO_read(io,buf,readLen);
				if(er<=0)
				{
					if(!BIO_should_retry(io))
					{
						closeSocket(false);
						er = 0;
					}
					else
					{
						er = -1;
					}
					lock.unlock();
					return er;
				}
				cntlen -= er;
				content.append(buf, er);
				memset(&buf[0], 0, sizeof(buf));
			}
			lock.unlock();
			return er;
		}
	}
	return cntlen;
}

int SocketUtil::readData(int cntlen, vector<unsigned char>& content)
{
	if(cntlen>0)
	{
		char buf[MAXBUFLENM];
		memset(buf, 0, sizeof(buf));
		if(SSLHandler::getInstance()->getIsSSL())
		{
			int er = 0;
			lock.lock();
			if(!closed)
			{
				int readLen = MAXBUFLENM;
				if(cntlen<MAXBUFLENM)
					readLen = cntlen;
				er = BIO_read(io,buf,readLen);
				er = handleSSlErrors(er)?0:er;
				if(er<=0)
				{
					lock.unlock();
					return er;
				}
				for(int cc=0;cc<er;cc++)
				{
					content.push_back(buf[cc]);
				}
				memset(&buf[0], 0, sizeof(buf));
			}
			lock.unlock();
			return er;
		}
		else
		{
			int er = 0;
			lock.lock();
			if(!closed)
			{
				int readLen = MAXBUFLENM;
				if(cntlen<MAXBUFLENM)
					readLen = cntlen;
				er = BIO_read(io,buf,readLen);
				if(er<=0)
				{
					if(!BIO_should_retry(io))
					{
						closeSocket(false);
						er = 0;
					}
					else
					{
						er = -1;
					}
					lock.unlock();
					return er;
				}
				cntlen -= er;
				for(int cc=0;cc<er;cc++)
				{
					content.push_back(buf[cc]);
				}
				memset(&buf[0], 0, sizeof(buf));
			}
			lock.unlock();
			return er;
		}
	}
	return cntlen;
}

void SocketUtil::closeSocket(const bool& lk)
{
	if(lk)lock.lock();
	if(!closed)
	{
		if(SSLHandler::getInstance()->getIsSSL())
		{
			SSLHandler::getInstance()->closeSSL(fd,ssl,io);
		}
		else
		{
			if(io!=NULL)BIO_free_all(io);
			close(fd);
		}
	}
	closed = true;
	if(lk)lock.unlock();
}

bool SocketUtil::checkSocketWaitForTimeout(const int& writing, const int& seconds, const int& micros)
{
	#ifdef OS_MINGW
		u_long iMode = 1;
		ioctlsocket(fd, FIONBIO, &iMode);
	#else
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
	#endif

	fd_set rset, wset;
	struct timeval tv = {seconds, micros};
	int rc;

	/* Guard against closed socket */
	if (fd < 0)
	{
		return false;
	}

	/* Construct the arguments to select */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	wset = rset;

	/* See if the socket is ready */
	switch (writing)
	{
		case 0:
			rc = select(fd+1, &rset, NULL, NULL, &tv);
			break;
		case 1:
			rc = select(fd+1, NULL, &wset, NULL, &tv);
			break;
		case 2:
			rc = select(fd+1, &rset, &wset, NULL, &tv);
			break;
	}
	FD_CLR(fd, &rset);
	#ifdef OS_MINGW
		u_long bMode = 0;
		ioctlsocket(fd, FIONBIO, &bMode);
	#else
		fcntl(fd, F_SETFL, O_SYNC);
	#endif

	FD_ZERO(&rset);
	FD_ZERO(&wset);
	/* Return SOCKET_TIMED_OUT on timeout, SOCKET_OPERATION_OK
	otherwise
	(when we are able to write or when there's something to
	read) */
	return rc <= 0 ? false : true;
}

SocketUtil::SocketUtil() {
	closed = true;
	ssl = NULL;
	io = NULL;
	ssl_bio = NULL;
	fd = -1;
	sbio = NULL;
	inited = false;
}

bool SocketUtil::isBlocking()
{
	int flags = fcntl(fd, F_GETFL, 0);
	return (flags&O_NONBLOCK)!=O_NONBLOCK;
}

bool SocketUtil::handleSSlErrors(const int& er)
{
	int sslerr = SSL_get_error(ssl,er);
	bool flag = false;
	switch(sslerr)
	{
		case SSL_ERROR_WANT_READ:
		{
			logger << "ssl more to read error" << endl;
			break;
		}
		case SSL_ERROR_WANT_WRITE:
		{
			logger << "ssl more to write error" << endl;
			break;
		}
		case SSL_ERROR_NONE:
		{
			break;
		}
		case SSL_ERROR_WANT_X509_LOOKUP:
		{
			logger << "SSL_ERROR_WANT_X509_LOOKUP" << endl;
			break;
		}
		case SSL_ERROR_SYSCALL:
		{
			ERR_print_errors_fp(stderr);
			perror("syscall error: ");
			closeSocket(false);
			flag = true;
			break;
		}
		case SSL_ERROR_SSL:
		{
			ERR_print_errors_fp(stderr);
			logger << "SSL_ERROR_SSL" << endl;
			break;
		}
		case SSL_ERROR_ZERO_RETURN:
		{
			closeSocket(false);
			flag = true;
			break;
		}
		default:
		{
			logger << "SSL read problem" << endl;
			closeSocket(false);
			flag = true;
		}
	}
	return flag;
}

bool SocketUtil::handleRenegotiation()
{
	if(SSLHandler::getInstance()->securityProperties.client_auth==CLIENT_AUTH_REHANDSHAKE)
	{
		SSL_set_verify(ssl,SSL_VERIFY_PEER |
				SSL_VERIFY_FAIL_IF_NO_PEER_CERT,0);

		/* Stop the client from just resuming the
			 un-authenticated session */
		SSL_set_session_id_context(ssl,
				(const unsigned char*)&SSLHandler::s_server_auth_session_id_context,
				sizeof(SSLHandler::s_server_auth_session_id_context));

		if(SSL_renegotiate(ssl)<=0)
		{
			logger << "SSL renegotiation error" << endl;
			closeSocket(false);
			return true;
		}
		if(SSL_do_handshake(ssl)<=0)
		{
			logger << "SSL renegotiation error" << endl;
			closeSocket(false);
			return true;
		}
		ssl->state = SSL_ST_ACCEPT;
		if(SSL_do_handshake(ssl)<=0)
		{
			logger << "SSL handshake error" << endl;
			closeSocket(false);
			return true;
		}
	}
	return false;
}

string SocketUtil::getAlpnProto() {
	return SSLHandler::getAlpnProto(fd);
}
