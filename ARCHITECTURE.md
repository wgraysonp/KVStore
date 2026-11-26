# Mini-Borg Distributed State Engine (Overview)

## 🚀 Project Vision
The goal of this project is to transition from local high-performance compute algorithms (like vector search math) to high-reliability, fault-tolerant infrastructure systems. You are building a Distributed, Replicated Key-Value Store from scratch using raw Linux system calls and Google's foundational open-source stack (Bazel, Abseil, Clang-Format). 

This architecture mirrors the core engineering principles of Google's **Chubby** (their internal lock manager) and **Borgmaster**, which coordinate global task scheduling and cluster state survival across millions of production machines.

---

## 🛠️ The Three Component Layers

### 1. The Network Core (Linux epoll Multiplexer)
* **What it does**: Handles thousands of concurrent client connections on a single execution thread without spawning massive amounts of resource-heavy OS threads.
* **Systems Concepts**: Non-blocking I/O, POSIX sockets (`socket`, `bind`, `listen`, `accept`), and Linux-native `epoll` (`epoll_create1`, `epoll_ctl`, `epoll_wait`).
* **Why it matters**: Demystifies exactly how Google's internal RPC architecture (Stubby/gRPC) and Meta's networking frameworks (Folly AsyncSocket) route packets at the kernel boundary.

### 2. The Storage Core (Write-Ahead Log / WAL)
* **What it does**: Intercepts mutating state commands (`PUT`). Before updating volatile memory, it persists the operations as a binary, length-prefixed stream to disk. If the server loses power or crashes, it parses this file on reboot to fully restore the database. 
* **Systems Concepts**: Memory vs. Storage durability, raw file descriptors (`open`, `write`), and forcing physical SSD block layout commits using the Linux `fsync()` kernel boundaries.
* **Why it matters**: Teaches you how database engines achieve true data durability and crash resilience.
* **NOTES** 9/24 - stick with global locking for now. Come back and try LSM-Tree later

### 3. The Consensus Engine (State Machine Replication)
* **What it does**: Connects multiple individual instances of your server over the network. They communicate via raw sockets to elect a "Leader" node and use a consensus mechanism (like Raft or Paxos) to guarantee that updates are replicated across a quorum of servers.
* **Systems Concepts**: Network partitions, split-brain mitigation, quorum math (⌊n/2⌋ + 1), and distributed logical time vs. physical clocks.
* **Why it matters**: This is the exact heartbeat of Google Borg. Distributed consensus allows cluster orchestrators to survive entire data centers catching fire without losing track of running services.

---

## 💻 Tech Stack Constraints (Google Standard Alignment)
* **Operating System**: Native Linux (ARM64 Ubuntu via OrbStack Hypervisor).
* **Build Architecture**: Google Bazel (utilizing modern `MODULE.bazel` with strict `rules_cc` encapsulation rules).
* **Data Structures & Locks**: Google Abseil (`absl::flat_hash_map`, `absl::Mutex`, `absl::StrCat`).
* **Style Guidelines**: Automated formatting running Google C++ Style Rules inside VS Code (`clang-format` and `buildifier`).
