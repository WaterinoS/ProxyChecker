#include "CBotProxy.h"

int proxy_timeout_delay = 5000;

bool CBotProxy::Connect() {

    if (m_sockTCP != INVALID_SOCKET) {
        closesocket(m_sockTCP);
    }

    m_sockTCP = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockTCP == INVALID_SOCKET) {
        m_Status = PROXY_FAILED;
        return false;
    }

    m_Status = PROXY_CONNECTING;

    sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = m_proxyIP;
    sa.sin_port = htons(m_proxyPort);

	if (setsockopt(m_sockTCP, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&proxy_timeout_delay), sizeof proxy_timeout_delay) == SOCKET_ERROR) {
		m_Status = PROXY_FAILED;
		return false;
	}
    
	if (setsockopt(m_sockTCP, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&proxy_timeout_delay), sizeof proxy_timeout_delay) == SOCKET_ERROR) {
		m_Status = PROXY_FAILED;
		return false;
	}

    if (connect(m_sockTCP, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == SOCKET_ERROR) {
        m_Status = PROXY_FAILED;
        //g_pErrorLog->Log("[CBotProxy::Connect] Proxy %s failed to connect. (0x0)", GetIP().c_str());
        return false;
    }

    BYTE authMethods[2] = { 0x00, 0x02 };
    int authMethodsCount = m_username.empty() ? 1 : 2;
    SOCKS5::AuthRequestHeader ahead = { 5, static_cast<BYTE>(authMethodsCount), {0} };
    memcpy(ahead.byteMethods, authMethods, authMethodsCount);
    send(m_sockTCP, reinterpret_cast<char*>(&ahead), sizeof(ahead) + (authMethodsCount - 1), 0);

    SOCKS5::AuthRespondHeader arhead{};
    ZeroMemory(&arhead, sizeof(SOCKS5::AuthRespondHeader));

	auto result = recv(m_sockTCP, reinterpret_cast<char*>(&arhead), sizeof(SOCKS5::AuthRespondHeader), 0);
    if (result == SOCKET_ERROR || arhead.byteVersion != 5) {
        closesocket(m_sockTCP);
        m_Status = PROXY_FAILED;
       // g_pErrorLog->Log("[CBotProxy::Connect] Proxy %s failed to connect. (%x, Last Err. %x)", GetIP().c_str(), result, WSAGetLastError());
        return false;
    }

    if (arhead.byteAuthMethod == 0x02) {
        if (!Authenticate()) {
            closesocket(m_sockTCP);
            m_Status = PROXY_FAILED;
            return false;
        }
    }
    else if (arhead.byteAuthMethod != 0x00) {
        closesocket(m_sockTCP);
        m_Status = PROXY_FAILED;
        return false;
    }

    {
        SOCKS5::ConnectRequestHeader reqHeader = { 5, 3, 0, 1, 0, 0 };
        send(m_sockTCP, reinterpret_cast<char*>(&reqHeader), sizeof(reqHeader), 0);
    }

    SOCKS5::ConnectRespondHeader resHeader{};
	ZeroMemory(&resHeader, sizeof(SOCKS5::ConnectRespondHeader));

	result = recv(m_sockTCP, reinterpret_cast<char*>(&resHeader), sizeof(SOCKS5::ConnectRespondHeader), 0);
    if (result == SOCKET_ERROR || resHeader.byteVersion != 5 || resHeader.byteResult != 0) {
        closesocket(m_sockTCP);
        m_Status = PROXY_FAILED;
       // g_pErrorLog->Log("[CBotProxy::Connect] Proxy %s failed to connect. (%x, Last Err. %x)", GetIP().c_str(), result, WSAGetLastError());
		//g_pDebugLog->Log("[CBotProxy::Connect] resHeader(%x, %x, %x)", resHeader.byteVersion, resHeader.byteResult, resHeader.byteAddressType);
        return false;
    }

    m_udpRelayAddr.sin_family = AF_INET;
    m_udpRelayAddr.sin_port = resHeader.usPort;
    m_udpRelayAddr.sin_addr.s_addr = resHeader.ulAddressIPv4;

    if (m_udpRelayAddr.sin_addr.s_addr == INADDR_NONE || m_udpRelayAddr.sin_addr.s_addr == 0) {
        m_Status = PROXY_FAILED;
        return false;
    }

    m_Status = PROXY_CONNECTED;
    return true;
}

ProxyStatus CBotProxy::GetStatus() {
    return m_Status;
}

