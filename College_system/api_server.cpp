#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <algorithm>
#include <sys/stat.h>

#include "Courses.h"
#include "Student.h"
#include "Doctors.h"
#include "Teaching_Assistant.h"
#include "Administrator.h"
#include "ShowData.h"
#include "Books.h"
#include "RedisClient.h"

using namespace std;

static const int DEFAULT_PORT = 8080;

#ifdef _WIN32
static CRITICAL_SECTION db_cs;
struct AutoLock {
    AutoLock() { EnterCriticalSection(&db_cs); }
    ~AutoLock() { LeaveCriticalSection(&db_cs); }
};
#else
static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
struct AutoLock {
    AutoLock() { pthread_mutex_lock(&db_mutex); }
    ~AutoLock() { pthread_mutex_unlock(&db_mutex); }
};
#endif

// Global Redis TCP Client instance
static RedisClient redisClient;

// Event buffer for real-time announcements
static deque<string> eventBuffer;

// Utility functions for string manipulation and JSON
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

string escapeJSON(const string& input) {
    string output;
    for (char c : input) {
        if (c == '"') output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\b') output += "\\b";
        else if (c == '\f') output += "\\f";
        else if (c == '\n') output += "\\n";
        else if (c == '\r') output += "\\r";
        else if (c == '\t') output += "\\t";
        else output += c;
    }
    return output;
}

// Simple JSON extraction helper
string extractJSONField(const string& json, const string& key) {
    string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == string::npos) return "";

    pos = json.find(':', pos);
    if (pos == string::npos) return "";
    pos++;

    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        pos++;
    }

    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end == string::npos) return "";
        return json.substr(pos, end - pos);
    } else {
        size_t end = json.find_first_of(",}\r\n ", pos);
        if (end == string::npos) end = json.size();
        return trim(json.substr(pos, end - pos));
    }
}

// Broadcast event to Redis Pub/Sub and in-memory queue
void broadcastEvent(const string& eventType, const string& title, const string& details) {
    stringstream eventJSON;
    eventJSON << "{"
              << "\"type\":\"" << escapeJSON(eventType) << "\","
              << "\"title\":\"" << escapeJSON(title) << "\","
              << "\"details\":\"" << escapeJSON(details) << "\","
              << "\"timestamp\":\"" << time(NULL) << "\""
              << "}";

    string payload = eventJSON.str();
    
    // 1. Publish to Redis network channel over TCP
    redisClient.publish("iiitm_campus_feed", payload);

    // 2. Keep in local event queue
    eventBuffer.push_back(payload);
    if (eventBuffer.size() > 20) {
        eventBuffer.pop_front();
    }
}

// Ensure required directory paths exist
void ensureDirectories() {
#ifdef _WIN32
    CreateDirectoryA("DataBase", NULL);
    CreateDirectoryA("DataBase\\StaffData", NULL);
    CreateDirectoryA("DataBase\\Courses", NULL);
    CreateDirectoryA("DataBase\\StudentCourses", NULL);
    CreateDirectoryA("DataBase\\DoctorCourses", NULL);
    CreateDirectoryA("DataBase\\TeachingAssistantCourses", NULL);
    CreateDirectoryA("DataBase\\Assignments", NULL);
    CreateDirectoryA("DataBase\\Exams", NULL);
    CreateDirectoryA("DataBase\\PracticalExams", NULL);
    CreateDirectoryA("DataBase\\Quizzes", NULL);
    CreateDirectoryA("DataBase\\Tables", NULL);
    CreateDirectoryA("DataBase\\Books", NULL);
#else
    mkdir("DataBase", 0777);
    mkdir("DataBase/StaffData", 0777);
    mkdir("DataBase/Courses", 0777);
    mkdir("DataBase/StudentCourses", 0777);
    mkdir("DataBase/DoctorCourses", 0777);
    mkdir("DataBase/TeachingAssistantCourses", 0777);
    mkdir("DataBase/Assignments", 0777);
    mkdir("DataBase/Exams", 0777);
    mkdir("DataBase/PracticalExams", 0777);
    mkdir("DataBase/Quizzes", 0777);
    mkdir("DataBase/Tables", 0777);
    mkdir("DataBase/Books", 0777);
#endif
}

// HTTP Request structure
struct HttpRequest {
    string method;
    string path;
    string queryString;
    map<string, string> queryParams;
    map<string, string> headers;
    string body;
};

