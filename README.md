# Proxy Checker

**Project:** Proxy Checker for UDP SA:MP Compatible Proxies  

---

## 📖 Description
Proxy Checker is a lightweight utility for validating **UDP proxies** that work with **GTA: San Andreas Multiplayer (SA:MP)**.  
It allows you to quickly test large proxy lists and filter out the ones that can establish communication with SA:MP servers.  

---

## 🚀 Features
- ✅ Validate UDP proxies for **SA:MP compatibility**  
- 🖱️ **Drag & drop** usage – no command line required  
- ⚡ Fast validation of large proxy lists  
- 📄 Generates output with only working proxies  

---

## 🛠️ Requirements
- Windows (x86/x64)  
- Input file: `proxy.txt`  

### Supported formats for `proxy.txt`
```
ip:port
ip:port:username:password
domain:port
domain:port:username:password
```

Each line must contain **one proxy entry**.  
✅ IPv4 is supported  
✅ Domain names are supported  

---

## 📦 Usage
1. Prepare your `proxy.txt` file with proxies in the supported format.  
2. Drag and drop the file onto the executable:  

```
ProxyChecker.exe  ← drag & drop →  proxy.txt
```

### Example `proxy.txt`
```
123.45.67.89:7777
98.76.54.32:1080:user:pass
proxy.example.com:3128
vpn.server.net:9050:login:secret
```

---

## 🤝 Contributing
Contributions are welcome!  
Feel free to open **issues** or submit **pull requests** to improve proxy detection and add new features.

---

## 📜 License
This project is licensed under the **MIT License**.  
See the [LICENSE](LICENSE) file for details.
