/**
 * IIITM GWL - College Management System
 * Frontend Application Controller & Interactive UI Logic
 */

document.addEventListener('DOMContentLoaded', () => {
  const state = {
    currentUser: null,
    activeTab: 'student',
    theme: localStorage.getItem('iiitm_theme') || 'dark',
    courses: [],
    myCourses: [],
    students: [],
    books: []
  };

  // DOM Elements
  const themeToggleBtn = document.getElementById('themeToggleBtn');
  const apiStatusBadge = document.getElementById('apiStatusBadge');
  const authModal = document.getElementById('authModal');
  const openAuthModalBtn = document.getElementById('openAuthModalBtn');
  const closeAuthModalBtn = document.getElementById('closeAuthModalBtn');
  const roleTabBtns = document.querySelectorAll('.role-tab-btn');
  const portalPanels = document.querySelectorAll('.portal-panel');
  const authForm = document.getElementById('authForm');
  const authTabBtns = document.querySelectorAll('.auth-tab-btn');
  const demoPills = document.querySelectorAll('.demo-pill');
  const userProfileChip = document.getElementById('userProfileChip');
  const logoutBtn = document.getElementById('logoutBtn');

  // Set Theme
  document.documentElement.setAttribute('data-theme', state.theme);
  updateThemeIcon();

  themeToggleBtn.addEventListener('click', () => {
    state.theme = state.theme === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', state.theme);
    localStorage.setItem('iiitm_theme', state.theme);
    updateThemeIcon();
  });

  function updateThemeIcon() {
    themeToggleBtn.innerHTML = state.theme === 'dark' ? '☀️' : '🌙';
  }

  // Toast Notification Function
  window.showToast = function (message, type = 'info') {
    const container = document.getElementById('toastContainer') || createToastContainer();
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    
    let icon = 'ℹ️';
    if (type === 'success') icon = '✅';
    else if (type === 'error') icon = '⚠️';

    toast.innerHTML = `<span>${icon}</span> <div>${message}</div>`;
    container.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateX(50px)';
      setTimeout(() => toast.remove(), 300);
    }, 4000);
  };

  function createToastContainer() {
    const el = document.createElement('div');
    el.id = 'toastContainer';
    el.className = 'toast-container';
    document.body.appendChild(el);
    return el;
  }

  // Check C++ Backend Connection
  async function checkBackend() {
    const res = await window.collegeAPI.checkConnection();
    if (res.online) {
      apiStatusBadge.className = 'api-status-badge';
      apiStatusBadge.innerHTML = `<span class="status-dot"></span> C++ REST API (Port 8080)`;
      apiStatusBadge.title = 'Live C++ Winsock REST API Connected';
    } else {
      apiStatusBadge.className = 'api-status-badge offline';
      apiStatusBadge.innerHTML = `<span class="status-dot"></span> Local Sync Active`;
      apiStatusBadge.title = 'C++ Server Offline - using local state';
    }
  }

  // Refresh Connection periodically
  checkBackend();
  setInterval(checkBackend, 10000);

  // Switch Portal Tabs (Student, Doctor, TA, Admin)
  roleTabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const tab = btn.getAttribute('data-tab');
      switchTab(tab);
    });
  });

  function switchTab(tab) {
    state.activeTab = tab;
    roleTabBtns.forEach(b => b.classList.toggle('active', b.getAttribute('data-tab') === tab));
    portalPanels.forEach(p => p.classList.toggle('active', p.id === `portal-${tab}`));
    loadTabData(tab);
  }

  // Load Active Tab Data
  async function loadTabData(tab) {
    if (tab === 'student') {
      loadStudentPortal();
    } else if (tab === 'doctor') {
      loadDoctorPortal();
    } else if (tab === 'ta') {
      loadTAPortal();
    } else if (tab === 'admin') {
      loadAdminPortal();
    }
  }

  // 1. Student Portal Logic
  async function loadStudentPortal() {
    const student = state.currentUser && state.currentUser.role === 'Student' 
      ? state.currentUser 
      : { name: 'Aarav Sharma', id: '2024BCS-042' };

    // Load All Courses
    const courses = await window.collegeAPI.getCourses();
    state.courses = courses;

    // Load My Enrolled Courses
    const myCourses = await window.collegeAPI.getStudentCourses(student.name, student.id);
    state.myCourses = myCourses;

    renderStudentMyCourses(myCourses);
    renderStudentAvailableCourses(courses, myCourses);
    loadStudentMaterials();
  }

  function renderStudentMyCourses(myCourses) {
    const container = document.getElementById('studentMyCoursesList');
    if (!container) return;

    if (!myCourses || myCourses.length === 0) {
      container.innerHTML = `
        <div style="grid-column: 1/-1; text-align: center; padding: 30px; color: var(--text-muted);">
          <p>No courses registered yet for this semester.</p>
          <p style="font-size: 0.85rem; margin-top: 6px;">Select courses from the catalog below to enroll.</p>
        </div>`;
      return;
    }

    container.innerHTML = myCourses.map(c => `
      <div class="course-item-card">
        <div>
          <span class="course-code-tag">${c.id}</span>
          <h4 class="course-name">${c.name}</h4>
        </div>
        <div class="course-meta">
          <span>⏱️ ${c.hours} Lecture Hours</span>
          <span class="card-badge">Enrolled</span>
        </div>
      </div>
    `).join('');
  }

  function renderStudentAvailableCourses(allCourses, myCourses) {
    const container = document.getElementById('studentCatalogList');
    if (!container) return;

    const myIds = new Set(myCourses.map(c => c.id));

    container.innerHTML = allCourses.map(c => {
      const isEnrolled = myIds.has(c.id);
      return `
        <div class="course-item-card">
          <div>
            <span class="course-code-tag">${c.id}</span>
            <h4 class="course-name">${c.name}</h4>
          </div>
          <div class="course-meta">
            <span>⏱️ ${c.hours} Hours</span>
            ${isEnrolled 
              ? `<span class="card-badge" style="background: rgba(16,185,129,0.15); color: var(--accent-emerald);">Registered</span>`
              : `<button class="btn btn-primary btn-sm enroll-btn" data-id="${c.id}">Enroll Now</button>`
            }
          </div>
        </div>
      `;
    }).join('');

    // Attach enroll listeners
    container.querySelectorAll('.enroll-btn').forEach(btn => {
      btn.addEventListener('click', async () => {
        const courseId = btn.getAttribute('data-id');
        const student = state.currentUser && state.currentUser.role === 'Student' 
          ? state.currentUser 
          : { name: 'Aarav Sharma', id: '2024BCS-042' };

        btn.disabled = true;
        btn.innerText = 'Enrolling...';

        try {
          const res = await window.collegeAPI.enrollStudentCourse(student.name, student.id, courseId);
          showToast(res.message, 'success');
          loadStudentPortal();
        } catch (err) {
          showToast(err.message, 'error');
          btn.disabled = false;
          btn.innerText = 'Enroll Now';
        }
      });
    });
  }

  // Student Materials (Exams, Assignments, Practicals, Tables)
  async function loadStudentMaterials() {
    const typeSelect = document.getElementById('materialTypeSelect');
    const yearSelect = document.getElementById('materialYearSelect');
    const semSelect = document.getElementById('materialSemSelect');
    const container = document.getElementById('studentMaterialsList');

    if (!typeSelect || !container) return;

    const type = typeSelect.value;
    const year = yearSelect.value;
    const sem = semSelect.value;

    container.innerHTML = '<div style="text-align: center; padding: 20px;">Fetching academic repository...</div>';

    const items = await window.collegeAPI.getMaterials(type, year, sem);

    if (!items || items.length === 0) {
      container.innerHTML = `<div style="text-align: center; padding: 24px; color: var(--text-muted);">No uploaded ${type} found for ${year} (${sem}).</div>`;
      return;
    }

    container.innerHTML = `
      <div class="table-responsive">
        <table class="custom-table">
          <thead>
            <tr>
              <th>Document Title</th>
              <th>Course Code</th>
              <th>Date / Uploaded</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            ${items.map(item => `
              <tr>
                <td><strong>📄 ${item.title}</strong></td>
                <td><span class="course-code-tag">${item.code || 'CS-GEN'}</span></td>
                <td>${item.date || 'Active'}</td>
                <td>
                  <button class="btn btn-secondary btn-sm" onclick="showToast('Opening document viewer for ${item.title}', 'info')">
                    👁️ View / Download
                  </button>
                </td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>
    `;
  }

  ['materialTypeSelect', 'materialYearSelect', 'materialSemSelect'].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.addEventListener('change', loadStudentMaterials);
  });

  // 2. Doctor Portal Logic
  async function loadDoctorPortal() {
    const doctor = state.currentUser && state.currentUser.role === 'Doctor'
      ? state.currentUser
      : { name: 'Dr. Rajesh Verma', id: 'FAC-108' };

    const allCourses = await window.collegeAPI.getCourses();
    const docCourses = await window.collegeAPI.getDoctorCourses(doctor.name, doctor.id);
    const students = await window.collegeAPI.getStudents();

    renderDoctorCourses(docCourses, allCourses, doctor);
    renderDoctorStudents(students);
  }

  function renderDoctorCourses(docCourses, allCourses, doctor) {
    const container = document.getElementById('doctorCoursesList');
    const select = document.getElementById('doctorAssignCourseSelect');
    if (!container) return;

    // Populate assignment dropdown
    if (select) {
      select.innerHTML = `<option value="">-- Choose Course to Assign --</option>` +
        allCourses.map(c => `<option value="${c.id}">${c.id} - ${c.name} (${c.hours} hrs)</option>`).join('');
    }

    if (!docCourses || docCourses.length === 0) {
      container.innerHTML = `<div style="grid-column: 1/-1; text-align: center; padding: 24px; color: var(--text-muted);">No courses assigned yet. Select a course from above to assign to your faculty profile.</div>`;
      return;
    }

    container.innerHTML = docCourses.map(c => `
      <div class="course-item-card">
        <div>
          <span class="course-code-tag">${c.id}</span>
          <h4 class="course-name">${c.name}</h4>
        </div>
        <div class="course-meta">
          <span>⏱️ ${c.hours} Hours</span>
          <span class="card-badge" style="background: rgba(139,92,246,0.15); color: var(--secondary);">Instructor</span>
        </div>
      </div>
    `).join('');
  }

  const doctorAssignForm = document.getElementById('doctorAssignForm');
  if (doctorAssignForm) {
    doctorAssignForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      const courseId = document.getElementById('doctorAssignCourseSelect').value;
      if (!courseId) return;

      const doctor = state.currentUser && state.currentUser.role === 'Doctor'
        ? state.currentUser
        : { name: 'Dr. Rajesh Verma', id: 'FAC-108' };

      try {
        const res = await window.collegeAPI.assignDoctorCourse(doctor.name, doctor.id, courseId);
        showToast(res.message, 'success');
        loadDoctorPortal();
      } catch (err) {
        showToast(err.message, 'error');
      }
    });
  }

  function renderDoctorStudents(students) {
    const container = document.getElementById('doctorStudentsTable');
    if (!container) return;

    if (!students || students.length === 0) {
      container.innerHTML = `<div style="text-align: center; padding: 20px; color: var(--text-muted);">No student records found.</div>`;
      return;
    }

    container.innerHTML = `
      <table class="custom-table">
        <thead>
          <tr>
            <th>Student ID</th>
            <th>Name</th>
            <th>Email</th>
            <th>Phone</th>
            <th>Address</th>
          </tr>
        </thead>
        <tbody>
          ${students.map(s => `
            <tr>
              <td><span class="course-code-tag">${s.id}</span></td>
              <td><strong>${s.name}</strong></td>
              <td>${s.email || '-'}</td>
              <td>${s.phone || '-'}</td>
              <td>${s.address || 'IIITM Hostel'}</td>
            </tr>
          `).join('')}
        </tbody>
      </table>
    `;
  }

  // Doctor Upload Material Modal
  const postMaterialForm = document.getElementById('postMaterialForm');
  if (postMaterialForm) {
    postMaterialForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      const type = document.getElementById('postMatType').value;
      const year = document.getElementById('postMatYear').value;
      const sem = document.getElementById('postMatSem').value;
      const title = document.getElementById('postMatTitle').value;
      const code = document.getElementById('postMatCode').value;

      try {
        const res = await window.collegeAPI.addMaterial(type, year, sem, {
          id: `M-${Date.now().toString().slice(-4)}`,
          title,
          code,
          date: new Date().toISOString().split('T')[0],
          format: 'PDF Document',
          status: 'Published'
        });
        showToast(res.message, 'success');
        postMaterialForm.reset();
        loadStudentPortal();
      } catch (err) {
        showToast(err.message, 'error');
      }
    });
  }

  // 3. Teaching Assistant Portal Logic
  async function loadTAPortal() {
    const allCourses = await window.collegeAPI.getCourses();
    const students = await window.collegeAPI.getStudents();

    const taCoursesList = document.getElementById('taCoursesList');
    if (taCoursesList) {
      taCoursesList.innerHTML = allCourses.slice(0, 4).map(c => `
        <div class="course-item-card">
          <div>
            <span class="course-code-tag">${c.id}</span>
            <h4 class="course-name">${c.name}</h4>
          </div>
          <div class="course-meta">
            <span>🔬 Practical Lab Session</span>
            <span class="card-badge">TA Assigned</span>
          </div>
        </div>
      `).join('');
    }

    const taStudentsTable = document.getElementById('taStudentsTable');
    if (taStudentsTable) {
      taStudentsTable.innerHTML = `
        <table class="custom-table">
          <thead>
            <tr>
              <th>Roll ID</th>
              <th>Student Name</th>
              <th>Lab Attendance</th>
              <th>Quiz 1 Grade</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            ${students.map((s, idx) => `
              <tr>
                <td><span class="course-code-tag">${s.id}</span></td>
                <td><strong>${s.name}</strong></td>
                <td><span class="card-badge" style="background: rgba(16,185,129,0.15); color: var(--accent-emerald);">94%</span></td>
                <td><strong>${85 + (idx % 15)} / 100</strong></td>
                <td>
                  <button class="btn btn-secondary btn-sm" onclick="showToast('Recorded grade for ${s.name}', 'success')">Grade</button>
                </td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      `;
    }
  }

  // 4. Admin Portal Logic
  async function loadAdminPortal() {
    const courses = await window.collegeAPI.getCourses();
    const students = await window.collegeAPI.getStudents();
    const books = await window.collegeAPI.getBooks();

    document.getElementById('adminCountStudents').innerText = students.length || 4;
    document.getElementById('adminCountCourses').innerText = courses.length || 8;
    document.getElementById('adminCountBooks').innerText = books.length || 5;

    // Render Admin Courses Table
    const adminCoursesTable = document.getElementById('adminCoursesTable');
    if (adminCoursesTable) {
      adminCoursesTable.innerHTML = `
        <table class="custom-table">
          <thead>
            <tr>
              <th>Course Code</th>
              <th>Course Name</th>
              <th>Lecture / Credit Hours</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            ${courses.map(c => `
              <tr>
                <td><span class="course-code-tag">${c.id}</span></td>
                <td><strong>${c.name}</strong></td>
                <td>${c.hours} Hours</td>
                <td><span class="card-badge" style="background: rgba(16,185,129,0.15); color: var(--accent-emerald);">Active</span></td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      `;
    }

    // Render Books Table
    const adminBooksTable = document.getElementById('adminBooksTable');
    if (adminBooksTable) {
      adminBooksTable.innerHTML = `
        <table class="custom-table">
          <thead>
            <tr>
              <th>Book ID</th>
              <th>Title</th>
              <th>Author</th>
              <th>Year / Sem</th>
              <th>Category</th>
            </tr>
          </thead>
          <tbody>
            ${books.map(b => `
              <tr>
                <td><span class="course-code-tag">${b.id}</span></td>
                <td><strong>${b.title}</strong></td>
                <td>${b.author}</td>
                <td>${b.year} (${b.semester})</td>
                <td><span class="card-badge">${b.category || 'Reference'}</span></td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      `;
    }
  }

  // Admin Add Course Form
  const adminAddCourseForm = document.getElementById('adminAddCourseForm');
  if (adminAddCourseForm) {
    adminAddCourseForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      const id = document.getElementById('adminCourseId').value.trim();
      const name = document.getElementById('adminCourseName').value.trim();
      const hours = document.getElementById('adminCourseHours').value.trim();

      if (!id || !name || !hours) return;

      try {
        const res = await window.collegeAPI.addCourse({ id, name, hours });
        showToast(res.message, 'success');
        adminAddCourseForm.reset();
        loadAdminPortal();
        loadStudentPortal();
      } catch (err) {
        showToast(err.message, 'error');
      }
    });
  }

  // Admin Add Book Form
  const adminAddBookForm = document.getElementById('adminAddBookForm');
  if (adminAddBookForm) {
    adminAddBookForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      const id = document.getElementById('adminBookId').value.trim();
      const title = document.getElementById('adminBookTitle').value.trim();
      const author = document.getElementById('adminBookAuthor').value.trim();
      const year = document.getElementById('adminBookYear').value;
      const semester = document.getElementById('adminBookSem').value;
      const category = document.getElementById('adminBookCat').value.trim();

      try {
        const res = await window.collegeAPI.addBook({ id, title, author, year, semester, category });
        showToast(res.message, 'success');
        adminAddBookForm.reset();
        loadAdminPortal();
      } catch (err) {
        showToast(err.message, 'error');
      }
    });
  }

  // Digital Library Global Tab
  async function loadLibraryView() {
    const books = await window.collegeAPI.getBooks();
    const container = document.getElementById('libraryBooksGrid');
    if (!container) return;

    container.innerHTML = books.map(b => `
      <div class="course-item-card">
        <div>
          <span class="course-code-tag">${b.id}</span>
          <h4 class="course-name">${b.title}</h4>
          <p style="font-size: 0.85rem; color: var(--text-secondary); margin-top: 4px;">By ${b.author}</p>
        </div>
        <div class="course-meta">
          <span>📚 ${b.year} (${b.semester})</span>
          <button class="btn btn-secondary btn-sm" onclick="showToast('Issued digital e-copy of ${b.title}', 'success')">Read Book</button>
        </div>
      </div>
    `).join('');
  }

  // Auth Modal & Form Handling
  let authMode = 'login'; // 'login' or 'signup'
  let selectedRole = 'Student';

  authTabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      authMode = btn.getAttribute('data-mode');
      authTabBtns.forEach(b => b.classList.toggle('active', b === btn));
      document.getElementById('authSubmitBtn').innerText = authMode === 'login' ? 'Sign In to Portal' : 'Create Account';
      document.getElementById('signupExtraFields').style.display = authMode === 'signup' ? 'block' : 'none';
    });
  });

  demoPills.forEach(pill => {
    pill.addEventListener('click', () => {
      demoPills.forEach(p => p.classList.remove('active'));
      pill.classList.add('active');
      selectedRole = pill.getAttribute('data-role');
      document.getElementById('authRoleSelect').value = selectedRole;

      // Fill demo credentials
      if (selectedRole === 'Student') {
        document.getElementById('authUsername').value = 'student1';
        document.getElementById('authPassword').value = '123';
      } else if (selectedRole === 'Doctor') {
        document.getElementById('authUsername').value = 'doctor1';
        document.getElementById('authPassword').value = '123';
      } else if (selectedRole === 'Teaching Assistant') {
        document.getElementById('authUsername').value = 'ta1';
        document.getElementById('authPassword').value = '123';
      } else if (selectedRole === 'Administrator') {
        document.getElementById('authUsername').value = 'admin';
        document.getElementById('authPassword').value = '123';
      }
    });
  });

  if (openAuthModalBtn) {
    openAuthModalBtn.addEventListener('click', () => authModal.classList.add('open'));
  }
  if (closeAuthModalBtn) {
    closeAuthModalBtn.addEventListener('click', () => authModal.classList.remove('open'));
  }
  if (authModal) {
    authModal.addEventListener('click', (e) => {
      if (e.target === authModal) authModal.classList.remove('open');
    });
  }

  authForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const username = document.getElementById('authUsername').value.trim();
    const password = document.getElementById('authPassword').value.trim();
    const role = document.getElementById('authRoleSelect').value;

    if (authMode === 'login') {
      try {
        const res = await window.collegeAPI.login(username, password, role);
        state.currentUser = res.user;
        setUserSession(res.user);
        showToast(`Welcome back, ${res.user.name} (${res.user.role})!`, 'success');
        authModal.classList.remove('open');

        // Automatically switch to user's role tab
        if (role === 'Student') switchTab('student');
        else if (role === 'Doctor') switchTab('doctor');
        else if (role === 'Teaching Assistant') switchTab('ta');
        else if (role === 'Administrator') switchTab('admin');
      } catch (err) {
        showToast(err.message, 'error');
      }
    } else {
      // Signup
      const id = document.getElementById('authId').value.trim();
      const name = document.getElementById('authName').value.trim();
      const email = document.getElementById('authEmail').value.trim();
      const phone = document.getElementById('authPhone').value.trim();
      const gender = document.getElementById('authGender').value;

      try {
        const res = await window.collegeAPI.signup({
          username,
          password,
          role,
          id,
          name,
          email,
          phone,
          gender,
          address: 'IIITM Gwalior Campus',
          birthDate: '2004-01-01'
        });
        showToast(res.message, 'success');
        state.currentUser = res.user;
        setUserSession(res.user);
        authModal.classList.remove('open');
      } catch (err) {
        showToast(err.message, 'error');
      }
    }
  });

  function setUserSession(user) {
    if (user) {
      userProfileChip.style.display = 'flex';
      openAuthModalBtn.style.display = 'none';
      document.getElementById('userProfileName').innerText = user.name || user.username;
      document.getElementById('userProfileRole').innerText = user.role || 'Member';
      document.getElementById('userAvatarLetter').innerText = (user.name || user.username)[0].toUpperCase();
    } else {
      userProfileChip.style.display = 'none';
      openAuthModalBtn.style.display = 'inline-flex';
    }
  }

  if (logoutBtn) {
    logoutBtn.addEventListener('click', () => {
      state.currentUser = null;
      setUserSession(null);
      showToast('Logged out successfully', 'info');
    });
  }

  // Initial tab load
  switchTab('student');
  loadLibraryView();
});
