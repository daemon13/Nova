//============================================================================
// Name        : MessageManager.h
// Copyright   : DataSoft Corporation 2011-2012
//	Nova is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   Nova is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with Nova.  If not, see <http://www.gnu.org/licenses/>.
// Description : Manages all incoming messages on sockets
//============================================================================

#include "MessageManager.h"
#include "../Lock.h"
#include "../Logger.h"
#include "messages/ErrorMessage.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include "event2/thread.h"

using namespace std;

namespace Nova
{

MessageManager *MessageManager::m_instance = NULL;

MessageManager::MessageManager()
{
	pthread_mutex_init(&m_endpointsMutex, NULL);
	pthread_mutex_init(&m_deleteEndpointMutex, NULL);
}

MessageManager &MessageManager::Instance()
{
	if(m_instance == NULL)
	{
		m_instance = new MessageManager();
	}
	return *MessageManager::m_instance;
}

Message *MessageManager::ReadMessage(Ticket &ticket, int timeout)
{
	//Read lock the Endpoint (so it can't get deleted from us while we're using it)
	MessageEndpointLock endpointLock = GetEndpoint(ticket.m_socketFD);

	if(endpointLock.m_endpoint == NULL)
	{
		return new ErrorMessage(ERROR_SOCKET_CLOSED);
	}

	Message *message = endpointLock.m_endpoint->PopMessage(ticket, timeout);
	if(message->m_messageType == ERROR_MESSAGE)
	{
		ErrorMessage *errorMessage = (ErrorMessage*)message;
		if(errorMessage->m_errorType == ERROR_SOCKET_CLOSED)
		{
			//TODO: Close the socket with libevent?
		}
	}

	return message;
}

bool MessageManager::WriteMessage(const Ticket &ticket, Message *message)
{
	if(ticket.m_socketFD == -1)
	{
		return false;
	}

	message->m_ourSerialNumber = ticket.m_ourSerialNum;
	message->m_theirSerialNumber = ticket.m_theirSerialNum;

	MessageEndpointLock endpointLock = GetEndpoint(ticket.m_socketFD);
	if(endpointLock.m_endpoint == NULL)
	{
		return false;
	}

	return endpointLock.m_endpoint->WriteMessage(message);
}

void MessageManager::StartSocket(int socketFD, struct bufferevent *bufferevent)
{
	//Initialize the MessageEndpoint if it doesn't yet exist
	Lock lock(&m_endpointsMutex);

	//If this socket doesn't even exist yet in the map, then it must be brand new
	if(m_endpoints.count(socketFD) == 0)
	{
		pthread_rwlock_t *rwLock = new pthread_rwlock_t;
		pthread_rwlock_init(rwLock, NULL);
		//Write lock this new lock because we don't want someone trying to read from it or write to it halfway through our addition
		Lock endLock(rwLock, WRITE_LOCK);
		m_endpoints[socketFD] = std::pair<MessageEndpoint*, pthread_rwlock_t*>( new MessageEndpoint(socketFD, bufferevent), rwLock);
		return;
	}

	//Write lock the message endpoint. Now we're allowed to access and write to it
	Lock endLock(m_endpoints[socketFD].second, WRITE_LOCK);

	if(m_endpoints[socketFD].first == NULL)
	{
		m_endpoints[socketFD].first = new MessageEndpoint(socketFD, bufferevent);
	}

}

Ticket MessageManager::StartConversation(int socketFD)
{
	Lock lock(&m_endpointsMutex);

	//If the endpoint doesn't exist, then it was closed. Just exit with failure
	if(m_endpoints.count(socketFD) == 0)
	{
		return Ticket();
	}

	Lock rwLock(m_endpoints[socketFD].second, READ_LOCK);
	if(m_endpoints[socketFD].first == NULL)
	{
		return Ticket();
	}

	return Ticket(m_endpoints[socketFD].first->StartConversation(), 0, false, false, socketFD, m_endpoints[socketFD].second);
}

void MessageManager::DeleteEndpoint(int socketFD)
{
	//Ensure that only one thread can be deleting an Endpoint at a time
	Lock functionLock(&m_deleteEndpointMutex);

	//Deleting the message endpoint requires that nobody else is using it!
	Lock lock(&m_endpointsMutex);
	if(m_endpoints.count(socketFD) > 0)
	{
		//First, shut down the endpoint, so as to signal anyone using it to clear out
		{
			Lock lock(m_endpoints[socketFD].second, READ_LOCK);
			if(m_endpoints[socketFD].first == NULL)
			{
				//If there is no endpoint, then just quit
				return;
			}
			m_endpoints[socketFD].first->Shutdown();
		}

		//We unlock and relock in order to prevent a deadlock here with ~Ticket
		pthread_rwlock_t *rwlock = m_endpoints[socketFD].second;
		pthread_mutex_unlock(&m_endpointsMutex);
		Lock lock(rwlock, WRITE_LOCK);
		pthread_mutex_lock(&m_endpointsMutex);

		delete m_endpoints[socketFD].first;

		m_endpoints[socketFD].first = NULL;
	}
}

bool MessageManager::RegisterCallback(int socketFD, Ticket &outTicket)
{
	MessageEndpointLock endpointLock = GetEndpoint(socketFD);
	if(endpointLock.m_endpoint != NULL)
	{
		return endpointLock.m_endpoint->RegisterCallback(outTicket);
	}

	return false;
}

std::vector <int>MessageManager::GetSocketList()
{
	Lock lock(&m_endpointsMutex);
	std::vector<int> sockets;

	std::map<int, std::pair<MessageEndpoint*, pthread_rwlock_t*>>::iterator it;
	for(it = m_endpoints.begin(); it != m_endpoints.end(); ++it)
	{
		//If the MessageEndpoint is NULL, don't count it
		if(it->second.first != NULL)
		{
			//Don't add the socket if the endpoint is shut down (is old)
			if(!it->second.first->m_isShutDown)
			{
				sockets.push_back(it->first);
			}
		}
	}

	return sockets;
}

MessageEndpointLock MessageManager::GetEndpoint(int socketFD)
{
	pthread_rwlock_t *endpointLock;

	//get the rw lock for the endpoint
	{
		Lock lock(&m_endpointsMutex);

		if(m_endpoints.count(socketFD) > 0)
		{
			endpointLock = m_endpoints[socketFD].second;
		}
		else
		{
			return MessageEndpointLock();
		}
	}

	pthread_rwlock_rdlock(endpointLock);
	return MessageEndpointLock( m_endpoints[socketFD].first, endpointLock);
}

void MessageManager::MessageDispatcher(struct bufferevent *bev, void *ctx)
{

	bufferevent_lock(bev);

	struct evbuffer *input = bufferevent_get_input(bev);
	evutil_socket_t socketFD = bufferevent_getfd(bev);

	uint32_t length = 0, evbufferLength;

	bool keepGoing = true;
	while(keepGoing)
	{
		evbufferLength = evbuffer_get_length(input);
		//If we don't even have enough data to read the length, just quit
		if(evbufferLength < sizeof(length))
		{
			keepGoing = false;
			continue;
		}

		//Copy the length field out of the message
		//	We only want to copy this data at first, because if the whole message hasn't reached us,
		//	we'll want the whole buffer still present here, undrained
		if(evbuffer_copyout(input, &length, sizeof(length)) != sizeof(length))
		{
			keepGoing = false;
			continue;
		}

		evbuffer_drain(input, sizeof(length));

		// Make sure the length appears valid
		// TODO: Assign some arbitrary max message size to avoid filling up memory by accident
		if(length < MESSAGE_MIN_SIZE)
		{
			LOG(WARNING, "Error parsing message: message too small.", "");
			evbuffer_drain(input, sizeof(length));
			keepGoing = false;
			continue;
		}

		//If we don't yet have enough data, then just quit and wait for more
		if(evbufferLength < length)
		{
			keepGoing = false;
			continue;
		}

		//Remove the length of the "length" variable itself
		length -= sizeof(length);
		char *buffer = (char*)malloc(length);

		if(buffer == NULL)
		{
			// This should never happen. If it does, probably because length is an absurd value (or we're out of memory)
			LOG(WARNING, "Error parsing message: malloc returned NULL. Out of memory?.", "");
			free(buffer);
			keepGoing = false;
			continue;
		}

		// Read in the actual message
		int bytesRead = evbuffer_remove(input, buffer, length);
		if(bytesRead == -1)
		{
			LOG(WARNING, "Error parsing message: couldn't remove data from buffer.", "");
		}
		else if((uint32_t)bytesRead != length)
		{
			LOG(WARNING, "Error parsing message: incorrect amount of data received than what expected.", "");
		}

		MessageEndpointLock endpoint = MessageManager::Instance().GetEndpoint(socketFD);
		if(endpoint.m_endpoint != NULL)
		{
			Message *message = Message::Deserialize(buffer, length);
			if(!endpoint.m_endpoint->PushMessage(message))
			{
				LOG(DEBUG, "Discarding message. Error in pushing it to a queue.", "");
			}
		}
		else
		{
			LOG(DEBUG, "Discarding message. Received it for a non-existent endpoint.", "");
		}

		free(buffer);
	}

	bufferevent_unlock(bev);
}

void MessageManager::ErrorDispatcher(struct bufferevent *bev, short error, void *ctx)
{
	if(error & BEV_EVENT_CONNECTED)
	{
		LOG(DEBUG, "New connection established", "");
		return;
	}

	if(error & (BEV_EVENT_EOF | BEV_EVENT_ERROR))
	{
		evutil_socket_t socketFD = bufferevent_getfd(bev);
		MessageManager::Instance().DeleteEndpoint(socketFD);
		return;
	}
}

void MessageManager::DoAccept(evutil_socket_t listener, short event, void *arg)
{
	struct CallbackArg *cbArg = (struct CallbackArg *)arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr*)&ss, &slen);
    if(fd < 0)
    {
    	LOG(ERROR, "Failed to connect to UI", "accept: " + string(strerror(errno)));
    }
    else
    {
		struct bufferevent *bev;
		evutil_make_socket_nonblocking(fd);
		bev = bufferevent_socket_new(cbArg->m_base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
		if(bev == NULL)
		{
			LOG(ERROR, "Failed to connect to UI: socket_new", "");
			return;
		}
		bufferevent_setcb(bev, MessageDispatcher, NULL, ErrorDispatcher, NULL);
		bufferevent_setwatermark(bev, EV_READ, 0, 0);
		if(bufferevent_enable(bev, EV_READ|EV_WRITE) == -1)
		{
			LOG(ERROR, "Failed to connect to UI: bufferevent_enable", "");
			return;
		}

		//Create the socket within the messaging subsystem
		//MessageManager::Instance().StartSocket(fd);
		//Start the callback thread for this new connection
		cbArg->m_callback->StartServerCallbackThread(fd, bev);
    }
}

