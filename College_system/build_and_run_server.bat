@echo off
title IIITM GWL - College Management System (C++ Backend Server)
color 0b
echo ===================================================================
echo     IIITM GWL - COLLEGE MANAGEMENT SYSTEM (C++ BACKEND)
echo ===================================================================
echo.

:: Check for g++
where g++ >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [1/2] Compiling C++ REST API Server using g++...
    g++ -std=c++11 -O2 api_server.cpp src/Courses.cpp src/StaffData.cpp src/Student.cpp src/Doctors.cpp src/Teaching_Assistant.cpp src/Administrator.cpp src/ShowData.cpp src/Books.cpp src/HandlingData.cpp -Iinclude -lws2_32 -o college_api_server.exe
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Compilation failed! Please check code or compiler.
        pause
        exit /b 1
    )
    echo [2/2] Launching IIITM GWL College API Server...
    echo.
    college_api_server.exe
    pause
    exit /b 0
)

:: Check for cl (MSVC)
where cl >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [1/2] Compiling C++ REST API Server using MSVC cl.exe...
    cl /EHsc /std:c++14 /O2 api_server.cpp src/Courses.cpp src/StaffData.cpp src/Student.cpp src/Doctors.cpp src/Teaching_Assistant.cpp src/Administrator.cpp src/ShowData.cpp src/Books.cpp src/HandlingData.cpp /Iinclude ws2_32.lib /Fe:college_api_server.exe
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Compilation failed!
        pause
        exit /b 1
    )
    echo [2/2] Launching IIITM GWL College API Server...
    echo.
    college_api_server.exe
    pause
    exit /b 0
)

echo [WARNING] No g++ or cl compiler found directly in PATH.
echo If you are using Code::Blocks:
echo 1. Open 'College_system.cbp' in Code::Blocks.
echo 2. Add 'api_server.cpp' to the project build target.
echo 3. Add '-lws2_32' in Project Build Options -^> Linker settings.
echo 4. Click 'Build and Run'.
echo.
pause
