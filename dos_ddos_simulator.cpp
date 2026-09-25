#include <bits/stdc++.h>
using namespace std;

enum class RequestOrigin
{
    LEGITIMATE,
    DOS_BURST,
    DDOS_BOTNET
};

struct Request
{
    uint64_t id;
    RequestOrigin origin;
    string clientIp;
    chrono::steady_clock::time_point arrivalTime;
};

struct WorkerStatus
{
    bool isBusy{false};
    uint64_t currentRequestId{0};
    RequestOrigin currentOrigin{RequestOrigin::LEGITIMATE};
    string currentClientIp{""};
};

class ApplicationServer
{
private:
    const size_t numWorkers;
    const size_t maxQueueCapacity;
    const chrono::milliseconds processingTime;

    vector<thread> workers;
    vector<WorkerStatus> workerStatuses;
    queue<Request> requestQueue;

    mutex queueMutex;
    condition_variable cvQueue;
    mutex statusMutex;

    atomic<bool> isRunning{true};

    atomic<uint64_t> totalLegitReceived{0};
    atomic<uint64_t> totalLegitProcessed{0};
    atomic<uint64_t> totalLegitDropped{0};

    atomic<uint64_t> totalAttackReceived{0};
    atomic<uint64_t> totalAttackProcessed{0};
    atomic<uint64_t> totalAttackDropped{0};

    uint64_t lastLegitDropped{0};
    uint64_t lastLegitReceived{0};
    uint64_t lastAttackDropped{0};
    uint64_t lastAttackReceived{0};

public:
    ApplicationServer(size_t workersCount, size_t queueCapacity, int serviceTimeMs)
        : numWorkers(workersCount),
          maxQueueCapacity(queueCapacity),
          processingTime(serviceTimeMs),
          workerStatuses(workersCount) {}

    ~ApplicationServer()
    {
        stop();
    }

    void start()
    {
        for (size_t i = 0; i < numWorkers; ++i)
        {
            workers.emplace_back(&ApplicationServer::workerLoop, this, i);
        }
    }

    void stop()
    {
        if (!isRunning)
            return;
        isRunning = false;
        cvQueue.notify_all();
        for (auto &th : workers)
        {
            if (th.joinable())
                th.join();
        }
    }

    bool submitRequest(const Request &req)
    {
        if (req.origin == RequestOrigin::LEGITIMATE)
        {
            totalLegitReceived++;
        }
        else
        {
            totalAttackReceived++;
        }

        {
            unique_lock<mutex> lock(queueMutex);

            if (requestQueue.size() >= maxQueueCapacity)
            {
                if (req.origin == RequestOrigin::LEGITIMATE)
                {
                    totalLegitDropped++;
                }
                else
                {
                    totalAttackDropped++;
                }
                return false;
            }

            requestQueue.push(req);
        }

        cvQueue.notify_one();
        return true;
    }

