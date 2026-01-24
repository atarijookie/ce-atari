package main

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
)

// loadEnvFromDotFile loads simple KEY=VALUE lines from the given .env file
// in the current working directory. It silently ignores missing files
// and malformed lines. Supports variable substitution like ${VAR}.
func loadEnvFromDotFile(path string) {
	data, err := os.ReadFile(path)
	if err != nil {
		// No .env file; nothing to do.
		return
	}

	// First pass: collect all variables into a map
	envVars := make(map[string]string)
	lines := strings.Split(string(data), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		if idx := strings.Index(line, "="); idx != -1 {
			key := strings.TrimSpace(line[:idx])
			val := strings.TrimSpace(line[idx+1:])

			// Optionally strip surrounding quotes.
			if len(val) >= 2 && ((val[0] == '"' && val[len(val)-1] == '"') || (val[0] == '\'' && val[len(val)-1] == '\'')) {
				val = val[1 : len(val)-1]
			}

			if key != "" {
				envVars[key] = val
			}
		}
	}

	// Second pass: resolve variable references and set environment variables
	for key, val := range envVars {
		resolved := resolveEnvVar(val, envVars, make(map[string]bool))
		_ = os.Setenv(key, resolved)
	}
}

// resolveEnvVar replaces ${VAR} references in a string with their values.
// It handles nested references and prevents circular dependencies.
func resolveEnvVar(value string, envVars map[string]string, visited map[string]bool) string {
	// Find all ${VAR} patterns
	var result strings.Builder
	i := 0
	for i < len(value) {
		// Look for ${ pattern
		if i < len(value)-1 && value[i] == '$' && value[i+1] == '{' {
			// Find the closing }
			end := strings.Index(value[i+2:], "}")
			if end == -1 {
				// No closing brace, keep as-is
				result.WriteByte(value[i])
				i++
				continue
			}
			end += i + 2 // Adjust for offset

			// Extract variable name
			varName := strings.TrimSpace(value[i+2 : end])

			// Check for circular reference
			if visited[varName] {
				// Circular reference detected, return original
				result.WriteString(value[i : end+1])
				i = end + 1
				continue
			}

			// Look up variable value
			varVal := ""
			if v, ok := envVars[varName]; ok {
				// Recursively resolve nested variables
				visited[varName] = true
				varVal = resolveEnvVar(v, envVars, visited)
				delete(visited, varName)
			} else if v := os.Getenv(varName); v != "" {
				// Check system environment as fallback
				varVal = v
			}

			result.WriteString(varVal)
			i = end + 1
		} else {
			result.WriteByte(value[i])
			i++
		}
	}
	return result.String()
}

// getPIDFilePath returns the path to the PID file based on PID_DIR environment variable
func getPIDFilePath() string {
	pidDir := os.Getenv("PID_DIR")
	if pidDir == "" {
		pidDir = "/tmp/ce/pid"
	}
	// Use the executable name as the PID file name
	exePath, err := os.Executable()
	if err != nil {
		// Fallback to a default name if we can't get executable path
		return filepath.Join(pidDir, "webserver.pid")
	}
	exeName := filepath.Base(exePath)
	return filepath.Join(pidDir, exeName+".pid")
}

// otherInstanceIsRunning checks if another instance of this application is running
// by reading the PID file and verifying the process is still running the same executable.
// If no other instance is found, it writes the current PID to the file.
// Returns true if another instance is running, false otherwise.
func otherInstanceIsRunning() bool {
	selfPid := os.Getpid()
	pidFilePath := getPIDFilePath()

	// Get the current executable path
	selfExe, err := os.Readlink("/proc/self/exe")
	if err != nil {
		// If /proc/self/exe is not available (e.g., on non-Linux systems), we can't verify
		// In that case, we'll just check if the PID file exists and contains a valid PID
		log.Printf("otherInstanceIsRunning - couldn't read /proc/self/exe: %v, using fallback check", err)
		return otherInstanceIsRunningFallback(selfPid, pidFilePath)
	}

	// Try to read the PID file
	data, err := os.ReadFile(pidFilePath)
	if err != nil {
		// Can't open file? Other instance probably not running
		log.Printf("otherInstanceIsRunning - couldn't open %s, assuming no other instance", pidFilePath)
	} else {
		// Parse the PID from the file
		pidStr := strings.TrimSpace(string(data))
		otherPid, err := strconv.Atoi(pidStr)
		if err != nil {
			log.Printf("otherInstanceIsRunning - can't parse pid in %s: %v, assuming no other instance", pidFilePath, err)
		} else {
			log.Printf("otherInstanceIsRunning - %s contains pid=%d (own pid=%d)", pidFilePath, otherPid, selfPid)

			// Check if the other process is still running and is the same executable
			procExePath := fmt.Sprintf("/proc/%d/exe", otherPid)
			otherExe, err := os.Readlink(procExePath)
			if err != nil {
				// Process doesn't exist or we can't read its exe link
				log.Printf("otherInstanceIsRunning - process %d doesn't exist or can't read exe: %v", otherPid, err)
			} else if otherExe == selfExe {
				// Found another instance running the same executable
				log.Printf("otherInstanceIsRunning - found another instance of %s with pid %d", otherExe, otherPid)
				return true
			} else {
				log.Printf("otherInstanceIsRunning - process %d exists but is running different executable: %s (ours: %s)", otherPid, otherExe, selfExe)
			}
		}
	}

	// No other instance found, write our PID to the file
	// Ensure the directory exists
	pidDir := filepath.Dir(pidFilePath)
	if err := os.MkdirAll(pidDir, 0o755); err != nil {
		log.Printf("otherInstanceIsRunning - failed to create PID directory %s: %v", pidDir, err)
		return false // Continue anyway
	}

	// Write PID to file
	if err := os.WriteFile(pidFilePath, []byte(fmt.Sprintf("%d\n", selfPid)), 0o644); err != nil {
		log.Printf("otherInstanceIsRunning - failed to write PID file %s: %v", pidFilePath, err)
		return false // Continue anyway
	}

	log.Printf("otherInstanceIsRunning - pid %d written to %s", selfPid, pidFilePath)
	return false
}

