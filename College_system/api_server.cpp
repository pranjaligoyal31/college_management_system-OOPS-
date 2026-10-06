#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sys/stat.h>

#include "Courses.h"
#include "Student.h"
#include "Doctors.h"
#include "Teaching_Assistant.h"
#include "Administrator.h"
#include "ShowData.h"
#include "Books.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

static const int DEFAULT_PORT = 8080;
static CRITICAL_SECTION db_cs;

struct AutoLock {
    AutoLock() { EnterCriticalSection(&db_cs); }
    ~AutoLock() { LeaveCriticalSection(&db_cs); }
};

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

// Ensure required directory paths exist
void ensureDirectories() {
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

// Send HTTP Response
void sendResponse(SOCKET clientSocket, int statusCode, const string& contentType, const string& body) {
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

    stringstream json;
    json << "{\n"
         << "  \"success\": true,\n"
         << "  \"college\": \"ABV-IIITM Gwalior (IIITM GWL)\",\n"
         << "  \"system\": \"College Management System C++ Backend\",\n"
         << "  \"version\": \"2.0.0\",\n"
         << "  \"stats\": {\n"
         << "    \"students\": " << studentsCount << ",\n"
         << "    \"doctors\": " << doctorsCount << ",\n"
         << "    \"teaching_assistants\": " << tasCount << ",\n"
         << "    \"courses\": " << coursesCount << "\n"
         << "  }\n"
         << "}";
    sendResponse(clientSocket, 200, "application/json", json.str());
}

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
        stringstream json;
        json << "{\n"
             << "  \"success\": true,\n"
             << "  \"message\": \"Login successful\",\n"
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

void handleGetCourses(SOCKET clientSocket) {
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
    json << "{\n  \"success\": true,\n  \"courses\": [\n";
    for (size_t i = 0; i < courses.size(); ++i) {
        json << "    {\n"
             << "      \"id\": \"" << escapeJSON(courses[i].getID()) << "\",\n"
             << "      \"name\": \"" << escapeJSON(courses[i].getName()) << "\",\n"
             << "      \"hours\": \"" << escapeJSON(courses[i].getHours()) << "\"\n"
             << "    }" << (i + 1 < courses.size() ? "," : "") << "\n";
    }
    json << "  ]\n}";
    sendResponse(clientSocket, 200, "application/json", json.str());
}

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

    sendResponse(clientSocket, 201, "application/json", "{\"success\":true,\"message\":\"Course added successfully to IIITM GWL database.\"}");
}

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
    sendResponse(clientSocket, 200, "application/json", json.str());
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

DWORD WINAPI ClientThreadRoutine(LPVOID lpParam) {
    SOCKET clientSocket = (SOCKET)(uintptr_t)lpParam;
    processClient(clientSocket);
    return 0;
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0) port = DEFAULT_PORT;
    }

    InitializeCriticalSection(&db_cs);
    ensureDirectories();

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[ERROR] WSAStartup failed.\n";
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        cerr << "[ERROR] Could not create socket: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    BOOL reuse = TRUE;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons((u_short)port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[ERROR] Socket bind failed on port " << port << ": " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "[ERROR] Listen failed: " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    cout << "\n========================================================\n";
    cout << "   IIITM GWL - COLLEGE MANAGEMENT SYSTEM BACKEND\n";
    cout << "   Powered by C++ & Windows Sockets (Winsock2)\n";
    cout << "========================================================\n";
    cout << " [OK] REST API Server running at: http://localhost:" << port << "/\n";
    cout << " [OK] API Endpoints available at: http://localhost:" << port << "/api/...\n";
    cout << " [OK] Serving Frontend from:      web/index.html\n";
    cout << "========================================================\n\n";

    while (true) {
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        HANDLE hThread = CreateThread(NULL, 0, ClientThreadRoutine, (LPVOID)(uintptr_t)clientSocket, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        }
    }

    closesocket(listenSocket);
    WSACleanup();
    DeleteCriticalSection(&db_cs);
    return 0;
}