void *MessageManager::AcceptDispatcher(void *arg)
{
	int len;
	string inKeyPath = Config::Inst()->GetPathHome() + "/config/keys" + NOVAD_LISTEN_FILENAME;
	evutil_socket_t IPCParentSocket;
	struct event_base *base;
	struct event *listener_event;
	struct sockaddr_un msgLocal;

	evthread_use_pthreads();
	base = event_base_new();
	if (!base)
	{
		return NULL;
	}

	if((IPCParentSocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	{
		LOG(ERROR, "Failed to connect to UI", "socket: "+string(strerror(errno)));
		return NULL;
	}

	evutil_make_socket_nonblocking(IPCParentSocket);

	msgLocal.sun_family = AF_UNIX;
	memset(msgLocal.sun_path, '\0', sizeof(msgLocal.sun_path));
	strncpy(msgLocal.sun_path, inKeyPath.c_str(), inKeyPath.length());
	unlink(msgLocal.sun_path);
	len = strlen(msgLocal.sun_path) + sizeof(msgLocal.sun_family);

	if(::bind(IPCParentSocket, (struct sockaddr *)&msgLocal, len) == -1)
	{
		LOG(ERROR, "Failed to connect to UI", "bind: "+string(strerror(errno)));
		close(IPCParentSocket);
		return NULL;
	}

	if(listen(IPCParentSocket, SOMAXCONN) == -1)
	{
		LOG(ERROR, "Failed to connect to UI", "listen: "+string(strerror(errno)));
		close(IPCParentSocket);
		return NULL;
	}

	struct CallbackArg *cbArg = new struct CallbackArg;
	cbArg->m_base = base;
	cbArg->m_callback = (ServerCallback*)arg;

	listener_event = event_new(base, IPCParentSocket, EV_READ|EV_PERSIST, DoAccept, (void*)cbArg);
	event_add(listener_event, NULL);
	event_base_dispatch(base);

	return NULL;
}

void MessageManager::StartServer(ServerCallback *callback)
{
	static bool hasServerStarted = false;

	if(!hasServerStarted)
	{
		pthread_create(&m_acceptThread, NULL, AcceptDispatcher, callback);
		hasServerStarted = true;
	}
}

}