// URL Query decoder
map<string, string> parseQueryParams(const string& query) {
    map<string, string> params;
    stringstream ss(query);
    string pair;
    while (getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != string::npos) {
            string key = pair.substr(0, eq);
            string val = pair.substr(eq + 1);
            string decodedVal;
            for (size_t i = 0; i < val.length(); ++i) {
                if (val[i] == '+') decodedVal += ' ';
                else if (val[i] == '%' && i + 2 < val.length()) {
                    string hexStr = val.substr(i + 1, 2);
                    char chr = (char)strtol(hexStr.c_str(), NULL, 16);
                    decodedVal += chr;
                    i += 2;
                } else {
                    decodedVal += val[i];
                }
            }
            params[key] = decodedVal;
        }
    }
    return params;
}

// Send HTTP Response with CORS & Custom Headers
void sendResponse(SOCKET clientSocket, int statusCode, const string& contentType, const string& body, const string& cacheHeader = "") {
    string statusText = "OK";
    if (statusCode == 200) statusText = "OK";
    else if (statusCode == 201) statusText = "Created";
    else if (statusCode == 204) statusText = "No Content";
    else if (statusCode == 400) statusText = "Bad Request";
    else if (statusCode == 401) statusText = "Unauthorized";
    else if (statusCode == 404) statusText = "Not Found";
    else if (statusCode == 500) statusText = "Internal Server Error";

    stringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    response << "Content-Type: " << contentType << "; charset=utf-8\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
    if (!cacheHeader.empty()) {
        response << "X-Cache-Status: " << cacheHeader << "\r\n";
    }
    response << "Connection: close\r\n\r\n";
    response << body;

    string respStr = response.str();
    send(clientSocket, respStr.c_str(), (int)respStr.length(), 0);
}

// Serve static web file
bool serveStaticFile(SOCKET clientSocket, const string& filepath, const string& mimeType) {
    ifstream file(filepath.c_str(), ios::binary);
    if (!file.is_open()) {
        return false;
    }
    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();
    sendResponse(clientSocket, 200, mimeType, content);
    return true;
}

// C++ API Route Handlers

// 1. Status & Redis Cache Stats
void handleStatus(SOCKET clientSocket) {
    AutoLock lock;
    int studentsCount = 0, doctorsCount = 0, tasCount = 0, coursesCount = 0;

    ifstream fs("DataBase/StaffData/Students.txt");
    string line;
    while (getline(fs, line)) if (!trim(line).empty()) studentsCount++;
    fs.close();

    ifstream fd("DataBase/StaffData/Doctors.txt");
    while (getline(fd, line)) if (!trim(line).empty()) doctorsCount++;
    fd.close();

    ifstream ft("DataBase/StaffData/Teaching Assistants.txt");
    while (getline(ft, line)) if (!trim(line).empty()) tasCount++;
    ft.close();

    ifstream fc("DataBase/Courses/Courses.txt");
    while (getline(fc, line)) if (!trim(line).empty()) coursesCount++;
    fc.close();

    bool isRedisUp = redisClient.isConnected();

    stringstream json;
    json << "{\n"
         << "  \"success\": true,\n"
         << "  \"college\": \"ABV-IIITM Gwalior (IIITM GWL)\",\n"
         << "  \"system\": \"College Management System C++ Backend\",\n"
         << "  \"version\": \"2.1.0\",\n"
         << "  \"network\": {\n"
         << "    \"redis_cache\": \"" << (isRedisUp ? "ONLINE (Sub-millisecond TCP RAM Cache)" : "STANDBY (Disk-Fallback Mode)") << "\",\n"
         << "    \"redis_host\": \"" << redisClient.getHost() << "\",\n"
         << "    \"redis_port\": " << redisClient.getPort() << ",\n"
         << "    \"cache_hits\": " << redisClient.getHits() << ",\n"
         << "    \"cache_misses\": " << redisClient.getMisses() << "\n"
         << "  },\n"
         << "  \"stats\": {\n"
         << "    \"students\": " << studentsCount << ",\n"
         << "    \"doctors\": " << doctorsCount << ",\n"
         << "    \"teaching_assistants\": " << tasCount << ",\n"
         << "    \"courses\": " << coursesCount << "\n"
         << "  }\n"
         << "}";
    sendResponse(clientSocket, 200, "application/json", json.str());
}

// 2. Real-Time Campus Events (Redis Pub/Sub Buffer)
void handleLiveEvents(SOCKET clientSocket) {
    AutoLock lock;
    stringstream json;
    json << "{\n"
         << "  \"success\": true,\n"
         << "  \"feed\": [\n";
    for (size_t i = 0; i < eventBuffer.size(); ++i) {
        json << "    " << eventBuffer[i] << (i + 1 < eventBuffer.size() ? "," : "") << "\n";
    }
    json << "  ]\n}";
    sendResponse(clientSocket, 200, "application/json", json.str());
}

