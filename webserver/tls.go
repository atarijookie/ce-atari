package main

import (
	"bufio"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"errors"
	"fmt"
	"log"
	"math/big"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

// Self-signed HTTPS: on start a local CA is created in SETTINGS_DIR/tls (once, long validity),
// and a server certificate signed by that CA is issued for the current hostname and IP addresses.
// The server certificate is re-issued automatically before it expires or when the IPs change.
// Installing the CA certificate (downloadable at /ca.crt) in the OS / browser removes the warnings.

const (
	caValidity            = 20 * 365 * 24 * time.Hour
	serverCertValidity    = 825 * 24 * time.Hour // Apple rejects longer validity, even for private CAs
	serverCertRenewBefore = 30 * 24 * time.Hour  // re-issue when expiring within this time
	certCheckInterval     = 1 * time.Hour        // how often to check expiry and IP changes
)

// The CA may only sign certificates for local names and private networks, so if its key ever leaked,
// it couldn't be used to impersonate other sites on machines where it is installed.
var caPermittedDNSDomains = []string{"localhost", "local", "lan", "home", "internal", "home.arpa"}

var caPermittedIPRanges = []string{
	"127.0.0.0/8", "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "169.254.0.0/16",
	"100.64.0.0/10", // CGNAT range, used by e.g. Tailscale
	"::1/128", "fc00::/7", "fe80::/10",
}

func tlsDir() string {
	// Get SETTINGS_DIR from environment, default to current directory if not set
	settingsDir := os.Getenv("SETTINGS_DIR")
	if settingsDir == "" {
		settingsDir = "."
	}
	return filepath.Join(settingsDir, "tls")
}

func caCertPath() string     { return filepath.Join(tlsDir(), "ca.crt") }
func caKeyPath() string      { return filepath.Join(tlsDir(), "ca.key") }
func serverCertPath() string { return filepath.Join(tlsDir(), "server.crt") }
func serverKeyPath() string  { return filepath.Join(tlsDir(), "server.key") }

// certManager holds the current server certificate and re-issues it when needed.
type certManager struct {
	mu    sync.RWMutex
	cert  *tls.Certificate
	ca    *x509.Certificate
	caKey *ecdsa.PrivateKey
	caPEM []byte
}

func newCertManager() (*certManager, error) {
	if err := os.MkdirAll(tlsDir(), 0o700); err != nil {
		return nil, err
	}

	m := &certManager{}
	if err := m.loadOrCreateCA(); err != nil {
		return nil, fmt.Errorf("CA: %w", err)
	}
	if err := m.refresh(); err != nil {
		return nil, fmt.Errorf("server certificate: %w", err)
	}

	// periodically check expiry and IP address changes
	go func() {
		ticker := time.NewTicker(certCheckInterval)
		defer ticker.Stop()
		for range ticker.C {
			if err := m.refresh(); err != nil {
				log.Printf("certManager - refresh failed: %v", err)
			}
		}
	}()

	return m, nil
}

func (m *certManager) getCertificate(*tls.ClientHelloInfo) (*tls.Certificate, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.cert, nil
}

// loadOrCreateCA loads the CA from SETTINGS_DIR/tls, or creates a new one if it's missing or unusable.
func (m *certManager) loadOrCreateCA() error {
	ca, caKey, caPEM, err := loadCertAndKey(caCertPath(), caKeyPath())
	if err == nil && ca.IsCA && time.Now().Before(ca.NotAfter) {
		m.ca, m.caKey, m.caPEM = ca, caKey, caPEM
		return nil
	}
	log.Printf("certManager - creating new CA (reason: %v)", errOrExpired(err))

	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return err
	}

	var permittedIPs []*net.IPNet
	for _, cidr := range caPermittedIPRanges {
		_, ipNet, _ := net.ParseCIDR(cidr)
		permittedIPs = append(permittedIPs, ipNet)
	}

	// the hostname is permitted too, so https://hostname/ works without a domain suffix
	permittedDNS := append([]string{}, caPermittedDNSDomains...)
	if host := shortHostname(); host != "" {
		permittedDNS = append(permittedDNS, host)
	}

	serial, err := randomSerial()
	if err != nil {
		return err
	}
	now := time.Now()
	template := &x509.Certificate{
		SerialNumber:                serial,
		Subject:                     pkix.Name{CommonName: "CosmosEx local CA " + serial.Text(16)[:8], Organization: []string{"CosmosEx"}},
		NotBefore:                   now.Add(-1 * time.Hour),
		NotAfter:                    now.Add(caValidity),
		IsCA:                        true,
		BasicConstraintsValid:       true,
		MaxPathLenZero:              true,
		KeyUsage:                    x509.KeyUsageCertSign | x509.KeyUsageCRLSign,
		PermittedDNSDomainsCritical: true,
		PermittedDNSDomains:         permittedDNS,
		PermittedIPRanges:           permittedIPs,
	}

	der, err := x509.CreateCertificate(rand.Reader, template, template, &key.PublicKey, key)
	if err != nil {
		return err
	}
	if err := writeCertAndKey(caCertPath(), caKeyPath(), der, key); err != nil {
		return err
	}

	// a new CA invalidates the old server certificate
	os.Remove(serverCertPath())
	os.Remove(serverKeyPath())

	m.ca, _ = x509.ParseCertificate(der)
	m.caKey = key
	m.caPEM = pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: der})
	return nil
}

