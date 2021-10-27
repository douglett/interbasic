#pragma once
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;


struct InputFile {
	struct codemap_inner_t {
		string type;
		int pos, top;
	};
	struct codemap_t {
		string type;
		int start, end;
		string fname;
		// vector<int> elselist;
		vector<codemap_inner_t> inner;
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
		codemap_build();  // make a map of all nested elements
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
	void codemap_build() {
		vector<int> nest;  // holds which nested blocks we are currently working on
		vector<int> nest_while;  // special nest for while command, to handle break statements
		lno = -1;  // reset pos
		while (nextline()) {
			auto& cmd = peek() == "" ? peek() : get();  // get line command
			if (cmd == "if" || cmd == "while" || cmd == "function") {
				codemap.push_back({ cmd, lno, -1 });
				if      (cmd == "function")  codemap.back().fname = get();
				else if (cmd == "while")     nest_while.push_back(codemap.size()-1);  // nest while-block
				nest.push_back(codemap.size()-1);  // nest block
			}
			else if (cmd == "else") {
				if (nest.size() == 0 || codemap.at(nest.back()).type != "if")  throw IBError("unexpected 'else' outside of if", lno);
				auto& cm = codemap.at(nest.back());
				string type = peek() == "if" ? "else-if" : "else";
				cm.inner.push_back({ type, lno, cm.start });
			}
			else if (cmd == "break" || cmd == "continue") {
				if (nest_while.size() == 0)  throw IBError(cmd+" outside of while", lno);
				auto& cm = codemap.at(nest_while.back());
				cm.inner.push_back({ cmd, lno, cm.start });
			}
			else if (cmd == "end") {
				auto& type = peek();
				if (nest.size() == 0 || codemap.at(nest.back()).type != type)  throw IBError("unexpected 'end "+type+"'", lno);
				if (type == "while")  nest_while.pop_back();  // un-nest whiles
				codemap.at(nest.back()).end = lno;
				nest.pop_back();  // un-nest
			}
		}
		// show results
		// for (const auto& c : codemap) {
		// 	printf("%02d : %02d   %s  %s\n", c.start+1, c.end+1, c.type.c_str(), c.fname.c_str());
		// 	for (auto& in : c.inner)
		// 		printf("   %02d   %s\n", in.pos, in.type.c_str());
		// }
	}

	const codemap_t& codemap_get(int line) const {
		for (auto& c : codemap) {
			if (c.start == line || c.end == line)  return c;
			for (auto& in : c.inner)
				if (in.pos == line)  return c;
		}
		throw IBError("codemap_get: no match for line "+to_string(line+1), lno);
	}
	const codemap_t& codemap_getfunc(const string& fname) const {
		for (auto& c : codemap)
			if (c.type == "function" && c.fname == fname)  return c;
		throw IBError("codemap_getfunc: function not found '"+fname+"'", lno);
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
		// return pos >= tok.size();
		return peek() == "" || is_comment(peek());
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
		if      (type == "eol")         { if (eol())  return s = peek(), 1; }
		else if (type == "integer")     { if (is_integer(peek()))  return s = get(), 1; }
		else if (type == "literal")     { if (is_literal(peek()))  return s = get(), 1; }
		else if (type == "identifier")  { if (is_identifier(peek()))  return s = get(), 1; }
		throw IBError("expected token; expected ["+type+"], got ["+peek()+"]", lno);
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