package main

import (
	"crypto/tls"
	"crypto/x509"
	"net"
	"net/http"
	"os"
	"testing"
	"time"
)

func newTestCertManager(t *testing.T) *certManager {
	t.Helper()
	useTempSettingsDir(t)
	m := &certManager{}
	if err := os.MkdirAll(tlsDir(), 0o700); err != nil {
		t.Fatal(err)
	}
	if err := m.loadOrCreateCA(); err != nil {
		t.Fatalf("loadOrCreateCA: %v", err)
	}
	if err := m.refresh(); err != nil {
		t.Fatalf("refresh: %v", err)
	}
	return m
}

func TestServerCertVerifiesAgainstCA(t *testing.T) {
	m := newTestCertManager(t)

	roots := x509.NewCertPool()
	roots.AddCert(m.ca)
	leaf := m.cert.Leaf

	// verify for every name in the certificate, so name constraints are checked too
	names := append([]string{}, leaf.DNSNames...)
	for _, ip := range leaf.IPAddresses {
		names = append(names, ip.String())
	}
	for _, name := range names {
		if _, err := leaf.Verify(x509.VerifyOptions{DNSName: name, Roots: roots}); err != nil {
			t.Errorf("verify for %q failed: %v", name, err)
		}
	}

	if leaf.NotAfter.Sub(leaf.NotBefore) > serverCertValidity+2*time.Hour {
		t.Errorf("server certificate validity too long: %v", leaf.NotAfter.Sub(leaf.NotBefore))
	}
}

func TestCAConstraintsRejectPublicNames(t *testing.T) {
	m := newTestCertManager(t)

	// a certificate for a public site signed by our CA must not verify
	cert, _, err := m.issueServerCert([]string{"www.google.com"}, []net.IP{net.ParseIP("8.8.8.8")})
	if err != nil {
		t.Fatal(err)
	}
	roots := x509.NewCertPool()
	roots.AddCert(m.ca)
	if _, err := cert.Verify(x509.VerifyOptions{DNSName: "www.google.com", Roots: roots}); err == nil {
		t.Errorf("expected public DNS name to be rejected by CA name constraints")
	}
	if _, err := cert.Verify(x509.VerifyOptions{DNSName: "8.8.8.8", Roots: roots}); err == nil {
		t.Errorf("expected public IP to be rejected by CA name constraints")
	}
}

func TestCertsPersistAndRenew(t *testing.T) {
	m := newTestCertManager(t)
	serial := m.cert.Leaf.SerialNumber

	// restart: same CA and server certificate are loaded from disk
	m2 := &certManager{}
	if err := m2.loadOrCreateCA(); err != nil {
		t.Fatal(err)
	}
	if err := m2.refresh(); err != nil {
		t.Fatal(err)
	}
	if !m2.ca.Equal(m.ca) {
		t.Fatalf("expected the same CA after restart")
	}
	if m2.cert.Leaf.SerialNumber.Cmp(serial) != 0 {
		t.Fatalf("expected the same server certificate after restart")
	}

	// expiring server certificate (written to disk) gets re-issued by the same CA
	dnsNames, ips := m2.wantedNames()
	shortCert, _, err := m2.issueServerCertValidFor(dnsNames, ips, serverCertRenewBefore-time.Hour)
	if err != nil {
		t.Fatal(err)
	}
	if time.Until(shortCert.NotAfter) >= serverCertRenewBefore {
		t.Fatal("test setup: certificate should be within renewal window")
	}
	m2.cert = nil
	if err := m2.refresh(); err != nil {
		t.Fatal(err)
	}
	if time.Until(m2.cert.Leaf.NotAfter) < serverCertRenewBefore {
		t.Fatalf("expected expiring certificate to be renewed, expires %v", m2.cert.Leaf.NotAfter)
	}
	if !m2.ca.Equal(m.ca) {
		t.Fatalf("renewal must keep the CA")
	}
}

func TestPlainHTTPRedirectsAndHTTPSServes(t *testing.T) {
	m := newTestCertManager(t)

	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	addr := ln.Addr().String()
	ln.Close()

	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { w.Write([]byte("hello")) })
	go listenAndServeHTTPS(addr, handler, m)

	noFollow := func(req *http.Request, via []*http.Request) error { return http.ErrUseLastResponse }

	var res *http.Response
	for i := 0; i < 50; i++ { // wait for server start
		res, err = (&http.Client{CheckRedirect: noFollow}).Get("http://" + addr + "/devices?x=1")
		if err == nil {
			break
		}
		time.Sleep(20 * time.Millisecond)
	}
	if err != nil {
		t.Fatal(err)
	}
	if res.StatusCode != http.StatusTemporaryRedirect || res.Header.Get("Location") != "https://"+addr+"/devices?x=1" {
		t.Fatalf("expected redirect to https, got %d %q", res.StatusCode, res.Header.Get("Location"))
	}

	roots := x509.NewCertPool()
	roots.AddCert(m.ca)
	client := &http.Client{Transport: &http.Transport{TLSClientConfig: &tls.Config{RootCAs: roots}}}
	res, err = client.Get("https://" + addr + "/")
	if err != nil {
		t.Fatalf("https request failed: %v", err)
	}
	if res.StatusCode != http.StatusOK {
		t.Fatalf("expected 200 over https, got %d", res.StatusCode)
	}
}