// refresh loads the server certificate, and re-issues it if it's missing, expiring, not signed by
// the current CA, or doesn't cover the current hostname / IP addresses.
func (m *certManager) refresh() error {
	dnsNames, ips := m.wantedNames()

	cert, key, _, err := loadCertAndKey(serverCertPath(), serverKeyPath())
	reason := ""
	switch {
	case err != nil:
		reason = err.Error()
	case cert.CheckSignatureFrom(m.ca) != nil:
		reason = "not signed by current CA"
	case time.Until(cert.NotAfter) < serverCertRenewBefore:
		reason = "expiring on " + cert.NotAfter.Format("2006-01-02")
	case !coversNames(cert, dnsNames, ips):
		reason = "hostname or IP addresses changed"
	}

	if reason != "" {
		log.Printf("certManager - issuing server certificate for %v %v (reason: %s)", dnsNames, ips, reason)
		if cert, key, err = m.issueServerCert(dnsNames, ips); err != nil {
			return err
		}
	} else {
		m.mu.RLock()
		loaded := m.cert != nil
		m.mu.RUnlock()
		if loaded {
			return nil // nothing changed
		}
	}

	tlsCert := &tls.Certificate{
		Certificate: [][]byte{cert.Raw},
		PrivateKey:  key,
		Leaf:        cert,
	}
	m.mu.Lock()
	m.cert = tlsCert
	m.mu.Unlock()
	return nil
}

func (m *certManager) issueServerCert(dnsNames []string, ips []net.IP) (*x509.Certificate, *ecdsa.PrivateKey, error) {
	return m.issueServerCertValidFor(dnsNames, ips, serverCertValidity)
}

func (m *certManager) issueServerCertValidFor(dnsNames []string, ips []net.IP, validity time.Duration) (*x509.Certificate, *ecdsa.PrivateKey, error) {
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, nil, err
	}
	serial, err := randomSerial()
	if err != nil {
		return nil, nil, err
	}

	commonName := "localhost"
	if len(dnsNames) > 0 {
		commonName = dnsNames[0]
	}

	now := time.Now()
	template := &x509.Certificate{
		SerialNumber: serial,
		Subject:      pkix.Name{CommonName: commonName, Organization: []string{"CosmosEx"}},
		NotBefore:    now.Add(-1 * time.Hour),
		NotAfter:     now.Add(validity),
		KeyUsage:     x509.KeyUsageDigitalSignature,
		ExtKeyUsage:  []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
		DNSNames:     dnsNames,
		IPAddresses:  ips,
	}

	der, err := x509.CreateCertificate(rand.Reader, template, m.ca, &key.PublicKey, m.caKey)
	if err != nil {
		return nil, nil, err
	}
	if err := writeCertAndKey(serverCertPath(), serverKeyPath(), der, key); err != nil {
		return nil, nil, err
	}
	cert, err := x509.ParseCertificate(der)
	return cert, key, err
}