    void printDashboard(double intervalSeconds)
    {
        vector<WorkerStatus> statusesCopy;
        size_t currentQueueSize = 0;

        {
            lock_guard<mutex> lock(queueMutex);
            currentQueueSize = requestQueue.size();
        }
        {
            lock_guard<mutex> lock(statusMutex);
            statusesCopy = workerStatuses;
        }

        uint64_t curLegitDrop = totalLegitDropped.load();
        uint64_t deltaLegitDrop = curLegitDrop - lastLegitDropped;
        lastLegitDropped = curLegitDrop;
        double legitDropRate = static_cast<double>(deltaLegitDrop) / intervalSeconds;

        uint64_t curLegitRecv = totalLegitReceived.load();
        uint64_t deltaLegitRecv = curLegitRecv - lastLegitReceived;
        lastLegitReceived = curLegitRecv;

        uint64_t curAttackDrop = totalAttackDropped.load();
        uint64_t deltaAttackDrop = curAttackDrop - lastAttackDropped;
        lastAttackDropped = curAttackDrop;

        uint64_t curAttackRecv = totalAttackReceived.load();
        uint64_t deltaAttackRecv = curAttackRecv - lastAttackReceived;
        lastAttackReceived = curAttackRecv;

        double legitSuccessPct = (curLegitRecv > 0)
                                     ? (100.0 * (curLegitRecv - curLegitDrop) / curLegitRecv)
                                     : 100.0;

        cout << "\n======================== [ SERVER MONITOR - 2.0s INTERVAL ] ========================\n";

        cout << "WORKER POOL STATUS:\n";
        for (size_t i = 0; i < numWorkers; ++i)
        {
            cout << "  Worker #" << i << ": ";
            if (statusesCopy[i].isBusy)
            {
                string typeStr = (statusesCopy[i].currentOrigin == RequestOrigin::LEGITIMATE) ? "LEGIT" : "ATTACK";
                cout << "[BUSY]  Processing Req #" << statusesCopy[i].currentRequestId
                     << " (" << typeStr << ") from " << statusesCopy[i].currentClientIp << "\n";
            }
            else
            {
                cout << "[IDLE]  Waiting for request...\n";
            }
        }

        cout << "\nREQUEST QUEUE CAPACITY: [" << currentQueueSize << " / " << maxQueueCapacity << "] ";
        int fillBars = static_cast<int>((static_cast<double>(currentQueueSize) / maxQueueCapacity) * 20.0);
        cout << "[";
        for (int b = 0; b < 20; ++b)
        {
            if (b < fillBars)
                cout << "#";
            else
                cout << ".";
        }
        cout << "]\n";

        cout << "\nTRAFFIC & DROP METRICS (Last 2s Window):\n";
        cout << "  * Normal User Requests : Received: " << deltaLegitRecv
             << " | Dropped: " << deltaLegitDrop
             << " | DROP RATE: " << fixed << setprecision(2) << legitDropRate << " req/sec\n";
        cout << "  * Attack Requests      : Received: " << deltaAttackRecv
             << " | Dropped: " << deltaAttackDrop << "\n";

        cout << "\nCUMULATIVE SERVER HEALTH:\n";
        cout << "  * Total Normal Requests  : " << curLegitRecv << " (Processed: " << totalLegitProcessed.load()
             << ", Dropped: " << curLegitDrop << ")\n";
        cout << "  * Total Attack Requests  : " << curAttackRecv << " (Processed: " << totalAttackProcessed.load()
             << ", Dropped: " << curAttackDrop << ")\n";
        cout << "  * Normal Service Health  : " << fixed << setprecision(1) << legitSuccessPct << "% Availability\n";
        cout << "====================================================================================\n";
        cout << "COMMANDS: Type a number (e.g. 50) for DoS burst, 'ddos <bots> <burst>' for DDoS, or 'exit':\n> " << flush;
    }

private:
    void workerLoop(size_t workerId)
    {
        while (isRunning)
        {
            Request currentReq;

            {
                unique_lock<mutex> lock(queueMutex);
                cvQueue.wait(lock, [this]()
                             { return !requestQueue.empty() || !isRunning; });

                if (!isRunning && requestQueue.empty())
                {
                    break;
                }

                if (requestQueue.empty())
                {
                    continue;
                }

                currentReq = requestQueue.front();
                requestQueue.pop();
            }

            {
                lock_guard<mutex> lock(statusMutex);
                workerStatuses[workerId].isBusy = true;
                workerStatuses[workerId].currentRequestId = currentReq.id;
                workerStatuses[workerId].currentOrigin = currentReq.origin;
                workerStatuses[workerId].currentClientIp = currentReq.clientIp;
            }

            this_thread::sleep_for(processingTime);

            if (currentReq.origin == RequestOrigin::LEGITIMATE)
            {
                totalLegitProcessed++;
            }
            else
            {
                totalAttackProcessed++;
            }

            {
                lock_guard<mutex> lock(statusMutex);
                workerStatuses[workerId].isBusy = false;
                workerStatuses[workerId].currentRequestId = 0;
            }
        }
    }
};

atomic<uint64_t> gRequestIdCounter{1000};
atomic<bool> gSimRunning{true};

void legitimateTrafficGenerator(ApplicationServer &server)
{
    mt19937 rng(1337);
    uniform_int_distribution<int> ipSuffix(10, 99);

    while (gSimRunning)
    {
        this_thread::sleep_for(chrono::milliseconds(280));
        if (!gSimRunning)
            break;

        Request req;
        req.id = gRequestIdCounter++;
        req.origin = RequestOrigin::LEGITIMATE;
        req.clientIp = "192.168.1." + to_string(ipSuffix(rng));
        req.arrivalTime = chrono::steady_clock::now();

        server.submitRequest(req);
    }
}

void serverMonitor(ApplicationServer &server)
{
    while (gSimRunning)
    {
        this_thread::sleep_for(chrono::seconds(2));
        if (!gSimRunning)
            break;
        server.printDashboard(2.0);
    }
}

