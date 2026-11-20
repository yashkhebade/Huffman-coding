// server.hpp - tiny local web server so a browser can upload a file and get the
// compressed result back. All compression is done by the C++ codec; the page
// served is plain HTML forms (no JavaScript).
//
// Safety: binds to 127.0.0.1 only (not reachable from other machines).
#pragma once
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include "codec.hpp"

#ifdef _WIN32
  #ifndef NOMINMAX
  #define NOMINMAX            // stop windows.h from defining min/max macros
  #endif
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using sock_t = SOCKET;
  static void closeSock(sock_t s) { closesocket(s); }
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  using sock_t = int;
  constexpr sock_t INVALID_SOCKET = -1;
  static void closeSock(sock_t s) { close(s); }
#endif

namespace server {

using Buf = std::vector<uint8_t>;
constexpr size_t kMaxBody = 256u * 1024 * 1024;
const size_t npos = std::string::npos;

// ---------- small helpers ----------
inline std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower((unsigned char)c));
    return s;
}
inline std::string htmlEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else o += c;
    }
    return o;
}
// Keep only safe filename characters (also strips any folder path).
inline std::string safeName(std::string n) {
    size_t slash = n.find_last_of("/\\");
    if (slash != npos) n = n.substr(slash + 1);
    for (char& c : n) if (!(std::isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_' || c == ' ')) c = '_';
    return n.empty() ? "file" : n;
}
inline bool endsWith(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

// ---------- multipart/form-data parsing ----------
struct Part { std::string name, filename; Buf data; };

inline std::string headerParam(const std::string& hdr, const std::string& key) {
    std::string pat = " " + key + "=\"";
    size_t p = hdr.find(pat);
    if (p == npos) return "";
    p += pat.size();
    size_t e = hdr.find('"', p);
    return e == npos ? "" : hdr.substr(p, e - p);
}

inline std::vector<Part> parseMultipart(const Buf& body, const std::string& boundary) {
    std::vector<Part> parts;
    const std::string delim = "--" + boundary;
    auto find = [&](const std::string& pat, size_t from) -> size_t {
        auto it = std::search(body.begin() + from, body.end(), pat.begin(), pat.end());
        return it == body.end() ? npos : size_t(it - body.begin());
    };
    size_t pos = find(delim, 0);
    while (pos != npos) {
        pos += delim.size();
        if (pos + 1 < body.size() && body[pos] == '-' && body[pos + 1] == '-') break;  // closing delimiter
        if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n') pos += 2;
        size_t headEnd = find("\r\n\r\n", pos);
        if (headEnd == npos) break;
        std::string hdr = " " + std::string(body.begin() + pos, body.begin() + headEnd);
        size_t dataStart = headEnd + 4;
        size_t next = find("\r\n" + delim, dataStart);
        if (next == npos) break;
        Part p;
        p.name = headerParam(hdr, "name");
        p.filename = headerParam(hdr, "filename");
        p.data.assign(body.begin() + dataStart, body.begin() + next);
        parts.push_back(std::move(p));
        pos = next + 2;
    }
    return parts;
}

// ---------- socket I/O ----------
inline bool sendAll(sock_t c, const char* d, size_t n) {
    while (n) {
        int k = (int)send(c, d, (int)std::min<size_t>(n, 1 << 20), 0);
        if (k <= 0) return false;
        d += k; n -= size_t(k);
    }
    return true;
}
inline void respond(sock_t c, int code, const std::string& status, const std::string& type,
                    const std::string& body, const std::string& extra = "") {
    std::ostringstream h;
    h << "HTTP/1.1 " << code << " " << status << "\r\nContent-Type: " << type
      << "\r\nContent-Length: " << body.size() << "\r\n" << extra << "Connection: close\r\n\r\n";
    std::string head = h.str();
    sendAll(c, head.data(), head.size());
    sendAll(c, body.data(), body.size());
}
inline std::string headerValue(const std::string& head, const std::string& key) {
    std::string l = lower(head);
    size_t p = l.find("\r\n" + lower(key) + ":");
    if (p == npos) return "";
    p = head.find(':', p + 2) + 1;
    size_t e = head.find("\r\n", p);
    std::string v = head.substr(p, e == npos ? npos : e - p);
    size_t a = v.find_first_not_of(" \t");
    return a == npos ? "" : v.substr(a);
}

// Read headers, then exactly Content-Length body bytes.
inline bool readRequest(sock_t c, std::string& head, Buf& body, bool& tooBig) {
    Buf buf; char tmp[65536]; size_t he = npos; tooBig = false;
    while (he == npos) {
        int n = (int)recv(c, tmp, sizeof tmp, 0);
        if (n <= 0) return false;
        buf.insert(buf.end(), tmp, tmp + n);
        static const std::string sep = "\r\n\r\n";
        auto it = std::search(buf.begin(), buf.end(), sep.begin(), sep.end());
        if (it != buf.end()) he = size_t(it - buf.begin());
        else if (buf.size() > 65536) return false;  // header absurdly large
    }
    head.assign(buf.begin(), buf.begin() + he + 2);  // keep trailing CRLF so last header is findable
    head = "\r\n" + head;                            // leading CRLF so first header is findable too
    size_t cl = 0;
    std::string cls = headerValue(head, "Content-Length");
    if (!cls.empty()) cl = size_t(std::strtoull(cls.c_str(), nullptr, 10));
    if (cl > kMaxBody) { tooBig = true; return true; }
    body.assign(buf.begin() + he + 4, buf.end());
    while (body.size() < cl) {
        int n = (int)recv(c, tmp, sizeof tmp, 0);
        if (n <= 0) return false;
        body.insert(body.end(), tmp, tmp + n);
    }
    body.resize(cl);
    return true;
}

// ---------- pages ----------
inline const char* kStyle = "<style>:root{color-scheme:light dark}body{font:16px/1.6 system-ui,Segoe UI,sans-serif;"
    "max-width:760px;margin:0 auto;padding:20px}fieldset{margin:0 0 18px;padding:14px 18px;border:1px solid #8886;border-radius:8px}"
    "legend{font-weight:bold}table{border-collapse:collapse;width:100%}td,th{padding:6px 10px;border-bottom:1px solid #8886;"
    "text-align:right}td:first-child,th:first-child{text-align:left}.ok{color:green}.er{color:crimson}</style>";

inline std::string indexPage() {
    return std::string("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Text Compression Tool</title>") + kStyle +
R"HTML(</head><body>
<h1>Lossless Text Compression Tool</h1>
<p>Huffman coding and run-length encoding (RLE), implemented in C++. This page only sends your file to the C++ program running on this computer.</p>

<form action="/compress" method="post" enctype="multipart/form-data"><fieldset><legend>1. Compress a file</legend>
<p><input type="file" name="file" required></p>
<p>Method: <select name="mode">
<option value="auto">Auto (smallest)</option><option value="huffman">Huffman</option>
<option value="rle">RLE</option>
</select> <button type="submit">Compress &amp; download</button></p></fieldset></form>

<form action="/compare" method="post" enctype="multipart/form-data"><fieldset><legend>2. Compare all methods</legend>
<p><input type="file" name="file" required> <button type="submit">Compare</button></p></fieldset></form>

<form action="/restore" method="post" enctype="multipart/form-data"><fieldset><legend>3. Restore a .huffz file</legend>
<p><input type="file" name="file" required> <button type="submit">Restore &amp; download</button></p></fieldset></form>
</body></html>)HTML";
}

inline std::string messagePage(const std::string& title, const std::string& cls, const std::string& msg) {
    return std::string("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>") + htmlEscape(title) + "</title>" + kStyle +
           "</head><body><h1>" + htmlEscape(title) + "</h1><p class=\"" + cls + "\">" + htmlEscape(msg) +
           "</p><p><a href=\"/\">&larr; Back</a></p></body></html>";
}

inline std::string comparePage(const std::string& fname, const Buf& data) {
    using Clock = std::chrono::steady_clock;
    std::ostringstream o;
    o << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Comparison</title>" << kStyle
      << "</head><body><h1>Comparison</h1><p><b>" << htmlEscape(fname) << "</b> &mdash; " << data.size()
      << " bytes</p><table><tr><th>Method</th><th>Size (bytes)</th><th>Ratio</th><th>Saved</th><th>Time (ms)</th><th>Round-trip</th></tr>";
    for (auto m : {codec::Mode::Huffman, codec::Mode::Rle}) {
        auto t0 = Clock::now();
        auto c = codec::compressWith(data, m);
        double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        bool ok = false;
        try { ok = codec::decompress(c) == data; } catch (...) {}
        double ratio = data.empty() ? 1.0 : double(c.size()) / double(data.size());
        o << "<tr><td>" << codec::modeName(m) << "</td><td>" << c.size() << "</td><td>" << std::fixed << std::setprecision(3) << ratio
          << "</td><td>" << std::setprecision(1) << (1 - ratio) * 100 << "%</td><td>" << std::setprecision(2) << ms
          << "</td><td class=\"" << (ok ? "ok\">PASS" : "er\">FAIL") << "</td></tr>";
    }
    o << "</table><p><a href=\"/\">&larr; Back</a></p></body></html>";
    return o.str();
}

// ---------- request handling ----------
inline void handle(sock_t c) {
    try {
        std::string head; Buf body; bool tooBig = false;
        if (!readRequest(c, head, body, tooBig)) return;
        if (tooBig) { respond(c, 413, "Payload Too Large", "text/html", messagePage("Too large", "er", "Maximum upload is 256 MB.")); return; }

        std::istringstream rl(head.substr(2));  // skip the leading CRLF we added
        std::string method, path; rl >> method >> path;

        if (method == "GET" && (path == "/" || path == "/index.html")) {
            respond(c, 200, "OK", "text/html; charset=utf-8", indexPage()); return;
        }
        if (method != "POST") { respond(c, 404, "Not Found", "text/html", messagePage("Not found", "er", "No such page.")); return; }

        std::string ctype = headerValue(head, "Content-Type");
        size_t b = ctype.find("boundary=");
        if (b == npos) { respond(c, 400, "Bad Request", "text/html", messagePage("Error", "er", "Expected a file upload.")); return; }
        std::string boundary = ctype.substr(b + 9);
        if (!boundary.empty() && boundary[0] == '"') boundary = boundary.substr(1, boundary.find('"', 1) - 1);

        const Part* file = nullptr; std::string mode = "auto";
        auto parts = parseMultipart(body, boundary);
        for (auto& p : parts) {
            if (p.name == "file") file = &p;
            else if (p.name == "mode") mode.assign(p.data.begin(), p.data.end());
        }
        if (!file || file->filename.empty()) { respond(c, 400, "Bad Request", "text/html", messagePage("No file", "er", "Please choose a file first.")); return; }
        std::string fname = safeName(file->filename);

        if (path == "/compress") {
            Buf out;
            if (mode == "auto") out = codec::compressBest(file->data);
            else {
                codec::Mode m = codec::Mode::Huffman;
                if (!codec::parseMode(mode, m)) throw std::runtime_error("unknown method: " + mode);
                out = codec::compressWith(file->data, m);
            }
            respond(c, 200, "OK", "application/octet-stream", std::string(out.begin(), out.end()),
                    "Content-Disposition: attachment; filename=\"" + fname + ".huffz\"\r\n");
        } else if (path == "/restore") {
            Buf out = codec::decompress(file->data);
            std::string rn = endsWith(fname, ".huffz") ? fname.substr(0, fname.size() - 6) : "restored_" + fname;
            respond(c, 200, "OK", "application/octet-stream", std::string(out.begin(), out.end()),
                    "Content-Disposition: attachment; filename=\"" + rn + "\"\r\n");
        } else if (path == "/compare") {
            respond(c, 200, "OK", "text/html; charset=utf-8", comparePage(fname, file->data));
        } else {
            respond(c, 404, "Not Found", "text/html", messagePage("Not found", "er", "No such page."));
        }
    } catch (const std::exception& e) {
        respond(c, 400, "Bad Request", "text/html", messagePage("Error", "er", e.what()));
    }
}

inline int run(int port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { std::cerr << "WSAStartup failed\n"; return 2; }
#endif
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) { std::cerr << "socket() failed\n"; return 2; }
#ifndef _WIN32
    int yes = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
#endif
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(uint16_t(port));
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0 || listen(s, 16) != 0) {
        std::cerr << "Cannot listen on port " << port << " (already in use?). Try: huffz serve 8081\n"; return 2;
    }
    std::cout << "huffz server running.  Open  http://localhost:" << port << "  in your browser.\nPress Ctrl+C to stop.\n" << std::flush;
#ifdef _WIN32
    std::system(("start http://localhost:" + std::to_string(port)).c_str());
#endif
    for (;;) {
        sock_t c = accept(s, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;
        // One thread per connection: browsers open idle "preconnect" sockets that would otherwise block us.
        std::thread([c] { handle(c); closeSock(c); }).detach();
    }
}

} // namespace server