// 3. User Login & Session Creation
void handleLogin(SOCKET clientSocket, const HttpRequest& req) {
    AutoLock lock;
    string username = extractJSONField(req.body, "username");
    string password = extractJSONField(req.body, "password");
    string role = extractJSONField(req.body, "role");

    if (username.empty() || password.empty()) {
        sendResponse(clientSocket, 400, "application/json", "{\"success\":false,\"message\":\"Username and password are required.\"}");
        return;
    }

    string filename = "DataBase/StaffData/Students.txt";
    if (role == "Doctor" || role == "1") filename = "DataBase/StaffData/Doctors.txt";
    else if (role == "Teaching Assistant" || role == "3") filename = "DataBase/StaffData/Teaching Assistants.txt";
    else if (role == "Administrator" || role == "4") filename = "DataBase/StaffData/Administrator.txt";

    ifstream in(filename.c_str());
    if (!in.is_open()) {
        sendResponse(clientSocket, 404, "application/json", "{\"success\":false,\"message\":\"User records database file not found.\"}");
        return;
    }

    bool matched = false;
    string u, p, id, name, phone, email, bdate, addr, gender, type;
    string line;

    while (getline(in, line)) {
        if (trim(line).empty()) continue;
        stringstream ss(line);
        getline(ss, u, ',');
        getline(ss, p, ',');
        getline(ss, id, ',');
        getline(ss, name, ',');
        getline(ss, phone, ',');
        getline(ss, email, ',');
        getline(ss, bdate, ',');
        getline(ss, addr, ',');
        getline(ss, gender, ',');
        getline(ss, type);

        if (u == username && p == password) {
            matched = true;
            break;
        }
    }
    in.close();

    if (matched) {
        // Cache user session in Redis with 1-hour TTL
        string sessionKey = "session:" + u;
        redisClient.set(sessionKey, id, 3600);

        stringstream json;
        json << "{\n"
             << "  \"success\": true,\n"
             << "  \"message\": \"Login successful\",\n"
             << "  \"session\": {\n"
             << "    \"token\": \"" << escapeJSON(sessionKey) << "\",\n"
             << "    \"ttl\": 3600\n"
             << "  },\n"
             << "  \"user\": {\n"
             << "    \"username\": \"" << escapeJSON(u) << "\",\n"
             << "    \"id\": \"" << escapeJSON(id) << "\",\n"
             << "    \"name\": \"" << escapeJSON(name) << "\",\n"
             << "    \"phone\": \"" << escapeJSON(phone) << "\",\n"
             << "    \"email\": \"" << escapeJSON(email) << "\",\n"
             << "    \"birthDate\": \"" << escapeJSON(bdate) << "\",\n"
             << "    \"address\": \"" << escapeJSON(addr) << "\",\n"
             << "    \"gender\": \"" << escapeJSON(gender) << "\",\n"
             << "    \"role\": \"" << escapeJSON(type) << "\",\n"
             << "    \"college\": \"IIITM GWL\"\n"
             << "  }\n"
             << "}";
        sendResponse(clientSocket, 200, "application/json", json.str());
    } else {
        sendResponse(clientSocket, 401, "application/json", "{\"success\":false,\"message\":\"Invalid username or password for specified role.\"}");
    }
}

// 4. Registration
void handleSignup(SOCKET clientSocket, const HttpRequest& req) {
    AutoLock lock;
    string username = extractJSONField(req.body, "username");
    string password = extractJSONField(req.body, "password");
    string id = extractJSONField(req.body, "id");
    string name = extractJSONField(req.body, "name");
    string phone = extractJSONField(req.body, "phone");
    string email = extractJSONField(req.body, "email");
    string birthDate = extractJSONField(req.body, "birthDate");
    string address = extractJSONField(req.body, "address");
    string gender = extractJSONField(req.body, "gender");
    string role = extractJSONField(req.body, "role");

    if (username.empty() || password.empty() || id.empty() || name.empty()) {
        sendResponse(clientSocket, 400, "application/json", "{\"success\":false,\"message\":\"Username, password, ID, and name are required.\"}");
        return;
    }

    if (role.empty()) role = "Student";

    string filename = "DataBase/StaffData/Students.txt";
    if (role == "Doctor" || role == "1") { filename = "DataBase/StaffData/Doctors.txt"; role = "Doctor"; }
    else if (role == "Teaching Assistant" || role == "3") { filename = "DataBase/StaffData/Teaching Assistants.txt"; role = "Teaching Assistant"; }
    else if (role == "Administrator" || role == "4") { filename = "DataBase/StaffData/Administrator.txt"; role = "Administrator"; }

    // Check if username already exists
    ifstream checkIn(filename.c_str());
    if (checkIn.is_open()) {
        string line;
        while (getline(checkIn, line)) {
            if (trim(line).empty()) continue;
            stringstream ss(line);
            string existUser;
            getline(ss, existUser, ',');
            if (existUser == username) {
                checkIn.close();
                sendResponse(clientSocket, 400, "application/json", "{\"success\":false,\"message\":\"Username already registered.\"}");
                return;
            }
        }
        checkIn.close();
    }

    ofstream out(filename.c_str(), ios::app);
    if (!out.is_open()) {
        sendResponse(clientSocket, 500, "application/json", "{\"success\":false,\"message\":\"Could not open database file to write.\"}");
        return;
    }

    out << username << ","
        << password << ","
        << id << ","
        << name << ","
        << phone << ","
        << email << ","
        << birthDate << ","
        << address << ","
        << gender << ","
        << role << endl;
    out.close();

    // Broadcast new registration
    broadcastEvent("NEW_REGISTRATION", name + " (" + role + ") joined IIITM Portal", "ID: " + id);

    stringstream json;
    json << "{\n"
         << "  \"success\": true,\n"
         << "  \"message\": \"Registration completed successfully\",\n"
         << "  \"user\": {\n"
         << "    \"username\": \"" << escapeJSON(username) << "\",\n"
         << "    \"id\": \"" << escapeJSON(id) << "\",\n"
         << "    \"name\": \"" << escapeJSON(name) << "\",\n"
         << "    \"role\": \"" << escapeJSON(role) << "\"\n"
         << "  }\n"
         << "}";
    sendResponse(clientSocket, 201, "application/json", json.str());
}

