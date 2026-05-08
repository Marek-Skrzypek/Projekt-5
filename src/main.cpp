#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#include <asio.hpp>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

// ════════════════════════════════════════════════════════════════
//  TcpClient
// ════════════════════════════════════════════════════════════════

class TcpClient {
    asio::io_context  ctx;
    asio::ip::tcp::socket sock;
    asio::streambuf   readBuf;
    std::string       address, servicePort, clientId;

    void transmit(const std::string& payload) {
        std::cout << clientId << ": " << payload << "\n";
        asio::write(sock, asio::buffer(payload + "\n"));
    }

public:
    TcpClient(std::string address, std::string servicePort, std::string clientId = "")
        : address(std::move(address)), servicePort(std::move(servicePort)),
          clientId(std::move(clientId)), sock(ctx) {}

    ~TcpClient() { if (sock.is_open()) sock.close(); }

    void connect() {
        asio::ip::tcp::resolver resolver(ctx);
        asio::connect(sock, resolver.resolve(address, servicePort));
        std::cout << "Connected to: " << address << ":" << servicePort << "\n";
    }

    void disconnect() {
        transmit("QUIT");
        sock.close();
        std::cout << "Disconnected\n";
    }

    void submitGuess(const std::vector<int>& sequence) {
        std::ostringstream ss;
        ss << "TRY";
        for (int x : sequence) ss << " " << x;
        transmit(ss.str());
    }

    std::string readLine() {
        asio::read_until(sock, readBuf, "\n");
        std::istream is(&readBuf);
        std::string line;
        std::getline(is, line);
        std::cout << "SERVER: " << line << "\n";
        return line;
    }
};

// ════════════════════════════════════════════════════════════════
//  Protocol
// ════════════════════════════════════════════════════════════════

struct ServerMessage {
    std::string              type;
    std::vector<std::string> params;
};

ServerMessage parseMessage(const std::string& raw) {
    std::istringstream ss(raw);
    ServerMessage msg;
    ss >> msg.type;
    std::string token;
    while (ss >> token) msg.params.push_back(token);
    return msg;
}

// ════════════════════════════════════════════════════════════════
//  Solver
// ════════════════════════════════════════════════════════════════

enum class SolverAction {
    RevertAndLockFull,
    ConfirmAndAdvance,
    RevertAndLock,
    AcceptAndAdvance,
    ReplaceInPlace,
};

SolverAction decideAction(int hits, int exacts, int prevHits, int prevExacts) {
    if      (exacts < prevExacts) return SolverAction::RevertAndLockFull;
    else if (exacts > prevExacts) return SolverAction::ConfirmAndAdvance;
    else if (hits   < prevHits)  return SolverAction::RevertAndLock;
    else if (hits   > prevHits)  return SolverAction::AcceptAndAdvance;
    else                         return SolverAction::ReplaceInPlace;
}

// ════════════════════════════════════════════════════════════════
//  Session config
// ════════════════════════════════════════════════════════════════

struct SessionConfig {
    int  gridSize     = 0;
    int  seqLength    = 0;
    int  candidateMin = 0;
    int  candidateMax = 0;
    bool uniqueTokens = true;
};

SessionConfig readHandshake(TcpClient& client) {
    SessionConfig cfg;
    ServerMessage msg = parseMessage(client.readLine());
    while (msg.type != "ENDHELLO") {
        if      (msg.type == "BOARD")    cfg.gridSize     = std::stoi(msg.params[0]) * std::stoi(msg.params[1]);
        else if (msg.type == "LENGTH")   cfg.seqLength    = std::stoi(msg.params[0]);
        else if (msg.type == "NUMBERS") { cfg.candidateMin = std::stoi(msg.params[0]); cfg.candidateMax = std::stoi(msg.params[1]); }
        else if (msg.type == "DISTINCT") cfg.uniqueTokens = (msg.params[0] == "YES");
        msg = parseMessage(client.readLine());
    }
    return cfg;
}

