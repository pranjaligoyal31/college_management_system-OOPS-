# ===================================================================
# Multi-Stage Dockerfile for IIITM GWL College Management System
# ===================================================================

# Stage 1: Build C++ Binary
FROM gcc:13-bookworm AS builder

WORKDIR /build

# Copy C++ source files
COPY College_system/include/ ./include/
COPY College_system/src/ ./src/
COPY College_system/api_server.cpp .

# Compile optimized static binary with POSIX socket support
RUN g++ -std=c++11 -O2 api_server.cpp \
    src/Courses.cpp \
    src/StaffData.cpp \
    src/Student.cpp \
    src/Doctors.cpp \
    src/Teaching_Assistant.cpp \
    src/Administrator.cpp \
    src/ShowData.cpp \
    src/Books.cpp \
    src/HandlingData.cpp \
    -Iinclude -lpthread -o college_api_server

# ===================================================================
# Stage 2: Minimal Production Runtime
# ===================================================================
FROM debian:bookworm-slim AS runner

WORKDIR /app

# Install runtime dependencies & curl for container healthcheck
RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy compiled binary from builder stage
COPY --from=builder /build/college_api_server /app/college_api_server

# Copy static frontend and database directories
COPY web/ /app/web/
COPY College_system/DataBase/ /app/DataBase/

# Environment Variables
ENV PORT=8080
ENV REDIS_HOST=redis
ENV REDIS_PORT=6379

# Expose HTTP Server Port
EXPOSE 8080

# Healthcheck
HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:8080/api/status || exit 1

# Start the C++ REST API Server
CMD ["./college_api_server", "8080"]