// 5. Get Courses with Redis In-Memory Caching
void handleGetCourses(SOCKET clientSocket) {
    // Check Redis in-memory cache first (Network Caching Concept)
    string cachedCourses = redisClient.get("cache:courses");
    if (!cachedCourses.empty()) {
        sendResponse(clientSocket, 200, "application/json", cachedCourses, "HIT (Redis In-Memory Cache)");
        return;
    }

    AutoLock lock;
    ifstream in("DataBase/Courses/Courses.txt");
    vector<Courses> courses;
    if (in.is_open()) {
        while (!in.eof()) {
            string line;
            if (getline(in, line)) {
                if (trim(line).empty()) continue;
                stringstream ss(line);
                string cid, cname, chours;
                getline(ss, cid, ',');
                getline(ss, cname, ',');
                getline(ss, chours);
                courses.push_back(Courses(cid, cname, chours));
            }
        }
        in.close();
    }

    stringstream json;
    json << "{\n  \"success\": true,\n  \"source\": \"Disk & Loaded into Redis Cache\",\n  \"courses\": [\n";
    for (size_t i = 0; i < courses.size(); ++i) {
        json << "    {\n"
             << "      \"id\": \"" << escapeJSON(courses[i].getID()) << "\",\n"
             << "      \"name\": \"" << escapeJSON(courses[i].getName()) << "\",\n"
             << "      \"hours\": \"" << escapeJSON(courses[i].getHours()) << "\"\n"
             << "    }" << (i + 1 < courses.size() ? "," : "") << "\n";
    }
    json << "  ]\n}";
    string jsonStr = json.str();

    // Cache in Redis for 180 seconds
    redisClient.set("cache:courses", jsonStr, 180);

    sendResponse(clientSocket, 200, "application/json", jsonStr, "MISS (Populated Redis Cache)");
}

// 6. Add Course with Cache Invalidation & Pub/Sub Broadcast
void handleAddCourse(SOCKET clientSocket, const HttpRequest& req) {
    AutoLock lock;
    string cid = extractJSONField(req.body, "id");
    string cname = extractJSONField(req.body, "name");
    string chours = extractJSONField(req.body, "hours");

    if (cid.empty() || cname.empty() || chours.empty()) {
        sendResponse(clientSocket, 400, "application/json", "{\"success\":false,\"message\":\"Course ID, Name, and Hours are required.\"}");
        return;
    }

    ofstream out("DataBase/Courses/Courses.txt", ios::app);
    if (!out.is_open()) {
        sendResponse(clientSocket, 500, "application/json", "{\"success\":false,\"message\":\"Failed to write course.\"}");
        return;
    }
    out << cid << "," << cname << "," << chours << endl;
    out.close();

    // 1. Invalidate Redis cache so subsequent GET calls get fresh data
    redisClient.del("cache:courses");

    // 2. Publish broadcast event over Redis Pub/Sub
    broadcastEvent("NEW_COURSE_ANNOUNCEMENT", "New Course Added: " + cname, "Course Code: " + cid + " (" + chours + " hrs)");

    sendResponse(clientSocket, 201, "application/json", "{\"success\":true,\"message\":\"Course added & broadcasted to campus network via Redis!\"}");
}

