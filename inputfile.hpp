#pragma once
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;


struct InputFile {
	vector<string> lines, tok;
	int lno = -1, pos = 0;

	
	// loading files
	int load(const string& fname) {
		// reset
		lines = tok = {};
		lno = -1, pos = 0;
		// load
		fstream fs(fname, ios::in);
		if (!fs.is_open())
			return fprintf(stderr, "error loading file %s\n", fname.c_str()), 1;
		string s;
		while (getline(fs, s))
			lines.push_back(s);
		printf("loaded file: %s (%d)\n", fname.c_str(), (int)lines.size());
		// if (lines.size())  tok = tokenize(lines[lno]);  // load first line into tok
		nextline();  // load first line into tok
		return 0;
	}


	// convert line string to token
	static vector<string> tokenize(const string& line) {
		vector<string> vs;
		string s;
		for (int i=0; i<line.length(); i++) {
			char c = line[i];
			if      (isspace(c))  s = s.length() ? (vs.push_back(s), "") : "";
			else if (isalnum(c) || c == '_')  s += c;
			else if (c == '"') {
				s = s.length() ? (vs.push_back(s), "") : "";
				s += c;
				for (++i; i<line.length(); i++) {
					s += line[i];
					if (line[i] == '"')  break;
				}
			}
			else if (c == '#') {
				s = s.length() ? (vs.push_back(s), "") : "";
				vs.push_back(line.substr(i));
				break;
			}
			else {
				s = s.length() ? (vs.push_back(s), "") : "";
				vs.push_back(string() + c);
			}
		}
		if (s.length())  vs.push_back(s);
		return vs;
	}


	// state display
	void showlines() const {
		for (int i=0; i<lines.size(); i++)
			showtokens( tokenize(lines[i]), i );
	}
	static void showtokens(const vector<string>& tok, int lno=0) {
		printf("%02d  ::  ", lno+1);
		for (auto &t : tok)
			printf("[%s]  ", t.c_str());
		printf("\n");
	}


	// token based parsing
	int eol() const {
		return pos >= tok.size();
	}
	int eof() const {
		return lno >= lines.size();
	}
	const string& peek(int ahead=0) const {
		static const string& blank = "";
		return pos+ahead >= tok.size() ? blank : tok[pos+ahead];
	}
	const string& get() {
		if (pos >= tok.size())  throw IBError("expected token", lno);
		return tok[pos++];
		// pos++;
		// return tok[pos-1];
	}
	int expect(const string& s) {
		if (get() == s)  return 1;
		throw IBError("expected token '"+s+"'", lno);
	}
	int expecttype(const string& type) {
		string s;
		return expecttype(type, s);
	}
	int expecttype(const string& type, string& s) {
		if      (type == "eol")         { if (peek() == "" || is_comment(s = get()))  return 1; }
		else if (type == "integer")     { if (is_integer(s = get()))  return 1; }
		else if (type == "literal")     { if (is_literal(s = get()))  return 1; }
		else if (type == "identifier")  { if (is_identifier(s = get()))  return 1; }
		throw IBError("expected token of type "+type, lno);
	}
	int nextline() {
		lno++, pos = 0;
		if (lno >= 0 && lno < lines.size())
			return tok = tokenize(lines.at(lno)), 1;
		return 0;
	}
	// int jump(int line) {
	// 	lno = line, pos = 0;
	// 	tok = tokenize(lines.at(lno));
	// 	return 1;
	// }
};