// otherInstanceIsRunningFallback is a fallback check for systems without /proc/self/exe
// It only checks if the PID file exists and contains a PID that's still running
func otherInstanceIsRunningFallback(selfPid int, pidFilePath string) bool {
	data, err := os.ReadFile(pidFilePath)
	if err != nil {
		// No PID file, write ours
		pidDir := filepath.Dir(pidFilePath)
		if err := os.MkdirAll(pidDir, 0o755); err != nil {
			log.Printf("otherInstanceIsRunningFallback - failed to create PID directory %s: %v", pidDir, err)
			return false
		}
		if err := os.WriteFile(pidFilePath, []byte(fmt.Sprintf("%d\n", selfPid)), 0o644); err != nil {
			log.Printf("otherInstanceIsRunningFallback - failed to write PID file %s: %v", pidFilePath, err)
		}
		return false
	}

	pidStr := strings.TrimSpace(string(data))
	otherPid, err := strconv.Atoi(pidStr)
	if err != nil {
		log.Printf("otherInstanceIsRunningFallback - can't parse pid in %s: %v", pidFilePath, err)
		// Write our PID
		if err := os.WriteFile(pidFilePath, []byte(fmt.Sprintf("%d\n", selfPid)), 0o644); err != nil {
			log.Printf("otherInstanceIsRunningFallback - failed to write PID file %s: %v", pidFilePath, err)
		}
		return false
	}

	// Check if the process is still running by sending signal 0 (doesn't kill, just checks)
	proc, err := os.FindProcess(otherPid)
	if err != nil {
		log.Printf("otherInstanceIsRunningFallback - can't find process %d: %v", otherPid, err)
		// Write our PID
		if err := os.WriteFile(pidFilePath, []byte(fmt.Sprintf("%d\n", selfPid)), 0o644); err != nil {
			log.Printf("otherInstanceIsRunningFallback - failed to write PID file %s: %v", pidFilePath, err)
		}
		return false
	}

	// Send signal 0 to check if process exists
	err = proc.Signal(syscall.Signal(0))
	if err != nil {
		// Process doesn't exist
		log.Printf("otherInstanceIsRunningFallback - process %d doesn't exist: %v", otherPid, err)
		// Write our PID
		if err := os.WriteFile(pidFilePath, []byte(fmt.Sprintf("%d\n", selfPid)), 0o644); err != nil {
			log.Printf("otherInstanceIsRunningFallback - failed to write PID file %s: %v", pidFilePath, err)
		}
		return false
	}

	// Process exists - could be another instance or a different process with the same PID
	// Since we can't verify the executable, we'll assume it's another instance
	log.Printf("otherInstanceIsRunningFallback - process %d is running (can't verify if it's the same executable)", otherPid)
	return true
}

// rotatingFileWriter is an io.Writer that writes to a single log file and
// rotates it when the size exceeds maxSize bytes.
type rotatingFileWriter struct {
	mu       sync.Mutex
	dir      string
	baseName string
	file     *os.File
	size     int64
	maxSize  int64
}

func newRotatingFileWriter(dir, baseName string, maxSize int64) (*rotatingFileWriter, error) {
	w := &rotatingFileWriter{
		dir:      dir,
		baseName: baseName,
		maxSize:  maxSize,
	}
	if err := w.openCurrent(); err != nil {
		return nil, err
	}
	return w, nil
}

func (w *rotatingFileWriter) Write(p []byte) (int, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.file == nil {
		if err := w.openCurrent(); err != nil {
			return 0, err
		}
	}

	// Rotate if this write would exceed maxSize.
	if w.size+int64(len(p)) > w.maxSize {
		if err := w.rotate(); err != nil {
			return 0, err
		}
	}

	n, err := w.file.Write(p)
	w.size += int64(n)
	return n, err
}

func (w *rotatingFileWriter) openCurrent() error {
	path := filepath.Join(w.dir, w.baseName)
	f, err := os.OpenFile(path, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0o644)
	if err != nil {
		return err
	}
	info, err := f.Stat()
	if err != nil {
		_ = f.Close()
		return err
	}
	w.file = f
	w.size = info.Size()
	return nil
}

func (w *rotatingFileWriter) rotate() error {
	if w.file != nil {
		_ = w.file.Close()
	}

	currentPath := filepath.Join(w.dir, w.baseName)
	rotatedPath := currentPath + ".1"

	// Keep only two files: base and base.1
	// Remove previous .1 (if any), then move current -> .1
	_ = os.Remove(rotatedPath)
	_ = os.Rename(currentPath, rotatedPath)

	// Open a fresh current file.
	f, err := os.OpenFile(currentPath, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0o644)
	if err != nil {
		return err
	}
	w.file = f
	w.size = 0
	return nil
}
