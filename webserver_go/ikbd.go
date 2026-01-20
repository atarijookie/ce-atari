package main

import (
	"encoding/json"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/go-chi/chi/v5"
)

const (
	defaultKeys0 = "A%S%D%W%LSHIFT"
	defaultKeys1 = "LEFT%DOWN%RIGHT%UP%RSHIFT"
)

type IKBDSettings struct {
	JoyFirstIs0           int    `json:"joy_first_is_0"`
	MouseWheelAsKeys      int    `json:"mouse_wheel_as_keys"`
	KeyboardJoy0          int    `json:"keyboard_joy0"`
	KeyboardJoy1          int    `json:"keyboard_joy1"`
	KeyboardKeysSettings0 string `json:"keyboard_keys_settings0"`
	KeyboardKeysSettings1 string `json:"keyboard_keys_settings1"`
}

func (s *Server) handleGetIKBD(w http.ResponseWriter, r *http.Request) {
	log.Printf("handleGetIKBD %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	macNoColons := stripMACColons(mac)

	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}

	deviceDir := filepath.Join(settingsDir, macNoColons)

	resp := IKBDSettings{
		JoyFirstIs0:           readSettingInt(deviceDir, "JOY_FIRST_IS_0"),
		MouseWheelAsKeys:      readSettingInt(deviceDir, "MOUSE_WHEEL_AS_KEYS"),
		KeyboardJoy0:          readSettingInt(deviceDir, "KEYBOARD_JOY0"),
		KeyboardJoy1:          readSettingInt(deviceDir, "KEYBOARD_JOY1"),
		KeyboardKeysSettings0: readKeySettingsWithDefault(deviceDir, "KEYBOARD_KEYS_SETTINGS0", defaultKeys0),
		KeyboardKeysSettings1: readKeySettingsWithDefault(deviceDir, "KEYBOARD_KEYS_SETTINGS1", defaultKeys1),
	}

	writeJSON(w, http.StatusOK, resp)
}

func (s *Server) handlePutIKBD(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePutIKBD %s %s", r.Method, r.URL.Path)

	mac := chi.URLParam(r, "mac")
	macNoColons := stripMACColons(mac)

	var payload IKBDSettings
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	if !isZeroOrOne(payload.JoyFirstIs0) ||
		!isZeroOrOne(payload.MouseWheelAsKeys) ||
		!isZeroOrOne(payload.KeyboardJoy0) ||
		!isZeroOrOne(payload.KeyboardJoy1) {
		http.Error(w, "numeric fields must be 0 or 1", http.StatusBadRequest)
		return
	}

	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}
	deviceDir := filepath.Join(settingsDir, macNoColons)

	if err := os.MkdirAll(deviceDir, 0o755); err != nil {
		log.Printf("handlePutIKBD - error creating directory %s: %v", deviceDir, err)
		http.Error(w, "cannot create device directory", http.StatusInternalServerError)
		return
	}

	if err := writeSettingInt(deviceDir, "JOY_FIRST_IS_0", payload.JoyFirstIs0); err != nil {
		http.Error(w, "cannot write setting", http.StatusInternalServerError)
		return
	}
	if err := writeSettingInt(deviceDir, "MOUSE_WHEEL_AS_KEYS", payload.MouseWheelAsKeys); err != nil {
		http.Error(w, "cannot write setting", http.StatusInternalServerError)
		return
	}
	if err := writeSettingInt(deviceDir, "KEYBOARD_JOY0", payload.KeyboardJoy0); err != nil {
		http.Error(w, "cannot write setting", http.StatusInternalServerError)
		return
	}
	if err := writeSettingInt(deviceDir, "KEYBOARD_JOY1", payload.KeyboardJoy1); err != nil {
		http.Error(w, "cannot write setting", http.StatusInternalServerError)
		return
	}
	if err := writeSettingString(deviceDir, "KEYBOARD_KEYS_SETTINGS0", payload.KeyboardKeysSettings0); err != nil {
		http.Error(w, "cannot write setting", http.StatusInternalServerError)
		return
	}
	if err := writeSettingString(deviceDir, "KEYBOARD_KEYS_SETTINGS1", payload.KeyboardKeysSettings1); err != nil {
		http.Error(w, "cannot write setting", http.StatusInternalServerError)
		return
	}

	// Send reload command to core
	cmd := map[string]string{"module": "ikbd", "action": "reload"}
	if cmdJSON, err := json.Marshal(cmd); err == nil {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePutIKBD - failed to send cmd socket: %v", err)
		}
	}

	w.WriteHeader(http.StatusNoContent)
}

func readSettingInt(deviceDir, fileName string) int {
	content, err := os.ReadFile(filepath.Join(deviceDir, fileName))
	if err != nil {
		return 0
	}
	val, err := strconv.Atoi(strings.TrimSpace(string(content)))
	if err != nil {
		return 0
	}
	if val != 0 {
		return 1
	}
	return 0
}

func readSettingString(deviceDir, fileName string) string {
	content, err := os.ReadFile(filepath.Join(deviceDir, fileName))
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(content))
}

func readKeySettingsWithDefault(deviceDir, fileName, def string) string {
	content, err := os.ReadFile(filepath.Join(deviceDir, fileName))
	if err != nil {
		return def
	}
	val := strings.TrimSpace(string(content))
	if val == "" {
		return def
	}
	return val
}

func writeSettingInt(deviceDir, fileName string, value int) error {
	return os.WriteFile(filepath.Join(deviceDir, fileName), []byte(strconv.Itoa(value)), 0o644)
}

func writeSettingString(deviceDir, fileName, value string) error {
	return os.WriteFile(filepath.Join(deviceDir, fileName), []byte(value), 0o644)
}

func isZeroOrOne(v int) bool {
	return v == 0 || v == 1
}

func (s *Server) handlePostHIDMouse(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePostHIDMouse %s %s", r.Method, r.URL.Path)

	// Parse JSON payload
	var payload map[string]interface{}
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	// Create base command
	cmd := map[string]interface{}{
		"module": "ikbd",
		"action": "mouse",
	}

	// Extend base command with payload content
	for key, value := range payload {
		cmd[key] = value
	}

	// Send to command socket (log error but don't fail request)
	cmdJSON, err := json.Marshal(cmd)
	if err != nil {
		log.Printf("handlePostHIDMouse - error marshaling command: %v", err)
	} else {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePostHIDMouse - failed to send cmd socket: %v", err)
		}
	}

	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) handlePostHIDKeyboard(w http.ResponseWriter, r *http.Request) {
	log.Printf("handlePostHIDKeyboard %s %s", r.Method, r.URL.Path)

	// Parse JSON payload
	var payload map[string]interface{}
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	// Create base command
	cmd := map[string]interface{}{
		"module": "ikbd",
		"action": "keyboard",
	}

	// Extend base command with payload content
	for key, value := range payload {
		cmd[key] = value
	}

	// Send to command socket (log error but don't fail request)
	cmdJSON, err := json.Marshal(cmd)
	if err != nil {
		log.Printf("handlePostHIDKeyboard - error marshaling command: %v", err)
	} else {
		if err := sendToCmdSocket(string(cmdJSON)); err != nil {
			log.Printf("handlePostHIDKeyboard - failed to send cmd socket: %v", err)
		}
	}

	w.WriteHeader(http.StatusNoContent)
}
