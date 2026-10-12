# DoS & DDoS Simulator (C++)

**Course:** CSE 4115 - Computer Security Lab  
**Student ID:** 2107064  
**Project:** In-Process Denial-of-Service (DoS) and Distributed Denial-of-Service (DDoS) Simulation

---

## 📌 Overview

This project is a high-performance, multithreaded simulation implemented in C++ to demonstrate the behavioral mechanics and system impact of **Denial-of-Service (DoS)** and **Distributed Denial-of-Service (DDoS)** attacks on a resource-constrained server.

The simulation runs entirely in-process using standard C++ concurrency primitives (threads, mutexes, condition variables, and atomic counters) without generating actual malicious network traffic. It models real-world server exhaustion: queue overflows, worker starvation, latency, and service unavailability for legitimate clients.

---

## 🏗️ Architecture & Core Components

1. **Application Server (`ApplicationServer`)**:
   - **Worker Pool**: 4 concurrent worker threads consuming requests from a shared queue.
   - **Finite Request Queue**: Fixed buffer capacity (default: 15 requests) modeling bounded socket/application backlog.
   - **Service Time**: Simulated processing latency (default: 250 ms per request).
   - **Tail-Drop Buffer Policy**: Drops incoming requests once the capacity limit is reached.

2. **Legitimate Traffic Generator (`legitimateTrafficGenerator`)**:
   - Runs on a dedicated background thread.
   - Injects realistic user traffic (~3.5 requests/second) originating from subnet `192.168.1.0/24`.

3. **Attack Generators**:
   - **Single-Source DoS Burst (`launchDosBurst`)**: Simulates a high-volume burst originating from a single malicious host (`10.0.0.99`), quickly saturating the server queue.
   - **Multi-Source DDoS Botnet (`launchDdosFlood`)**: Spawns multiple concurrent threads representing botnet agents (each assigned unique virtual botnet IP addresses) flooding the server concurrently.

4. **Real-Time Monitor Dashboard (`serverMonitor`)**:
   - Periodically refreshes every 2.0 seconds.
   - Displays real-time worker thread states (`IDLE` vs. `BUSY`), queue saturation bar, drop rates, and cumulative availability percentage for legitimate requests.

---

## 📊 Monitored Metrics

- **Worker Pool Status:** Individual thread activity, active request ID, origin (`LEGIT` vs. `ATTACK`), and client IP.
- **Queue Utilization Bar:** Visual progress representation of queue occupancy `[####......]` against maximum capacity.
- **Traffic & Drop Rates (2s Window):**
  - Legitimate requests received, dropped, and drop rate in `req/sec`.
  - Attack requests received and dropped.
- **Cumulative Server Health:**
  - Total legitimate processed vs. dropped.
  - Normal service health availability percentage: $\frac{\text{Processed Legit}}{\text{Total Legit Recv}} \times 100\%$.

---

## ⚙️ Requirements & Prerequisites

- **Compiler:** Modern C++ compiler supporting C++17 (or C++11/C++14 minimum)
  - GCC / MinGW (`g++` 7.0+)
  - Clang (`clang++`)
  - MSVC (Visual Studio 2017+)
- **OS Support:** Windows, Linux, macOS

---

## 🚀 Compilation & Running

### Using GCC / MinGW (`g++`)

```bash
# Compile with C++17 and optimization enabled
g++ -std=c++17 -O2 -pthread dos_ddos_simulator.cpp -o dos_ddos_simulator

# Run the executable
./dos_ddos_simulator
```

*(On Windows PowerShell / CMD with MinGW, run `dos_ddos_simulator.exe`)*

### Using Clang (`clang++`)

```bash
clang++ -std=c++17 -O2 -pthread dos_ddos_simulator.cpp -o dos_ddos_simulator
./dos_ddos_simulator
```

---

## 🎮 Interactive Controls & Usage

Once the program is running, enter commands directly into the terminal prompt (`>`):

| Command | Example | Description |
|---|---|---|
| `<number>` | `40` | Fires a single-source **DoS burst** of `<number>` requests from a single attacker IP. |
| `ddos <bots> <reqs>` | `ddos 10 5` | Fires a distributed **DDoS flood** with `<bots>` concurrent bot threads, each sending `<reqs>` requests (Total: $\text{bots} \times \text{reqs}$). |
| `exit` or `quit` | `exit` | Gracefully terminates all background threads, stops the server, and exits. |

### Sample Workflow

1. Start the simulator and observe steady-state operation (~100% legitimate availability).
2. Enter `50` to observe queue saturation and immediate legitimate packet drops caused by a DoS burst.
3. Allow the server to recover once the queue drains.
4. Enter `ddos 12 8` to trigger a distributed botnet flood (96 concurrent attack requests) and analyze the impact on server health.
5. Enter `exit` to stop the simulation.

---

## 📁 Repository Structure

```text
2107064/
├── dos_ddos_simulator.cpp        # Complete C++ simulation source code
├── 2107064_DoS_DDoS_Report.pdf   # Detailed lab report and experimental analysis
└── README.md                     # Documentation and execution instructions
```

---

## 📄 License & Academic Integrity

This project is created for educational and academic evaluation under **CSE 4115: Computer Security Lab**. All network mechanisms and attack patterns are simulated in-process for analytical and learning purposes only.