// 7. Student Enrollment
void handleGetStudentCourses(SOCKET clientSocket, const HttpRequest& req) {
    AutoLock lock;
    string name = req.queryParams.count("name") ? req.queryParams.at("name") : "";
    string id = req.queryParams.count("id") ? req.queryParams.at("id") : "";

    string filepath = "DataBase/StudentCourses/" + name + id + ".txt";
    ifstream in(filepath.c_str());
    vector<Courses> list;
    if (in.is_open()) {
        string line;
        while (getline(in, line)) {
            if (trim(line).empty()) continue;
            stringstream ss(line);
            string cid, cname, chours;
            getline(ss, cid, ',');
            getline(ss, cname, ',');
            getline(ss, chours);
            list.push_back(Courses(cid, cname, chours));
        }
        in.close();
    }

    stringstream json;
    json << "{\n  \"success\": true,\n  \"courses\": [\n";
    for (size_t i = 0; i < list.size(); ++i) {
        json << "    {\n"
             << "      \"id\": \"" << escapeJSON(list[i].getID()) << "\",\n"
             << "      \"name\": \"" << escapeJSON(list[i].getName()) << "\",\n"
             << "      \"hours\": \"" << escapeJSON(list[i].getHours()) << "\"\n"
             << "    }" << (i + 1 < list.size() ? "," : "") << "\n";
    }
    json << "  ]\n}";
    sendResponse(clientSocket, 200, "application/json", json.str());
}

void handleStudentEnroll(SOCKET clientSocket, const HttpRequest& req) {
    AutoLock lock;
    string name = extractJSONField(req.body, "name");
    string studentId = extractJSONField(req.body, "id");
    string courseId = extractJSONField(req.body, "courseId");

    if (name.empty() || studentId.empty() || courseId.empty()) {
        sendResponse(clientSocket, 400, "application/json", "{\"success\":false,\"message\":\"Student name, student ID, and course ID are required.\"}");
        return;
    }

    // Lookup course details
    ifstream in("DataBase/Courses/Courses.txt");
    string foundName = "", foundHours = "";
    bool found = false;
    if (in.is_open()) {
        string line;
        while (getline(in, line)) {
            if (trim(line).empty()) continue;
            stringstream ss(line);
            string cid, cname, chours;
            getline(ss, cid, ',');
            getline(ss, cname, ',');
            getline(ss, chours);
            if (cid == courseId) {
                found = true;
                foundName = cname;
                foundHours = chours;
                break;
            }
        }
        in.close();
    }

    if (!found) {
        sendResponse(clientSocket, 404, "application/json", "{\"success\":false,\"message\":\"Course not found with provided ID.\"}");
        return;
    }

    string filepath = "DataBase/StudentCourses/" + name + studentId + ".txt";

    // Check if already enrolled
    ifstream checkIn(filepath.c_str());
    if (checkIn.is_open()) {
        string line;
        while (getline(checkIn, line)) {
            if (trim(line).empty()) continue;
            stringstream ss(line);
            string cid;
            getline(ss, cid, ',');
            if (cid == courseId) {
                checkIn.close();
                sendResponse(clientSocket, 400, "application/json", "{\"success\":false,\"message\":\"Student is already enrolled in this course.\"}");
                return;
            }
        }
        checkIn.close();
    }

    ofstream out(filepath.c_str(), ios::app);
    if (!out.is_open()) {
        sendResponse(clientSocket, 500, "application/json", "{\"success\":false,\"message\":\"Failed to save student course enrollment.\"}");
        return;
    }
    out << courseId << "," << foundName << "," << foundHours << endl;
    out.close();

    // Broadcast enrollment
    broadcastEvent("COURSE_ENROLLMENT", name + " enrolled in " + foundName, "Roll: " + studentId);

    sendResponse(clientSocket, 200, "application/json", "{\"success\":true,\"message\":\"Enrolled successfully in " + escapeJSON(foundName) + "\"}");
}

void handleGetDoctorCourses(SOCKET clientSocket, const HttpRequest& req) {
    AutoLock lock;
    string name = req.queryParams.count("name") ? req.queryParams.at("name") : "";
    string id = req.queryParams.count("id") ? req.queryParams.at("id") : "";

    string filepath = "DataBase/DoctorCourses/" + name + id + ".txt";
    ifstream in(filepath.c_str());
    vector<Courses> list;
    if (in.is_open()) {
        string line;
        while (getline(in, line)) {
            if (trim(line).empty()) continue;
            stringstream ss(line);
            string cid, cname, chours;
            getline(ss, cid, ',');
            getline(ss, cname, ',');
            getline(ss, chours);
            list.push_back(Courses(cid, cname, chours));
        }
        in.close();
    }

    stringstream json;
    json << "{\n  \"success\": true,\n  \"courses\": [\n";
    for (size_t i = 0; i < list.size(); ++i) {
        json << "    {\n"
             << "      \"id\": \"" << escapeJSON(list[i].getID()) << "\",\n"
             << "      \"name\": \"" << escapeJSON(list[i].getName()) << "\",\n"
             << "      \"hours\": \"" << escapeJSON(list[i].getHours()) << "\"\n"
             << "    }" << (i + 1 < list.size() ? "," : "") << "\n";
    }
    json << "  ]\n}";
    sendResponse(clientSocket, 200, "application/json", json.str());
}

