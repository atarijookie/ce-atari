package main

import (
	"log"
	"net/http"
	"os"
	"sort"
)

// Host-related handlers.

func (s *Server) handleHostDir(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleHostDir %s %s", r.Method, r.URL.Path)

	path := r.URL.Query().Get("path")
	if path == "" {
		path = "."
	}
	entries, err := os.ReadDir(path)
	if err != nil {
		http.Error(w, "cannot read directory", http.StatusBadRequest)
		return
	}
	
	// Separate directories and files
	dirs := make([]string, 0)
	files := make([]string, 0)
	for _, e := range entries {
		if e.IsDir() {
			dirs = append(dirs, e.Name())
		} else {
			files = append(files, e.Name())
		}
	}
	
	// Sort both lists
	sort.Strings(dirs)
	sort.Strings(files)
	
	// Return in the requested format
	response := map[string]interface{}{
		"dirs":  dirs,
		"files": files,
	}
	writeJSON(w, http.StatusOK, response)
}

func (s *Server) handleHostDevices(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleHostDevices %s %s", r.Method, r.URL.Path)

	s.mu.RLock()
	defer s.mu.RUnlock()
	writeJSON(w, http.StatusOK, s.hostDevices)
}
