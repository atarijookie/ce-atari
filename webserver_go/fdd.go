package main

import (
	"encoding/json"
	"log"
	"net/http"

	"github.com/go-chi/chi/v5"
)

// FDD-related handlers.

func (s *Server) handleGetFDDImage(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetFDDImage %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	s.mu.RLock()
	defer s.mu.RUnlock()
	image, ok := s.fddImage[mac]
	if !ok {
		http.Error(w, "unknown mac", http.StatusNotFound)
		return
	}
	writeJSON(w, http.StatusOK, map[string]string{"mac": mac, "image": image})
}

func (s *Server) handlePutFDDImage(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePutFDDImage %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	var body struct {
		Image string `json:"image"`
	}
	if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}
	if body.Image == "" {
		http.Error(w, "image is required", http.StatusBadRequest)
		return
	}
	s.mu.Lock()
	s.fddImage[mac] = body.Image
	s.mu.Unlock()
	w.WriteHeader(http.StatusNoContent)
}
