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

## 🚀 How to Run

### Option 1: Open Frontend Directly (Instant Browser Mode)
Double-click or open **[`web/index.html`](file:///d:/college%20management%20system/College_System/web/index.html)** in any modern web browser (Chrome, Edge, Firefox, Brave).
* Runs locally with interactive role switching, course enrollment, library catalog, and academic materials.

### Option 2: Run C++ REST API Server
To connect the web interface with the native C++ backend:
1. Open a terminal in [`College_system`](file:///d:/college%20management%20system/College_System/College_system).
2. Execute the build script:
   ```cmd
   build_and_run_server.bat
   ```
   Or compile manually using `g++`:
   ```cmd
   g++ -std=c++11 api_server.cpp src/*.cpp -Iinclude -lws2_32 -o college_api_server.exe
   college_api_server.exe
   ```
3. Open `http://localhost:8080/` in your browser. The live connection badge will turn green (`C++ REST API (Port 8080) Connected`).

---

## 📡 C++ REST API Endpoints

| Endpoint | Method | Description |
| :--- | :--- | :--- |
| `/api/status` | `GET` | Health check & college overview statistics |
| `/api/auth/login` | `POST` | Authenticate Student, Doctor, TA, or Administrator |
| `/api/auth/signup` | `POST` | Register new user account |
| `/api/courses` | `GET`, `POST` | List all courses / Add new course |
| `/api/student/courses` | `GET` | Get enrolled courses for a student |
| `/api/student/enroll` | `POST` | Enroll student into a course |
| `/api/doctor/courses` | `GET` | Get courses assigned to a faculty member |
| `/api/doctor/assign` | `POST` | Assign course to a doctor |
| `/api/students` | `GET` | Retrieve student roster and profiles |
| `/api/books` | `GET`, `POST` | Digital library books catalog |
| `/api/materials` | `GET`, `POST` | Filter & publish exams, assignments, quizzes |

---

## 👤 Developer
* Developed by Me
