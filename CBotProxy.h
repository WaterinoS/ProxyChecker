#define _WINSOCK_DEPRECATED_NO_WARNINGS


// illegal instruction size
#pragma warning( disable : 4409 )

// 'class1' : inherits 'class2::member' via dominance
#pragma warning( disable : 4250 )

// unreferenced formal parameter
#pragma warning( disable : 4100 )

// handler not registered as safe handler
#pragma warning( disable : 4733 )

#pragma warning( disable : 4244 )
#pragma warning( disable : 4828 )

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>

#include <memory>
#include <random>
#include <map>
#include <list>
#include <queue>
#include <set>
#include <array>
#include <future>
#include <chrono>
#include <shared_mutex>
#include <filesystem>

#pragma comment(lib, "Ws2_32.lib")

enum ProxyStatus {
    PROXY_NONE,
    PROXY_FAILED,
    PROXY_CONNECTING,
    PROXY_CONNECTED
};

namespace SOCKS5 {
#pragma pack(push, 1)
    struct AuthRequestHeader {
        BYTE byteVersion;
        BYTE byteAuthMethodsCount;
        BYTE byteMethods[1];
    };

    struct AuthRespondHeader {
        BYTE byteVersion;
        BYTE byteAuthMethod;
    };

    struct AuthUserPassRequestHeader {
        BYTE byteVersion;
        BYTE byteUsernameLength;
        std::string username;
        BYTE bytePasswordLength;
        std::string password;
    };

    struct AuthUserPassRespondHeader {
        BYTE byteVersion;
        BYTE byteStatus;
    };

    struct ConnectRequestHeader {
        BYTE byteVersion;
        BYTE byteCommand;
        BYTE byteReserved;
        BYTE byteAddressType;
        ULONG ulAddressIPv4;
        USHORT usPort;
    };

    struct ConnectRequestDomainHeader {
        BYTE byteVersion;
        BYTE byteCommand;
        BYTE byteReserved;
        BYTE byteAddressType;
        BYTE byteDomainLength;
        USHORT usPort;
    };

    struct ConnectRespondHeader {
        BYTE byteVersion;
        BYTE byteResult;
        BYTE byteReserved;
        BYTE byteAddressType;
        ULONG ulAddressIPv4;
        USHORT usPort;
    };

    struct UDPDatagramHeader {
        USHORT usReserved;
        BYTE byteFragment;
        BYTE byteAddressType;
        ULONG ulAddressIPv4;
        USHORT usPort;
    };
#pragma pack(pop)
}

class CBotProxy {
public:
    CBotProxy(ULONG proxyIP, USHORT proxyPort, std::string proxy_ip, std::string username = "", std::string password = "", std::string domainName = "")
        : m_sockTCP(INVALID_SOCKET), m_proxyIP(proxyIP), m_proxyPort(proxyPort), m_szProxyIP(std::move(proxy_ip)), m_username(std::move(username)), m_password(std::move(password)), m_Status(PROXY_NONE), bUsed(false), m_proxyServerAddr({}), m_udpRelayAddr({}), m_domainIP(domainName) {}

    bool Connect();
    int SendTo(SOCKET socket, const char* data, int dataLength, int flags, const sockaddr_in* to, int tolen);
    int RecvFrom(SOCKET socket, char* data, int dataLength, int flags, sockaddr_in* from, int* fromlen);
    ProxyStatus GetStatus();

    std::string GetIP() const {
        return m_szProxyIP;
    }

	SOCKET GetTCP() const {
		return m_sockTCP;
	}

    friend std::ostream& operator<<(std::ostream& os, CBotProxy* obj) {
        std::string str = obj->GetIP();

        if (!obj->m_username.empty() && !obj->m_password.empty())
            str += ":" + obj->m_username + ":" + obj->m_password;

        return os << str.c_str();
    }

    ~CBotProxy();

private:
    bool Authenticate();

    std::string m_domainIP;
    SOCKET m_sockTCP;
    sockaddr_in m_proxyServerAddr;
    sockaddr_in m_udpRelayAddr;
    ULONG m_proxyIP;
    USHORT m_proxyPort;
    std::string m_szProxyIP;
public:
    bool bUsed;
    std::string m_username;
    std::string m_password;
    ProxyStatus m_Status;
};