void handleDoctorAssign(SOCKET clientSocket, const HttpRequest& req) {
    AutoLock lock;
    string name = extractJSONField(req.body, "name");
    string doctorId = extractJSONField(req.body, "id");
    string courseId = extractJSONField(req.body, "courseId");

    if (name.empty() || doctorId.empty() || courseId.empty()) {
        sendResponse(clientSocket, 400, "application/json", "{\"success\":false,\"message\":\"Doctor name, ID, and Course ID are required.\"}");
        return;
    }

    // Lookup course
    ifstream in("DataBase/Courses/Courses.txt");
    string foundName = "", foundHours = "";
    bool found = false;
    if (in.is_open()) {
        string line;
        while (getline(in, line)) {
            if (trim(line).empty()) continue;
            stringstream ss(line);
            string cid, cname, chours;
            getline(ss, cid, ',');
            getline(ss, cname, ',');
            getline(ss, chours);
            if (cid == courseId) {
                found = true;
                foundName = cname;
                foundHours = chours;
                break;
            }
        }
        in.close();
    }

    if (!found) {
        sendResponse(clientSocket, 404, "application/json", "{\"success\":false,\"message\":\"Course not found.\"}");
        return;
    }

    string filepath = "DataBase/DoctorCourses/" + name + doctorId + ".txt";
    ofstream out(filepath.c_str(), ios::app);
    if (!out.is_open()) {
        sendResponse(clientSocket, 500, "application/json", "{\"success\":false,\"message\":\"Failed to assign course to doctor.\"}");
        return;
    }
    out << courseId << "," << foundName << "," << foundHours << endl;
    out.close();

    broadcastEvent("FACULTY_ASSIGNMENT", name + " assigned to teach " + foundName, "Course: " + courseId);

    sendResponse(clientSocket, 200, "application/json", "{\"success\":true,\"message\":\"Course assigned to Doctor successfully.\"}");
}

void handleGetStudents(SOCKET clientSocket) {
    AutoLock lock;
    ifstream in("DataBase/StaffData/Students.txt");
    stringstream json;
    json << "{\n  \"success\": true,\n  \"students\": [\n";
    bool first = true;
    if (in.is_open()) {
        string line;
        while (getline(in, line)) {
            if (trim(line).empty()) continue;
            stringstream ss(line);
            string u, p, id, name, phone, email, bdate, addr, gender, type;
            getline(ss, u, ',');
            getline(ss, p, ',');
            getline(ss, id, ',');
            getline(ss, name, ',');
            getline(ss, phone, ',');
            getline(ss, email, ',');
            getline(ss, bdate, ',');
            getline(ss, addr, ',');
            getline(ss, gender, ',');
            getline(ss, type);

            if (!first) json << ",\n";
            first = false;
            json << "    {\n"
                 << "      \"username\": \"" << escapeJSON(u) << "\",\n"
                 << "      \"id\": \"" << escapeJSON(id) << "\",\n"
                 << "      \"name\": \"" << escapeJSON(name) << "\",\n"
                 << "      \"phone\": \"" << escapeJSON(phone) << "\",\n"
                 << "      \"email\": \"" << escapeJSON(email) << "\",\n"
                 << "      \"gender\": \"" << escapeJSON(gender) << "\",\n"
                 << "      \"address\": \"" << escapeJSON(addr) << "\"\n"
                 << "    }";
        }
        in.close();
    }
    json << "\n  ]\n}";
    sendResponse(clientSocket, 200, "application/json", json.str());
}