// ════════════════════════════════════════════════════════════════
//  Main
// ════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    std::string address = "127.0.0.1", servicePort = "9000";
    int argi = 1;

    if (argi < argc && argv[argi][0] != '-') address     = argv[argi++];
    if (argi < argc && argv[argi][0] != '-') servicePort = argv[argi++];
    if (argi < argc) { std::cerr << "Unknown flag: " << argv[argi] << "\n"; return 1; }

    TcpClient client(address, servicePort);
    client.connect();

    const SessionConfig cfg = readHandshake(client);

    // ── State ──
    int candidate  = cfg.candidateMin;
    int prevHits   = 0, prevExacts = 0;
    int iterations = 0;

    std::vector<int>  sequence;
    std::vector<bool> locked, confirmed;

    for (int i = 0; i < cfg.seqLength; i++) {
        sequence.push_back(candidate++);
        locked.push_back(false);
        confirmed.push_back(false);
    }
    std::pair<int,int> lastSwap = { cfg.seqLength - 1, candidate - 1 };

    auto nextUnlocked = [&](int from) {
        int idx = from % cfg.seqLength;
        while (locked[idx]) idx = (idx + 1) % cfg.seqLength;
        return idx;
    };

    // ── First query ──
    client.submitGuess(sequence);
    ServerMessage msg = parseMessage(client.readLine());
    prevHits   = std::stoi(msg.params[0]);
    prevExacts = std::stoi(msg.params[1]);

    // ── Main loop ──
    while (msg.type != "SOLVED") {

        if (msg.type == "RESULT") {
            int hits   = std::stoi(msg.params[0]);
            int exacts = std::stoi(msg.params[1]);

            if (hits < cfg.seqLength) {
                if (candidate > cfg.candidateMax) candidate = cfg.candidateMin;

                switch (decideAction(hits, exacts, prevHits, prevExacts)) {

                case SolverAction::RevertAndLockFull:
                    sequence[lastSwap.first]  = lastSwap.second;
                    locked[lastSwap.first]    = confirmed[lastSwap.first] = true;
                    client.submitGuess(sequence);
                    break;

                case SolverAction::ConfirmAndAdvance:
                    locked[lastSwap.first] = confirmed[lastSwap.first] = true;
                    {
                        int idx = nextUnlocked(lastSwap.first + 1);
                        lastSwap = { idx, sequence[idx] };
                        sequence[idx] = candidate++;
                        prevHits = hits; prevExacts = exacts;
                    }
                    client.submitGuess(sequence);
                    break;

                case SolverAction::RevertAndLock:
                    sequence[lastSwap.first] = lastSwap.second;
                    locked[lastSwap.first]   = true;
                    client.submitGuess(sequence);
                    break;

                case SolverAction::AcceptAndAdvance:
                    locked[lastSwap.first] = true;
                    {
                        int idx = nextUnlocked(lastSwap.first + 1);
                        lastSwap = { idx, sequence[idx] };
                        sequence[idx] = candidate++;
                        prevHits = hits; prevExacts = exacts;
                    }
                    client.submitGuess(sequence);
                    break;

                case SolverAction::ReplaceInPlace:
                    {
                        int idx = nextUnlocked(lastSwap.first);
                        lastSwap = { idx, sequence[idx] };
                        sequence[idx] = candidate++;
                        prevHits = hits; prevExacts = exacts;
                    }
                    client.submitGuess(sequence);
                    break;
                }

            } else {
                std::next_permutation(sequence.begin(), sequence.end());
                client.submitGuess(sequence);
            }

        } else if (msg.type == "ERROR" && msg.params[0] == "101") {
            int idx = nextUnlocked(lastSwap.first);
            lastSwap = { idx, sequence[idx] };
            sequence[idx] = candidate++;
            client.submitGuess(sequence);

        } else break;

        msg = parseMessage(client.readLine());
        if (++iterations > 5040) { std::cout << "EMERGENCY BREAK!\n"; break; }
    }

    client.disconnect();
    return 0;
}