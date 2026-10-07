# ABV-IIITM Gwalior - College Management System

An integrated College Management System for **ABV-IIITM Gwalior** (Atal Bihari Vajpayee Indian Institute of Information Technology and Management).

The system combines a high-performance **C++ Object-Oriented REST API Backend** with a modern **HTML5, CSS3 & JavaScript Web Portal**.

---

## 🔑 Demo Role Credentials (1-Click Auto-Fill)

The web portal includes 1-click role selection pills that automatically populate the login fields with these credentials:

| Role | Username | Password | User Profile / ID | Key Capabilities |
| :--- | :--- | :--- | :--- | :--- |
| **🎓 Student** | `student1` | `123` | Aarav Sharma (`2024BCS-042`) | View enrolled courses, enroll in curriculum subjects, check class timetables, browse exam/quiz papers & library books |
| **👨‍🏫 Doctor / Faculty** | `doctor1` | `123` | Dr. Rajesh Verma (`FAC-108`) | Assign teaching courses, upload lecture exams & assignments, view enrolled students roster |
| **🧑‍💻 Teaching Assistant** | `ta1` | `123` | Priya Mukherjee (`TA-204`) | Manage practical lab courses, post weekly lab quizzes, track student lab attendance & grading |
| **⚙️ Administrator** | `admin` | `123` | Dean Academics (`ADM-001`) | Add new courses to IIITM database, catalog digital library books, view system metrics & records |

*(Configuration is also defined in [`.env`](file:///d:/college%20management%20system/College_System/.env) and [`.env.example`](file:///d:/college%20management%20system/College_System/.env.example))*

---

## 🏛️ System Architecture

* **Frontend:**
  * Modern Responsive Single Page Application: [`web/index.html`](file:///d:/college%20management%20system/College_System/web/index.html)
  * Design System & Glassmorphism Styles: [`web/styles.css`](file:///d:/college%20management%20system/College_System/web/styles.css)
  * REST API Client Layer & State Sync: [`web/api.js`](file:///d:/college%20management%20system/College_System/web/api.js)
  * UI Controller & Navigation: [`web/app.js`](file:///d:/college%20management%20system/College_System/web/app.js)
* **Backend (C++):**
  * Multi-threaded Winsock2 REST API Server: [`College_system/api_server.cpp`](file:///d:/college%20management%20system/College_System/College_system/api_server.cpp)
  * Object-Oriented Domain Models: [`Courses`](file:///d:/college%20management%20system/College_System/College_system/include/Courses.h), [`Student`](file:///d:/college%20management%20system/College_System/College_system/include/Student.h), [`Doctors`](file:///d:/college%20management%20system/College_System/College_system/include/Doctors.h), [`Teaching_Assistant`](file:///d:/college%20management%20system/College_System/College_system/include/Teaching_Assistant.h), [`Administrator`](file:///d:/college%20management%20system/College_System/College_system/include/Administrator.h), [`StaffData`](file:///d:/college%20management%20system/College_System/College_system/include/StaffData.h), [`ShowData`](file:///d:/college%20management%20system/College_System/College_system/include/ShowData.h), [`Books`](file:///d:/college%20management%20system/College_System/College_system/include/Books.h).
  * Storage: Flat-file database in [`College_system/DataBase/`](file:///d:/college%20management%20system/College_System/College_system/DataBase).

---

## 🌐 Computer Networks & Docker Architecture (Redis Container)

The project integrates a **`redis:7-alpine` Docker container** communicating over an isolated Docker bridge network (**`iiitm-network`**) with the C++ backend. This directly demonstrates core Computer Networks principles:

```
+-------------------------------------------------------------------------------+
|                       ISOLATED DOCKER BRIDGE (iiitm-network)                  |
|                                                                               |
|   +--------------------------+               +----------------------------+   |
|   |   iiitm-college-portal   |               |     iiitm-redis-cache      |   |
|   |   (C++ REST & Web :8080) | <--- TCP ---> |    (redis:7-alpine :6379)  |   |
|   +--------------------------+  RESP Protocol+----------------------------+   |
|                 |                                                             |
+-----------------|-------------------------------------------------------------+
                  | Port 8080 (HTTP)
                  v
       +--------------------+
       | Active Web Clients | (Real-Time Pub/Sub Feeds & Sub-ms Cache Hits)
       +--------------------+
```

### 🧠 Concepts Demonstrated:
1. **In-Memory Network Caching & RTT Optimization:**
   * High-traffic course catalog and book queries are cached in Redis RAM over TCP (`RESP` protocol).
   * Reduces Round-Trip Time (RTT) from disk I/O latency (**~45ms**) to sub-millisecond RAM lookups (**< 0.4ms**), preventing database contention during registration rushes.
2. **Publish-Subscribe (Pub/Sub) Campus Broadcasts:**
   * Decoupled asynchronous messaging on Redis channel `iiitm_campus_feed`.
   * When Doctors upload exams or Admins publish courses, the backend publishes an event packet over the internal TCP network, broadcasting instantaneously to all connected student browser sessions.
3. **Session State & TTL Key Expiration:**
   * User login tokens (`session:student1`) are stored with 3600-second Time-To-Live (TTL) expiration in Redis memory for secure, stateless multi-node authentication.
4. **Multi-Container Bridge Networking:**
   * Container service discovery and inter-process communication using Docker's internal DNS resolver over the private `iiitm-network` subnet.

---

## 🐳 Running with Docker Compose (Recommended)

To run the complete multi-container stack (Redis + C++ API + Web Portal) with one command:

```bash
docker compose up --build
```

Then navigate to: **`http://localhost:8080/`** in your browser.

To stop the containers:
```bash
docker compose down
```

---

## 🚀 Running Locally (Native Binary Mode)

### Option 1: Open Web Portal Directly (Browser Mode)
Double-click or open **[`web/index.html`](file:///d:/college%20management%20system/College_System/web/index.html)** in any web browser.

### Option 2: Run Native C++ Server
1. Navigate to [`College_system`](file:///d:/college%20management%20system/College_System/College_system).
2. Execute the build & launch script:
   ```cmd
   build_and_run_server.bat
   ```
3. Open **`http://localhost:8080/`** in your browser.

---

## 👤 Developer
* Developed by Me
* ABV-IIITM Gwalior (IIITM GWL)