void handleGetBooks(SOCKET clientSocket) {
    // Check Redis Cache
    string cachedBooks = redisClient.get("cache:books");
    if (!cachedBooks.empty()) {
        sendResponse(clientSocket, 200, "application/json", cachedBooks, "HIT (Redis In-Memory Cache)");
        return;
    }

    stringstream json;
    json << "{\n  \"success\": true,\n  \"books\": [\n"
         << "    {\"id\": \"BK101\", \"title\": \"Discrete Mathematics and Its Applications\", \"author\": \"Kenneth Rosen\", \"year\": \"First Year\", \"semester\": \"Semester 1\", \"category\": \"Mathematics\"},\n"
         << "    {\"id\": \"BK102\", \"title\": \"Introduction to Algorithms (CLRS)\", \"author\": \"Cormen, Leiserson, Rivest, Stein\", \"year\": \"Second Year\", \"semester\": \"Semester 1\", \"category\": \"Computer Science\"},\n"
         << "    {\"id\": \"BK103\", \"title\": \"Digital Design and Computer Architecture\", \"author\": \"David Harris, Sarah Harris\", \"year\": \"First Year\", \"semester\": \"Semester 2\", \"category\": \"Hardware\"},\n"
         << "    {\"id\": \"BK104\", \"title\": \"Operating System Concepts\", \"author\": \"Silberschatz, Galvin, Gagne\", \"year\": \"Second Year\", \"semester\": \"Semester 2\", \"category\": \"Systems\"},\n"
         << "    {\"id\": \"BK105\", \"title\": \"Database System Concepts\", \"author\": \"Abraham Silberschatz\", \"year\": \"Third Year\", \"semester\": \"Semester 1\", \"category\": \"Databases\"},\n"
         << "    {\"id\": \"BK106\", \"title\": \"Computer Networking: A Top-Down Approach\", \"author\": \"James Kurose, Keith Ross\", \"year\": \"Third Year\", \"semester\": \"Semester 2\", \"category\": \"Networks\"},\n"
         << "    {\"id\": \"BK107\", \"title\": \"Artificial Intelligence: A Modern Approach\", \"author\": \"Stuart Russell, Peter Norvig\", \"year\": \"Fourth Year\", \"semester\": \"Semester 1\", \"category\": \"AI / ML\"}\n"
         << "  ]\n}";
    string jsonStr = json.str();

    // Cache in Redis for 300 seconds
    redisClient.set("cache:books", jsonStr, 300);

    sendResponse(clientSocket, 200, "application/json", jsonStr, "MISS (Populated Redis Cache)");
}

void handleGetMaterials(SOCKET clientSocket, const HttpRequest& req) {
    string type = req.queryParams.count("type") ? req.queryParams.at("type") : "exams";
    string year = req.queryParams.count("year") ? req.queryParams.at("year") : "FirstYear";
    string sem = req.queryParams.count("semester") ? req.queryParams.at("semester") : "SemesterOne";

    stringstream json;
    json << "{\n"
         << "  \"success\": true,\n"
         << "  \"type\": \"" << escapeJSON(type) << "\",\n"
         << "  \"year\": \"" << escapeJSON(year) << "\",\n"
         << "  \"semester\": \"" << escapeJSON(sem) << "\",\n"
         << "  \"items\": [\n"
         << "    {\"title\": \"" << escapeJSON(year) << " " << escapeJSON(sem) << " " << escapeJSON(type) << " Paper 1\", \"code\": \"CS-101\", \"date\": \"2026-10-15\", \"status\": \"Published\"},\n"
         << "    {\"title\": \"" << escapeJSON(year) << " " << escapeJSON(sem) << " " << escapeJSON(type) << " Paper 2\", \"code\": \"IT-202\", \"date\": \"2026-10-22\", \"status\": \"Published\"}\n"
         << "  ]\n"
         << "}";
    sendResponse(clientSocket, 200, "application/json", json.str());
}