void launchDosBurst(ApplicationServer &server, int count)
{
    cout << "\n[!] SIMULATING DoS ATTACK: single-source burst of " << count << " requests...\n"
         << flush;
    string attackerIp = "10.0.0.99 (Attacker-Host, simulated)";

    for (int i = 0; i < count; ++i)
    {
        Request req;
        req.id = gRequestIdCounter++;
        req.origin = RequestOrigin::DOS_BURST;
        req.clientIp = attackerIp;
        req.arrivalTime = chrono::steady_clock::now();
        server.submitRequest(req);
    }
}

void launchDdosFlood(ApplicationServer &server, int botCount, int reqsPerBot)
{
    cout << "\n[!] SIMULATING DDoS ATTACK: " << botCount << " simulated botnet nodes sending "
         << reqsPerBot << " requests each (Total: " << (botCount * reqsPerBot) << ")...\n"
         << flush;

    vector<thread> botThreads;
    botThreads.reserve(botCount);

    for (int b = 0; b < botCount; ++b)
    {
        botThreads.emplace_back([&server, b, reqsPerBot]()
                                {
            string botIp = "bot-" + to_string(b + 1) + ".botnet.example (" +
                                to_string(172) + "." + to_string(16 + (b % 16)) + "." +
                                to_string(b / 254) + "." + to_string(b % 254 + 1) + ")";
            for (int r = 0; r < reqsPerBot; ++r) {
                Request req;
                req.id = gRequestIdCounter++;
                req.origin = RequestOrigin::DDOS_BOTNET;
                req.clientIp = botIp;
                req.arrivalTime = chrono::steady_clock::now();
                server.submitRequest(req);
                this_thread::sleep_for(chrono::microseconds(500));
            } });
    }

    for (auto &th : botThreads)
    {
        if (th.joinable())
            th.join();
    }
}

int main()
{
    const size_t WORKERS = 4;
    const size_t QUEUE_SIZE = 15;
    const int SERVICE_TIME_MS = 250;

    cout << "===============================================================\n";
    cout << "   CSE 4115 - COMPUTER SECURITY LAB: DoS & DDoS SIMULATOR (C++)\n";
    cout << "   (Educational in-process simulation -- no real network traffic)\n";
    cout << "===============================================================\n";
    cout << "Server Setup: " << WORKERS << " Parallel Workers | Queue Capacity: "
         << QUEUE_SIZE << " | Service Time: " << SERVICE_TIME_MS << "ms/req\n";
    cout << "Normal Traffic : Generating ~3.5 requests/second in background.\n";
    cout << "---------------------------------------------------------------\n";
    cout << "Instructions:\n";
    cout << "  1. Type a number (e.g. 40) and press Enter -> Fires a simulated DoS burst.\n";
    cout << "  2. Type 'ddos <bots> <reqs>' (e.g. ddos 10 5) -> Fires a simulated DDoS botnet.\n";
    cout << "  3. Type 'exit' to quit.\n";
    cout << "===============================================================\n\n";

    ApplicationServer server(WORKERS, QUEUE_SIZE, SERVICE_TIME_MS);
    server.start();

    thread normalTrafficThread(legitimateTrafficGenerator, ref(server));
    thread monitorThread(serverMonitor, ref(server));

    string line;
    cout << "> " << flush;
    while (getline(cin, line))
    {
        if (line.empty())
        {
            cout << "> " << flush;
            continue;
        }

        if (line == "exit" || line == "quit")
        {
            break;
        }

        istringstream iss(line);
        string command;
        iss >> command;

        if (command == "ddos")
        {
            int bots = 8;
            int reqs = 10;
            if (iss >> bots)
            {
                iss >> reqs;
            }
            if (bots <= 0 || reqs <= 0)
            {
                cout << "Bot count and requests-per-bot must both be positive.\n";
            }
            else
            {
                launchDdosFlood(server, bots, reqs);
            }
        }
        else
        {
            try
            {
                size_t parsedChars = 0;
                int burstSize = stoi(command, &parsedChars);
                if (parsedChars != command.size())
                {
                    throw invalid_argument("trailing characters");
                }
                if (burstSize > 0)
                {
                    launchDosBurst(server, burstSize);
                }
                else
                {
                    cout << "Please specify a positive burst count.\n";
                }
            }
            catch (...)
            {
                cout << "Unknown command. Enter a number (e.g. 50), 'ddos 10 5', or 'exit'.\n";
            }
        }
        cout << "> " << flush;
    }

    cout << "\nShutting down simulation...\n";
    gSimRunning = false;
    server.stop();

    if (normalTrafficThread.joinable())
        normalTrafficThread.join();
    if (monitorThread.joinable())
        monitorThread.join();

    cout << "Simulation finished cleanly.\n";
    return 0;
}