int CBotProxy::SendTo(SOCKET socket, const char* data, int dataLength, int flags, const sockaddr_in* to, int tolen) {
    if (m_Status != PROXY_CONNECTED)
    {
        return SOCKET_ERROR;
    }
    if (socket == INVALID_SOCKET) {
        m_Status = PROXY_FAILED;
        return SOCKET_ERROR;
    }

    sockaddr_in relayAddr = m_udpRelayAddr;
    if (relayAddr.sin_addr.s_addr == INADDR_NONE || relayAddr.sin_addr.s_addr == 0) {
        m_Status = PROXY_FAILED;

        return SOCKET_ERROR;
    }

    int data_len = sizeof(SOCKS5::UDPDatagramHeader) + dataLength;
    std::vector<BYTE> proxyData(data_len);
    auto* udph = reinterpret_cast<SOCKS5::UDPDatagramHeader*>(proxyData.data());

    udph->usReserved = 0;
    udph->byteFragment = 0;
    udph->byteAddressType = 1;
    udph->ulAddressIPv4 = to->sin_addr.s_addr;
    udph->usPort = to->sin_port;

    std::memcpy(proxyData.data() + sizeof(SOCKS5::UDPDatagramHeader), data, dataLength);
    
    auto sentLen = sendto(socket, reinterpret_cast<const char*>(proxyData.data()), proxyData.size(), flags, reinterpret_cast<const sockaddr*>(&relayAddr), sizeof(sockaddr_in));

    if (sentLen <= 0) {
        int error = WSAGetLastError();
    }

    return sentLen;
}

int CBotProxy::RecvFrom(SOCKET socket, char* buffer, int bufferLength, int flags, sockaddr_in* from, int* fromlen) {
    if (GetStatus() != PROXY_CONNECTED) {
        return SOCKET_ERROR;
    }

    if (socket == INVALID_SOCKET) {
        return SOCKET_ERROR;
    }

    sockaddr_in relayAddr = m_udpRelayAddr;
    if (relayAddr.sin_addr.s_addr == INADDR_NONE || relayAddr.sin_addr.s_addr == 0) {
        m_Status = PROXY_FAILED;
        return SOCKET_ERROR;
    }

    int udphsize = sizeof(SOCKS5::UDPDatagramHeader);
    std::vector<char> data(bufferLength + udphsize, 0);
    int len = recvfrom(socket, data.data(), data.size(), flags, reinterpret_cast<sockaddr*>(from), fromlen);
    if (len == SOCKET_ERROR) 
    {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return 0;
        }
        else if (error == WSAECONNRESET) 
        {
            m_Status = PROXY_FAILED;
            return SOCKET_ERROR;
        }
        else {
            return SOCKET_ERROR;
        }
    }

	//g_pDebugLog->Log("[CBotProxy::RecvFrom] Received %d bytes from %s:%d", len, inet_ntoa(from->sin_addr), ntohs(from->sin_port));

    if (len <= udphsize) {
        std::memcpy(buffer, data.data(), len);
        return len;
    }

    auto* udph = reinterpret_cast<SOCKS5::UDPDatagramHeader*>(data.data());

    if (from) 
    {
        from->sin_family = AF_INET;
        from->sin_addr.s_addr = udph->ulAddressIPv4;
        from->sin_port = udph->usPort;
    }

    int dataLen = len - udphsize;
    std::memcpy(buffer, data.data() + udphsize, dataLen);
    return dataLen;
}

CBotProxy::~CBotProxy() {
    if (m_sockTCP != INVALID_SOCKET) {
        closesocket(m_sockTCP);
    }
}

bool CBotProxy::Authenticate() {
    SOCKS5::AuthUserPassRequestHeader authUserPassRequest;
    authUserPassRequest.byteVersion = 0x01;
    authUserPassRequest.byteUsernameLength = static_cast<BYTE>(m_username.length());
    authUserPassRequest.username = m_username;
    authUserPassRequest.bytePasswordLength = static_cast<BYTE>(m_password.length());
    authUserPassRequest.password = m_password;

    std::vector<char> buffer;
    buffer.push_back(authUserPassRequest.byteVersion);
    buffer.push_back(authUserPassRequest.byteUsernameLength);
    buffer.insert(buffer.end(), authUserPassRequest.username.begin(), authUserPassRequest.username.end());
    buffer.push_back(authUserPassRequest.bytePasswordLength);
    buffer.insert(buffer.end(), authUserPassRequest.password.begin(), authUserPassRequest.password.end());

    //g_pDebugLog->Log("[CBotProxy::Authenticate] Data length: %d", buffer.size());

    if (send(m_sockTCP, buffer.data(), buffer.size(), 0) == SOCKET_ERROR) {
        return false;
    }

    SOCKS5::AuthUserPassRespondHeader authUserPassResponse;
    if (recv(m_sockTCP, (char*)&authUserPassResponse, sizeof(authUserPassResponse), 0) == SOCKET_ERROR || authUserPassResponse.byteStatus != 0x00) {
        return false;
    }

    return true;
}