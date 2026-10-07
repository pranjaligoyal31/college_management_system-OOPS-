/**
 * IIITM GWL - College Management System
 * API Client Layer for C++ REST Backend
 */

const API_BASE_URL = (window.location.protocol === 'http:' || window.location.protocol === 'https:')
  ? `${window.location.origin}/api`
  : 'http://localhost:8080/api';

class CollegeAPI {
  constructor() {
    this.isBackendOnline = false;
    this.initMockStorage();
  }

  // Check connection to C++ Backend Server
  async checkConnection() {
    try {
      const res = await fetch(`${API_BASE_URL}/status`, { method: 'GET', cache: 'no-cache' });
      if (res.ok) {
        this.isBackendOnline = true;
        const data = await res.json();
        return { online: true, data };
      }
    } catch (err) {
      this.isBackendOnline = false;
    }
    return { online: false, message: 'Running on browser persistent storage mode (Start C++ backend on port 8080 to sync)' };
  }

  // 1. User Authentication (Login)
  async login(username, password, role) {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/auth/login`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ username, password, role })
        });
        const data = await res.json();
        if (res.ok && data.success) {
          return data;
        }
        throw new Error(data.message || 'Login failed');
      } catch (err) {
        console.warn('Backend login error, falling back to local storage:', err);
      }
    }

    // Fallback Mock/Local Auth
    const users = this.getLocalData('users');
    const user = users.find(u => u.username === username && u.password === password && (u.role === role || !role));
    if (user) {
      return { success: true, message: 'Logged in successfully', user };
    }
    throw new Error('Invalid username, password, or role.');
  }

  // 2. User Authentication (Sign Up)
  async signup(userData) {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/auth/signup`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(userData)
        });
        const data = await res.json();
        if (res.ok && data.success) {
          return data;
        }
        throw new Error(data.message || 'Registration failed');
      } catch (err) {
        console.warn('Backend signup error, falling back to local storage:', err);
      }
    }

    // Local storage registration
    const users = this.getLocalData('users');
    if (users.find(u => u.username === userData.username)) {
      throw new Error('Username already exists');
    }
    users.push(userData);
    this.setLocalData('users', users);
    return { success: true, message: 'Account registered successfully', user: userData };
  }

  // 3. Courses API
  async getCourses() {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/courses`);
        if (res.ok) {
          const data = await res.json();
          return data.courses || [];
        }
      } catch (err) {
        console.warn('Backend error fetching courses:', err);
      }
    }
    return this.getLocalData('courses');
  }

  async addCourse(course) {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/courses`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(course)
        });
        const data = await res.json();
        if (res.ok && data.success) return data;
        throw new Error(data.message || 'Failed to add course');
      } catch (err) {
        console.warn('Backend error adding course:', err);
      }
    }

    const courses = this.getLocalData('courses');
    courses.push(course);
    this.setLocalData('courses', courses);
    return { success: true, message: 'Course added successfully to IIITM GWL' };
  }

  // 4. Student Enrollment API
  async getStudentCourses(studentName, studentId) {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/student/courses?name=${encodeURIComponent(studentName)}&id=${encodeURIComponent(studentId)}`);
        if (res.ok) {
          const data = await res.json();
          return data.courses || [];
        }
      } catch (err) {
        console.warn('Backend error getting student courses:', err);
      }
    }

    const key = `student_courses_${studentName}_${studentId}`;
    return this.getLocalData(key) || [];
  }

  async enrollStudentCourse(studentName, studentId, courseId) {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/student/enroll`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ name: studentName, id: studentId, courseId })
        });
        const data = await res.json();
        if (res.ok && data.success) return data;
        throw new Error(data.message || 'Failed to enroll');
      } catch (err) {
        console.warn('Backend error enrolling:', err);
      }
    }

    const key = `student_courses_${studentName}_${studentId}`;
    const studentCourses = this.getLocalData(key) || [];
    const allCourses = this.getLocalData('courses');
    const course = allCourses.find(c => c.id === courseId);

    if (!course) throw new Error('Course not found');
    if (studentCourses.some(c => c.id === courseId)) throw new Error('Already enrolled in this course');

    studentCourses.push(course);
    this.setLocalData(key, studentCourses);
    return { success: true, message: `Enrolled successfully in ${course.name}` };
  }

  // 5. Doctor Assigned Courses
  async getDoctorCourses(doctorName, doctorId) {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/doctor/courses?name=${encodeURIComponent(doctorName)}&id=${encodeURIComponent(doctorId)}`);
        if (res.ok) {
          const data = await res.json();
          return data.courses || [];
        }
      } catch (err) {
        console.warn('Backend error getting doctor courses:', err);
      }
    }

    const key = `doctor_courses_${doctorName}_${doctorId}`;
    return this.getLocalData(key) || [];
  }

  async assignDoctorCourse(doctorName, doctorId, courseId) {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/doctor/assign`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ name: doctorName, id: doctorId, courseId })
        });
        const data = await res.json();
        if (res.ok && data.success) return data;
        throw new Error(data.message || 'Failed to assign course');
      } catch (err) {
        console.warn('Backend error assigning doctor course:', err);
      }
    }

    const key = `doctor_courses_${doctorName}_${doctorId}`;
    const docCourses = this.getLocalData(key) || [];
    const allCourses = this.getLocalData('courses');
    const course = allCourses.find(c => c.id === courseId);

    if (!course) throw new Error('Course not found');
    if (docCourses.some(c => c.id === courseId)) throw new Error('Course already assigned to this doctor');

    docCourses.push(course);
    this.setLocalData(key, docCourses);
    return { success: true, message: `Course ${course.name} assigned successfully` };
  }

  // 6. Registered Students Roster
  async getStudents() {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/students`);
        if (res.ok) {
          const data = await res.json();
          return data.students || [];
        }
      } catch (err) {
        console.warn('Backend error getting students:', err);
      }
    }

    const users = this.getLocalData('users');
    return users.filter(u => u.role === 'Student');
  }

  // 7. Library Books API
  async getBooks() {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/books`);
        if (res.ok) {
          const data = await res.json();
          return data.books || [];
        }
      } catch (err) {
        console.warn('Backend error getting books:', err);
      }
    }
    return this.getLocalData('books');
  }

  async addBook(book) {
    const books = this.getLocalData('books');
    books.push(book);
    this.setLocalData('books', books);
    return { success: true, message: 'Book cataloged successfully' };
  }

  // 8. Academic Materials (Exams, Assignments, Quizzes, Tables)
  async getMaterials(type, year, semester) {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/materials?type=${type}&year=${year}&semester=${semester}`);
        if (res.ok) {
          const data = await res.json();
          return data.items || [];
        }
      } catch (err) {
        console.warn('Backend error getting materials:', err);
      }
    }

    const key = `materials_${type}_${year}_${semester}`;
    const items = this.getLocalData(key);
    if (items && items.length > 0) return items;

    // Default seeded academic resources
    return [
      { id: 'M-101', title: `${year} ${semester} - ${type.toUpperCase()} Paper (Set A)`, code: 'CS-201', date: '2026-10-15', format: 'PDF Document', status: 'Available' },
      { id: 'M-102', title: `${year} ${semester} - Problem Sheet & Syllabus Guide`, code: 'IT-304', date: '2026-10-20', format: 'PDF Document', status: 'Available' },
      { id: 'M-103', title: `${year} ${semester} - Reference Solutions & Rubrics`, code: 'CS-402', date: '2026-10-25', format: 'PDF Document', status: 'Available' }
    ];
  }

  async addMaterial(type, year, semester, material) {
    const key = `materials_${type}_${year}_${semester}`;
    const items = this.getLocalData(key) || [];
    items.push(material);
    this.setLocalData(key, items);
    return { success: true, message: `${type.toUpperCase()} uploaded successfully for ${year} (${semester})` };
  }

  // 9. Real-Time Campus Events (Redis Pub/Sub Buffer)
  async getLiveEvents() {
    if (this.isBackendOnline) {
      try {
        const res = await fetch(`${API_BASE_URL}/live-events`);
        if (res.ok) {
          const data = await res.json();
          return data.feed || [];
        }
      } catch (err) {
        console.warn('Backend error getting live events:', err);
      }
    }
    return [
      { type: 'REDIS_INIT', title: 'Redis Pub/Sub Active', details: 'Listening for live campus broadcasts on iiitm_campus_feed' },
      { type: 'EXAM_POSTED', title: 'Final Exam Posted', details: 'CS-201 Data Structures End-Sem schedule updated' },
      { type: 'COURSE_ADDED', title: 'New Course Registered', details: 'IT-301 Computer Networks added to curriculum' }
    ];
  }

  // Local Storage Helpers
  getLocalData(key) {
    try {
      const data = localStorage.getItem(`iiitm_college_${key}`);
      return data ? JSON.parse(data) : null;
    } catch {
      return null;
    }
  }

  setLocalData(key, val) {
    try {
      localStorage.setItem(`iiitm_college_${key}`, JSON.stringify(val));
    } catch (e) {
      console.error(e);
    }
  }

  initMockStorage() {
    // Seed initial demo courses if empty
    if (!this.getLocalData('courses')) {
      this.setLocalData('courses', [
        { id: 'CS-101', name: 'Computer Programming & Problem Solving', hours: '45' },
        { id: 'CS-102', name: 'Digital Logic & Computer Organization', hours: '40' },
        { id: 'CS-201', name: 'Data Structures and Algorithms', hours: '50' },
        { id: 'IT-202', name: 'Database Management Systems', hours: '45' },
        { id: 'IT-301', name: 'Computer Networks & Internet Protocols', hours: '42' },
        { id: 'CS-302', name: 'Theory of Computation & Automata', hours: '38' },
        { id: 'AI-401', name: 'Artificial Intelligence & Machine Learning', hours: '48' },
        { id: 'IT-402', name: 'Cloud Computing & Distributed Systems', hours: '40' }
      ]);
    }

    // Seed initial users
    if (!this.getLocalData('users')) {
      this.setLocalData('users', [
        { username: 'student1', password: '123', id: '2024BCS-042', name: 'Aarav Sharma', role: 'Student', email: 'aarav.s@iiitm.ac.in', phone: '+91 9876543210', gender: 'Male', address: 'Boys Hostel 1, IIITM GWL', birthDate: '2004-05-12' },
        { username: 'doctor1', password: '123', id: 'FAC-108', name: 'Dr. Rajesh Verma', role: 'Doctor', email: 'rverma@iiitm.ac.in', phone: '+91 9811223344', gender: 'Male', address: 'Faculty Quarters, IIITM GWL', birthDate: '1982-11-20' },
        { username: 'ta1', password: '123', id: 'TA-204', name: 'Priya Mukherjee', role: 'Teaching Assistant', email: 'priya.m@iiitm.ac.in', phone: '+91 9871122334', gender: 'Female', address: 'Girls Hostel, IIITM GWL', birthDate: '1999-08-15' },
        { username: 'admin', password: '123', id: 'ADM-001', name: 'Dean Academics', role: 'Administrator', email: 'dean@iiitm.ac.in', phone: '+91 7512449800', gender: 'Male', address: 'Admin Block, ABV-IIITM Gwalior', birthDate: '1975-01-01' }
      ]);
    }

    // Seed library books
    if (!this.getLocalData('books')) {
      this.setLocalData('books', [
        { id: 'BK-101', title: 'Discrete Mathematics and Its Applications', author: 'Kenneth H. Rosen', year: 'First Year', semester: 'Semester 1', category: 'Mathematics' },
        { id: 'BK-102', title: 'Introduction to Algorithms (CLRS 4th Ed)', author: 'Cormen, Leiserson, Rivest, Stein', year: 'Second Year', semester: 'Semester 1', category: 'Algorithms' },
        { id: 'BK-103', title: 'Database System Concepts (7th Ed)', author: 'Silberschatz, Korth, Sudarshan', year: 'Second Year', semester: 'Semester 2', category: 'Databases' },
        { id: 'BK-104', title: 'Computer Networking: A Top-Down Approach', author: 'James Kurose, Keith Ross', year: 'Third Year', semester: 'Semester 1', category: 'Networks' },
        { id: 'BK-105', title: 'Pattern Recognition and Machine Learning', author: 'Christopher M. Bishop', year: 'Fourth Year', semester: 'Semester 1', category: 'AI & Data Science' }
      ]);
    }
  }
}

// Global API instance
window.collegeAPI = new CollegeAPI();
