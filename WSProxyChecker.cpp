#include "CBotProxy.h"

std::vector<std::shared_ptr<CBotProxy>> proxy{};
bool bFinish = false;

std::vector<std::string> split(const std::string& str, const std::string& token)
{
    std::vector<std::string> result;
    size_t start = 0, end;
    while ((end = str.find(token, start)) != std::string::npos)
    {
        result.push_back(str.substr(start, end - start));
        start = end + token.length();
    }
    result.push_back(str.substr(start));
    return result;
}

ULONG ResolveDomainToIP(const std::string& domain) {
    addrinfo hints = {}, * res = nullptr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int result = getaddrinfo(domain.c_str(), nullptr, &hints, &res);
    if (result != 0 || res == nullptr) {
        return 0;
    }

    sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
    ULONG ip = addr->sin_addr.s_addr;

    freeaddrinfo(res);
    return ip;
}

void ConnectProxies(const std::vector<std::string>& data)
{
    if (data.empty()) return;

    std::vector<std::future<void>> proxyConnections;
    proxyConnections.reserve(data.size());

    for (const std::string& proxy_ip : data)
    {
        const auto ip_port = split(proxy_ip, ":");

        if (ip_port.size() != 2 && ip_port.size() != 4)
        {
            continue;
        }

        std::string isDomainIP = "";
        ULONG ip;

        if (inet_addr(ip_port.at(0).c_str()) == INADDR_NONE)
        {
            ip = ResolveDomainToIP(ip_port.at(0));
            isDomainIP = ip_port.at(0);
        }
        else
        {
            ip = inet_addr(ip_port.at(0).c_str());
        }

        try
        {
            int port = std::stoi(ip_port.at(1));

            if (port <= 0 || port > 65535)
            {
                continue;
            }

            if (ip_port.size() == 2)
            {
                proxy.push_back(std::make_shared<CBotProxy>(ip, port, proxy_ip, "", "", isDomainIP));
            }
            else if (ip_port.size() == 4)
            {
                proxy.push_back(std::make_shared<CBotProxy>(ip, port, proxy_ip, ip_port.at(2), ip_port.at(3), isDomainIP));
            }

            proxyConnections.push_back(std::async(std::launch::async, [proxy = proxy.back()] {
                proxy->Connect();
                }));
        }
        catch (...)
        {

        }
    }

    for (auto& connection : proxyConnections)
    {
        connection.get();
    }
}

bool PrepareProxies(const std::string& fileName)
{
    if (!std::filesystem::exists(fileName))
    {
        std::cout << "File not found: " << fileName << "\n";
        return false;
    }

    std::ifstream m_read(fileName);
    if (!m_read.is_open())
    {
        std::cout << "Unable to open file: " << fileName << "\n";
        return false;
    }

    std::vector<std::string> myLines;
    std::string line;
    while (std::getline(m_read, line))
    {
        if (!line.empty())
            myLines.push_back(line);
    }
    m_read.close();

    if (myLines.empty())
    {
        std::cout << "File is empty: " << fileName << "\n";
        return false;
    }

    std::cout << "Loaded " << myLines.size() << " proxies from " << fileName << "\n";
    std::cout << "Connecting proxies...\n";
    ConnectProxies(myLines);

    bFinish = true;
    std::cout << "Key: END to export\n";
    return true;
}

const int GetConnectedProxiesNum()
{
    return std::count_if(proxy.begin(), proxy.end(), [](const std::shared_ptr<CBotProxy>& p) { return p->GetStatus() == PROXY_CONNECTED; });
}

std::vector<std::shared_ptr<CBotProxy>> GetProxies()
{
    std::vector<std::shared_ptr<CBotProxy>> out_value;
    std::for_each(proxy.begin(), proxy.end(), [&out_value](const std::shared_ptr<CBotProxy>& p)
        {
            if (p->GetStatus() == PROXY_CONNECTED) { out_value.push_back(p); }
        });
    return out_value;
}

void Export()
{
    auto example = GetProxies();

    std::ofstream output_file("proxies_online.txt");
    std::ostream_iterator<std::shared_ptr<CBotProxy>> output_iterator(output_file, "\n");
    std::copy(example.begin(), example.end(), output_iterator);
    output_file.close();
}

bool IsValidTextFile(const std::string& fileName)
{
    if (fileName.length() < 4) return false;

    std::string extension = fileName.substr(fileName.length() - 4);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    return extension == ".txt";
}

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return 1;
    }

    std::string proxyFileName = "proxies.txt"; // default file

    // Check if file was dragged onto executable
    if (argc > 1)
    {
        std::string droppedFile = argv[1];

        if (IsValidTextFile(droppedFile))
        {
            proxyFileName = droppedFile;
            std::cout << "Using dropped file: " << proxyFileName << "\n";
        }
        else
        {
            std::cout << "Invalid file type. Only .txt files are supported.\n";
            std::cout << "Using default file: " << proxyFileName << "\n";
        }
    }
    else
    {
        std::cout << "No file dropped. Using default file: " << proxyFileName << "\n";
        std::cout << "Tip: You can drag & drop a .txt file onto this executable\n";
    }

    std::cout << "Preparing proxies...\n";

    std::thread t1(PrepareProxies, proxyFileName);
    t1.detach();

    while (true)
    {
        if (bFinish && GetAsyncKeyState(VK_END) & 1)
            break;

        SetConsoleTitleA(("Connected proxies: " + std::to_string(GetConnectedProxiesNum()) + " of " + std::to_string(proxy.size())).c_str());

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Exporting connected proxies to file...\n";
    Export();

    system("pause");

    WSACleanup();
    return 0;
}