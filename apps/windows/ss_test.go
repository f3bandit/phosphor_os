package main

import "testing"

func TestSSAnswer(t *testing.T) {
	for _, c := range []struct {
		rc, http int
		body     string
		stop     bool
		has      string
	}{
		{2, 403, "Erreur de login : Vérifier vos identifiants développeur !", true, "developer ID"},
		{2, 403, "Erreur de login : Vérifier les identifiants utilisateurs !", true, "your login"},
		{1, 430, "", true, "used up"},
		{1, 423, "", true, "maintenance"},
		{-1, 0, "", false, "Can't reach"},
		{0, 200, `{"response":{"jeu":{}}}`, false, ""},
		{1, 404, "Erreur : Rom/Iso/Dossier non trouvée !", false, ""},
	} {
		m, stop := ssAnswer(c.rc, c.http, []byte(c.body))
		if stop != c.stop || (c.has == "" && m != "") || (c.has != "" && !contains(m, c.has)) {
			t.Errorf("%d %d %q: %q %v", c.rc, c.http, c.body, m, stop)
		}
	}
	acct := func(m map[string]string) func(string, string) string { return func(_, k string) string { return m[k] } }
	if m := ssMissing(acct(map[string]string{"user": "u", "password": "p"})); !contains(m, "developer ID") {
		t.Error("a login alone isn't enough:", m)
	}
	if m := ssMissing(acct(map[string]string{"user": "u", "password": "p", "devid": "d", "devpassword": "x"})); m != "" {
		t.Error(m)
	}
	ssBuiltinID, ssBuiltinPw = "phosphor", "secret"
	defer func() { ssBuiltinID, ssBuiltinPw = "", "" }()
	if m := ssMissing(acct(map[string]string{"user": "u", "password": "p"})); m != "" {
		t.Error("with Phosphor's own developer ID, the login is enough:", m)
	}
	if id, _ := ssDevLogin(acct(map[string]string{"user": "u", "password": "p", "devid": "mine"})); id != "phosphor" {
		t.Error(id)
	}
}

func contains(s, sub string) bool { return len(sub) == 0 || (len(s) >= len(sub) && indexOf(s, sub) >= 0) }
func indexOf(s, sub string) int {
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub {
			return i
		}
	}
	return -1
}
