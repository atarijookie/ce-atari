package main

import (
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"encoding/json"

	"github.com/go-chi/chi/v5"
)

// Misc / other handlers (status).

// stripMACColons removes colons from MAC address "aa:bb:cc:dd:ee:01" -> "aabbccddee01"
func stripMACColons(mac string) string {
	return strings.ReplaceAll(mac, ":", "")
}


func (s *Server) handleGetStatus(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetStatus %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	logDir := os.Getenv("LOG_DIR")
	if logDir == "" {
		logDir = "/tmp/ce/log"
	}
	statusFile := filepath.Join(logDir, mac+".txt")

	content, err := os.ReadFile(statusFile)
	if err != nil {
		msg := "File " + statusFile + " not present."
		log.Printf("handleGetStatus - %s", msg)
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(msg))
		return
	}

	cmd := map[string]string{"module": "all", "action": "generate_status", "mac": mac}
	if cmdJSON, err := json.Marshal(cmd); err == nil {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handleGetStatus - failed to send cmd socket: %v", err)
		}
	}

	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write(content)
}