// wantedNames returns hostnames and IP addresses the server certificate should cover,
// limited to those the CA is permitted to sign (others would make the whole certificate invalid).
func (m *certManager) wantedNames() ([]string, []net.IP) {
	var dnsNames []string
	if host := shortHostname(); host != "" {
		dnsNames = append(dnsNames, host, host+".local", host+".lan", host+".home")
	}
	dnsNames = append(dnsNames, "localhost")

	ips := []net.IP{net.ParseIP("127.0.0.1"), net.ParseIP("::1")}
	if addrs, err := net.InterfaceAddrs(); err == nil {
		for _, addr := range addrs {
			if ipNet, ok := addr.(*net.IPNet); ok && !ipNet.IP.IsLoopback() {
				ips = append(ips, ipNet.IP)
			}
		}
	}

	var okNames []string
	for _, name := range dnsNames {
		if m.caPermitsDNS(name) {
			okNames = append(okNames, name)
		}
	}
	var okIPs []net.IP
	for _, ip := range ips {
		if m.caPermitsIP(ip) {
			okIPs = append(okIPs, ip)
		}
	}
	return okNames, okIPs
}

func (m *certManager) caPermitsDNS(name string) bool {
	if len(m.ca.PermittedDNSDomains) == 0 {
		return true
	}
	for _, domain := range m.ca.PermittedDNSDomains {
		if name == domain || strings.HasSuffix(name, "."+domain) {
			return true
		}
	}
	return false
}

func (m *certManager) caPermitsIP(ip net.IP) bool {
	if len(m.ca.PermittedIPRanges) == 0 {
		return true
	}
	for _, ipNet := range m.ca.PermittedIPRanges {
		if ipNet.Contains(ip) {
			return true
		}
	}
	return false
}

// coversNames reports whether cert contains all the wanted DNS names and IP addresses.
func coversNames(cert *x509.Certificate, dnsNames []string, ips []net.IP) bool {
	have := map[string]bool{}
	for _, name := range cert.DNSNames {
		have[name] = true
	}
	for _, ip := range cert.IPAddresses {
		have[ip.String()] = true
	}
	for _, name := range dnsNames {
		if !have[name] {
			return false
		}
	}
	for _, ip := range ips {
		if !have[ip.String()] {
			return false
		}
	}
	return true
}

// shortHostname returns the lowercase hostname without domain, or "" if it can't be used in a certificate.
func shortHostname() string {
	host, err := os.Hostname()
	if err != nil {
		return ""
	}
	host = strings.ToLower(strings.SplitN(host, ".", 2)[0])
	if host == "" || host == "localhost" || strings.ContainsAny(host, " _") {
		return ""
	}
	return host
}

func randomSerial() (*big.Int, error) {
	return rand.Int(rand.Reader, new(big.Int).Lsh(big.NewInt(1), 128))
}

func errOrExpired(err error) string {
	if err != nil {
		return err.Error()
	}
	return "expired or not a CA"
}

func loadCertAndKey(certPath, keyPath string) (*x509.Certificate, *ecdsa.PrivateKey, []byte, error) {
	certPEM, err := os.ReadFile(certPath)
	if err != nil {
		return nil, nil, nil, err
	}
	keyPEM, err := os.ReadFile(keyPath)
	if err != nil {
		return nil, nil, nil, err
	}

	certBlock, _ := pem.Decode(certPEM)
	keyBlock, _ := pem.Decode(keyPEM)
	if certBlock == nil || keyBlock == nil {
		return nil, nil, nil, errors.New("invalid PEM in " + certPath + " or " + keyPath)
	}

	cert, err := x509.ParseCertificate(certBlock.Bytes)
	if err != nil {
		return nil, nil, nil, err
	}
	key, err := x509.ParseECPrivateKey(keyBlock.Bytes)
	if err != nil {
		return nil, nil, nil, err
	}
	return cert, key, certPEM, nil
}

func writeCertAndKey(certPath, keyPath string, der []byte, key *ecdsa.PrivateKey) error {
	keyDER, err := x509.MarshalECPrivateKey(key)
	if err != nil {
		return err
	}
	if err := writeFileAtomic(keyPath, pem.EncodeToMemory(&pem.Block{Type: "EC PRIVATE KEY", Bytes: keyDER}), 0o600); err != nil {
		return err
	}
	return writeFileAtomic(certPath, pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: der}), 0o644)
}