// Main HTTP Client Connection Processor
void processClient(SOCKET clientSocket) {
    char buffer[8192];
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        closesocket(clientSocket);
        return;
    }

    buffer[bytesRead] = '\0';
    string requestText(buffer);

    stringstream ss(requestText);
    string requestLine;
    getline(ss, requestLine);

    stringstream reqLineStream(requestLine);
    string method, fullPath, httpVersion;
    reqLineStream >> method >> fullPath >> httpVersion;

    // Handle OPTIONS Preflight request for CORS
    if (method == "OPTIONS") {
        sendResponse(clientSocket, 204, "text/plain", "");
        closesocket(clientSocket);
        return;
    }

    // Separate path and query string
    string path = fullPath;
    string queryString = "";
    size_t qPos = fullPath.find('?');
    if (qPos != string::npos) {
        path = fullPath.substr(0, qPos);
        queryString = fullPath.substr(qPos + 1);
    }

    HttpRequest req;
    req.method = method;
    req.path = path;
    req.queryString = queryString;
    req.queryParams = parseQueryParams(queryString);

    // Read headers
    string headerLine;
    while (getline(ss, headerLine) && headerLine != "\r" && !headerLine.empty()) {
        size_t colon = headerLine.find(':');
        if (colon != string::npos) {
            string hKey = trim(headerLine.substr(0, colon));
            string hVal = trim(headerLine.substr(colon + 1));
            req.headers[hKey] = hVal;
        }
    }

    // Read body if available
    size_t bodyPos = requestText.find("\r\n\r\n");
    if (bodyPos != string::npos) {
        req.body = requestText.substr(bodyPos + 4);
    }

    // Route dispatch
    if (path == "/api/status") {
        handleStatus(clientSocket);
    } else if (path == "/api/events/live") {
        handleLiveEvents(clientSocket);
    } else if (path == "/api/auth/login" && method == "POST") {
        handleLogin(clientSocket, req);
    } else if (path == "/api/auth/signup" && method == "POST") {
        handleSignup(clientSocket, req);
    } else if (path == "/api/courses" && method == "GET") {
        handleGetCourses(clientSocket);
    } else if (path == "/api/courses" && method == "POST") {
        handleAddCourse(clientSocket, req);
    } else if (path == "/api/student/courses" && method == "GET") {
        handleGetStudentCourses(clientSocket, req);
    } else if (path == "/api/student/enroll" && method == "POST") {
        handleStudentEnroll(clientSocket, req);
    } else if (path == "/api/doctor/courses" && method == "GET") {
        handleGetDoctorCourses(clientSocket, req);
    } else if (path == "/api/doctor/assign" && method == "POST") {
        handleDoctorAssign(clientSocket, req);
    } else if (path == "/api/students" && method == "GET") {
        handleGetStudents(clientSocket);
    } else if (path == "/api/books" && method == "GET") {
        handleGetBooks(clientSocket);
    } else if (path == "/api/materials" && method == "GET") {
        handleGetMaterials(clientSocket, req);
    } else {
        // Try serving static files from web/ or public/
        string filePath = "web" + path;
        if (path == "/" || path.empty()) {
            filePath = "web/index.html";
        }

        string mimeType = "text/html";
        if (filePath.find(".css") != string::npos) mimeType = "text/css";
        else if (filePath.find(".js") != string::npos) mimeType = "application/javascript";
        else if (filePath.find(".json") != string::npos) mimeType = "application/json";
        else if (filePath.find(".png") != string::npos) mimeType = "image/png";
        else if (filePath.find(".jpg") != string::npos || filePath.find(".jpeg") != string::npos) mimeType = "image/jpeg";
        else if (filePath.find(".pdf") != string::npos) mimeType = "application/pdf";

        if (!serveStaticFile(clientSocket, filePath, mimeType)) {
            // Check in parent directory
            if (!serveStaticFile(clientSocket, "../web" + path, mimeType)) {
                sendResponse(clientSocket, 404, "application/json", "{\"success\":false,\"message\":\"Resource not found\"}");
            }
        }
    }

    closesocket(clientSocket);
}

#ifdef _WIN32
DWORD WINAPI ClientThreadRoutine(LPVOID lpParam) {
    SOCKET clientSocket = (SOCKET)(uintptr_t)lpParam;
    processClient(clientSocket);
    return 0;
}
#else
void* ClientThreadRoutine(void* lpParam) {
    SOCKET clientSocket = (SOCKET)(uintptr_t)lpParam;
    processClient(clientSocket);
    return NULL;
}
#endif

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0) port = DEFAULT_PORT;
    }

#ifdef _WIN32
    InitializeCriticalSection(&db_cs);
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[ERROR] WSAStartup failed.\n";
        return 1;
    }
#endif

    ensureDirectories();

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        cerr << "[ERROR] Could not create socket\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    int reuse = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons((u_short)port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[ERROR] Socket bind failed on port " << port << "\n";
        closesocket(listenSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "[ERROR] Listen failed\n";
        closesocket(listenSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    cout << "\n========================================================\n";
    cout << "   IIITM GWL - COLLEGE MANAGEMENT SYSTEM BACKEND\n";
    cout << "   Powered by C++ & Redis Network Caching (Port 6379)\n";
    cout << "========================================================\n";
    cout << " [OK] REST API Server running at: http://0.0.0.0:" << port << "/\n";
    cout << " [OK] API Endpoints available at: http://localhost:" << port << "/api/...\n";
    cout << " [OK] Redis TCP Cache Target:     " << redisClient.getHost() << ":" << redisClient.getPort() << "\n";
    cout << " [OK] Live Pub/Sub Feed Channel:  iiitm_campus_feed\n";
    cout << "========================================================\n\n";

    // Initial broadcast
    broadcastEvent("SERVER_ONLINE", "IIITM GWL Academic Server is Online", "Ready to serve student & faculty requests.");

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

#ifdef _WIN32
        HANDLE hThread = CreateThread(NULL, 0, ClientThreadRoutine, (LPVOID)(uintptr_t)clientSocket, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        }
#else
        pthread_t tid;
        if (pthread_create(&tid, NULL, ClientThreadRoutine, (void*)(uintptr_t)clientSocket) == 0) {
            pthread_detach(tid);
        }
#endif
    }

    closesocket(listenSocket);
#ifdef _WIN32
    WSACleanup();
    DeleteCriticalSection(&db_cs);
#endif
    return 0;
}
