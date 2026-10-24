package main

// This PC's SSH key for the SS1 (Settings > Connection > SSH): an ed25519 key made once and kept in
// %APPDATA%\Phosphor\ssh_key (phosphor-link logs in with it too, so "Keys only" on the SS1 still lets it
// in). Offered to the SS1 together with the public keys in %USERPROFILE%\.ssh; each one only gets in
// after a yes on the TV. The private key never leaves this PC.

import (
	"crypto/ed25519"
	"crypto/rand"
	"crypto/x509"
	"encoding/base64"
	"encoding/binary"
	"encoding/pem"
	"net/http"
	"os"
	"path/filepath"
	"strings"
)

func sshKeyPath() string { return filepath.Join(configDir(), "ssh_key") }

func sshWireString(b []byte) []byte {
	out := make([]byte, 4+len(b))
	binary.BigEndian.PutUint32(out, uint32(len(b)))
	copy(out[4:], b)
	return out
}

// the public half in authorized_keys form, making the key first if there isn't one
func phosphorSSHKey() (string, error) {
	p := sshKeyPath()
	if b, err := os.ReadFile(p); err == nil {
		if blk, _ := pem.Decode(b); blk != nil {
			if k, err := x509.ParsePKCS8PrivateKey(blk.Bytes); err == nil {
				if ek, ok := k.(ed25519.PrivateKey); ok {
					return authorizedLine(ek.Public().(ed25519.PublicKey)), nil
				}
			}
		}
	}
	pub, priv, err := ed25519.GenerateKey(rand.Reader)
	if err != nil {
		return "", err
	}
	der, err := x509.MarshalPKCS8PrivateKey(priv)
	if err != nil {
		return "", err
	}
	if err := os.MkdirAll(configDir(), 0o700); err != nil {
		return "", err
	}
	if err := os.WriteFile(p+".new", pem.EncodeToMemory(&pem.Block{Type: "PRIVATE KEY", Bytes: der}), 0o600); err != nil {
		return "", err
	}
	if err := os.Rename(p+".new", p); err != nil {
		return "", err
	}
	line := authorizedLine(pub)
	_ = os.WriteFile(p+".pub", []byte(line+" phosphor\n"), 0o644)
	return line, nil
}

func authorizedLine(pub ed25519.PublicKey) string {
	blob := append(sshWireString([]byte("ssh-ed25519")), sshWireString(pub)...)
	return "ssh-ed25519 " + base64.StdEncoding.EncodeToString(blob)
}

// GET /api/sshkeys: the keys this PC can offer: Phosphor's own, then the ones in .ssh
func apiSSHKeys(w http.ResponseWriter, r *http.Request) {
	type key struct {
		Key  string `json:"key"`
		Name string `json:"name"`
	}
	var keys []key
	if k, err := phosphorSSHKey(); err == nil {
		keys = append(keys, key{k, "Phosphor app and phosphor-link"})
	}
	if home, err := os.UserHomeDir(); err == nil {
		files, _ := filepath.Glob(filepath.Join(home, ".ssh", "*.pub"))
		for _, f := range files {
			b, err := os.ReadFile(f)
			if err != nil || len(b) > 16<<10 {
				continue
			}
			line := strings.TrimSpace(strings.SplitN(string(b), "\n", 2)[0])
			if strings.HasPrefix(line, "ssh-") || strings.HasPrefix(line, "ecdsa-") || strings.HasPrefix(line, "sk-") {
				keys = append(keys, key{line, filepath.Base(f)})
			}
		}
	}
	writeJSON(w, map[string]any{"ok": true, "keys": keys})
}