// writeFileAtomic writes via temp file + rename, so the file is never half-written.
func writeFileAtomic(path string, data []byte, perm os.FileMode) error {
	tmpPath := path + ".tmp"
	if err := os.WriteFile(tmpPath, data, perm); err != nil {
		return err
	}
	return os.Rename(tmpPath, path)
}

// handleCACert serves the CA certificate, so users can install it and get rid of browser warnings.
func (m *certManager) handleCACert(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/x-x509-ca-cert")
	w.Header().Set("Content-Disposition", `attachment; filename="cosmosex-ca.crt"`)
	w.Write(m.caPEM)
}

//------------------------------------------------------------------------------
// HTTP and HTTPS on the same port: TLS connections start with a handshake record (0x16),
// anything else is plain HTTP and gets redirected to https:// on the same host and port.

// listenAndServeHTTPS serves handler over HTTPS on addr, and redirects plain HTTP on addr to HTTPS.
func listenAndServeHTTPS(addr string, handler http.Handler, certs *certManager) error {
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}

	tlsLn := newChanListener(ln.Addr())
	plainLn := newChanListener(ln.Addr())

	go func() {
		for {
			conn, err := ln.Accept()
			if err != nil {
				tlsLn.close(err)
				plainLn.close(err)
				return
			}
			go sniffConn(conn, tlsLn, plainLn)
		}
	}()

	redirectServer := &http.Server{
		Handler:           http.HandlerFunc(redirectToHTTPS),
		ReadHeaderTimeout: 10 * time.Second,
	}
	go redirectServer.Serve(plainLn)

	tlsServer := &http.Server{
		Handler:           handler,
		ReadHeaderTimeout: 10 * time.Second,
		TLSConfig: &tls.Config{
			MinVersion:     tls.VersionTLS12,
			GetCertificate: certs.getCertificate,
		},
	}
	return tlsServer.ServeTLS(tlsLn, "", "")
}

// sniffConn peeks at the first byte of conn and passes it to the TLS or the plain HTTP listener.
func sniffConn(conn net.Conn, tlsLn, plainLn *chanListener) {
	conn.SetReadDeadline(time.Now().Add(10 * time.Second))
	br := bufio.NewReader(conn)
	first, err := br.Peek(1)
	conn.SetReadDeadline(time.Time{})
	if err != nil {
		conn.Close()
		return
	}

	peeked := &peekedConn{Conn: conn, r: br}
	if first[0] == 0x16 { // TLS handshake record
		tlsLn.push(peeked)
	} else {
		plainLn.push(peeked)
	}
}

func redirectToHTTPS(w http.ResponseWriter, r *http.Request) {
	target := "https://" + r.Host + r.URL.RequestURI()
	log.Printf("redirectToHTTPS - %s %s -> %s", r.Method, r.URL.RequestURI(), target)
	// 307 keeps the method (POST stays POST) and isn't cached permanently by browsers
	http.Redirect(w, r, target, http.StatusTemporaryRedirect)
}

// peekedConn is a net.Conn whose already peeked bytes are read from the bufio.Reader first.
type peekedConn struct {
	net.Conn
	r *bufio.Reader
}

func (c *peekedConn) Read(p []byte) (int, error) { return c.r.Read(p) }

// chanListener is a net.Listener fed with connections from a channel.
type chanListener struct {
	addr  net.Addr
	conns chan net.Conn
	done  chan struct{}
	once  sync.Once
	err   error
}

func newChanListener(addr net.Addr) *chanListener {
	return &chanListener{addr: addr, conns: make(chan net.Conn), done: make(chan struct{})}
}

func (l *chanListener) push(conn net.Conn) {
	select {
	case l.conns <- conn:
	case <-l.done:
		conn.Close()
	}
}

func (l *chanListener) close(err error) {
	l.once.Do(func() {
		l.err = err
		close(l.done)
	})
}

func (l *chanListener) Accept() (net.Conn, error) {
	select {
	case conn := <-l.conns:
		return conn, nil
	case <-l.done:
		return nil, l.err
	}
}

func (l *chanListener) Close() error {
	l.close(net.ErrClosed)
	return nil
}

func (l *chanListener) Addr() net.Addr { return l.addr }
