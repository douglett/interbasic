#pragma once
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;


struct InputFile {
	struct codemap_t {
		string type;
		int start, end;
		vector<int> elselist;
	};
	vector<string>     lines, tok;
	vector<codemap_t>  codemap;
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
		createcodemap();  // make a map of all nested elements
		lno = -1;  nextline();  // load first line into tok
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


	// create code map
	void createcodemap() {
		vector<int> nest;  // holds which nested blocks we are currently working on
		lno = -1;  // reset pos
		while (nextline()) {
			auto& cmd = peek() == "" ? peek() : get();  // get line command
			if (cmd == "if" || cmd == "while" || cmd == "function") {
				codemap.push_back({ cmd, lno, -1 });
				nest.push_back(codemap.size()-1);  // nest block
			}
			else if (cmd == "else") {
				if (codemap.at(nest.at(nest.size()-1)).type != "if")  throw IBError("unexpected 'else' outside of if", lno);
				codemap.at(nest.back()).elselist.push_back(lno);  // special case with if/else
			}
			else if (cmd == "end") {
				auto& type = peek();  // get end if/while/function type
				if (codemap.at(nest.at(nest.size()-1)).type != type)  throw IBError("unexpected 'end "+type+"'", lno);
				codemap.at(nest.back()).end = lno;
				nest.pop_back();  // un-nest
			}
		}
		// show results
		// for (const auto& m : codemap) {
		// 	printf("%02d : %02d   %s\n", m.start+1, m.end+1, m.type.c_str());
		// 	for (int e : m.elselist)
		// 		printf("   %02d   else\n", e+1);
		// }
	}

	const codemap_t& getcodemap(int line) const {
		for (auto& c : codemap) {
			if (c.start == line || c.end == line)  return c;
			for (int e : c.elselist)
				if (e == line)  return c;
		}
		throw IBError("getcodemap: no match for line "+to_string(line+1), lno);
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
		return pos+ahead < 0 || pos+ahead >= tok.size() ? blank : tok[pos+ahead];
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