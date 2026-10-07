#ifndef STAFFDATA_H
#define STAFFDATA_H
#include "Courses.h"
#include "ShowData.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <windows.h>


using namespace std;

class StaffData : public ShowData, public Courses {
public:
  HANDLE cout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  template <typename type>
  inline void printline(type msg, bool end_line = true, int color_code = 15) {
    SetConsoleTextAttribute(cout_handle, color_code);
    cout << msg << (end_line ? "\n" : "\t");
  }
  virtual void print() = 0;
  virtual void printDoctors();
  virtual void signUP() = 0;
  virtual void AddPracticalExam() {};
  virtual void AddQuizzes() {};
  virtual void AddExam() {};
  virtual void AddCourses() {};
  virtual void AddAssignments() {};
  virtual void assignCourses() = 0;
  virtual void SaveToFile(ofstream &) = 0;
  virtual void LoadFromFile(ifstream &) = 0;
  virtual bool signIn(string, string) = 0;
  bool LoadCoursesFromFile();
  virtual string getID() { return id; }
  virtual string getName() { return name; }

  string getPassword();
  string getUser();
  int getTypeId();
  string getPhone() const { return phone; }
  string getEmail() const { return email; }
  string getBirthDate() const { return birthDate; }
  string getGender() const { return gender; }
  string getAddress() const { return address; }
  string getType() const { return type; }

  void setID(const string &val) { id = val; }
  void setName(const string &val) { name = val; }
  void setPhone(const string &val) { phone = val; }
  void setEmail(const string &val) { email = val; }
  void setBirthDate(const string &val) { birthDate = val; }
  void setGender(const string &val) { gender = val; }
  void setAddress(const string &val) { address = val; }
  void setType(const string &val) { type = val; }
  void setUsername(const string &val) { username = val; }
  void setPassword(const string &val) {
    password = val;
    passwordvalidaition = val;
  }
  void setTypeId(int val) { typeID = val; }

  StaffData();
  virtual ~StaffData();

protected:
  string id;
  string name;
  string phone;
  string email;
  string birthDate;
  string gender;
  string address;
  string type;
  int typeID;
  string username;
  string password;
  string passwordvalidaition;
  vector<Courses> CoursesList;
};

#endif // STAFFDATA_H
